#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <math.h>

static inline int snap_i(float value)
{
    return value >= 0.0f ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

#endif
