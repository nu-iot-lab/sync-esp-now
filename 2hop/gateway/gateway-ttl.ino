// Include Libraries
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define TIME_PERIOD 5
#define S_TO_MS 1000
#define MAX_TX_COUNT 500
#define SPECIAL_COUNT 10
#define MS_TO_S 1000
#define MIN_TO_S 60

// MAC Address of responder/receiver/slave - edit as required
// addr of r1 = 0x0C,0xB8,0x15,0xD7,0x76,0xD4

uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
unsigned int packetNumber = 1;

wifi_country_t country = {
  .cc = "JP",
  .schan = 1,
  .nchan = 14,
  .max_tx_power = 20,
  .policy = WIFI_COUNTRY_POLICY_AUTO,
};

// Info about the peer
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

// Create a structured object
struct_message myData;

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
  // delay(5* MIN_TO_S * MS_TO_S);

  myData.dist1 = 1.0;
  myData.time1 = 1.0;
  myData.dist2 = 2.0;
  myData.time2 = 2.0;
  myData.dist3 = 3.0;
  myData.time3 = 3.0;
  myData.dist4 = 4.0;
  myData.time4 = 4.0;
  myData.time_to_live = 2;
}

// ------- LOOP --------------------------------------------------

int tx = 0;
int stx = 0;

void loop() {
  while (tx < MAX_TX_COUNT) {
  // while (1) {
    // Format structured data
    // myData.time = 24; // put time here
    myData.packetNumber = packetNumber;

    packetNumber++;

    Serial.print("\nSENDING ...\n");
    // Serial.printf("%lu\t", millis());
    esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
    delay(1);

    esp_err_t result2 = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
    delay(1);

    esp_err_t result3 = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
    // Serial.printf("%lu\n", millis());

    myData.dist1++;
    myData.time1++;
    myData.dist2++;
    myData.time2++;
    myData.dist3++;
    myData.time3++;
    myData.dist4++;
    myData.time4++;

    // // waiting time...
    delay(TIME_PERIOD * S_TO_MS - 2);
    // delay(TIME_PERIOD * S_TO_MS);

    tx++;
  }

  while (stx < SPECIAL_COUNT) {
    stx++;

    // Format structured data
    // myData.time = 0; // put time here
    myData.packetNumber = 0;

    Serial.print("\n SENDING SPECIAL packet ...\n");
    esp_err_t result = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
    delay(1);

    esp_err_t result2 = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));
    delay(1);

    esp_err_t result3 = esp_now_send(slaveInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));

    // waiting time...

    if (stx != SPECIAL_COUNT) {
      delay(TIME_PERIOD * S_TO_MS - 2);
    }
  }

  Serial.println("Turning off...");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
}
