#include "text.h"
#include <string.h>
#include <ctype.h>

void draw_text_wrapped(const char *text, int x, int y, int max_width, int font_size, int line_spacing, Color color)
{
    if (!text || !*text) return;

    char buf[512];
    int bi = 0;
    int line_w = 0;
    int draw_y = y;

    const char *p = text;
    while (*p)
    {
        // Build up word
        char word[128];
        int wi = 0;
        while (*p && *p != ' ' && *p != '\n' && wi < 126)
            word[wi++] = *p++;
        word[wi] = '\0';

        int ww = MeasureText(word, font_size);
        int space_w = (bi > 0) ? MeasureText(" ", font_size) : 0;

        if (line_w + ww + space_w > max_width && bi > 0)
        {
            buf[bi] = '\0';
            DrawText(buf, x, draw_y, font_size, color);
            draw_y += font_size + line_spacing;
            bi = 0;
            line_w = 0;
            space_w = 0;
        }

        if (bi > 0) { buf[bi++] = ' '; line_w += space_w; }
        memcpy(buf + bi, word, wi);
        bi += wi;
        line_w += ww;

        // Skip spaces
        while (*p == ' ') p++;
        if (*p == '\n') { p++; buf[bi] = '\0'; DrawText(buf, x, draw_y, font_size, color); draw_y += font_size + line_spacing; bi = 0; line_w = 0; }
    }

    if (bi > 0)
    {
        buf[bi] = '\0';
        DrawText(buf, x, draw_y, font_size, color);
    }
}

int measure_text_wrapped(const char *text, int max_width, int font_size)
{
    (void)text;
    (void)max_width;
    (void)font_size;
    return font_size + 2;
}
