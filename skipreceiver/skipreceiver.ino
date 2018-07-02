#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "HTTPClient.h"
#include "WiFi.h"
#include "PubSubClient.h"

#include "editline.h"
#include "cmdproc.h"
#include "print.h"

//#define MQTT_HOST   "mosquitto.space.revspace.nl"
#define MQTT_HOST   "aliensdetected.com"
#define MQTT_PORT   1883

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

static void append_param(char *url, const char *param, const char *value)
{
    char buf[8];
    char c;

    strcat(url, param);
    for (const char *p = value; (c = *p) != 0; p++) {
        snprintf(buf, sizeof(buf), isalnum(c) ? "%c" : "%%%02X", c);
        strcat(url, buf);
    }
}

static void append_jukebox_path(char *url, const char *p0, const char *p1, const char *p2, const char *player)
{
    strcat(url, "/Classic/status_header.html");
    append_param(url, "?p0=", p0);
    append_param(url, "&p1=", p1);
    append_param(url, "&p2=", p2);
    append_param(url, "&player=", player);
}

static void mqtt_send(const char *topic, const char *payload)
{
    if (!mqttClient.connected()) {
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.connect(esp_id);
    }
    if (mqttClient.connected()) {
        print("Publishing '%s' to '%s' ...", payload, topic);
        int result = mqttClient.publish(topic, payload, false);
        print(result ? "OK\n" : "FAIL\n");
    }
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

static int do_skip(int argc, char *argv[])
{
    char url[256];

    strcpy(url, "http://jukebox.space.revspace.nl:9000");
    append_jukebox_path(url, "playlist", "jump", "+1", "b8:27:eb:ba:bc:d5");

    print("url = %s\n", url);

    HTTPClient httpClient;
    httpClient.begin(url);
    int result = httpClient.GET();
    httpClient.end();

    return result;
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
        char *ssid = argv[1];
        if (argc == 2) {
            // connect without password
            print("Connecting to AP %s\n", ssid);
            WiFi.begin(ssid);
        } else if (argc == 3) {
            // connect with password
            char *pass = argv[2];
            print("Connecting to AP '%s', password '%s'\n", ssid, pass);
            WiFi.begin(ssid, pass);
        }
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
    {"skip",    do_skip,    "Send a skip command"},
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
    
    // split
    char *payload = NULL;
    char *topic = strtok(line, " ");
    if (topic != NULL) {
        payload = strtok(NULL, " ");
    }
    
    // process
    if (topic && payload) {
        // send as MQTT
        mqtt_send(topic, payload);

        // handle skip button events
        if (strcmp(topic, "revspace/button/skip") == 0) {
            char *argv[] = {(char *)"skip", topic, payload};
            int result = do_skip(3, argv);
            print("%d\n", result);
        }
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
    WiFi.softAP(AP_NAME, nullptr, 1);
    
    WifiEspNow.begin();
    WifiEspNow.onReceive(onReceiveCallback, &espnow_data);

    WiFi.begin("revspace-pub-2.4ghz");
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
    
    // keep mqtt alive
    mqttClient.loop();
}

