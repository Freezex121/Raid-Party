#include "shake.h"
#include <math.h>
#include <stdlib.h>

static Vector2 screen_offset = { 0 };
static float screen_amplitude = 0.0f;
static float screen_timer = 0.0f;
static float screen_duration = 0.0f;

static float random_signed(void)
{
    return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

float shake_amplitude_for_value(int value, float base_amplitude, float amplitude_per_value, float max_amplitude)
{
    if (value <= 0 || max_amplitude <= 0.0f) return 0.0f;
    if (base_amplitude < 0.0f) base_amplitude = 0.0f;
    if (amplitude_per_value < 0.0f) amplitude_per_value = 0.0f;

    float amplitude = base_amplitude + (float)(value - 1) * amplitude_per_value;
    return amplitude < max_amplitude ? amplitude : max_amplitude;
}

float shake_wave(float time, float frequency, float amplitude)
{
    return sinf(time * frequency) * amplitude;
}

void shake_trigger(float amplitude)
{
    shake_trigger_for(amplitude, 0.15f);
}

void shake_trigger_for(float amplitude, float duration)
{
    if (amplitude <= 0.0f || duration <= 0.0f) return;
    if (amplitude > screen_amplitude)
        screen_amplitude = amplitude;
    if (duration > screen_timer)
        screen_timer = duration;
    if (duration > screen_duration)
        screen_duration = duration;
}

void shake_update(float dt)
{
    if (screen_timer <= 0.0f) return;

    screen_timer -= dt;
    if (screen_timer <= 0.0f)
    {
        screen_offset = (Vector2){ 0 };
        screen_amplitude = 0.0f;
        screen_duration = 0.0f;
        return;
    }

    float strength = screen_duration > 0.0f ? screen_timer / screen_duration : 0.0f;
    screen_offset.x = random_signed() * screen_amplitude * strength;
    screen_offset.y = random_signed() * screen_amplitude * strength;
}

Vector2 shake_screen_offset(void)
{
    return screen_offset;
}
