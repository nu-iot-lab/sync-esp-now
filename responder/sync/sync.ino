// authors: Yermakhan Magzym, Dimitrios Zorbas, Aida Eduard

#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>
#include "rssi.h"
#include "helper_funcs.h"

// Callback function executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
//    esp_wifi_stop();
    rx_time = micros();
    Serial.printf("Received after %f ms\n", (rx_time-start_time)/1000.0); // it must be RADIO_READY+GUARD_TIME
    rcv_done = 0;
    memcpy(&p, incomingData, sizeof(p));
    if (bootCount == 1 && MISS_COUNT != 0){
        sleep_correction = 0;
    }else if (bootCount > 1 && MISS_COUNT == 0){
        unsigned long delta = micros() - start_time - RADIO_READY*1000 - del*1000; // this is how long the device waited with radio on until it received the beacon
        sleep_correction = int(delta/1000.0 - GUARD_TIME - OVERHEAD*(HOP_NUM_N-p.ttl)); // that might be tricky to sync with HOP>1
        if (sleep_correction > OVERHEAD*(HOP_NUM_N-p.ttl+1))
            sleep_correction = 0;
    }

    // repeated packets
    if (p.packetNumber != previous_packet){
        previous_packet = p.packetNumber;
        packetReceived++;
    }else{
        packetReceived--;
        is_repeated = 1;
        return;
    }

    // check for special packet
    if (p.packetNumber == 0){
        SpecialHandler();
        return;
    }

    // output stuff ...
    _RSSI_SUM += rssi_display;
    Serial.printf("%d\t%i\t%i\t%i\t%i\n", packetReceived, p.ttl, rssi_display, p.packetNumber, sleep_correction);

    // reduce hop count
    p.ttl--;
    if (p.ttl > 0){
        retr = 1;
        _PRIMARY_COUNT++;
    }
    
    got_packet = 1;
    rcv_done = 1;
    KEEP_ON = 0;
}

void setup()
{
    Serial.begin(115200);
    start_time = micros();
    Serial.printf("---\n");

    // special packet check
    if (TURN_OFF)
    {
        Serial.println("I will not wake up again");
        esp_deep_sleep_start();
    }

    // initial output and setup
    if (bootCount == 0)
    { 
        Serial.printf("rx_pkt\tttl\trssi\ttx_pkt\n", packetReceived, p.ttl, rssi_display, p.packetNumber);
    }
    bootCount++;

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

    // Register the slave
    memcpy(slaveInfo.peer_addr, broadcastAddress, 6);
    // adding peer
    if (esp_now_add_peer(&slaveInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }

    // channel and country
    slaveInfo.channel = 14;
    esp_wifi_set_channel(14, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_country(&country);

    // Register callback function
    esp_now_register_recv_cb(OnDataRecv);

    // RSSI
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

    // turn on wifi
    esp_wifi_start();
    radio_on_time = micros();
    Serial.printf("Radio ready after %f ms\n", (radio_on_time-start_time)/1000.0);
}

void loop()
{
    got_packet = 0;
    // wait indefinetly (this is for the first time the device boots up)
    if (bootCount == 1 || KEEP_ON){
        while (got_packet == 0){
            delay(0.001);
        }
    }

    // wait for the beacon
    while ( (bootCount > 1) && ((micros() - start_time) < max_radio_on*MS_TO_US) ){
        if (got_packet == 1)
            break;
    }

    if (got_packet == 0){
        MISS_COUNT++;
        if (MISS_COUNT == 1){
            _TEMP_DESYNC++;
            Serial.printf("x\n");
        }else if (MISS_COUNT == 2){
            // wake up way earlier
            esp_sleep_enable_timer_wakeup((period / 2) * MS_TO_US);
            Serial.printf("xx\n");
            KEEP_ON = 1;
            MISS_COUNT = 0;
            _FULL_DESYNC++;
            AFTER_FDESYNC = 1;
        }
    }else{ // got packet
        MISS_COUNT = 0;
        // wait for the rcvCallback to finish
        while (rcv_done == 0){
            delay(0.001);
        }
        if (AFTER_FDESYNC == 1){
          AFTER_FDESYNC = 2;
        }else if (AFTER_FDESYNC == 2){
          AFTER_FDESYNC = 0;
        }
        // if got packet then retransmit
        if (retr == 1){
            retransmit();
        }
    }
//    Serial.printf("time since rx: %f ms\n", (micros()-rx_time)/1000);
//    Serial.printf("time since wake-up: %f ms\n", (micros()-start_time)/1000);
    if (bootCount > 1 && AFTER_FDESYNC == 0 && got_packet == 1){ // typical case
        Serial.printf("sleep time: %f ms\n", (period - (micros()-start_time)/MS_TO_US - BOOT_TIME + sleep_correction));
        esp_sleep_enable_timer_wakeup((period - (micros()-start_time)/MS_TO_US - BOOT_TIME + sleep_correction) * MS_TO_US);
    }else if (bootCount > 1 && AFTER_FDESYNC == 0 && got_packet == 0){ // missed one
        Serial.printf("sleep time: %f ms\n", (period - (micros()-start_time)/MS_TO_US - BOOT_TIME - 20));
        esp_sleep_enable_timer_wakeup((period - (micros()-start_time)/MS_TO_US - BOOT_TIME - 20) * MS_TO_US);
    }else if (bootCount > 1 && AFTER_FDESYNC == 1 && got_packet == 1){ // after a desync 
        Serial.printf("sleep time: %f ms\n", (period - (micros()-start_time)/MS_TO_US - BOOT_TIME));
        esp_sleep_enable_timer_wakeup((period - (micros()-start_time)/MS_TO_US - BOOT_TIME) * MS_TO_US);
    }else if (bootCount > 1 && AFTER_FDESYNC == 0 && got_packet == 0){ // missed 1 
        Serial.printf("sleep time: %f ms\n", (period - (micros()-start_time)/MS_TO_US - BOOT_TIME - OVERHEAD));
        esp_sleep_enable_timer_wakeup((period - (micros()-start_time)/MS_TO_US - BOOT_TIME - OVERHEAD) * MS_TO_US);
    }else{
        Serial.printf("sleep time: %f ms\n", (period - RADIO_READY - AFTER_RX + BOOT_TIME + sleep_correction));
        esp_sleep_enable_timer_wakeup((period - RADIO_READY - AFTER_RX + BOOT_TIME + sleep_correction) * MS_TO_US);
    }
    Serial.flush();
    esp_deep_sleep_start();
}
