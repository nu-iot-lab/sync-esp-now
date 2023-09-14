
#define US_TO_S 1000000
#define MS_TO_US 1000
#define TIME_PERIOD_MS 5000

#define TOTAL_PKTS 500

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int packetReceived = 0;
RTC_DATA_ATTR int MISS_COUNT = 0;
RTC_DATA_ATTR int KEEP_ON = 0;
RTC_DATA_ATTR int TURN_OFF = 0;
RTC_DATA_ATTR int previous_packet = -1;

// variables for metrics
RTC_DATA_ATTR int _RSSI_SUM = 0;
RTC_DATA_ATTR int _TEMP_DESYNC = 0;
RTC_DATA_ATTR int _FULL_DESYNC = 0;
RTC_DATA_ATTR int _PRIMARY_COUNT = 0;


// flags, clock_correction
int radio_on_ms = 60;
int backoff_ms = 30;
unsigned long start_time = 0;
int got_packet = 0;
int clock_correction = 0;
int rcv_done = 0;
int hopping = 0;
int retr = 0;
int is_repeated = 0;
int rx_time = 0;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t slaveInfo;

// Define a data structure
typedef struct struct_message
{
    // unsigned int time;
    unsigned int packetNumber;
    unsigned short time_to_live;
    float dist1;
    float time1;
    float dist2;
    float time2;
    float dist3;
    float time3;
    float dist4;
    float time4;
} struct_message;

struct_message myData;



wifi_country_t country = {
    .cc = "JP",
    .schan = 1,
    .nchan = 14,
    .max_tx_power = 20,
    .policy = WIFI_COUNTRY_POLICY_AUTO,
};

void SpecialHandler()
{
    TURN_OFF = 1;

    // the special packet does not count
    packetReceived--;
    bootCount--;
    int avg_rssi = 0;
    float succ_ratio = 0;

    myData.time_to_live--;
    if (myData.time_to_live > 0)
        retr = 1;

    if (packetReceived != 0)
        avg_rssi = int(_RSSI_SUM / packetReceived);
    int sum = TOTAL_PKTS -2;

    if (sum != 0)
    {
        succ_ratio = (float)(packetReceived) / (float)(sum);
    }

    Serial.printf("\n--------- RESULTS --------------\n");
    Serial.printf("Avg RSSI: %d\tTemp desyncs: %d\t Complete desyncs: %d\n", avg_rssi, _TEMP_DESYNC, _FULL_DESYNC);
    Serial.printf("TTL 1s (Primary): %d\n", _PRIMARY_COUNT);


    Serial.printf("Success Ratio: %.2f%%\n", succ_ratio * 100);

    Serial.print("Received Special packet\t Turning Off... \n");
    rcv_done = 1;
}

void retransmit()
{
    // this takes approx. 12 ms
    //    Serial.println("retransmitting...");
    esp_wifi_start();
    myData.time_to_live = 1;

    for (int i=0; i<3; i++){
      esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
      delay(1);
    }
    retr = 0;

    // this is to turn off completely at the end!
    if (TURN_OFF)
    {
        Serial.flush();
        esp_deep_sleep_start();
    }

    // esp_wifi_stop();
    //    Serial.println(micros() - rst);
}
