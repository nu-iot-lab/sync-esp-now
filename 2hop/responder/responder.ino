// authors: Yermakhan Magzym, Dimitrios Zorbas

#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>
#include "rssi.h"
#include "helper_funcs.h"

// Callback function executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    rx_time = micros() - start_time;
    Serial.printf("Received at %f ms\n", rx_time/1000.0); // it must be RADIO_READY+30
    got_packet = 1;
    rcv_done = 0;
    if (bootCount == 1 && MISS_COUNT != 0){
      clock_correction = 0;
    }else if (bootCount > 1 && MISS_COUNT == 0){
      unsigned long delta = micros() - start_time - RADIO_READY*1000 - del; // this is how long the device waited with radio on until it received the beacon
      clock_correction = int(delta/1000.0 - GUARD_TIME);
    }

    packetReceived++;
    memcpy(&p, incomingData, sizeof(p));

    // repeated packets
    if (p.packetNumber != previous_packet)
    {
        previous_packet = p.packetNumber;
    }
    else
    {
        packetReceived--;
        got_packet = 0;
        is_repeated = 1;
        return;
    }

    // check for special packet
    if (p.packetNumber == 0)
    {
        SpecialHandler();
        return;
    }

    p.ttl--;
    if (p.ttl > 0)
    {
        retr = 1;
        _PRIMARY_COUNT++;
    }
    else
    {
        hopping = 1;
    }

//    esp_wifi_stop();

    // output stuff ...
    _RSSI_SUM += rssi_display;
    Serial.printf("%d\t%i\t%i\t%i\t%i\n", packetReceived, p.ttl, rssi_display, p.packetNumber, clock_correction);
    // Serial.printf("%.2f, %.2f\t%.2f, %.2f\t%.2f, %.2f\t%.2f, %.2f\n", p.time1, p.dist1, p.time2, p.dist2, p.time3, p.dist3, p.time4, p.dist4);

    if ((MISS_COUNT == 0) && (bootCount != 1) && !KEEP_ON)
    {
        // Serial.printf("I will align the clock by %d ms\n", clock_correction);
        if (hopping == 1)
        {
            esp_sleep_enable_timer_wakeup((period - AFTER_RX - RADIO_READY + clock_correction) * MS_TO_US);
        }
        else
        {
            esp_sleep_enable_timer_wakeup((period - AFTER_RX - RADIO_READY + clock_correction) * MS_TO_US);
        }
    }

    rcv_done = 1;
    KEEP_ON = 0;
}

void setup()
{
    Serial.begin(115200);
    start_time = micros();

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
    Serial.printf("Radio ready at %f ms\n", (micros()-start_time)/1000.0);

    // set default sleep time
    esp_sleep_enable_timer_wakeup((period - RADIO_READY - AFTER_RX) * MS_TO_US);
    esp_wifi_start();
}

void loop()
{
    // wait indefinetly (this is for the first time the device boots up)
    if (bootCount == 1 || KEEP_ON)
    {
        while (!got_packet)
        {
            delay(1);
        }
    }

    // wait for the beacon
    while ( (bootCount > 1) && ((micros() - start_time) < 3*radio_on_ms*MS_TO_US) )
    {
        delay(1);
    }

    if (!got_packet && !is_repeated)
    {
        //  if didn't get a packet, wait a bit for transmissions from other nodes
        delay(10);
        if (hopping == 1)
        {
            while (!rcv_done)
            {
                delay(1);
            }
            Serial.flush();
            esp_deep_sleep_start();
        }

        MISS_COUNT++;
        if (MISS_COUNT == 1)
        {
            esp_sleep_enable_timer_wakeup((period - RADIO_READY - 3*radio_on_ms)*MS_TO_US);
            _TEMP_DESYNC++;
            Serial.printf("x\n");
        }
        else if (MISS_COUNT == 2)
        {
            // wake up way earlier
            esp_sleep_enable_timer_wakeup((period / 2) * MS_TO_US);
            Serial.printf("xx\n");
            KEEP_ON = 1;
            MISS_COUNT = 0;
            _FULL_DESYNC++;
        }
    }
    else // got packet
    {
        MISS_COUNT = 0;
        // wait for the rcvCallback to finish
        while (!rcv_done)
        {
            delay(1);
        }

        // if got packet then retransmit
        if (rcv_done && retr)
        {
            retransmit();
        }
    }
    // Serial.printf("time since rx: %d\n", micros()-rx_time);
    Serial.flush();
    esp_deep_sleep_start();
}
