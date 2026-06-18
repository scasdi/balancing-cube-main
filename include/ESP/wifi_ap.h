#pragma once
#include <WiFi.h>
#include "ESP/pins.h"

void init_wifi_AP();
void check_wifi_commands();
bool wifi_has_client();
bool send_wifi_message(const char* msg);
