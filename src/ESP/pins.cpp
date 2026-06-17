#include <Arduino.h>
#include "ESP/pins.h"

void init_pins() {
  pinMode(LED_Y, OUTPUT);
  digitalWrite(LED_Y, LOW);

  pinMode(LED_O, OUTPUT);
  digitalWrite(LED_O, LOW);

  pinMode(BUTTON, INPUT_PULLUP);
}
