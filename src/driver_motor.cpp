#include <SimpleFOC.h>

// GM4108H-120T typically has 11 pole pairs. Verify with your datasheet!
BLDCMotor motor = BLDCMotor(11);

// B-G431B-ESC1 uses a 6-PWM driver architecture
BLDCDriver6PWM driver = BLDCDriver6PWM(A_PHASE_UH, A_PHASE_UL, A_PHASE_VH, A_PHASE_VL, A_PHASE_WH, A_PHASE_WL);

// AS5048A PWM setup on the ESC's Hall 1 pin (PA15)
// The 4 and 904 represent the min and max raw microsecond counts for this specific sensor
#define SENSOR_PWM_PIN PA15
MagneticSensorPWM sensor = MagneticSensorPWM(SENSOR_PWM_PIN, 4, 904);

// Hardware interrupt routine to read the PWM pulse in the background
void handlePWM() {
  sensor.handlePWM();
}

// Instantiate commander to listen to UART (Serial) from the ESP32
Commander command = Commander(Serial);
void doTarget(char* cmd) { command.motor(&motor, cmd); }

void setup() {
  Serial.begin(115200);

  // Initialize sensor and attach the interrupt
  sensor.init();
  sensor.enableInterrupt(handlePWM);
  motor.linkSensor(&sensor);

  // Configure driver
  driver.voltage_power_supply = 12; // Assuming 12V power supply
  driver.init();
  motor.linkDriver(&driver);

  // Set control loop type to position control
  motor.controller = MotionControlType::angle;

  // Initialize motor
  motor.init();
  
  // Align sensor and start FOC
  motor.initFOC();

  // Add the motor to the commander (listens for "M" prefix)
  command.add('M', doTarget, "motor");

  Serial.println("AS5048A PWM Ready. Waiting for ESP32 commands...");
}

void loop() {
  // Main FOC algorithm loop
  motor.loopFOC();

  // Motion control function
  motor.move();

  // Listen for UART commands from ESP32
  command.run();
}