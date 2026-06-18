#include <Arduino.h>

// --- ESP Subsystems ---
#include "ESP/pins.h"
#include "ESP/wifi_ap.h"
#include "ESP/web_server.h"
#include "ESP/controller.h"
#include "ESP/commands.h"
#include "ESP/comms.h"

// --- Components ---
#include "components/servo.h"
#include "components/imu_sensor.h"

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  delay(1000);

  init_pins();

  // Initialize modules
  servo_init();
  init_wifi_AP();
  if (!init_IMU()) {
    Serial.println("CRITICAL ERROR: IMU initialization failed!");
  }
  commands_init();
  get_params_init();
  init_controller();
}

void loop() {
  unsigned long currentMillis = millis();

  // High-level module updates only — main is an orchestrator now.
  commands_update(currentMillis);
  get_params_update(currentMillis);
  servo_update(currentMillis);
}