#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "ESP8266WiFi.h"
#include "EEPROM.h"
#include <SparkFunBME280.h>

static BME280 bme280;
static const char NETWORK_NAME[] = "revspace-espnow";
static char temp_topic[64];

void setup(void)
{
    // welcome
    Serial.begin(115200);
    Serial.println("\nESP-TEMPSENSOR");

    // setup BME280
    bme280.setI2CAddress(0x76);
    bme280.beginI2C();

    snprintf(temp_topic, sizeof(temp_topic), "revspace/sensors/temperature/%06x", ESP.getChipId());

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
    // get temperature
    float tempC = bme280.readTempC();

    // send it
    struct WifiEspNowPeerInfo recv;
    EEPROM.get(0, recv);
    bool ok = valid_peer(&recv);
    if (!ok) {
        Serial.printf("Discovery...\n");
        ok = discover(&recv, NETWORK_NAME);
        if (ok) {
            EEPROM.put(0, recv);
            EEPROM.end();
            Serial.printf("Saved to EEPROM\n");
        }
    }
    if (ok) {
        WifiEspNow.addPeer(recv.mac, recv.channel, nullptr);
        char temp_value[32];
        snprintf(temp_value, sizeof(temp_value), "%.1f °C", tempC);
        ok = send(&recv, temp_topic, temp_value, 300);
    }
    // back to deep sleep until next measurement
    Serial.printf("was awake for %lu ms... Zzz ...\n", millis());
    ESP.deepSleep(60000000UL);
}

