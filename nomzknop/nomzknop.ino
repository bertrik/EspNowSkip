#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "ESP8266WiFi.h"
#include "EEPROM.h"

#define PIN_KNOP D6

static const char AP_NAME[] = "revspace-espnow";

void setup(void)
{
    // welcome
    Serial.begin(115200);
    Serial.printf("\nESPNOW-NOMZ\n");

    // use blue LED to indicate we are on
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 1);

    pinMode(PIN_KNOP, INPUT_PULLUP);

    WifiEspNow.begin();
    EEPROM.begin(512);
}

static bool find_ap(const char *name, struct WifiEspNowPeerInfo *peer)
{
    // scan for networks and try to find our AP
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        Serial.printf("%s\n", WiFi.SSID(i).c_str());
        if (strcmp(name, WiFi.SSID(i).c_str()) == 0) {
            // copy receiver data
            peer->channel = WiFi.channel(i);
            memcpy(peer->mac, WiFi.BSSID(i), sizeof(peer->mac));
            return true;
        }
    }
    // not found
    return false;
}


static bool valid_peer(struct WifiEspNowPeerInfo *peer)
{
    return (peer->channel >= 1) && (peer->channel <= 14);
}

static bool discover(struct WifiEspNowPeerInfo *recv, const char *ssid)
{
    if (find_ap(ssid, recv)) {
        // save it in EEPROM
        Serial.printf("found '%s' at %02X:%02X:%02X:%02X:%02X:%02X (chan %d), saving to EEPROM\n",
                      ssid, recv->mac[0], recv->mac[1], recv->mac[2],
                      recv->mac[3], recv->mac[4], recv->mac[5], recv->channel);
        return true;
    }

    return false;
}

static bool send(struct WifiEspNowPeerInfo *recv, const char *topic,
                 const char *payload, unsigned int timeout_ms)
{
    char buf[250];
    snprintf(buf, sizeof(buf), "%s %s", topic, payload);

    // send it
    WifiEspNow.send(recv->mac, (uint8_t *) buf, strlen(buf));

    // wait for result
    WifiEspNowSendStatus status;
    unsigned long starttime = millis();
    do {
        status = WifiEspNow.getSendStatus();
        yield();
    } while ((status == WifiEspNowSendStatus::NONE) && ((millis() - starttime) < timeout_ms));

    return (status == WifiEspNowSendStatus::OK);
}


void loop(void)
{
    struct WifiEspNowPeerInfo recv;

    digitalWrite(LED_BUILTIN, 0);
    Serial.printf("Nomz detected...\n");

    EEPROM.get(0, recv);
    bool ok = valid_peer(&recv);
    if (ok) {
        WifiEspNow.addPeer(recv.mac, recv.channel, nullptr);
        ok = send(&recv, "revspace/button/nomz", "lekker! ", 300);
    }
    if (!ok) {
        Serial.printf("Discovery...\n");
        if (discover(&recv, "revspace-espnow")) {
            EEPROM.put(0, recv);
            EEPROM.end();
            Serial.printf("Saved to EEPROM\n");
        }
    }
    digitalWrite(LED_BUILTIN, 1);

    ESP.deepSleep(0);
}
