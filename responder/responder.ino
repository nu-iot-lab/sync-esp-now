// BY NU IOT LAB //
// REFERENCES:
// rssi: https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino

#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

#define US_TO_S 1000000
#define MS_TO_US 1000
#define GUARD_MS 90
#define TIME_PERIOD_MS 10000

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int packetReceived = 0;
RTC_DATA_ATTR int RADIO_ON_MS = 60;
RTC_DATA_ATTR int MISS_COUNT = 0;
RTC_DATA_ATTR int BACKOFF_MS = 30;
RTC_DATA_ATTR int KEEP_ON = 0;
RTC_DATA_ATTR int TURN_OFF = 0;

// variables for metrics
RTC_DATA_ATTR int _RSSI_SUM = 0;
RTC_DATA_ATTR int _TEMP_DESYNC = 0;
RTC_DATA_ATTR int _FULL_DESYNC = 0;

// flags, clock_correction
unsigned long start_time = 0;
int got_packet = 0;
int clock_correction = 0;
int rcv_done = 0;

//  Set the MASTER MAC Address
uint8_t masterAddress[] = {0x0C, 0xB8, 0x15, 0xD7, 0x77, 0x3C};
esp_now_peer_info_t masterInfo;

// Define a data structure
typedef struct struct_message
{
    unsigned int time;
    unsigned int packetNumber;
} struct_message;

struct_message myData;

// use 14th channel to decrease interference
wifi_country_t country = {
    .cc = "JP",
    .schan = 1,
    .nchan = 14,
    .max_tx_power = 20,
    .policy = WIFI_COUNTRY_POLICY_AUTO,
};

/////////////////////////////////////   RSSI  //////////////////////////////////////

int rssi_display;

typedef struct
{
    unsigned frame_ctrl : 16;
    unsigned duration_id : 16;
    uint8_t addr1[6]; /* receiver address */
    uint8_t addr2[6]; /* sender address */
    uint8_t addr3[6]; /* filtering address */
    unsigned sequence_ctrl : 16;
    uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct
{
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

// La callback que hace la magia
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
    if (type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    int rssi = ppkt->rx_ctrl.rssi;
    rssi_display = rssi;
}

//////////////////////////////////// END RSSI /////////////////////////////////

void SpecialHandler()
{
    TURN_OFF = 1;

    // the special packet does not count
    packetReceived--;
    bootCount--;
    int avg_rssi = 0;
    float succ_ratio = 0;

    if (packetReceived != 0)
        avg_rssi = int(_RSSI_SUM / packetReceived);
    int sum = 500;

    if (sum != 0)
    {
        succ_ratio = (float)(packetReceived) / (float)(sum);
    }

    Serial.printf("\n--------- RESULTS --------------\n");
    Serial.printf("Avg RSSI: %d\tTemp desyncs: %d\t Complete desyncs: %d\n", avg_rssi, _TEMP_DESYNC, _FULL_DESYNC);

    Serial.printf("Success Ratio: %.2f%%\n", succ_ratio * 100);

    Serial.print("Received Special packet\t Turning Off... \n");
    rcv_done = 1;
}

// Callback function executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    got_packet = 1;
    rcv_done = 0;
    unsigned long end_time = micros();
    unsigned long delta = micros() - start_time;
    clock_correction = int(delta / 1000 - 30);

    packetReceived++;
    memcpy(&myData, incomingData, sizeof(myData));
    esp_wifi_stop();

    // check for special packet
    if (myData.time == 0 && myData.packetNumber == 0)
    {
        SpecialHandler();
        return;
    }
    _RSSI_SUM += rssi_display;

    unsigned long sd_start = micros();
    Serial.printf("%d\t%lu\t%i\t%i\n", packetReceived, myData.time, rssi_display, myData.packetNumber);

    if ((MISS_COUNT == 0) && (bootCount != 1) && !KEEP_ON)
    {
        // Serial.printf("I will align the clock by %d ms\n", clock_correction);
        esp_sleep_enable_timer_wakeup((TIME_PERIOD_MS - GUARD_MS + clock_correction) * MS_TO_US);
    }

    rcv_done = 1;
    KEEP_ON = 0;
}

void setup()
{
    // int set_st = micros();
    Serial.begin(115200);

    // special packet check
    if (TURN_OFF)
    {
        Serial.println("I will not wake up again");
        esp_deep_sleep_start();
    }

    if (bootCount == 0)
    {
        Serial.printf("rx_pkt\tdata\trssi\ttx_pkt\n", packetReceived, myData.time, rssi_display, myData.packetNumber);
        esp_sleep_enable_timer_wakeup((TIME_PERIOD_MS - GUARD_MS + 30 ) * MS_TO_US);
    }
    else
    {
        // deep sleep schedule
        esp_sleep_enable_timer_wakeup((TIME_PERIOD_MS - GUARD_MS) * MS_TO_US);
    }

    // Increment boot number and print it every reboot
    ++bootCount;
    //Serial.printf("Boot number: %d\n", bootCount);

    // Set ESP32 as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
    esp_wifi_config_espnow_rate(WIFI_IF_AP, WIFI_PHY_RATE_LORA_250K);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("There was an error initializing ESP-NOW");
        return;
    }

    // set the country
    esp_wifi_set_country(&country);

    // Register the master
    memcpy(masterInfo.peer_addr, masterAddress, 6);
    masterInfo.channel = 14;

    // Add master
    if (esp_now_add_peer(&masterInfo) != ESP_OK)
    {
        Serial.println("There was an error registering the master");
        return;
    }

    // Register callback function
    esp_now_register_recv_cb(OnDataRecv);

    // RSSI
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

    esp_wifi_set_channel(14, WIFI_SECOND_CHAN_NONE);

    // Serial.printf("SETUP TIME MS: %d\n", (micros()- set_st)/1000 );
}

void loop()
{
    esp_wifi_start();
    start_time = micros();

    // wait indefinetly
    if (bootCount == 1 || KEEP_ON)
    {
        while (!got_packet)
        {
            delay(1);
        }
    }
    //Serial.print("ready\n");

    // wait for the transmission
    while ((bootCount >= 2) && (micros() - start_time < RADIO_ON_MS * MS_TO_US))
    {
        delay(1);
    }

    if (!got_packet)
    {
        MISS_COUNT++;
        if (MISS_COUNT == 1)
        {
            esp_sleep_enable_timer_wakeup((TIME_PERIOD_MS - (GUARD_MS + BACKOFF_MS)) * MS_TO_US);
            _TEMP_DESYNC++;
            // inidicates a temporary desynchronization
            Serial.printf("0\n");
        }
        else
        {
            // wake up way earlier
            esp_sleep_enable_timer_wakeup((TIME_PERIOD_MS / 2) * MS_TO_US);
            Serial.printf("-1\n");
            KEEP_ON = 1;
            MISS_COUNT = 0;
            // inidicates a hard desynchronization
            _FULL_DESYNC++;
        }
    }
    else
    {
        MISS_COUNT = 0;
        // wait for the rcvCallback to finish
        while (!rcv_done)
        {
            delay(1);
        }
    }

    Serial.flush();
    esp_deep_sleep_start();
}
