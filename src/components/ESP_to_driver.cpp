const int pwmPin = 18;

const int pwmFreq = 50;
const int pwmChannel = 0;
const int pwmResolution = 16;

const float max_delta = 10.0; 

void setup() {
  Serial.begin(115200);
  
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);
  
  sendDeltaOmega(0.0); 
}

void loop() {

    sendDeltaOmega(2.5);
  delay(2000);
  
  sendDeltaOmega(-5.0);
  delay(2000);
}

void sendDeltaOmega(float delta) {

    delta = constrain(delta, -max_delta, max_delta);
  
  float pulse_width_us = 1500.0 + (delta / max_delta) * 500.0;
  
  uint32_t duty = (pulse_width_us / 20000.0) * 65535;
  
  ledcWrite(pwmChannel, duty);
}