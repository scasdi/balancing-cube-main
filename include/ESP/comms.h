#pragma once
#include <Arduino.h>

/**
 * @brief Initializes unified communication channels (Serial, WiFi, Bluetooth).
 */
void comms_init();

/**
 * @brief Broadcasts a message across all available communication channels.
 * @param msg The null-terminated string message to send.
 */
void send_comm_message(const char* msg);

/**
 * @brief Processes incoming data from the Bluetooth Serial buffer.
 */
void check_bluetooth_commands();