#pragma once
#include <Arduino.h>

// Unified communication helpers. Prefers WiFi when available, falls back to Serial.
void comms_init();
void send_comm_message(const char* msg);
