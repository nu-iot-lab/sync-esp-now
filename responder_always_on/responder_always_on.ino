// BY NU IOT LAB //
// REFERENCES:
// rssi: https://github.com/TenoTrash/ESP32_ESPNOW_RSSI/blob/main/Modulo_Receptor_OLED_SPI_RSSI.ino

#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

#define TOTAL_TX_COUNT 500

int packetReceived = 0;
int TURN_OFF = 0;

// variables for metrics
int _RSSI_SUM = 0;

// flags, clock_correction
int got_packet = 0;
int clock_correction = 0;
int sd_done = 0;
int previous_packet = -1;

uint8_t masterAddress[] = {0x0C, 0xB8, 0x15, 0xD7, 0x77, 0x3C};
esp_now_peer_info_t masterInfo;

// Define a data structure
typedef struct struct_message
{
    unsigned int time;
    unsigned int packetNumber;
} struct_message;

struct_message myData;


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
    // the special packet does not count
    packetReceived--;
    
    float avg_rssi = (float)_RSSI_SUM / (float)packetReceived;
    
    float succ_ratio = (float)(packetReceived) / (float)TOTAL_TX_COUNT;
    

    Serial.printf("\n--------- RESULTS --------------\n");
    Serial.printf("Avg RSSI: %.2f \n", avg_rssi);

    Serial.printf("Success Ratio: %.2f%%\n", succ_ratio * 100);

    Serial.print("Received Special packet\t Turning Off... \n");

    Serial.flush();
    esp_deep_sleep_start();
}

// Callback function executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    packetReceived++;
    memcpy(&myData, incomingData, sizeof(myData));

    if (myData.packetNumber != previous_packet) {
      previous_packet = myData.packetNumber;
    } else {
      packetReceived--;
      return;
    }
    // check for special packet
    if (myData.time == 0 && myData.packetNumber == 0)
    {
        SpecialHandler();
        return;
    }
    _RSSI_SUM += rssi_display;

    Serial.printf("%d\t%lu\t%i\t%i\n", packetReceived, myData.time, rssi_display, myData.packetNumber);
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

    // start the esp wifi
    esp_wifi_start();

}

void loop()
{   
}
