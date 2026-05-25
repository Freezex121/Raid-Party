#include "layout.h"

int layout_party_frame_width(int party_count)
{
    return FRAME_W;
}

int layout_party_frame_gap(int party_count)
{
    return party_count >= 5 ? 5 : FRAME_GAP;
}

int layout_party_start_x(int party_count)
{
    int fw = layout_party_frame_width(party_count);
    int gap = layout_party_frame_gap(party_count);
    int total_w = party_count * fw + (party_count - 1) * gap;
    return (VIRT_W - total_w) / 2;
}

Rectangle layout_party_frame_rect(int party_count, int index)
{
    int fw = layout_party_frame_width(party_count);
    int gap = layout_party_frame_gap(party_count);
    int sx = layout_party_start_x(party_count);
    return (Rectangle){
        (float)(sx + index * (fw + gap)),
        (float)FRAME_Y,
        (float)fw,
        (float)FRAME_H
    };
}

Vector2 layout_enemy_position(int enemy_count, int index)
{
    int left = 240;
    int right = VIRT_W - 240;
    int span = right - left;
    int x = enemy_count <= 1 ? VIRT_W / 2 : left + (span * index) / (enemy_count - 1);

    return (Vector2){ (float)x, (float)ENEMY_Y };
}

Rectangle layout_enemy_hit_rect(Vector2 pos)
{
    return (Rectangle){
        pos.x - ENEMY_SIZE / 2.0f - 8.0f,
        pos.y - ENEMY_SIZE / 2.0f - 12.0f,
        (float)ENEMY_SIZE + 16.0f,
        (float)ENEMY_SIZE + 22.0f
    };
}

Rectangle layout_enemy_cast_bar_rect(Vector2 pos, int enemy_count, int enemy_index)
{
    float y = pos.y + ENEMY_SIZE / 2.0f + 29.0f;
    if (enemy_count == 3 && enemy_index == 1)
        y += CAST_BAR_HEIGHT;
    return (Rectangle){
        pos.x - CAST_BAR_W / 2.0f,
        y,
        (float)CAST_BAR_W,
        (float)CAST_BAR_HEIGHT
    };
}

HandLayout layout_hand(int hand_count)
{
    const int lane_x = 100;
    const int lane_w = 440;

    HandLayout layout = {
        hand_count,
        HAND_CARD_W,
        HAND_CARD_H,
        HAND_CARD_W + HAND_GAP,
        lane_x,
        HAND_Y,
        false
    };

    if (hand_count <= 0)
        return layout;

    int full_w = hand_count * HAND_CARD_W + (hand_count - 1) * HAND_GAP;
    if (full_w <= lane_w)
    {
        layout.start_x = lane_x + (lane_w - full_w) / 2;
        layout.step = HAND_CARD_W + HAND_GAP;
        layout.overlapping = false;
    }
    else if (hand_count == 1)
    {
        layout.start_x = lane_x + (lane_w - HAND_CARD_W) / 2;
        layout.step = 0;
        layout.overlapping = false;
    }
    else
    {
        layout.start_x = lane_x;
        layout.step = (lane_w - HAND_CARD_W) / (hand_count - 1);
        if (layout.step < 34) layout.step = 34;
        layout.overlapping = true;
    }

    return layout;
}

Rectangle layout_hand_card_rect(HandLayout layout, int index)
{
    return (Rectangle){
        (float)(layout.start_x + index * layout.step),
        (float)layout.y,
        (float)layout.card_w,
        (float)layout.card_h
    };
}

Rectangle layout_action_feed_panel(void)
{
    return (Rectangle){ 12.0f, 64.0f, 160.0f, 92.0f };
}

Rectangle layout_card_inspector_panel(void)
{
    return (Rectangle){ 456.0f, 64.0f, 172.0f, 112.0f };
}

Rectangle layout_energy_panel(void)
{
    return (Rectangle){ 12.0f, 274.0f, 76.0f, 48.0f };
}

Rectangle layout_end_turn_button(void)
{
    return (Rectangle){ 552.0f, 297.0f, 76.0f, (float)BTN_H };
}

Rectangle layout_discard_pile_rect(void)
{
    return (Rectangle){ 552.0f, 238.0f, 76.0f, 48.0f };
}

Rectangle layout_deck_browser_viewport(void)
{
    return (Rectangle){ 20.0f, 54.0f, 408.0f, 262.0f };
}

Rectangle layout_deck_inspector_panel(void)
{
    return (Rectangle){ 448.0f, 54.0f, 172.0f, 126.0f };
}

Rectangle layout_reward_card_rect(int reward_count, int index)
{
    int total_w = reward_count * REWARD_CARD_W + (reward_count - 1) * REWARD_GAP;
    int start_x = (VIRT_W - total_w) / 2;
    return (Rectangle){
        (float)(start_x + index * (REWARD_CARD_W + REWARD_GAP)),
        (float)REWARD_Y,
        (float)REWARD_CARD_W,
        (float)REWARD_CARD_H
    };
}

Rectangle layout_reward_inspector_panel(void)
{
    return (Rectangle){ 120.0f, 214.0f, 400.0f, 104.0f };
}
