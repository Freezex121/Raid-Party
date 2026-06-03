#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <math.h>
#include <stdlib.h>

static inline int snap_i(float value)
{
    return value >= 0.0f ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

static inline float random_range(float min, float max)
{
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

#endif
