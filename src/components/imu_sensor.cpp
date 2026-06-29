#include "components/imu_sensor.h"
#include <Arduino.h>

Adafruit_BNO08x imu;
sh2_SensorValue_t sensor_value;

static imu_data_t current_data = {0.0f, 0.0f};
static bool imu_is_ready = false;

/**
 * @brief Initializes the BNO08x IMU and enables required hardware reports.
 * @return true if successfully initialized, false otherwise.
 */
bool init_IMU() {
    Serial.println("Initializing IMU...");
    if (!imu.begin_I2C()) {
        Serial.println("Failed to initialize IMU!");
        return false;
    }
    
    // Enable Rotation Vector & Gyroscope at 50Hz (20000 microseconds)
    if (!imu.enableReport(SH2_ROTATION_VECTOR, 20000) || 
        !imu.enableReport(SH2_GYROSCOPE_CALIBRATED, 20000)) {
        Serial.println("Failed to enable IMU reports!");
        return false;
    }
    
    Serial.println("IMU initialized successfully.");
    imu_is_ready = true;
    return true;
}

/**
 * @brief Fetches and processes the latest IMU events from the hardware queue.
 * @return current imu_data_t struct containing pitch and pitch_rate.
 */
imu_data_t get_imu_data() {
    if (!imu_is_ready) {
        return current_data; 
    }

    // Flush all available events in the sensor queue
    while (imu.getSensorEvent(&sensor_value)) {
        if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
            float qw = sensor_value.un.rotationVector.real;
            float qx = sensor_value.un.rotationVector.i;
            float qy = sensor_value.un.rotationVector.j;
            float qz = sensor_value.un.rotationVector.k;

            current_data.pitch = atan2(2.0f * (qw * qy - qz * qx), 1.0f - 2.0f * (qy * qy + qx * qx));
        }
        else if (sensor_value.sensorId == SH2_GYROSCOPE_CALIBRATED) {
            current_data.pitch_rate = sensor_value.un.gyroscope.y;
        }
    }
    return current_data;
}