#pragma once
#include <Adafruit_BNO08x.h>

struct imu_data_t {
    float pitch;
    float pitch_rate;
};

bool init_IMU();
imu_data_t get_imu_data();