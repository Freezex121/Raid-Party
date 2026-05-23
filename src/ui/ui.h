#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "game_text.h"
#include <stdbool.h>

typedef struct {
    Rectangle bounds;
    const char *text;
    Color bg_color;
    Color hover_color;
    Color text_color;
    Color border_color;
    float hover_t;
    int tween_id;
    bool last_hovered;
    bool pressed_this_frame;
} Button;

Button button_create(Rectangle bounds, const char *text, Color bg, Color hover, Color text_col);
void button_update(Button *btn);
void button_draw(Button *btn);
void button_set_disabled(Button *btn, bool disabled);

void draw_panel(Rectangle bounds, Color bg, float corner_radius, Color border);
void draw_label(const char *text, Vector2 pos, int size, Color color);

#endif
