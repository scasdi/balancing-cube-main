#include "IMU sensor.h"

Adafruit_BNO08x imu;
sh2_SensorValue_t sensor_value;

bool init_IMU() {
  Serial.println("Initializing IMU...");
    if (!imu.begin_I2C()) {
        Serial.println("Failed to initialize IMU!");
        return false;
    }
    Serial.println("IMU initialized successfully.");
    imu.enableReport(SH2_ROTATION_VECTOR, 10000); // Enable rotation vector report with a 10ms interval
    return true;
}

float get_pitch_angle() {
    if (imu.getSensorEvent(&sensor_value)) {
        if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
            // Convert the rotation vector to Euler angles
            float qw = sensor_value.un.rotationVector.real;
            float qx = sensor_value.un.rotationVector.i;
            float qy = sensor_value.un.rotationVector.j;
            float qz = sensor_value.un.rotationVector.k;

            // Calculate pitch angle (in degrees)
            float pitch = atan2(2.0f * (qw * qy - qz * qx), 1.0f - 2.0f * (qy * qy + qx * qx)) * 180.0f / PI;
            return pitch;
        }
    }
    return 0.0f; // Return 0 if no valid data is available
}