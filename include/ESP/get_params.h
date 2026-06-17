#pragma once
#include <Arduino.h>

void get_params_init();
void get_params_update(unsigned long currentMillis);
void get_params_start(unsigned long duration_ms);
bool get_params_is_active();
