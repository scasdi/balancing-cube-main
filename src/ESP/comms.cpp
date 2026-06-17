#include "ESP/comms.h"
#include "ESP/wifi_ap.h"
#include <Arduino.h>

void comms_init() {
  // nothing for now
}

void send_comm_message(const char* msg) {
  if (wifi_has_client()) {
    send_wifi_message(msg);
  } else {
    Serial.println(msg);
  }
}
