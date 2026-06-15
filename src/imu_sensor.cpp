#include "imu_sensor.h"
#include <Arduino.h>

Adafruit_BNO08x imu;
sh2_SensorValue_t sensor_value;

// משתנה סטטי שישמור את הזווית האחרונה בין קריאות
static float last_pitch = 0.0f; 

bool init_IMU() {
    Serial.println("Initializing IMU...");
    if (!imu.begin_I2C()) {
        Serial.println("Failed to initialize IMU!");
        return false;
    }
    Serial.println("IMU initialized successfully.");
    
    // קצב של 100 הרץ מתאים מאוד לבקרה
    imu.enableReport(SH2_ROTATION_VECTOR, 10000); 
    return true;
}

float get_pitch_angle() {
    // בודקים אם יש נתון חדש
    if (imu.getSensorEvent(&sensor_value)) {
        if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
            float qw = sensor_value.un.rotationVector.real;
            float qx = sensor_value.un.rotationVector.i;
            float qy = sensor_value.un.rotationVector.j;
            float qz = sensor_value.un.rotationVector.k;

            // חישוב הזווית ושמירתה למשתנה הסטטי
            last_pitch = atan2(2.0f * (qw * qy - qz * qx), 1.0f - 2.0f * (qy * qy + qx * qx)) * 180.0f / PI;
        }
    }
    // החזרת הערך העדכני ביותר (או הישן במקרה שאין עדכון)
    return last_pitch;
}