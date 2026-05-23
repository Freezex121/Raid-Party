#ifndef TWEEN_H
#define TWEEN_H

#include <stdbool.h>

typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD, EASE_OUT_QUAD, EASE_IN_OUT_QUAD,
    EASE_OUT_CUBIC, EASE_IN_OUT_CUBIC,
    EASE_OUT_ELASTIC,
    EASE_OUT_BOUNCE,
    EASE_IN_BACK, EASE_OUT_BACK
} Easing;

int tween_create(float *target, float end, float duration, Easing easing);
void tween_on_complete(int id, void (*callback)(void));
int tween_chain(int prev_id, float *target, float end, float duration, Easing easing);
void tween_update(float dt);
void tween_kill(int id);
void tween_kill_all(void);
bool tween_is_active(int id);
float tween_ease(Easing easing, float t);

#endif
