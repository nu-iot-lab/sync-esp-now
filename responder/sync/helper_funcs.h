#define MS_TO_US 1000
#define TIME_PERIOD_MS 5000

#define TOTAL_PKTS 500
#define HOP_NUM_N 2
#define RETR_NUM_K 3

#define TX_DELAY 1
#define BOOT_TIME 25 // experimentally found
#define RADIO_READY 20 // experimentally found
#define AFTER_RX 105 // experimentally found
#define GUARD_TIME 40
#define OVERHEAD 120 // experimentally found
#define R 1
#define IEEE80211_OVERHEAD 43

RTC_DATA_ATTR unsigned int bootCount = 0;
RTC_DATA_ATTR unsigned short packetReceived = 0;
RTC_DATA_ATTR unsigned short previous_packet = 0;
RTC_DATA_ATTR uint8_t MISS_COUNT = 0;
RTC_DATA_ATTR uint8_t KEEP_ON = 0;
RTC_DATA_ATTR uint8_t TURN_OFF = 0;
RTC_DATA_ATTR uint8_t AFTER_FDESYNC = 0;

// variables for metrics
RTC_DATA_ATTR int _RSSI_SUM = 0;
RTC_DATA_ATTR unsigned short int _TEMP_DESYNC = 0;
RTC_DATA_ATTR uint8_t _FULL_DESYNC = 0;
RTC_DATA_ATTR unsigned short int _PRIMARY_COUNT = 0;

// flags, sleep_correction
unsigned long start_time = 0;
unsigned long radio_on_time = 0;
uint8_t got_packet = 0;
short int sleep_correction = 0;
uint8_t rcv_done = 0;
uint8_t retr = 0; // pkt needs to be forwarded
uint8_t is_repeated = 0;
unsigned int rx_time = 0;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t slaveInfo;

// Define a data structure
struct packet {
  unsigned short int packetNumber;
  uint8_t ttl;
  uint8_t id;
  float dist1;
  float time1;
  char lat;
  char lon;
  unsigned int CRC; // optional
};

packet p;
float del = RETR_NUM_K * (sizeof(p)+IEEE80211_OVERHEAD) * 8.0 / 250.0 + (RETR_NUM_K-1) * TX_DELAY * 1.0;
float period = TIME_PERIOD_MS - del;
unsigned short int max_radio_on = GUARD_TIME + HOP_NUM_N * (del + OVERHEAD);

wifi_country_t country = {
  .cc = "JP",
  .schan = 1,
  .nchan = 14,
  .max_tx_power = 20,
  .policy = WIFI_COUNTRY_POLICY_AUTO,
};

void SpecialHandler(){
  TURN_OFF = 1;

  packetReceived--;
  float avg_rssi = 0.0;
  float succ_ratio = 0.0;

  p.ttl--;
  if (p.ttl > 0)
    retr = 1;

  if (packetReceived == 0)
    packetReceived = 1;
  avg_rssi = (float)_RSSI_SUM / (float)packetReceived;
  int sum = TOTAL_PKTS;

  if (sum != 0){
    succ_ratio = (float)(packetReceived) / (float)(sum);
  }

  Serial.printf("\n--------- RESULTS --------------\n");
  Serial.printf("Avg RSSI: %f\tTemp desyncs: %d\t Complete desyncs: %d\n", avg_rssi, _TEMP_DESYNC, _FULL_DESYNC);
  Serial.printf("TTL 1s (Primary): %d\n", _PRIMARY_COUNT);


  Serial.printf("Success Ratio: %.2f%%\n", succ_ratio * 100);

  Serial.print("Received Special packet\t Turning Off... \n");
  rcv_done = 1;
}

void retransmit(){
//  esp_wifi_start();
  uint32_t r = esp_random();
  if (r < 0)
      r *= -1;
  //Serial.printf("%f\n", (float)r/4294967295.0);
  delay(r/4294967295);
  for (int i = 0; i < RETR_NUM_K; i++){
    esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&p, sizeof(p));
    delay(1);
  }
  retr = 0;

  // this is to turn off completely at the end!
  if (TURN_OFF){
    Serial.flush();
    esp_deep_sleep_start();
  }

  esp_wifi_stop();
}
