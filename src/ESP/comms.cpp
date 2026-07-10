#include "ESP/comms.h"
#include "ESP/wifi_ap.h"
#include "ESP/commands.h"
#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

void comms_init() {
    SerialBT.begin("cool cube");
    Serial.println("Bluetooth initialized as 'cool cube'.");
}

void send_comm_message(const char* msg) {
    if (wifi_has_client()) {
        send_wifi_message(msg);
    }
    
    if (SerialBT.hasClient()) {
        SerialBT.println(msg);
    }
    
    Serial.println(msg);
}

void check_bluetooth_commands() {
    if (SerialBT.available()) {
        String incoming_cmd = SerialBT.readStringUntil('\n');
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