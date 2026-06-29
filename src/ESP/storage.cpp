#include "ESP/storage.h"
#include <Preferences.h>

static Preferences prefs;

void storage_init() {
    prefs.begin("cube_cfg", false);
}

void storage_save_gains(float k1, float k2, float k3) {
    prefs.putFloat("k1", k1);
    prefs.putFloat("k2", k2);
    prefs.putFloat("k3", k3);
}

void storage_load_gains(float* k1, float* k2, float* k3) {
    *k1 = prefs.getFloat("k1", 0.0f);
    *k2 = prefs.getFloat("k2", 0.0f);
    *k3 = prefs.getFloat("k3", 0.0f);
}

void storage_save_pitch_offset(float offset) {
    prefs.putFloat("p_off", offset);
}

float storage_load_pitch_offset() {
    return prefs.getFloat("p_off", 0.0f);
}