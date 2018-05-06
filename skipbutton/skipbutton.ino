#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "ESP8266WiFi.h"
#include "EEPROM.h"

static const char AP_NAME[] = "revspace-espnow";
static const uint8_t SKIP_TXT[] = "SKIP";

typedef struct {
    uint8_t     mac[6];
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
    Serial.println("\nESPNOW-SKIP");

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
            memcpy(receiver->mac, WiFi.BSSID(i), sizeof(receiver->mac));
            return true;
        }
    }
    // not found
    return false;
}

void loop(void)
{
    WifiEspNowSendStatus status;
    receiver_t recv;
    char line[128];

    switch (mode) {

    case E_SEND:
        // read last known receiver info from EEPROM
        EEPROM.get(0, recv);

        // send SKIP message to last known address
        sprintf(line, "Sending SKIP to %02X:%02X:%02X:%02X:%02X:%02X (chan %d)...",
                recv.mac[0], recv.mac[1], recv.mac[2], recv.mac[3], recv.mac[4], recv.mac[5], recv.channel);
        Serial.print(line);

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
                Serial.println("TX ack timeout");
                mode = E_DISCOVER;
            }
            break;
        case WifiEspNowSendStatus::OK:
            Serial.println("TX success");
            mode = E_SLEEP;
            break;
        case WifiEspNowSendStatus::FAIL:
        default:
            Serial.println("TX failed");
            mode = E_DISCOVER;
            break;
        }
        break;

    case E_DISCOVER:
        Serial.println("Discovering master ...");
        if (find_ap(AP_NAME, &recv)) {
            // save it in EEPROM
            sprintf(line, "found '%s' at %02X:%02X:%02X:%02X:%02X:%02X (chan %d), saving to EEPROM", AP_NAME,
                recv.mac[0], recv.mac[1], recv.mac[2], recv.mac[3], recv.mac[4], recv.mac[5], recv.channel);
            Serial.println(line);
            EEPROM.put(0, recv);
            EEPROM.end();
        } else {
            Serial.println("no master found!");
        }
        mode = E_SLEEP;
        break;

    case E_SLEEP:
    default:
        Serial.println("Going to sleep...");
        ESP.deepSleep(0, WAKE_RF_DEFAULT);
        break;
    }
}

