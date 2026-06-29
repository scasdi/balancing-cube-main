/**
 * @file commands.cpp
 * @brief Implementation of command processing and communication handling.
 */

#include "ESP/commands.h"
#include "ESP/comms.h"
#include "ESP/get_params.h"
#include "ESP/controller.h"
#include "ESP/LQR.h"
#include "ESP/storage.h"
#include "ESP/pins.h"
#include "components/servo.h"
#include <Arduino.h>

/**
 * @brief Stores the previous state of the user button for edge detection.
 */
static int last_button_state = LOW;

/**
 * @brief Stores the last time the button state changed for debouncing.
 */
static unsigned long last_debounce_time = 0;

/**
 * @brief Debounce delay in milliseconds.
 */
static const unsigned long DEBOUNCE_DELAY_MS = 50;

/**
 * @brief Processes an incoming command string and executes the corresponding action.
 * @param command The incoming command string.
 * @return String containing the response message.
 */
String process_command(String command) {
    command.trim(); 

    if (command.startsWith("SET_GAINS:")) {
        String gains_str = command.substring(10);
        int first_comma = gains_str.indexOf(',');
        int second_comma = gains_str.indexOf(',', first_comma + 1);
        
        if (first_comma != -1 && second_comma != -1) {
            float k1 = gains_str.substring(0, first_comma).toFloat();
            float k2 = gains_str.substring(first_comma + 1, second_comma).toFloat();
            float k3 = gains_str.substring(second_comma + 1).toFloat();
            
            lqr_set_gains(k1, k2, k3);
            storage_save_gains(k1, k2, k3);
            return "GAINS_SYNC_OK";
        }
        return "ERROR: INVALID_GAIN_FORMAT";
    }
    else if (command == "BALANCE") {
        set_state(EDGE_BALANCE);
        return "BALANCE_MODE_ACTIVATED";
    }
    else if (command == "STOP") {
        set_state(IDLE);
        return "MOTORS_STOPPED";
    }
    else if (command == "CALIBRATE_ZERO") {
        set_state(CALIBRATING);
        return "STARTING_CALIBRATION";
    }
    else if (command == "GET_PITCH") {
        float pitch_deg = get_shared_pitch_rad() * 180.0f / PI;
        char buf[32];
        snprintf(buf, sizeof(buf), "CURRENT_PITCH: %.2f", pitch_deg);
        return String(buf);
    }

    return "UNKNOWN_CMD: " + command;
}

/**
 * @brief Initializes the command module and hardware button pin.
 */
void commands_init() {
    pinMode(BUTTON, INPUT);
    last_button_state = digitalRead(BUTTON);
}

/**
 * @brief Updates the command module, handling incoming serial data and hardware button events.
 * @param currentMillis The current system time in milliseconds.
 */
void commands_update(unsigned long currentMillis) {
    // --- Button Edge Detection & Debounce ---
    int current_button_state = digitalRead(BUTTON);
    
    if (current_button_state != last_button_state) {
        last_debounce_time = currentMillis;
    }

    if ((currentMillis - last_debounce_time) > DEBOUNCE_DELAY_MS) {
        static int button_validated_state = LOW;
        if (current_button_state != button_validated_state) {
            button_validated_state = current_button_state;
            
            if (button_validated_state == HIGH) {
                send_comm_message("BUTTON_PRESSED");
                servo_jump(); // <-- הקריאה לסרוו לקפוץ!
            }
        }
    }
    last_button_state = current_button_state;

    // --- Serial Command Handling ---
    if (Serial.available()) {
        String incoming_cmd = Serial.readStringUntil('\n');
        incoming_cmd.trim();
        if (incoming_cmd.length() > 0) {
            if (incoming_cmd == "START_PENDULUM") {
                set_state(SYS_ID_TEST); 
                get_params_start(10000);
            } 
            else if (incoming_cmd == "START_MOTOR_TEST") {
                set_state(SYS_ID_TEST);
                get_params_start(5000);
            }
            else {
                String reply = process_command(incoming_cmd);
                send_comm_message(reply.c_str());
            }
        }
    }
}