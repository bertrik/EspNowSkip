#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "WiFi.h"
#include "PubSubClient.h"

#include "editline.h"
#include "cmdproc.h"
#include "print.h"

#define MQTT_HOST   "mosquitto.space.revspace.nl"
//#define MQTT_HOST   "aliensdetected.com"
#define MQTT_PORT   1883

#define ESP_NOW_CHANNEL 1

// structure for non-volatile data in EEPROM
typedef struct {
    uint8_t id[6];
} nvstore_t;

static const char AP_NAME[] = "revspace-espnow";
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static char esp_id[16];
static char line[128];

typedef struct {
    uint8_t mac[6];
    uint8_t buf[256];
    size_t count;
} espnow_t;

static espnow_t espnow_data;
static volatile boolean have_espnow_data = false;

static void show_help(const cmd_t *cmds)
{
    for (const cmd_t *cmd = cmds; cmd->cmd != NULL; cmd++) {
        print("%10s: %s\n", cmd->name, cmd->help);
    }
}

static bool mqtt_send(const char *topic, const char *payload)
{
    bool result = false;
    if (!mqttClient.connected()) {
        Serial.print("Connecting MQTT...");
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        result = mqttClient.connect(esp_id);
        Serial.println(result ? "OK" : "FAIL");
    }
    if (mqttClient.connected()) {
        print("Publishing '%s' to '%s' ...", payload, topic);
        result = mqttClient.publish(topic, payload, false);
        print(result ? "OK\n" : "FAIL\n");
    }
    return result;
}

static int do_mqtt(int argc, char *argv[])
{
    if (argc < 3) {
        print("Please provide a topic and payload text\n");
        return -1;
    }
    
    char *topic = argv[1];
    char *payload = argv[2];
    
    mqtt_send(topic, payload);

    return 0;
}

static int do_softap(int argc, char *argv[])
{
    if (argc < 2) {
        print("Specify channel\n");
        return -1;
    }
    int channel = atoi(argv[1]);
    print("Setup softap on channel %d\n", channel);

    if (!WiFi.softAP(AP_NAME, nullptr, channel)) {
        print("sofAP failed!\n");
        return -1;
    }
    print("MAC: %s\n", WiFi.softAPmacAddress().c_str());
    return 0;
}

static int do_disc(int argc, char *argv[])
{
    print("Disconnecting SofAP\n");
    return WiFi.softAPdisconnect(false) ? 0 : -1;
}

static int do_wifi(int argc, char *argv[])
{
    if (argc > 1) {
        const char *ssid = argv[1];
        const char *pass = (argc > 2) ? argv[2] : "";
        print("Connecting to AP %s\n", ssid);
        WiFi.begin(ssid, pass, ESP_NOW_CHANNEL);

        // wait for connection
        for (int i = 0; i < 20; i++) {
            print(".");
            if (WiFi.status() == WL_CONNECTED) {
                break;
            }
            delay(500);
        }
    }

    // show wifi status
    int status = WiFi.status();
    print("Wifi status = %d\n", status);

    return (status == WL_CONNECTED) ? 0 : status;
}

const cmd_t commands[] = {
    {"softap",  do_softap,  "[channel] set up softap on channel"},
    {"disc",    do_disc,    "softap disconnect"},
    {"wifi",    do_wifi,    "<ssid> [pass] setup wifi"},
    {"mqtt",    do_mqtt,    "<topic> <payload> publish mqtt"},

    {NULL, NULL, NULL}
};

static void onReceiveCallback(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg)
{
    if (!have_espnow_data) {
        // copy to the main task
        espnow_t *data = (espnow_t *)cbarg;
        memcpy(data->mac, mac, 6);
        memcpy(data->buf, buf, count);
        data->count = count;
        have_espnow_data = true;
    }
}

static void process_espnow_data(const uint8_t mac[6], const uint8_t *buf, size_t count)
{
    char line[256];

    // print metadata
    print("# got %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n", 
          count, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) ;
    
    // print as hex
    print("# HEX: '");
    for (size_t i = 0; i < count; i++) {
        print("%02X", buf[i]);
    }
    print("'\n");
    
    // print as text
    memcpy(line, buf, count);
    line[count] = 0;
    print("%s\n", line);
    
    // split on first space
    char *topic = line;
    char *payload;
    char *space = strchr(line, ' ');
    if (space == NULL) {
        payload = "";
    } else {
        *space = '\0';
        payload = space + 1;
    }
    
    // process
    if (topic && payload) {
        // send as MQTT
        mqtt_send(topic, payload);
    }
}

static void mqtt_alive(void)
{
    static unsigned long int last_alive = 0;

    // keep mqtt alive
    mqttClient.loop();
    
    // publish alive every minute
    unsigned long int minute = millis() / 60000UL;
    if (minute != last_alive) {
        if (!mqtt_send("revspace/espnow/status", "alive")) {
            Serial.println("MQTT alive failed, restarting ...");
            ESP.restart();
        }
        last_alive = minute;
    }
}

void setup(void)
{
    PrintInit(115200);
    print("\n#ESPNOW-RECV\n");

    // get ESP id
    sprintf(esp_id, "%" PRIX64, ESP.getEfuseMac());
    print("ESP ID: %s\n", esp_id);

    EditInit(line, sizeof(line));

    WiFi.persistent(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NAME, nullptr, ESP_NOW_CHANNEL);
    
    WifiEspNow.begin();
    WifiEspNow.onReceive(onReceiveCallback, &espnow_data);

    WiFi.begin("revspace-pub-2.4ghz", "", ESP_NOW_CHANNEL);
}

void loop(void)
{
    // check for incoming ESP Now
    if (have_espnow_data) {
        print("Received ESP NOW data...");
        process_espnow_data(espnow_data.mac, espnow_data.buf, espnow_data.count);
        have_espnow_data = false;
    }

    // handle command input
    bool haveLine = false;
    if (Serial.available()) {
        char c;
        haveLine = EditLine(Serial.read(), &c);
        print("%c", c);
    }
    if (haveLine) {
        int result = cmd_process(commands, line);
        switch (result) {
        case CMD_OK:
            print("OK\n");
            break;
        case CMD_NO_CMD:
            break;
        case CMD_UNKNOWN:
            print("Unknown command, available commands:\n");
            show_help(commands);
            break;
        default:
            print("%d\n", result);
            break;
        }
        print(">");
    }

    // keep mqtt (and ourselves) alive
    mqtt_alive();
}

