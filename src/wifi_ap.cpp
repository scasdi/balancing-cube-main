#include <WiFi.h>
#include <WiFiUdp.h>
#include <cstring>
#include "wifi_ap.h"
#include "commands.h"
#include "pins.h"
#include "network_config.h"

const char* ssid = "um... banana?";
const char* password = "yes banana";

WiFiUDP udp;

static IPAddress lastClientIP;
static uint16_t lastClientPort = 0;

void init_wifi_AP() {
    Serial.println("Initializing WiFi Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("Wi-Fi AP Started successfully!");
    
    // start UDP server after AP is up
    udp.begin(UDP_PORT);
    Serial.printf("Listening for UDP packets on port %d\n", UDP_PORT);
}

bool wifi_has_client() {
    return WiFi.softAPgetStationNum() > 0 && lastClientPort != 0;
}

bool send_wifi_message(const char* msg) {
    if (!wifi_has_client()) return false;
    udp.beginPacket(lastClientIP, lastClientPort);
    udp.print(msg);
    return udp.endPacket() == 1;
}

void check_wifi_commands() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        lastClientIP = udp.remoteIP();
        lastClientPort = udp.remotePort();

        char incomingPacket[255];
        int len = udp.read(incomingPacket, 255);
        if (len > 0) incomingPacket[len] = '\0';
        else incomingPacket[0] = '\0';

        // simple token compare without heap allocation
        // trim trailing newlines/spaces
        int end = strlen(incomingPacket) - 1;
        while (end >= 0 && (incomingPacket[end] == '\n' || incomingPacket[end] == '\r' || incomingPacket[end] == ' ')) {
            incomingPacket[end] = '\0';
            end--;
        }

        // process command through central processor and reply
        String reply = process_command(String(incomingPacket));

        udp.beginPacket(lastClientIP, lastClientPort);
        udp.print(reply);
        udp.endPacket();
    }
}
