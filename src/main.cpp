/**
 * @file main.cpp
 * @brief Main entry point and orchestrator for the balancing cube system.
 */
#include <Arduino.h>

// --- ESP Subsystems ---
#include "ESP/pins.h"
#include "ESP/wifi_ap.h"
#include "ESP/web_server.h"
#include "ESP/controller.h"
#include "ESP/commands.h"
#include "ESP/comms.h"
#include "ESP/get_params.h"

// --- Components ---
#include "components/servo.h"
#include "components/imu_sensor.h"

/**
 * @brief Initializes hardware peripherals, communication interfaces, and control subsystems.
 */
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  delay(1000); // Allow hardware to stabilize before initialization

  init_pins();

  // Initialize hardware components
  servo_init();
  init_IMU(); 
  
  // Initialize communication and control subsystems
  init_wifi_AP();
  commands_init();
  get_params_init();
  init_controller();
}

/**
 * @brief Main execution loop handling non-blocking updates for telemetry, commands, and actuators.
 */
void loop() {
  unsigned long currentMillis = millis();

  commands_update(currentMillis);
  get_params_update(currentMillis); 
  servo_update(currentMillis);
}