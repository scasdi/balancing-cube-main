#include "ESP/commands.h"
#include "ESP/comms.h"
#include "ESP/pins.h"
#include "ESP/wifi_ap.h"
#include "components/servo.h"
#include "ESP/get_params.h"
#include "components/imu_sensor.h"
#include <Arduino.h>

String process_command(String command) {
    command.trim(); 

    if (command == "HELLO") {
        return "banana confirmed sir.";
    } 
    else if (command == "ON_Y") {
        digitalWrite(LED_Y, HIGH);
        return "LED_ON_OK";
    } 
    else if (command == "OFF_Y") {
        digitalWrite(LED_Y, LOW);
        return "LED_OFF_OK";
    }
    else if (command == "ON_O") {
        digitalWrite(LED_O, HIGH);
        return "LED_O_ON_OK";
    }
    else if (command == "OFF_O") {
        digitalWrite(LED_O, LOW);
        return "LED_O_OFF_OK";
    }

    return "UNKNOWN_CMD: " + command;
}

void commands_init() {
}

static bool lastPressed = false;
static bool stablePressed = false;
static bool lastRawPressed = false;
static unsigned long lastDebounceMillis = 0;
static constexpr unsigned long BUTTON_DEBOUNCE_MS = 20u; 

static unsigned long lastImuPrintMillis = 0;
static constexpr unsigned long IMU_PRINT_INTERVAL_MS = 50u;

void commands_update(unsigned long currentMillis) {
    check_wifi_commands();

    bool rawPressed = digitalRead(BUTTON) == LOW;
    if (rawPressed != lastRawPressed) {
        lastDebounceMillis = currentMillis;
    }

    if ((currentMillis - lastDebounceMillis) > BUTTON_DEBOUNCE_MS) {
        if (rawPressed != stablePressed) {
            stablePressed = rawPressed;
            if (stablePressed == true && lastPressed == false) {
                if (!servo_is_jumped()) {
                    servo_jump();
                }
            }

            if (stablePressed != lastPressed) {
                const char* cmd = stablePressed ? "ON_O" : "OFF_O";
                String reply = process_command(String(cmd));
                send_comm_message(reply.c_str());
                lastPressed = stablePressed;
            }
        }
    }
    lastRawPressed = rawPressed;
    digitalWrite(LED_O, stablePressed ? HIGH : LOW);

    if (Serial.available()) {
        String incoming_cmd = Serial.readStringUntil('\n');
        incoming_cmd.trim();
        if (incoming_cmd.length() > 0) {
            if (incoming_cmd == "JUMP") {
                servo_jump();
                send_comm_message("SNAP_TRIGGERED");
            } else if (incoming_cmd == "START_PENDULUM") {
                get_params_start(10000);
            } else {
                String reply = process_command(incoming_cmd);
                send_comm_message(reply.c_str());
            }
        }
    }

    if ((currentMillis - lastImuPrintMillis) > IMU_PRINT_INTERVAL_MS) {
        lastImuPrintMillis = currentMillis; 
        
        imu_data_t imu_raw = get_imu_data();
        float pitch_deg = imu_raw.pitch * 180.0f / PI;
        
        char buf[64];
        snprintf(buf, sizeof(buf), "Pitch: %.2f", pitch_deg);
        send_comm_message(buf);
    }
}