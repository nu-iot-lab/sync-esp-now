#define MS_TO_US 1000
#define TIME_PERIOD_MS 5000

#define TOTAL_PKTS 500
#define HOP_NUM_N 2
#define RETR_NUM_K 1
#define TX_DELAY 1
#define RADIO_READY 20 // experimentally found
#define AFTER_RX 105 // experimentally found
#define GUARD_TIME 30
#define SWITCH_TIME 10 // experimentally found
#define R 1

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
unsigned long start_time = 0;
uint8_t got_packet = 0;
short int clock_correction = 0;
uint8_t rcv_done = 0;
uint8_t hopping = 0; // pkt from ed
uint8_t retr = 0; // pkt needs to be forwarded
unsigned short int is_repeated = 0;
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
  unsigned int CRC;
  //  uint8_t pad;
};

packet p;
float del = RETR_NUM_K * sizeof(p) * 8.0 / 250 + RETR_NUM_K * TX_DELAY * 1.0;
float period = TIME_PERIOD_MS - del;
unsigned short int radio_on_ms = GUARD_TIME + HOP_NUM_N * (del + SWITCH_TIME + R);

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

  p.ttl--;
  if (p.ttl > 0)
    retr = 1;

  if (packetReceived != 0)
    avg_rssi = int(_RSSI_SUM / packetReceived);
  int sum = TOTAL_PKTS - 2;

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
//  esp_wifi_start();
  // p.ttl = 1;

  for (int i = 0; i < RETR_NUM_K; i++) {
    esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&p, sizeof(p));
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
