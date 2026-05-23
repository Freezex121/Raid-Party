#define GAME_TEXT_NO_REDIRECT
#include "ui/game_text.h"
#include "assets.h"
#include <math.h>

static float text_spacing(int font_size)
{
    (void)font_size;
    return 0.0f;
}

static Font active_font(void)
{
    if (g_assets.ui_font.texture.id != 0)
        return g_assets.ui_font;
    return GetFontDefault();
}

void game_draw_text(const char *text, int pos_x, int pos_y, int font_size, Color color)
{
    if (!text || font_size <= 0) return;
    DrawTextEx(active_font(), text, (Vector2){ (float)pos_x, (float)pos_y }, (float)font_size, text_spacing(font_size), color);
}

int game_measure_text(const char *text, int font_size)
{
    if (!text || font_size <= 0) return 0;
    Vector2 size = MeasureTextEx(active_font(), text, (float)font_size, text_spacing(font_size));
    return (int)ceilf(size.x);
}
