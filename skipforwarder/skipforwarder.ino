#include <ctype.h>

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <HttpClient.h>
//#include <ESP8266WiFi.h>
#include "ESP8266HTTPClient.h"

#include "cmdproc.h"
#include "editline.h"
#include "print.h"

static char esp_id[16];

static WiFiManager wifiManager;
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static char line[200];

void setup(void)
{
    PrintInit();
    EditInit(line, sizeof(line));

    // initialize serial port
    print("ESPNOW forwarder\n");

    // get ESP id
    sprintf(esp_id, "%08X", ESP.getChipId());
    print("ESP ID: %s\n", esp_id);
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

static String append(const char *param, const char *value)
{
    String s = String(param) + "=";
    for (const char *p = value; *p != 0; p++) {
        char c = *p;
        if (isalnum(c)) {
            s += c;
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            s += buf;
        }
    }
    return s;
}

static String create_jukebox_url(const char *p0, const char *p1, const char *p2, const char *player)
{
    String url = "/Classic/status_header.html";
    url += append("?p0", p0);
    url += append("&p1", p1);
    url += append("&p2", p2);
    url += append("&player", player);

    return url;
}


static int do_skip(int argc, char *argv[])
{
    String url = "http://jukebox.space.revspace.nl:9000" + 
        create_jukebox_url("playlist", "jump", "+1", "b8:27:eb:ba:bc:d5");
    print("url = %s\n", url.c_str());

    HTTPClient httpClient;
    httpClient.begin(url);
    int result = httpClient.GET();
    httpClient.end();

    return result;
}

static int do_get(int argc, char *argv[])
{
    if (argc < 2) {
        print("Please provide a URL\n");
        return CMD_ARG;
    }

    char *url = argv[1];

    HTTPClient httpClient;
    httpClient.begin(url);
    int result = httpClient.GET();
    httpClient.end();

    print("Result: %d\n", result);

    return CMD_OK;
}

static int do_help(int argc, char *argv[]);

const cmd_t commands[] = {
    {"wifi",    do_wifi,    "<ssid> [password] connect to wifi"},
    {"skip",    do_skip,    "Send a skip command"},
    {"get",     do_get,     "[url] Perform a HTTP GET"},
    {"help",    do_help,    "Show help"},
    {NULL, NULL, NULL}
};

static int do_help(int argc, char *argv[])
{
    for (const cmd_t *cmd = commands; cmd->cmd != NULL; cmd++) {
        print("%10s: %s\n", cmd->name, cmd->help);
    }
    return 0;
}

void loop(void)
{
    bool haveLine = false;
    if (Serial.available()) {
        char c;
        haveLine = EditLine(Serial.read(), &c);
        Serial.print(c);
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
            do_help(0, NULL);
            break;
        default:
            print("%d\n", result);
            break;
        }
        print(">");
    }

    mqttClient.loop();
}

