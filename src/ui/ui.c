#include "ui.h"
#include "assets.h"
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

static void draw_button_label_centered(Rectangle bounds, const char *text, Color color)
{
    if (!text || !text[0]) return;

    const int font_size = 10;
    const float pad_x = 4.0f;
    int text_w = snap_i(bounds.width - pad_x * 2.0f);
    if (text_w < 1) text_w = 1;

    int text_h = measure_text_box(text, text_w, font_size, 0);
    if (text_h <= 0) text_h = ui_line_height(font_size);

    int y = snap_i(bounds.y + (bounds.height - (float)text_h) * 0.5f);
    int min_y = snap_i(bounds.y + 1.0f);
    if (y < min_y) y = min_y;

    draw_text_box((Rectangle){
            bounds.x + pad_x,
            (float)y,
            bounds.width - pad_x * 2.0f,
            bounds.y + bounds.height - (float)y - 1.0f
        },
        text, font_size, 0, color, TEXT_ALIGN_CENTER);
}

void button_update(Button *btn)
{
    btn->pressed_this_frame = false;
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, btn->bounds);
    btn->hover_t = hovered ? 1.0f : 0.0f;

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

    draw_button_label_centered(btn->bounds, btn->text, btn->text_color);
}

void button_set_disabled(Button *btn, bool disabled)
{
    (void)disabled;
}

void button_draw_9slice(Button *btn)
{
    float t = btn->hover_t;
    Color tint;
    tint.r = (unsigned char)(btn->bg_color.r + (btn->hover_color.r - btn->bg_color.r) * t);
    tint.g = (unsigned char)(btn->bg_color.g + (btn->hover_color.g - btn->bg_color.g) * t);
    tint.b = (unsigned char)(btn->bg_color.b + (btn->hover_color.b - btn->bg_color.b) * t);
    tint.a = 255;

    draw_9slice(g_assets.btn_standard, 6, 6, btn->bounds, tint);

    draw_button_label_centered(btn->bounds, btn->text, btn->text_color);
}

void draw_9slice(Texture2D tex, int left, int right, Rectangle dest, Color tint)
{
    if (tex.id == 0)
    {
        DrawRectangleRec(dest, tint);
        return;
    }
    int src_h = tex.height;
    int center_src_w = tex.width - left - right;
    if (center_src_w < 1) center_src_w = 1;
    float cw = dest.width - (float)left - (float)right;
    if (cw < 1.0f) cw = 1.0f;

    // Left cap
    DrawTexturePro(tex,
        (Rectangle){ 0.0f, 0.0f, (float)left, (float)src_h },
        (Rectangle){ dest.x, dest.y, (float)left, dest.height },
        (Vector2){ 0, 0 }, 0.0f, tint);
    // Center stretch
    DrawTexturePro(tex,
        (Rectangle){ (float)left, 0.0f, (float)center_src_w, (float)src_h },
        (Rectangle){ dest.x + (float)left, dest.y, cw, dest.height },
        (Vector2){ 0, 0 }, 0.0f, tint);
    // Right cap
    DrawTexturePro(tex,
        (Rectangle){ (float)(left + center_src_w), 0.0f, (float)right, (float)src_h },
        (Rectangle){ dest.x + dest.width - (float)right, dest.y, (float)right, dest.height },
        (Vector2){ 0, 0 }, 0.0f, tint);
}

void draw_btn_standard(Rectangle dest, Color normal, Color hover, const char *label)
{
    if (!label || !label[0]) return;
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, dest);
    Color tint = hovered ? hover : normal;
    Texture2D tex = g_assets.btn_standard;
    int left = 6, right = 6;
    draw_9slice(tex, left, right, dest, tint);
    draw_button_label_centered(dest, label, RAYWHITE);
}

void draw_btn_large(Rectangle dest, Color normal, Color hover, const char *title, const char *subtitle)
{
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, dest);
    Color tint = hovered ? hover : normal;
    Texture2D tex = g_assets.btn_large;
    int left = 8, right = 8;
    draw_9slice(tex, left, right, dest, tint);
    int title_h = title && title[0] ? ui_line_height(10) : 0;
    int sub_h = subtitle && subtitle[0] ? ui_line_height(10) : 0;
    int gap = title_h > 0 && sub_h > 0 ? 2 : 0;
    int total_h = title_h + gap + sub_h;
    int y = snap_i(dest.y + (dest.height - (float)total_h) * 0.5f);
    if (title && title[0])
        draw_text_box((Rectangle){ dest.x + 4.0f, (float)y, dest.width - 8.0f, (float)title_h },
            title, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    if (subtitle && subtitle[0])
    {
        Color sub_col = { 180, 185, 210, 220 };
        draw_text_box((Rectangle){ dest.x + 4.0f, (float)(y + title_h + gap), dest.width - 8.0f, (float)sub_h },
            subtitle, 10, 0, sub_col, TEXT_ALIGN_CENTER);
    }
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

void scroll_panel_begin(ScrollPanel *panel, Rectangle bounds)
{
    if (panel->bounds.x != bounds.x || panel->bounds.y != bounds.y ||
        panel->bounds.width != bounds.width || panel->bounds.height != bounds.height)
    {
        panel->scroll_y = 0;
        panel->bounds = bounds;
    }
    panel->content_height = 0;
}

int scroll_panel_y(ScrollPanel *panel)
{
    return (int)panel->bounds.y + 8 - panel->scroll_y;
}

void scroll_panel_end(ScrollPanel *panel)
{
    int view_h = (int)panel->bounds.height - 16;
    if (panel->content_height > view_h)
    {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, panel->bounds))
        {
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
                panel->scroll_y -= (int)(wheel * 24.0f);
        }
        int max_scroll = panel->content_height - view_h;
        if (panel->scroll_y < 0) panel->scroll_y = 0;
        if (panel->scroll_y > max_scroll) panel->scroll_y = max_scroll;

        // Scrollbar
        int bar_x = (int)(panel->bounds.x + panel->bounds.width - 5);
        int bar_y = (int)panel->bounds.y + 8;
        int bar_h = view_h;
        DrawRectangle(bar_x, bar_y, 2, bar_h, (Color){ 40, 42, 56, 210 });

        float ratio = (float)view_h / (float)panel->content_height;
        int thumb_h = (int)(bar_h * ratio);
        if (thumb_h < 8) thumb_h = 8;
        float t = max_scroll > 0 ? (float)panel->scroll_y / (float)max_scroll : 0.0f;
        int thumb_y = bar_y + (int)((bar_h - thumb_h) * t);
        DrawRectangle(bar_x, thumb_y, 2, thumb_h, (Color){ 135, 145, 185, 235 });
    }
}

