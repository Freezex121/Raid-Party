#define GAME_TEXT_NO_REDIRECT
#include "ui/game_text.h"
#include "assets.h"
#include <math.h>

static float text_spacing(int font_size)
{
    (void)font_size;
    return 2.0f;
}

static Font active_font(int font_size)
{
    if (font_size >= UI_FONT_MIN_SIZE && font_size <= UI_FONT_MAX_SIZE &&
        g_assets.ui_font_sizes_loaded[font_size] &&
        g_assets.ui_fonts[font_size].texture.id != 0)
        return g_assets.ui_fonts[font_size];

    if (g_assets.ui_font.texture.id != 0)
        return g_assets.ui_font;

    return GetFontDefault();
}

static float font_draw_size(int font_size)
{
    float scale = (font_size >= 0 && font_size <= UI_FONT_MAX_SIZE)
        ? g_assets.ui_font_scales[font_size] : 1.0f;
    return (float)font_size * scale;
}

void game_draw_text(const char *text, int pos_x, int pos_y, int font_size, Color color)
{
    if (!text || font_size <= 0) return;
    Font font = active_font(font_size);
    float draw_size = font_draw_size(font_size);
    DrawTextEx(font, text, (Vector2){ (float)pos_x, (float)pos_y }, draw_size, text_spacing(font_size), color);
}

int game_measure_text(const char *text, int font_size)
{
    if (!text || font_size <= 0) return 0;
    Font font = active_font(font_size);
    float draw_size = font_draw_size(font_size);
    Vector2 size = MeasureTextEx(font, text, draw_size, text_spacing(font_size));
    return (int)ceilf(size.x);
}
