#include "ESP/get_params.h"
#include "ESP/controller.h" // Changed: We now pull telemetry from the controller safely
#include "ESP/comms.h"
#include <Arduino.h>
#include <cmath> // Added for fabs()

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

/**
 * @brief Non-blocking update loop for telemetry parameter fetching.
 */
void get_params_update(unsigned long currentMillis) {
    if (!active) return;

    if (currentMillis < endTime) {
        if (currentMillis - lastSampleTime >= PENDULUM_SAMPLE_RATE_MS) {
            lastSampleTime = currentMillis;
            
            // Changed: Fetching safely from the RTOS controller task, NOT hardware directly
            float pitch_deg = get_shared_pitch_rad() * 180.0f / PI;
            
            static float last_pitch_deg = -999.0f; 
            
            // Fixed: Using fabs() instead of abs() for floating point arithmetic
            if (fabs(pitch_deg - last_pitch_deg) > 0.05f) { 
                char buf[64];
                snprintf(buf, sizeof(buf), "TEST_DATA:%lu,%.2f", currentMillis, pitch_deg);
                send_comm_message(buf);
                last_pitch_deg = pitch_deg; 
            }
        }
    } else {
        active = false;
        send_comm_message("TEST_DONE");
    }
}