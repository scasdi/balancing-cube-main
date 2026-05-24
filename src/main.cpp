#include <Arduino.h>
#include "IMU sensor.h"

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize
  if (!init_IMU()) {
    while (1); // Halt if IMU initialization fails
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  float pitch = get_pitch_angle();
  Serial.print("Pitch Angle: ");
  Serial.println(pitch);
  delay(10); // Delay to match the IMU report interval
}
