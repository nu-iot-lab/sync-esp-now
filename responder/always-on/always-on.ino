#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>
#include "rssi.h"

#define TOTAL_TX_COUNT 500
#define RETR_NUM_K 1
// #define HOP_NUM_N 1 // not needed in always-on

int packetReceived = 0;
uint8_t TURN_OFF = 0;

// variables for metrics
int _RSSI_SUM = 0;
int _RETR_SUM = 0;

// flags
uint8_t rcv_done = 0;
uint8_t retr = 0;
int previous_packet = -1;
uint8_t end = 0;

uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
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


wifi_country_t country = {
  .cc = "JP",
  .schan = 1,
  .nchan = 14,
  .max_tx_power = 20,
  .policy = WIFI_COUNTRY_POLICY_AUTO,
};

void retransmit(){
  uint32_t r = esp_random();
  if (r < 0)
      r *= -1;
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
}

void SpecialHandler() {
  // the special packet does not count
  packetReceived--;
  rcv_done = 0;
  end = 1;

  p.ttl--;
  if (p.ttl > 0) {
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
//  esp_wifi_stop();
  rcv_done = 0;
  retr = 0;
  // Serial.printf("%lu\n", millis());
  packetReceived++;
  memcpy(&p, incomingData, sizeof(p));

  if (p.packetNumber != previous_packet) {
    previous_packet = p.packetNumber;
  } else {
    packetReceived--;
    return;
  }

  // check for special packet
  if (p.packetNumber == 0) {
    SpecialHandler();
    return;
  }
  _RSSI_SUM += rssi_display;

  Serial.printf("%d\t%i\t%i\t%i\n", packetReceived, p.ttl, rssi_display, p.packetNumber);

  p.ttl--;
  if (p.ttl > 0) {
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
    retransmit();
  }
}
