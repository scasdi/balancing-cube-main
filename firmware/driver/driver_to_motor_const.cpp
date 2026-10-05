#include <SimpleFOC.h>

BLDCMotor motor = BLDCMotor(11);

BLDCDriver3PWM driver = BLDCDriver3PWM(PA8, PA9, PA10, PB12);

float target_velocity = 5.0; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting SimpleFOC Open Loop Setup...");

  driver.voltage_power_supply = 12.0;
  
  driver.voltage_limit = 3.0; 
  driver.init();

  motor.linkDriver(&driver);

  motor.voltage_limit = 3.0;

  motor.controller = MotionControlType::velocity_openloop;

  motor.init();

  Serial.println("Motor initialized in Open Loop. Spinning up...");
}

void loop() {
  // The move() function must be executed in every loop iteration
  // In open-loop, it calculates the phase voltages based on the target velocity
  motor.move(target_velocity);
}