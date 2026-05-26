#pragma once
#include <Arduino.h>

enum cube_state {
  IDLE,
  EDGE_BALANCE,
  CORNER_BALANCE,
  JUMP_TO_EDGE,
  JUMP_TO_CORNER,
  FALL_TO_EDGE,
  FALL_TO_IDLE
};

extern cube_state current_state;

void set_state(cube_state new_state);
void init_controller();   


