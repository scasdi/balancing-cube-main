#ifndef ESP_CONTROLLER_H
#define ESP_CONTROLLER_H

#include <Arduino.h>

// 1. Define the enum so every file that includes this header knows what it is
enum cube_state {
    IDLE,
    EDGE_BALANCE,
    FALL_TO_IDLE,
    SYS_ID_TEST,
    CALIBRATING
};

// 2. Declare external access to the current state (if needed elsewhere)
extern cube_state current_state;

// 3. Function Prototypes (No bodies here!)
float get_shared_pitch_rad();
void set_state(cube_state new_state);
void init_controller();

#endif // ESP_CONTROLLER_H