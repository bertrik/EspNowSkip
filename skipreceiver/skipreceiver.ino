#include <stdint.h>
#include <stdio.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "ESP8266WiFi.h"

static const char AP_NAME[] = "revspace-espnow";
static const int AP_CHANNEL = 1;

static const uint8_t BCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void onReceiveCallback(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg)
{
    char line[128];

    // print some comments
    sprintf(line, "# got %d bytes from %02X:%02X:%02X:%02X:%02X:%02X", 
            count, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) ;
    Serial.println(line);
    
    // print as hex to serial out
    for (size_t i = 0; i < count; i++) {
        sprintf(line, "%02X", buf[i]);
        Serial.print(line);
    }
    Serial.println();
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println("SKIP receiver");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NAME, nullptr, AP_CHANNEL);

    WifiEspNow.begin();
    WifiEspNow.addPeer(BCAST_MAC, 0, nullptr);
    WifiEspNow.onReceive(onReceiveCallback, NULL);
}

void loop(void)
{
    // nothing to do here ...
}

