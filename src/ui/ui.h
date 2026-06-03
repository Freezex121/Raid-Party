#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "game_text.h"
#include <stdbool.h>

#define BTN_ICON   22
#define BTN_NARROW 80
#define BTN_MED    120
#define BTN_WIDE   160
#define BTN_FULL   200

#define MAX_BTN_IDS 64

typedef enum {
    BTN_ID_RUN_DECK,
    BTN_ID_RUN_END_TURN,
    BTN_ID_MAP_DECK,
    BTN_ID_MAP_SET,
    BTN_ID_REWARD_SKIP,
    BTN_ID_REWARD_REROLL,
    BTN_ID_REWARD_EXTRA,
    BTN_ID_DISCARD_SKIP,
    BTN_ID_DISCARD_CONFIRM,
    BTN_ID_ACHIEVEMENTS_BACK,
    BTN_ID_META_SHOP_BUY,
    BTN_ID_REST_HEAL,
    BTN_ID_REST_UPGRADE,
    BTN_ID_COUNT
} ButtonId;

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
void button_draw_9slice(Button *btn);

void draw_9slice(Texture2D tex, int left, int right, Rectangle dest, Color tint);
void draw_btn_standard(Rectangle dest, Color normal, Color hover, const char *label, int btn_id);
void draw_btn_large(Rectangle dest, Color normal, Color hover, const char *title, const char *subtitle, int btn_id);
void draw_panel(Rectangle bounds, Color bg, float corner_radius, Color border);
void draw_label(const char *text, Vector2 pos, int size, Color color);

typedef struct {
    Rectangle bounds;
    int content_height;
    int scroll_y;
} ScrollPanel;

void scroll_panel_begin(ScrollPanel *panel, Rectangle bounds);
int  scroll_panel_y(ScrollPanel *panel);
void scroll_panel_end(ScrollPanel *panel);

#endif
