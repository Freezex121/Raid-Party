#ifndef LAYOUT_H
#define LAYOUT_H

#include "raylib.h"
#include "constants.h"
#include <stdbool.h>

typedef struct {
    int count;
    int card_w;
    int card_h;
    int step;
    int start_x;
    int y;
    bool overlapping;
} HandLayout;

int layout_party_frame_width(int party_count);
int layout_party_frame_gap(int party_count);
int layout_party_start_x(int party_count);
Rectangle layout_party_frame_rect(int party_count, int index);

Vector2 layout_enemy_position(int enemy_count, int index);
Rectangle layout_enemy_hit_rect(Vector2 pos);
Rectangle layout_enemy_cast_bar_rect(Vector2 pos);

HandLayout layout_hand(int hand_count);
Rectangle layout_hand_card_rect(HandLayout layout, int index);

Rectangle layout_action_feed_panel(void);
Rectangle layout_card_inspector_panel(void);
Rectangle layout_energy_panel(void);
Rectangle layout_end_turn_button(void);
Rectangle layout_discard_pile_rect(void);

Rectangle layout_deck_browser_viewport(void);
Rectangle layout_deck_inspector_panel(void);

Rectangle layout_reward_card_rect(int reward_count, int index);
Rectangle layout_reward_inspector_panel(void);

#endif
