#pragma once
#include <Arduino.h>

void servo_init();
void servo_update(unsigned long currentMillis);
void servo_jump();
bool servo_is_jumped();
