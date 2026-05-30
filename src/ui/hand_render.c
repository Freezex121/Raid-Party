#include "hand_render.h"
#include "data/synergy_defs.h"
#include "ui/theme.h"
#include "util/text.h"
#include "util/log.h"
#include "ui/theme.h"
#include "ui/layout.h"
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

static bool card_is_heal_card(const CardDef *card)
{
    if (!card) return false;
    return card->heal > 0 || card->heal2 > 0 ||
        card_has_effect(card, CARD_EFFECT_REVIVE_TARGET) ||
        card_has_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ALLY) ||
        card_has_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES);
}

static int effective_cost(const CardDef *card, int combo_prime_index)
{
    if (!card) return 0;
    if (combo_prime_index >= 0 && combo_prime_index < synergy_combo_count())
    {
        const SynergyComboDef *c = synergy_combo_by_index(combo_prime_index);
        if (c && c->free_cost)
        {
            if (strcmp(c->consume_card_type, "heal") == 0 && card_is_heal_card(card))
                return 0;
            if (strcmp(c->consume_card_type, "mage_fire_spell") == 0 && card->class == CLASS_MAGE && card->damage > 0)
                return 0;
        }
    }
    return card->cost;
}

static void draw_hand_card(Rectangle r, const CardDef *card, int upgrade_level, unsigned int seed, bool show_highlight, bool low_energy, bool locked)
{
    if (!card) return;

    theme_draw_card_art_seeded(r, card, upgrade_level, seed);

    if (locked)
    {
        DrawRectangleRec(r, (Color){ 180, 50, 50, 80 });
        draw_text_box((Rectangle){ r.x + 4, r.y + r.height / 2 - 8, r.width - 8, 16 },
            "CHANNELING", 10, 0, (Color){ 255, 200, 100, 230 }, TEXT_ALIGN_CENTER);
        return;
    }

    if (low_energy)
    {
        DrawRectangleRec(r, (Color){ 40, 40, 60, 140 });
    }

    if (show_highlight)
        DrawRectangleLinesEx(r, 2.0f, (Color){ 255, 255, 200, 220 });
}

void hand_render_draw(Deck *deck, Energy *energy, int hovered_card, ClassType channel_class, int target_idx, float target_offset, int combo_prime_index)
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

    HandLayout hand_layout = layout_hand(deck->hand_count);
    Rectangle draw_rects[MAX_HAND_SIZE] = { 0 };
    bool draw_valid[MAX_HAND_SIZE] = { 0 };
    bool draw_low_energy[MAX_HAND_SIZE] = { 0 };
    bool draw_locked[MAX_HAND_SIZE] = { 0 };
    int draw_upgrade_level[MAX_HAND_SIZE] = { 0 };
    unsigned int draw_seed[MAX_HAND_SIZE] = { 0 };
    const CardDef *draw_cards[MAX_HAND_SIZE] = { 0 };

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
        int upgrade_level = inst->upgrade_level;
        Rectangle base_rect = layout_hand_card_rect(hand_layout, i);
        int x = (int)base_rect.x;

        float entry = visual_entry[i];
        if (entry < 0.0f) entry = 0.0f;
        float hover_offset = -28.0f * visual_hover[i];
        float deal_offset = (1.0f - entry) * 90.0f;
        float select_offset = (i == target_idx) ? target_offset : 0.0f;
        float draw_y = base_rect.y + hover_offset + select_offset + deal_offset;

        bool low_energy = energy->current < effective_cost(card, combo_prime_index);
        int cw = hand_layout.card_w;
        int ch = hand_layout.card_h;
        int cx = x;
        int cy = (int)draw_y;

        bool locked = (channel_class != CLASS_NONE && card->class == channel_class);
        draw_rects[i] = (Rectangle){ (float)cx, (float)cy, (float)cw, (float)ch };
        draw_valid[i] = true;
        draw_low_energy[i] = low_energy;
        draw_locked[i] = locked;
        draw_upgrade_level[i] = upgrade_level;
        draw_seed[i] = (unsigned int)uid;
        draw_cards[i] = card;
    }

    for (int i = 0; i < deck->hand_count; i++)
    {
        if (!draw_valid[i] || i == hovered_card || i == target_idx) continue;
        draw_hand_card(draw_rects[i], draw_cards[i], draw_upgrade_level[i], draw_seed[i], false, draw_low_energy[i], draw_locked[i]);
    }

    if (target_idx >= 0 && target_idx < deck->hand_count && target_idx != hovered_card && draw_valid[target_idx])
    {
        draw_hand_card(draw_rects[target_idx], draw_cards[target_idx], draw_upgrade_level[target_idx], draw_seed[target_idx], true, draw_low_energy[target_idx], draw_locked[target_idx]);
    }

    if (hovered_card >= 0 && hovered_card < deck->hand_count && draw_valid[hovered_card])
    {
        draw_hand_card(draw_rects[hovered_card], draw_cards[hovered_card], draw_upgrade_level[hovered_card], draw_seed[hovered_card], false, draw_low_energy[hovered_card], draw_locked[hovered_card]);
    }
    LOG_T("HRD: end");
}

