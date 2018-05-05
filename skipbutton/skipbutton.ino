#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "ESP8266WiFi.h"
#include "EEPROM.h"

const char AP_NAME[] = "revspace-espnow";
static const uint8_t SKIP_TXT[] = "SKIP";

typedef struct {
    uint8_t     mac[8];
    int         channel;
} receiver_t;

typedef enum {
    E_SEND,
    E_ACK,
    E_DISCOVER,
    E_SLEEP
} skip_mode_t;

static skip_mode_t mode = E_SEND;

void setup(void)
{
    // welcome
    Serial.begin(115200);
    Serial.println("ESP-SKIP");

    WifiEspNow.begin();
    EEPROM.begin(512);
}

static bool find_ap(const char *name, receiver_t *receiver)
{
    // scan for networks and try to find our AP
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        if (strcmp(name, WiFi.SSID(i).c_str()) == 0) {
            // copy receiver data
            receiver->channel = WiFi.channel(i);
            memcpy(receiver->mac, WiFi.BSSID(i), 6);
            return true;
        }
    }
    // not found
    return false;
}

void loop(void)
{
    WifiEspNowSendStatus status;
    char line[128];

    switch (mode) {

    case E_SEND:
        // read last known receiver info from EEPROM
        receiver_t recv;
        EEPROM.get(0, recv);

        // send SKIP message to last known address
        sprintf(line, "Sending SKIP to %02X:%02X:%02X:%02X:%02X:%02X (chan %d)...",
                recv.mac[0], recv.mac[1], recv.mac[2], recv.mac[3], recv.mac[4], recv.mac[5], recv.channel);
        Serial.println(line);

        WifiEspNow.addPeer(recv.mac, recv.channel, nullptr);
        WifiEspNow.send(nullptr, SKIP_TXT, 4);

        mode = E_ACK;
        break;

    case E_ACK:
        // wait for tx ack
        status = WifiEspNow.getSendStatus();
        switch (status) {
        case WifiEspNowSendStatus::NONE:
            if (millis() > 3000) {
                Serial.println("TX ack timeout, going into discovery...");
                mode = E_DISCOVER;
            }
            break;
        case WifiEspNowSendStatus::OK:
            Serial.println("TX success, going to sleep ...");
            mode = E_SLEEP;
            break;
        case WifiEspNowSendStatus::FAIL:
        default:
            Serial.println("TX failed, going into discovery ...");
            mode = E_DISCOVER;
            break;
        }
        break;

    case E_DISCOVER:
        receiver_t receiver;
        if (find_ap(AP_NAME, &receiver)) {
            // save it in EEPROM
            sprintf(line, "Found '%s', saving to EEPROM ...", AP_NAME);
            Serial.println(line);
            EEPROM.put(0, receiver);
            EEPROM.end();
        } else {
            Serial.println("No master found!");
        }
        mode = E_SLEEP;
        break;

    case E_SLEEP:
    default:
        Serial.println("Going to sleep...");
        delay(100);
        ESP.deepSleep(0, WAKE_RF_DEFAULT);
        break;
    }
}

