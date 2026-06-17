#pragma once
#include <Arduino.h>

// central command processor (keeps existing behavior)
String process_command(String command);

// Initialize command subsystem (button, serial handling)
void commands_init();

// Called from main loop with current millis
void commands_update(unsigned long currentMillis);
