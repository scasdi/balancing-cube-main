#include "components/imu_sensor.h"
#include <Arduino.h>

Adafruit_BNO08x imu;
sh2_SensorValue_t sensor_value;

static imu_data_t current_data = {0.0f, 0.0f};

bool init_IMU() {
    Serial.println("Initializing IMU...");
    if (!imu.begin_I2C()) {
        Serial.println("Failed to initialize IMU!");
        return false;
    }
    Serial.println("IMU initialized successfully.");
    
    // Enable Rotation Vector at 50Hz (20000 microseconds) to match LQR rate
    if (!imu.enableReport(SH2_ROTATION_VECTOR, 20000)) {
        Serial.println("Failed to enable rotation vector report!");
        return false;
    }
    
    // Enable Calibrated Gyroscope at 50Hz (20000 microseconds)
    if (!imu.enableReport(SH2_GYROSCOPE_CALIBRATED, 20000)) {
        Serial.println("Failed to enable gyroscope report!");
        return false;
    }
    
    return true;
}

imu_data_t get_imu_data() {
    // Non-blocking loop to flush all available events in the sensor queue
    while (imu.getSensorEvent(&sensor_value)) {
        if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
            float qw = sensor_value.un.rotationVector.real;
            float qx = sensor_value.un.rotationVector.i;
            float qy = sensor_value.un.rotationVector.j;
            float qz = sensor_value.un.rotationVector.k;

            // Calculate pitch angle in radians for direct LQR consumption
            current_data.pitch = atan2(2.0f * (qw * qy - qz * qx), 1.0f - 2.0f * (qy * qy + qx * qx));
        }
        else if (sensor_value.sensorId == SH2_GYROSCOPE_CALIBRATED) {
            // Extract angular velocity around the Y-axis (pitch rate) in rad/s
            current_data.pitch_rate = sensor_value.un.gyroscope.y;
        }
    }
    return current_data;
}