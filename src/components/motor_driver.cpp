// // Define the UART pins for ESP32 to talk to the ESC
// #define RXp2 4
// #define TXp2 5

// void setup() {
//   // Serial for your computer monitor
//   Serial.begin(115200);
  
//   // Serial2 for communicating with the B-G431B-ESC1
//   Serial2.begin(115200, SERIAL_8N1, RXp2, TXp2);
  
//   Serial.println("ESP32 Master Initialized.");
//   delay(2000); // Give the ESC time to boot and align the FOC
// }

// void loop() {
//   // Command the ESC to move the motor to 3.14 radians (~180 degrees)
//   Serial.println("Commanding: 3.14 rads");
//   Serial2.println("M3.14"); 
//   delay(1000);

//   // Command the ESC to move back to 0 radians
//   Serial.println("Commanding: 0 rads");
//   Serial2.println("M0");
//   delay(1000);
  
// }