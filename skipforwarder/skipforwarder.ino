#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#include "cmdproc.h"
#include "editline.h"
#include "print.h"

static char esp_id[16];

static WiFiManager wifiManager;
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static char line[100];

void setup(void)
{
    PrintInit();
    EditInit(line, sizeof(line));

    // initialize serial port
    print("ESPNOW forwarder\n");

    // get ESP id
    sprintf(esp_id, "%08X", ESP.getChipId());
    print("ESP ID: %s\n", esp_id);

    // connect to wifi
    print("Starting WIFI manager ...");
    wifiManager.autoConnect("ESP-SKIP");
    print("setup done\n");
}

static int do_jukebox(int argc, char *argv[])
{
    return 0;
}

static int do_get(int argc, char *argv[])
{
    if (argc < 2) {
        print("Please provide an URL\n");
        return CMD_ARG;
    }
    
    return CMD_OK;
}

static int do_help(int argc, char *argv[]);

const cmd_t commands[] = {
    {"jukebox", do_jukebox, "Send a jukebox command"},
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

