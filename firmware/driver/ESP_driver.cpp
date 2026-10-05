#include <Arduino.h>

// Pin definitions
const int buttonPin = 1;
const int rxPin = 2;
const int txPin = 3;

// State machine variables
int motorState = 0; // 0: Stopped, 1: 300 RPM CW, 2: 300 RPM CCW

// Debounce variables
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, rxPin, txPin);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("ESP32 Ready. Press button to start.");
}

void loop() {
  int reading = digitalRead(buttonPin);

  // Check for button state change
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // If the state has been stable longer than the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      // Execute on button press (active LOW because of INPUT_PULLUP)
      if (buttonState == LOW) {
        motorState++;
        if (motorState > 2) {
          motorState = 0; // Reset to stopped state
        }

        // Send the corresponding SimpleFOC command via Serial2
        // 'T' is the default commander letter for Target velocity
        switch (motorState) {
          case 0:
            Serial.println("Command: Stop");
            Serial2.println("T0");
            break;
          case 1:
            Serial.println("Command: 300 RPM CW");
            Serial2.println("T31.42"); // 300 RPM in rad/s
            break;
          case 2:
            Serial.println("Command: 300 RPM CCW");
            Serial2.println("T-31.42"); // Reverse direction
            break;
        }
      }
    }
  }
  lastButtonState = reading;
}