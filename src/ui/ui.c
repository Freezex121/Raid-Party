#include "ui.h"
#include "util/tween.h"
#include "util/text.h"
#include "util/math_utils.h"
#include <string.h>

Button button_create(Rectangle bounds, const char *text, Color bg, Color hover, Color text_col)
{
    Button btn;
    btn.bounds = bounds;
    btn.text = text;
    btn.bg_color = bg;
    btn.hover_color = hover;
    btn.text_color = text_col;
    btn.border_color = (Color){0, 0, 0, 0};
    btn.hover_t = 0.0f;
    btn.tween_id = -1;
    btn.last_hovered = false;
    btn.pressed_this_frame = false;
    return btn;
}

void button_update(Button *btn)
{
    btn->pressed_this_frame = false;
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, btn->bounds);

    if (hovered && !btn->last_hovered && !tween_is_active(btn->tween_id))
    {
        btn->tween_id = tween_create(&btn->hover_t, 1.0f, 0.12f, EASE_OUT_QUAD);
    }
    else if (!hovered && btn->last_hovered && !tween_is_active(btn->tween_id))
    {
        btn->tween_id = tween_create(&btn->hover_t, 0.0f, 0.15f, EASE_OUT_QUAD);
    }

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        btn->pressed_this_frame = true;

    btn->last_hovered = hovered;
}

void button_draw(Button *btn)
{
    Color bg = { 0 };

    float t = btn->hover_t;
    bg.r = (unsigned char)(btn->bg_color.r + (btn->hover_color.r - btn->bg_color.r) * t);
    bg.g = (unsigned char)(btn->bg_color.g + (btn->hover_color.g - btn->bg_color.g) * t);
    bg.b = (unsigned char)(btn->bg_color.b + (btn->hover_color.b - btn->bg_color.b) * t);
    bg.a = (unsigned char)(btn->bg_color.a + (btn->hover_color.a - btn->bg_color.a) * t);

    DrawRectangleRec(btn->bounds, bg);

    if (btn->border_color.a > 0)
        DrawRectangleLinesEx(btn->bounds, 2.0f, btn->border_color);

    int font_size = 10;
    int text_h = measure_text_box(btn->text, snap_i(btn->bounds.width - 8), font_size, 0);
    if (text_h <= 0) text_h = ui_line_height(font_size);
    int y = snap_i(btn->bounds.y + (btn->bounds.height - text_h) * 0.5f);
    if (y < snap_i(btn->bounds.y + 2)) y = snap_i(btn->bounds.y + 2);

    draw_text_box((Rectangle){
            btn->bounds.x + 4.0f,
            (float)y,
            btn->bounds.width - 8.0f,
            btn->bounds.y + btn->bounds.height - y - 2.0f
        },
        btn->text, font_size, 0, btn->text_color, TEXT_ALIGN_CENTER);
}

void button_set_disabled(Button *btn, bool disabled)
{
    (void)disabled;
}

void draw_panel(Rectangle bounds, Color bg, float corner_radius, Color border)
{
    (void)corner_radius;
    DrawRectangleRec(bounds, bg);
    if (border.a > 0)
        DrawRectangleLinesEx(bounds, 1.0f, border);
}

void draw_label(const char *text, Vector2 pos, int size, Color color)
{
    DrawText(text, snap_i(pos.x), snap_i(pos.y), ui_font_size(size), color);
}

