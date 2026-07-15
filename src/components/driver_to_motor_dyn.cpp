#include <SimpleFOC.h>

BLDCMotor motor = BLDCMotor(11);
BLDCDriver3PWM driver = BLDCDriver3PWM(PA8, PA9, PA10, PC10);


MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);

const float MOMENT_OF_INERTIA = 1.0f; 

float target_velocity = 0.0f;

String inputString = "";
bool stringComplete = false;

void setup() {

    Serial.begin(500000); 
  
  sensor.init();
  motor.linkSensor(&sensor);

  driver.voltage_power_supply = 12;
  driver.init();
  motor.linkDriver(&driver);

  motor.controller = MotionControlType::velocity;

  motor.PID_velocity.P = 0.0f;
  motor.PID_velocity.I = 2.0f;
  motor.PID_velocity.D = 0.0f;
  
  motor.voltage_limit = 3.0f; // V

  // Low pass filter for velocity to smooth out noise
  motor.LPF_velocity.Tf = 0.01f;

  Serial.println("Initializing FOC...");
  motor.init();
  motor.initFOC();
  Serial.println("FOC Ready.");
  
  inputString.reserve(32);
}

void loop() {

    motor.loopFOC();
  motor.move(target_velocity);

  receiveESPCommand();

  if (stringComplete) {
    float delta_L = inputString.toFloat();
    
    float delta_omega = delta_L / MOMENT_OF_INERTIA;
    
    target_velocity = motor.shaft_velocity + delta_omega;
    
    inputString = "";
    stringComplete = false;
  }
}

void receiveESPCommand() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
      break;
    } else {
      inputString += inChar;
    }
  }
}