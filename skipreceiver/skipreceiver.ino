#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "WifiEspNow.h"
#include "ESP8266WiFi.h"

#include "editline.h"
#include "cmdproc.h"
#include "print.h"

static const char AP_NAME[] = "revspace-espnow";
static const int AP_CHANNEL = 1;

static char line[128];

static void show_help(const cmd_t *cmds)
{
    for (const cmd_t *cmd = cmds; cmd->cmd != NULL; cmd++) {
        print("%10s: %s\n", cmd->name, cmd->help);
    }
}

static void onReceiveCallback(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg)
{
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
    print("# TEXT:'%s'\n", buf);
}

void setup(void)
{
    Serial.begin(115200);
    print("\n#ESPNOW-RECV\n");

    EditInit(line, sizeof(line));

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);

    WifiEspNow.begin();
    WifiEspNow.onReceive(onReceiveCallback, NULL);
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

const cmd_t commands[] = {
    {"softap",  do_softap,  "[channel] set up softap on channel"},
    {"disc",    do_disc,    "softap disconnect"},

    {NULL, NULL, NULL}
};

void loop(void)
{
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
}

