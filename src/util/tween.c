#include "tween.h"
#include <math.h>
#include <stddef.h>

#define MAX_TWEENS 64

typedef struct {
    float *target;
    float start, end;
    float duration, elapsed;
    Easing easing;
    bool active, running;
    void (*on_complete)(void);
    int chain_prev;
} TweenEntry;

static TweenEntry tweens[MAX_TWEENS];

float tween_ease(Easing easing, float t)
{
    if (t >= 1.0f) return 1.0f;
    if (t <= 0.0f) return 0.0f;
    switch (easing)
    {
        case EASE_LINEAR:       return t;
        case EASE_IN_QUAD:      return t * t;
        case EASE_OUT_QUAD:     return t * (2.0f - t);
        case EASE_IN_OUT_QUAD:  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case EASE_OUT_CUBIC:    { float f = t - 1.0f; return f * f * f + 1.0f; }
        case EASE_IN_OUT_CUBIC: return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
        case EASE_OUT_ELASTIC:
        {
            if (t == 0.0f || t == 1.0f) return t;
            return powf(2.0f, -10.0f * t) * sinf((t - 0.075f) * (2.0f * 3.14159265f) / 0.3f) + 1.0f;
        }
        case EASE_OUT_BOUNCE:
        {
            if (t < 1.0f / 2.75f) return 7.5625f * t * t;
            else if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
            else if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
            else { t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f; }
        }
        case EASE_IN_BACK:
        {
            float c1 = 1.70158f;
            return t * t * ((c1 + 1.0f) * t - c1);
        }
        case EASE_OUT_BACK:
        {
            float c1 = 1.70158f;
            float f = t - 1.0f;
            return f * f * ((c1 + 1.0f) * f + c1) + 1.0f;
        }
    }
    return t;
}

int tween_create(float *target, float end, float duration, Easing easing)
{
    for (int i = 0; i < MAX_TWEENS; i++)
    {
        if (!tweens[i].active)
        {
            tweens[i].target = target;
            tweens[i].start = *target;
            tweens[i].end = end;
            tweens[i].duration = duration;
            tweens[i].elapsed = 0.0f;
            tweens[i].easing = easing;
            tweens[i].active = true;
            tweens[i].running = true;
            tweens[i].on_complete = NULL;
            tweens[i].chain_prev = -1;
            return i;
        }
    }
    return -1;
}

void tween_on_complete(int id, void (*callback)(void))
{
    if (id >= 0 && id < MAX_TWEENS && tweens[id].active)
        tweens[id].on_complete = callback;
}

int tween_chain(int prev_id, float *target, float end, float duration, Easing easing)
{
    int id = tween_create(target, end, duration, easing);
    if (id >= 0)
    {
        tweens[id].running = false;
        tweens[id].chain_prev = prev_id;
    }
    return id;
}

void tween_update(float dt)
{
    for (int i = 0; i < MAX_TWEENS; i++)
    {
        if (!tweens[i].active) continue;

        if (!tweens[i].running)
        {
            if (tweens[i].chain_prev >= 0 && !tweens[tweens[i].chain_prev].active)
                tweens[i].running = true;
            else
                continue;
        }

        tweens[i].elapsed += dt;
        float t = tweens[i].elapsed / tweens[i].duration;

        if (t >= 1.0f)
        {
            *tweens[i].target = tweens[i].end;
            tweens[i].active = false;
            if (tweens[i].on_complete)
                tweens[i].on_complete();
        }
        else
        {
            *tweens[i].target = tweens[i].start + (tweens[i].end - tweens[i].start) * tween_ease(tweens[i].easing, t);
        }
    }
}

void tween_kill(int id)
{
    if (id >= 0 && id < MAX_TWEENS)
        tweens[id].active = false;
}

void tween_kill_all(void)
{
    for (int i = 0; i < MAX_TWEENS; i++)
        tweens[i].active = false;
}

bool tween_is_active(int id)
{
    return id >= 0 && id < MAX_TWEENS && tweens[id].active;
}
