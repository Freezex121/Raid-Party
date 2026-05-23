#include "ui.h"
#include "util/tween.h"
#include "util/text.h"
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

    int font_size = (btn->bounds.height <= 18.0f) ? 7 : 20;
    if (btn->bounds.height > 18.0f && btn->bounds.height < 36.0f)
        font_size = (int)(btn->bounds.height - 8.0f);
    if (font_size < 6) font_size = 6;

    int text_w = MeasureText(btn->text, font_size);
    int btn_center_x = (int)(btn->bounds.x + btn->bounds.width / 2.0f);
    if (text_w > btn->bounds.width - 8)
    {
        draw_text_wrapped(btn->text,
            (int)(btn->bounds.x + 4),
            (int)(btn->bounds.y + 3),
            (int)(btn->bounds.width - 8), font_size, 2, btn->text_color);
    }
    else
    {
        DrawText(btn->text,
            btn_center_x - text_w / 2,
            (int)(btn->bounds.y + btn->bounds.height / 2.0f - font_size / 2.0f),
            font_size, btn->text_color);
    }
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
    DrawText(text, (int)pos.x, (int)pos.y, size, color);
}

