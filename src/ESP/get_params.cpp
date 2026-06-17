#include "ESP/get_params.h"
#include "components/imu_sensor.h"
#include "ESP/comms.h"
#include <Arduino.h>

static bool active = false;
static unsigned long endTime = 0;
static unsigned long lastSampleTime = 0;
static const unsigned long PENDULUM_SAMPLE_RATE_MS = 10;

void get_params_init() {
  active = false;
  endTime = 0;
  lastSampleTime = 0;
}

void get_params_start(unsigned long duration_ms) {
  active = true;
  endTime = millis() + duration_ms;
  lastSampleTime = 0;
  send_comm_message("ACK_START_PENDULUM");
}

bool get_params_is_active() {
  return active;
}

void get_params_update(unsigned long currentMillis) {
  if (!active) return;

  if (currentMillis < endTime) {
    if (currentMillis - lastSampleTime >= PENDULUM_SAMPLE_RATE_MS) {
      lastSampleTime = currentMillis;
      float pitch = get_pitch_angle();
      char buf[64];
      snprintf(buf, sizeof(buf), "TEST_DATA:%lu,%.2f", currentMillis, pitch);
      send_comm_message(buf);
    }
  } else {
    active = false;
    send_comm_message("TEST_DONE");
  }
}
