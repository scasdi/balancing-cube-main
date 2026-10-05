/**
 * @file main.cpp
 * @brief Main entry point and orchestrator for the balancing cube system.
 */
#include <Arduino.h>

// --- ESP Subsystems ---
#include "ESP/pins.h"
#include "ESP/wifi_ap.h"
#include "ESP/web_server.h"
#include "ESP/state_machine.h"
#include "ESP/commands.h"
#include "ESP/comms.h"
#include "ESP/get_params.h"

// --- Components ---
#include "components/servo.h"
#include "components/motor_driver.h"

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
  motor_driver_init();   // Begins Serial2. Without it every torque command is a silent no-op.

  // Initialize communication and control subsystems
  init_wifi_AP();
  comms_init();          // Begins BluetoothSerial. Without it the BT link never comes up.
  commands_init();
  get_params_init();

  // Last: the control task starts here and expects comms and the motor link
  // to already exist, because it reports its first state transition through them.
  state_machine_init();
}

/**
 * @brief Main execution loop handling non-blocking updates for telemetry, commands, and actuators.
 */
void loop() {
  unsigned long currentMillis = millis();

  // Work the control task deferred because it cannot bound the duration:
  // comms and flash writes. loop() shares core 1 with the control task but
  // runs at priority 1 against its 3, so it is always the one preempted.
  state_machine_service();

  check_wifi_commands();
  commands_update(currentMillis);
  get_params_update(currentMillis);
  servo_update(currentMillis);
}