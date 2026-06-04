#ifndef CLASS_COLORS_H
#define CLASS_COLORS_H

#include "raylib.h"
#include "party.h"

static inline Color class_accent_color(ClassType ct)
{
    return (Color){ class_color_r(ct), class_color_g(ct), class_color_b(ct), 255 };
}

#endif
