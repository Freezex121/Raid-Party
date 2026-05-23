#ifndef CLASS_COLORS_H
#define CLASS_COLORS_H

#include "raylib.h"
#include "party.h"

static inline Color class_accent_color(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return (Color){ 0, 0, 255, 255 };
        case CLASS_CLERIC:   return (Color){ 255, 255, 255, 255 };
        case CLASS_MAGE:     return (Color){ 255, 0, 0, 255 };
        case CLASS_ROGUE:    return (Color){ 0, 255, 0, 255 };
        case CLASS_SHAMAN:   return (Color){ 0, 157, 0, 255 };
        case CLASS_RANGER:   return (Color){ 0, 0, 157, 255 };
        default:             return (Color){ 100, 100, 120, 255 };
    }
}

#endif
