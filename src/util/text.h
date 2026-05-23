#ifndef TEXT_H
#define TEXT_H

#include "raylib.h"

typedef struct {
    const char *text;
    int count;
    Color color;
} StyledSegment;

void draw_text_wrapped(const char *text, int x, int y, int max_width, int font_size, int line_spacing, Color color);
int  measure_text_wrapped(const char *text, int max_width, int font_size);

#endif
