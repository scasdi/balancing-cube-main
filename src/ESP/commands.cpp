#include "ESP/commands.h"
#include "ESP/comms.h"
#include "ESP/pins.h"
#include "ESP/wifi_ap.h"
#include "components/servo.h"
#include "ESP/get_params.h"
#include "ESP/controller.h"
#include "ESP/LQR.h"
#include <Arduino.h>

String process_command(String command) {
    command.trim(); 

    if (command == "HELLO") {
        return "banana confirmed sir.";
    } 
    else if (command.startsWith("SET_GAINS:")) {
        // Expected format: SET_GAINS:K1,K2,K3 (e.g., SET_GAINS:-31.6,-5.2,-0.3)
        String gains_str = command.substring(10);
        int first_comma = gains_str.indexOf(',');
        int second_comma = gains_str.indexOf(',', first_comma + 1);
        
        if (first_comma != -1 && second_comma != -1) {
            float k1 = gains_str.substring(0, first_comma).toFloat();
            float k2 = gains_str.substring(first_comma + 1, second_comma).toFloat();
            float k3 = gains_str.substring(second_comma + 1).toFloat();
            
            lqr_set_gains(k1, k2, k3);
            return "GAINS_UPDATED_OK";
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

    return "UNKNOWN_CMD: " + command;
}

void commands_init() {
}

static unsigned long lastImuPrintMillis = 0;
static constexpr unsigned long IMU_PRINT_INTERVAL_MS = 50u;

void commands_update(unsigned long currentMillis) {
    check_wifi_commands();

    // Button handling omitted for brevity, keeping original logic if needed...

    if (Serial.available()) {
        String incoming_cmd = Serial.readStringUntil('\n');
        incoming_cmd.trim();
        if (incoming_cmd.length() > 0) {
            if (incoming_cmd == "START_PENDULUM") {
                get_params_start(10000);
            } else {
                String reply = process_command(incoming_cmd);
                send_comm_message(reply.c_str());
            }
        }
    }

    // Telemetry Peeking - Thread Safe and Zero Delay
    if ((currentMillis - lastImuPrintMillis) > IMU_PRINT_INTERVAL_MS) {
        lastImuPrintMillis = currentMillis; 
        
        float pitch_rad = get_shared_pitch_rad();
        float pitch_deg = pitch_rad * 180.0f / PI;
        
        char buf[64];
        snprintf(buf, sizeof(buf), "Pitch: %.2f", pitch_deg);
        send_comm_message(buf);
    }
}