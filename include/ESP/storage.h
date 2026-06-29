#ifndef STORAGE_H
#define STORAGE_H

/**
 * @brief Initializes the Non-Volatile Storage (NVS) partition.
 */
void storage_init();

/**
 * @brief Saves LQR control gains to NVS.
 */
void storage_save_gains(float k1, float k2, float k3);

/**
 * @brief Loads LQR control gains from NVS.
 */
void storage_load_gains(float* k1, float* k2, float* k3);

/**
 * @brief Saves the static pitch offset calibration to NVS.
 */
void storage_save_pitch_offset(float offset);

/**
 * @brief Loads the static pitch offset calibration from NVS.
 * @return Pitch offset in radians.
 */
float storage_load_pitch_offset();

#endif