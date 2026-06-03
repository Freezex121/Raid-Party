#ifndef SHAKE_H
#define SHAKE_H

#include "raylib.h"

float shake_amplitude_for_value(int value, float base_amplitude, float amplitude_per_value, float max_amplitude);
float shake_wave(float time, float frequency, float amplitude);
void shake_trigger(float amplitude);
void shake_trigger_for(float amplitude, float duration);
void shake_update(float dt);
Vector2 shake_screen_offset(void);

#endif
