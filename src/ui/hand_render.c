#include "hand_render.h"
#include "util/text.h"
#include "util/log.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

static int visual_uid[MAX_HAND_SIZE];
static float visual_entry[MAX_HAND_SIZE];
static float visual_hover[MAX_HAND_SIZE];
static bool visual_initialized = false;

static Color card_type_color(CardType type)
{
    switch (type)
    {
        case CARD_ATTACK: return (Color){ 180, 60, 60, 255 };
        case CARD_SKILL:  return (Color){ 60, 120, 180, 255 };
        case CARD_POWER:  return (Color){ 140, 80, 180, 255 };
    }
    return (Color){ 80, 80, 100, 255 };
}

static Color class_accent(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return (Color){ 74, 144, 217, 255 };
        case CLASS_CLERIC:   return (Color){ 225, 170, 50, 255 };
        case CLASS_MAGE:     return (Color){ 155, 89, 182, 255 };
        case CLASS_ROGUE:    return (Color){ 39, 174, 96, 255 };
        case CLASS_SHAMAN:   return (Color){ 230, 126, 34, 255 };
        case CLASS_RANGER:   return (Color){ 70, 190, 120, 255 };
        default:             return (Color){ 100, 100, 120, 255 };
    }
}

void hand_render_draw(Deck *deck, Energy *energy, int hovered_card, ClassType channel_class, int target_idx, float target_offset)
{
    LOG_T("HRD: start");

    if (deck->hand_count == 0) { LOG_T("HRD: empty hand"); return; }

    if (!visual_initialized)
    {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
        {
            visual_uid[i] = -1;
            visual_entry[i] = 1.0f;
            visual_hover[i] = 0.0f;
        }
        visual_initialized = true;
    }

    float dt = GetFrameTime();

    int card_w = HAND_CARD_W, card_h = HAND_CARD_H;
    int gap = HAND_GAP;
    int total_w = deck->hand_count * (card_w + gap) - gap;
    int start_x = (VIRT_W - total_w) / 2;
    int card_y = HAND_Y;

    for (int i = 0; i < deck->hand_count; i++)
    {
        LOG_T("HRD: card %d", i);
        CardInstance *inst = &deck->cards[deck->hand[i]];
        const CardDef *card = inst->def;
        if (!card || !card->name) continue;

        int uid = inst->uid;
        if (visual_uid[i] != uid)
        {
            visual_uid[i] = uid;
            visual_entry[i] = -0.18f - i * 0.055f;
            visual_hover[i] = 0.0f;
        }

        visual_entry[i] += dt * 4.8f;
        if (visual_entry[i] > 1.0f) visual_entry[i] = 1.0f;

        float hover_target = (i == hovered_card) ? 1.0f : 0.0f;
        float hover_speed = dt * 12.0f;
        if (hover_speed > 1.0f) hover_speed = 1.0f;
        visual_hover[i] += (hover_target - visual_hover[i]) * hover_speed;

        LOG_T("HRD: card=%s ch=%d turns=%d tgt=%d", card->name, card->channel, card->channel_turns, card->target);
        bool ug = inst->upgraded;
        int dmg = card_damage(card, ug);
        int hl  = card_heal(card, ug);
        int sh  = card_shield(card, ug);
        int x = start_x + i * (card_w + gap);

        float entry = visual_entry[i];
        if (entry < 0.0f) entry = 0.0f;
        float hover_offset = -22.0f * visual_hover[i];
        float deal_offset = (1.0f - entry) * 90.0f;
        float select_offset = (i == target_idx) ? target_offset : 0.0f;
        float draw_y = card_y + hover_offset + select_offset + deal_offset;

        Color type_col = theme_card_type_color(card->type);
        Color accent = theme_class_color(card->class);

        bool low_energy = !energy_has(energy, card->cost);
        float scale = 1.0f + 0.08f * visual_hover[i];
        int cw = (int)(card_w * scale);
        int ch = (int)(card_h * scale);
        int cx = x - (cw - card_w) / 2;
        int cy = (int)(draw_y - (ch - card_h) / 2);

        bool locked = (channel_class != CLASS_NONE && card->class == channel_class);
        Rectangle card_rect = { (float)cx, (float)cy, (float)cw, (float)ch };
        theme_draw_card_art(card_rect, card, ug);

        LOG_T("HRD: border");
        DrawRectangleLinesEx(card_rect, (i == hovered_card) ? 2.5f : 1.0f, low_energy ? (Color){ 95, 90, 105, 210 } : ((i == hovered_card) ? RAYWHITE : accent));
        LOG_T("HRD: border done");

        if (locked)
        {
            DrawRectangleRec(card_rect, (Color){ 40, 20, 22, 165 });
            DrawText("CHANNELING", cx + cw / 2 - MeasureText("CHANNELING", 6) / 2, cy + ch / 2 - 3, 6, (Color){ 240, 120, 120, 230 });
            continue;
        }

        if (low_energy)
        {
            DrawRectangleRec(card_rect, (Color){ 8, 8, 12, 120 });
            DrawRectangle(cx, cy + ch - 7, cw, 7, (Color){ 190, 60, 65, 210 });
            if (i == hovered_card)
                DrawText("Not enough energy", cx + 4, cy + ch - 15, 5, (Color){ 240, 110, 115, 240 });
        }
    }
    LOG_T("HRD: end");
}





