#include <SimpleFOC.h>

// GM4108H motors typically have 11 pole pairs. 
// If it stutters or spins at the wrong speed, check the manufacturer spec.
BLDCMotor motor = BLDCMotor(11);

// B-G431B-ESC1 specific 6-PWM driver setup
BLDCDriver6PWM driver = BLDCDriver6PWM(A_PHASE_UH, A_PHASE_UL, A_PHASE_VH, A_PHASE_VL, A_PHASE_WH, A_PHASE_WL);

// Instantiate the commander to listen to the Serial port
Commander command = Commander(Serial);
void doTarget(char* cmd) { command.motor(&motor, cmd); }

void setup() {
  Serial.begin(115200);
  // Driver configuration
  driver.voltage_power_supply = 12; // Set to your power supply voltage (e.g., 12V or 24V)
  driver.init();
  motor.linkDriver(&driver);

  // Limiting voltage to prevent the gimbal motor from overheating
  motor.voltage_limit = 3; 

  // Configure Open Loop Velocity control
  motor.controller = MotionControlType::velocity_openloop;

  // Initialize motor
  motor.init();

  // Add the 'T' command to the commander, linking it to the motor target
  command.add('T', doTarget, "target velocity");

  Serial.println("Motor ready!");
}

void loop() {
  // Run the open-loop motor movement
  motor.move();
  
  // Listen for commands from the ESP32
  command.run();
}