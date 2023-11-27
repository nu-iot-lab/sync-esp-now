#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

#define TOTAL_TX_COUNT 500

int packetReceived = 0;
int TURN_OFF = 0;

// variables for metrics
int _RSSI_SUM = 0;
int _RETR_SUM = 0;

// flags
int rcv_done = 0;
int retr = 0;
int previous_packet = -1;
int end = 0;

// uint8_t masterAddress[] = {0x0C, 0xB8, 0x15, 0xD7, 0x77, 0x3C};
uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
// esp_now_peer_info_t masterInfo;
esp_now_peer_info_t slaveInfo;

// Define a data structure
typedef struct struct_message {
  // unsigned int time;
  unsigned int packetNumber;
  unsigned int time_to_live;
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

/////////////////////////////////////   RSSI  //////////////////////////////////////

int rssi_display;

// Estructuras para calcular los paquetes, el RSSI, etc
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
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
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

void retransmit() {
  myData.time_to_live = 1;
  // Serial.println("retransmitting");

  esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
  delay(1);

  esp_err_t result2 = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
  delay(1);

  esp_err_t result3 = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
  retr = 0;
  if (end) {
    Serial.flush();
    esp_deep_sleep_start();
  }
}

void SpecialHandler() {
  // the special packet does not count
  packetReceived--;
  rcv_done = 0;
  end = 1;

  myData.time_to_live--;
  if (myData.time_to_live > 0) {
    retr = 1;
  }

  float avg_rssi = (float)_RSSI_SUM / (float)packetReceived;

  float succ_ratio = (float)(packetReceived) / (float)TOTAL_TX_COUNT;

  float retr_pcks = (float)_RETR_SUM / (float)packetReceived;


  Serial.printf("\n--------- RESULTS --------------\n");
  Serial.printf("Avg RSSI: %.2f \n", avg_rssi);

  Serial.printf("# of retransmitted packets: %d\t %% of the total: %.2f\n", _RETR_SUM, retr_pcks);

  Serial.printf("Success Ratio: %.2f%%\n", succ_ratio * 100);

  Serial.print("Received Special packet\t Turning Off... \n");
  rcv_done = 1;
}

// Callback function executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  rcv_done = 0;
  retr = 0;
  // Serial.printf("%lu\n", millis());
  packetReceived++;
  memcpy(&myData, incomingData, sizeof(myData));

  if (myData.packetNumber != previous_packet) {
    previous_packet = myData.packetNumber;
  } else {
    packetReceived--;
    return;
  }

  // check for special packet
  if (myData.packetNumber == 0) {
    SpecialHandler();
    return;
  }
  _RSSI_SUM += rssi_display;

  // Serial.println(myData.time_to_live);
  // if (myData.time_to_live == 1) {

  Serial.printf("%d\t%i\t%i\t%i\n", packetReceived, myData.time_to_live, rssi_display, myData.packetNumber);
  // Serial.printf("%.2f, %.2f\t%.2f, %.2f\t%.2f, %.2f\t%.2f, %.2f\n", myData.time1, myData.dist1, myData.time2, myData.dist2, myData.time3, myData.dist3, myData.time4, myData.dist4);

  myData.time_to_live--;
  if (myData.time_to_live > 0) {
    retr = 1;
  } else {
    _RETR_SUM++;
  }
  rcv_done = 1;
}

void setup() {
  // int set_st = micros();
  Serial.begin(115200);

  // special packet check
  if (TURN_OFF) {
    Serial.println("I will not wake up again");
    esp_deep_sleep_start();
  }

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
  esp_wifi_config_espnow_rate(WIFI_IF_AP, WIFI_PHY_RATE_LORA_250K);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("There was an error initializing ESP-NOW");
    return;
  }

  // set the country
  esp_wifi_set_country(&country);

  // // Register the master
  // memcpy(masterInfo.peer_addr, masterAddress, 6);
  // masterInfo.channel = 14;

  // // Add master
  // if (esp_now_add_peer(&masterInfo) != ESP_OK)
  // {
  //     Serial.println("There was an error registering the master");
  //     return;
  // }

  // Register the slave
  memcpy(slaveInfo.peer_addr, broadcastAddress, 6);

  // adding peer
  if (esp_now_add_peer(&slaveInfo) != ESP_OK) {
  Serial.println("Failed to add peer");
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

void loop() {
  if (rcv_done && retr) {
    delay(5);
    retransmit();
  }
}