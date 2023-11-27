// Include Libraries
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <random>

#define TIME_PERIOD 5
#define S_TO_MS 1000
#define MAX_TX_COUNT 500
#define SPECIAL_COUNT 10

#define RETR_NUM_K 3
#define HOP_NUM_N 1
#define TX_DELAY 1
#define IEEE80211_OVERHEAD 43

uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

wifi_country_t country = {
  .cc = "JP",
  .schan = 1,
  .nchan = 14,
  .max_tx_power = 20,
  .policy = WIFI_COUNTRY_POLICY_AUTO,
};

// Info about the peer
esp_now_peer_info_t slaveInfo;

struct packet{
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

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);
  delay(1000);  // take some time to open SM

  Serial.println("ESP-NOW Broadcast initiator");

  WiFi.useStaticBuffers(true);

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
  esp_wifi_config_espnow_rate(WIFI_IF_AP, WIFI_PHY_RATE_LORA_250K);

  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_wifi_set_country(&country);

  esp_wifi_set_channel(14, WIFI_SECOND_CHAN_NONE);

  // Register the slave
  memcpy(slaveInfo.peer_addr, broadcastAddress, 6);

  // adding peer
  if (esp_now_add_peer(&slaveInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  slaveInfo.channel = 14;

  p.packetNumber = 1;
  p.ttl = HOP_NUM_N;
  p.id = 1;
  p.dist1 = 3000*rand()/1000;
  p.time1 = p.dist1/16.6667;
  p.lat = 'N';
  p.lon = 'W';
  p.CRC = 2565615138;
//  p.pad = 0x0;
}

// ------- LOOP --------------------------------------------------

void loop() {
  while (p.packetNumber <= MAX_TX_COUNT) {

    Serial.print("\nSENDING ...\n");
    for (int i=0; i<RETR_NUM_K; i++){
      esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&p, sizeof(p));
      delay(TX_DELAY);
    }

    // waiting time: period - propagation time - time between transmissions
    float del = RETR_NUM_K*(sizeof(p)+IEEE80211_OVERHEAD)*8.0/250 + RETR_NUM_K*TX_DELAY*1.0;
    delay(TIME_PERIOD * S_TO_MS - del);

    p.packetNumber++;
  }
  p.packetNumber = 0;
  int stx = 0;
  while (stx < SPECIAL_COUNT) {

    Serial.print("\n SENDING SPECIAL packet ...\n");

    for (int i=0; i<RETR_NUM_K; i++){
      esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&p, sizeof(p));
      delay(TX_DELAY);
    }
    stx++;
  }

  Serial.println("Turning off...");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
}
