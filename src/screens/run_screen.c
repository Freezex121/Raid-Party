#include "screens.h"
#include "game.h"
#include "combat/combat.h"
#include "ui/party_frames.h"
#include "ui/hand_render.h"
#include "ui/cast_bar.h"
#include "ui/enemy_render.h"
#include "ui/floating_text.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "ui/relic_tray.h"
#include "data/area_defs.h"
#include "data/enemy_defs.h"
#include "data/encounter_defs.h"
#include "systems/relic.h"
#include "util/log.h"
#include "util/math_utils.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool combat_initialized = false;

static float preview_combo_mult(CombatState *cs, const CardDef *card)
{
    if (!card) return 1.0f;
    if (cs->combo_prime == COMBO_PRIME_STORM_VOLLEY && card->target == TARGET_ALL_ENEMIES)
        return 1.5f;
    if (cs->combo_prime == COMBO_PRIME_BACKDRAFT && card->damage > 0)
        return 1.25f;
    if (cs->combo_prime == COMBO_PRIME_SACRED_CHORUS && card->target == TARGET_ALL_ALLIES)
        return 1.5f;
    return 1.0f;
}

static int preview_hits(const CardDef *card)
{
    return card_repeat_hits(card);
}

static void draw_preview_box(int x, int y, const char *title, const char *line1, const char *line2, Color accent)
{
    int w = 132;
    int h = line2 && line2[0] ? 42 : 32;
    if (x < 2) x = 2;
    if (x + w > VIRT_W - 2) x = VIRT_W - w - 2;
    if (y + h > HAND_Y - 3) y = HAND_Y - h - 3;
    DrawRectangleRec((Rectangle){ (float)x, (float)y, (float)w, (float)h }, (Color){ 12, 12, 22, 235 });
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 1.0f, accent);
    draw_text_box((Rectangle){ (float)(x + 5), (float)(y + 5), (float)(w - 10), 12.0f },
        title, 10, 0, accent, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ (float)(x + 5), (float)(y + 17), (float)(w - 10), 12.0f },
        line1, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
    if (line2 && line2[0])
        draw_text_box((Rectangle){ (float)(x + 5), (float)(y + 31), (float)(w - 10), 12.0f },
            line2, 10, 0, (Color){ 180, 180, 205, 230 }, TEXT_ALIGN_LEFT);
}

static void draw_target_preview(CombatState *cs)
{
    if (cs->target_mode == TGT_NONE) return;
    if (cs->target_hand_idx < 0 || cs->target_hand_idx >= cs->deck.hand_count) return;

    CardInstance *inst = &cs->deck.cards[cs->deck.hand[cs->target_hand_idx]];
    const CardDef *card = inst->def;
    if (!card) return;

    float mult = preview_combo_mult(cs, card);
    int dmg = (int)(card_damage(card, inst->upgraded) * mult);
    int heal = (int)(card_heal(card, inst->upgraded) * mult);
    int shield = (int)(card_shield(card, inst->upgraded) * mult);
    int hits = preview_hits(card);

    char line1[96] = "";
    char line2[96] = "";

    if (cs->target_mode == TGT_SELECT_ENEMY && cs->hovered_enemy >= 0)
    {
        EnemyState *e = &cs->enemies[cs->hovered_enemy];
        int total_damage = dmg * hits;
        if (total_damage > 0)
            snprintf(line1, sizeof(line1), "-%d HP%s", total_damage, hits > 1 ? TextFormat(" (%dx)", hits) : "");
        else
            snprintf(line1, sizeof(line1), "Apply effect");

        if (card->interrupt)
            snprintf(line2, sizeof(line2), "Interrupts current cast");
        else if (card->burn_stacks > 0)
            snprintf(line2, sizeof(line2), "+%d Burning", card->burn_stacks);
        else if (card->aggro_self > 0)
            snprintf(line2, sizeof(line2), "+%d aggro to caster", card->aggro_self);

        draw_preview_box(e->pos_x - 46, e->pos_y + 28, "Preview", line1, line2, (Color){ 255, 230, 110, 245 });
    }
    else if (cs->target_mode == TGT_SELECT_ALLY && cs->hovered_ally >= 0)
    {
        PartyMember *pm = &cs->party.members[cs->hovered_ally];
        bool revive = card_has_effect(card, CARD_EFFECT_REVIVE_TARGET);
        if (!pm->alive && !revive)
            snprintf(line1, sizeof(line1), "Invalid target");
        else if (revive && pm->alive)
            snprintf(line1, sizeof(line1), "Target is already alive");
        else if (revive && !pm->alive)
            snprintf(line1, sizeof(line1), "Revive at %d HP", pm->max_hp / 2);
        else if (heal > 0 && shield > 0)
            snprintf(line1, sizeof(line1), "+%d HP, +%d Shield", heal, shield);
        else if (heal > 0)
            snprintf(line1, sizeof(line1), "+%d HP", heal);
        else if (shield > 0)
            snprintf(line1, sizeof(line1), "+%d Shield", shield);
        else
            snprintf(line1, sizeof(line1), "Apply effect");

        if (card_has_effect(card, CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN))
            snprintf(line2, sizeof(line2), "Moves aggro to Guardian");
        else if (card_has_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ALLY))
            snprintf(line2, sizeof(line2), "Renew: +5 HP for 3 turns");

        Rectangle ally_rect = layout_party_frame_rect(cs->party.count, cs->hovered_ally);
        draw_preview_box(snap_i(ally_rect.x), snap_i(ally_rect.y + ally_rect.height + 6), "Preview", line1, line2, (Color){ 120, 245, 165, 245 });
    }
}

static void draw_panel(Rectangle panel, const char *title, Color accent)
{
    DrawRectangleRec(panel, (Color){ 9, 10, 17, 210 });
    DrawRectangleLinesEx(panel, 1.0f, (Color){ accent.r, accent.g, accent.b, 165 });
    if (title && title[0])
        draw_text_box((Rectangle){ panel.x + 6.0f, panel.y + 5.0f, panel.width - 12.0f, 12.0f },
            title, 10, 0, accent, TEXT_ALIGN_LEFT);
}

static bool enemy_has_status_for_ui(EnemyState *enemy, StatusType status)
{
    if (!enemy || !enemy->def || enemy->hp <= 0) return false;
    return status_find(enemy->statuses, enemy->status_count, status) >= 0;
}

static bool any_enemy_has_status(CombatState *cs, StatusType status)
{
    for (int i = 0; i < cs->enemy_count; i++)
        if (enemy_has_status_for_ui(&cs->enemies[i], status))
            return true;
    return false;
}

static Color synergy_status_color(StatusType status)
{
    switch (status)
    {
        case STATUS_MARKED:     return (Color){ 245, 220, 75, 160 };
        case STATUS_CONDUCTIVE: return (Color){ 95, 185, 255, 160 };
        case STATUS_BLIGHT:     return (Color){ 190, 95, 230, 160 };
        default:                return (Color){ 220, 220, 235, 120 };
    }
}

static StatusType card_ready_status(CombatState *cs, const CardDef *card)
{
    if (!cs || !card) return STATUS_NONE;
    bool marked = any_enemy_has_status(cs, STATUS_MARKED);
    bool conductive = any_enemy_has_status(cs, STATUS_CONDUCTIVE);
    bool blight = any_enemy_has_status(cs, STATUS_BLIGHT);

    if (marked && (
        strcmp(card->id, "rog_backstab") == 0 ||
        strcmp(card->id, "rog_evis") == 0 ||
        strcmp(card->id, "mag_missiles") == 0 ||
        strcmp(card->id, "clr_smite") == 0 ||
        strcmp(card->id, "rng_pounce") == 0 ||
        strcmp(card->id, "pal_holy_strike") == 0 ||
        strcmp(card->id, "wlk_drain_life") == 0))
        return STATUS_MARKED;

    if (conductive && (
        strcmp(card->id, "mag_fireball") == 0 ||
        strcmp(card->id, "mag_meteor") == 0 ||
        strcmp(card->id, "rog_shadow") == 0 ||
        strcmp(card->id, "wlk_shadow_bolt") == 0))
        return STATUS_CONDUCTIVE;

    if (blight && (
        strcmp(card->id, "grd_shield_slam") == 0 ||
        strcmp(card->id, "clr_holy_fire") == 0 ||
        strcmp(card->id, "pal_judgment") == 0 ||
        strcmp(card->id, "wlk_dark_harvest") == 0 ||
        strcmp(card->id, "wlk_hellfire") == 0 ||
        strcmp(card->id, "pal_aegis_aura") == 0))
        return STATUS_BLIGHT;

    if ((marked || conductive || blight) && (
        strcmp(card->id, "brd_battle_hymn") == 0 ||
        strcmp(card->id, "brd_finale") == 0))
        return marked ? STATUS_MARKED : (conductive ? STATUS_CONDUCTIVE : STATUS_BLIGHT);

    return STATUS_NONE;
}

static void draw_hand_synergy_glows(CombatState *cs)
{
    HandLayout hand_layout = layout_hand(cs->deck.hand_count);
    for (int i = 0; i < cs->deck.hand_count; i++)
    {
        CardInstance *inst = &cs->deck.cards[cs->deck.hand[i]];
        if (!inst->def) continue;
        StatusType status = card_ready_status(cs, inst->def);
        if (status == STATUS_NONE) continue;
        Rectangle r = layout_hand_card_rect(hand_layout, i);
        if (i == cs->target_hand_idx)
            r.y += cs->target_offset;
        else if (i == cs->hovered_card)
            r.y -= 28.0f;
        r.x -= 2.0f;
        r.y -= 2.0f;
        r.width += 4.0f;
        r.height += 4.0f;
        Color c = synergy_status_color(status);
        float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 7.0f);
        c.a = (unsigned char)(85 + 75 * pulse);
        DrawRectangleRec(r, (Color){ c.r, c.g, c.b, 35 });
        DrawRectangleLinesEx(r, 2.0f, c);
    }
}

static void draw_pair_passives(CombatState *cs)
{
    const struct { ClassType a, b; const char *label; const char *desc; Color color; } passives[] = {
        { CLASS_GUARDIAN, CLASS_MAGE, "MOLTEN", "Guardian + Mage: when Guardian gains Shield, a random enemy takes 2 damage.", { 245, 105, 70, 210 } },
        { CLASS_ROGUE, CLASS_RANGER, "AMBUSH", "Rogue + Ranger: the first damaging card each combat deals double damage.", { 245, 220, 75, 210 } },
        { CLASS_CLERIC, CLASS_ROGUE, "MEND", "Cleric + Rogue: Rogue aggro drops heal the Rogue for 8.", { 120, 245, 170, 210 } },
        { CLASS_GUARDIAN, CLASS_SHAMAN, "BULWARK", "Guardian + Shaman: Healing Totem lasts 2 extra turns.", { 95, 185, 255, 210 } },
        { CLASS_PALADIN, CLASS_WARLOCK, "ABSOLVE", "Paladin + Warlock: consuming BLIGHT heals the lowest HP ally.", { 230, 205, 95, 210 } },
    };
    Rectangle hovered_rect = { 0 };
    const char *hovered_label = NULL;
    const char *hovered_desc = NULL;
    Color hovered_color = RAYWHITE;
    Vector2 mouse = GetMousePosition();
    int y = FRAME_Y + FRAME_H + 4;
    int x = VIRT_W / 2 - 134;
    int passive_count = (int)(sizeof(passives) / sizeof(passives[0]));
    for (int i = 0; i < passive_count; i++)
    {
        bool active = false;
        for (int p = 0; p < cs->party.count; p++)
            if (cs->party.members[p].alive && cs->party.members[p].class == passives[i].a)
                active = true;
        if (!active) continue;
        active = false;
        for (int p = 0; p < cs->party.count; p++)
            if (cs->party.members[p].alive && cs->party.members[p].class == passives[i].b)
                active = true;
        if (!active) continue;
        int w = MeasureText(passives[i].label, 10) + 10;
        Rectangle pill = { (float)x, (float)y, (float)w, 12.0f };
        DrawRectangleRec(pill, (Color){ passives[i].color.r, passives[i].color.g, passives[i].color.b, 45 });
        DrawRectangleLinesEx(pill, 1.0f, passives[i].color);
        draw_text_box((Rectangle){ pill.x + 5.0f, pill.y + 1.0f, pill.width - 10.0f, pill.height - 2.0f },
            passives[i].label, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        if (CheckCollisionPointRec(mouse, pill))
        {
            hovered_rect = pill;
            hovered_label = passives[i].label;
            hovered_desc = passives[i].desc;
            hovered_color = passives[i].color;
        }
        x += w + 5;
    }

    if (hovered_label && hovered_desc)
    {
        int tip_w = 236;
        int body_h = measure_text_box(hovered_desc, tip_w - 12, 10, 0);
        if (body_h < ui_line_height(10)) body_h = ui_line_height(10);
        int tip_h = 24 + body_h;
        int tip_x = (int)(hovered_rect.x + hovered_rect.width * 0.5f - tip_w * 0.5f);
        int tip_y = (int)(hovered_rect.y + hovered_rect.height + 5.0f);
        if (tip_x < 4) tip_x = 4;
        if (tip_x + tip_w > VIRT_W - 4) tip_x = VIRT_W - tip_w - 4;
        Rectangle tip = { (float)tip_x, (float)tip_y, (float)tip_w, (float)tip_h };
        DrawRectangleRec(tip, (Color){ 10, 11, 18, 245 });
        DrawRectangleLinesEx(tip, 1.0f, (Color){ hovered_color.r, hovered_color.g, hovered_color.b, 230 });
        draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, 12.0f },
            hovered_label, 10, 0, hovered_color, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 18.0f, tip.width - 12.0f, tip.height - 22.0f },
            hovered_desc, 10, 0, (Color){ 210, 214, 235, 235 }, TEXT_ALIGN_LEFT);
    }
}

static void draw_discard_pile(CombatState *cs)
{
    Rectangle pile = layout_discard_pile_rect();
    DrawRectangleRec(pile, (Color){ 12, 13, 20, 215 });
    DrawRectangleLinesEx(pile, 1.0f, (Color){ 135, 125, 165, 175 });

    Rectangle c1 = { pile.x + 28.0f, pile.y + 12.0f, 22.0f, 28.0f };
    Rectangle c2 = { pile.x + 23.0f, pile.y + 9.0f, 22.0f, 28.0f };
    DrawRectangleRec(c1, (Color){ 34, 34, 50, 255 });
    DrawRectangleLinesEx(c1, 1.0f, (Color){ 75, 76, 100, 230 });
    DrawRectangleRec(c2, (Color){ 42, 42, 62, 255 });
    DrawRectangleLinesEx(c2, 1.0f, (Color){ 110, 105, 145, 230 });

    draw_text_box((Rectangle){ pile.x + 6.0f, pile.y + 5.0f, pile.width - 12.0f, 12.0f },
        "DISCARD", 10, 0, (Color){ 170, 165, 205, 220 }, TEXT_ALIGN_LEFT);
    char count[8];
    snprintf(count, sizeof(count), "%d", cs ? cs->deck.discard_count : 0);
    draw_text_box((Rectangle){ pile.x + 6.0f, pile.y + 35.0f, pile.width - 13.0f, 12.0f },
        count, 10, 0, (Color){ 205, 200, 230, 235 }, TEXT_ALIGN_RIGHT);
}

static void draw_combat_feedback(CombatState *cs)
{
    bool danger = false;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (!e->def || e->hp <= 0 || e->intent.ability_idx < 0) continue;
        const EnemyAbility *ab = &e->def->abilities[e->intent.ability_idx];
        if (ab->intent == INTENT_WIPE || ab->intent == INTENT_TANK_BUSTER || ab->is_wipe)
            if (e->intent.remaining_turns <= 1)
                danger = true;
    }

    if (danger)
    {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 8.0f);
        DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){ 200, 35, 45, (unsigned char)(18 + 24 * pulse) });
    }

    if (cs->end_turn_flash > 0.0f)
    {
        float a = cs->end_turn_flash / 0.35f;
        DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){ 255, 220, 120, (unsigned char)(35 * a) });
    }

    if (cs->synergy_flash_timer > 0.0f)
    {
        float a = cs->synergy_flash_timer / 0.32f;
        DrawRectangleLinesEx((Rectangle){ 3.0f, 3.0f, (float)VIRT_W - 6.0f, (float)VIRT_H - 6.0f },
            2.0f,
            (Color){ 245, 220, 90, (unsigned char)(190 * a) });
    }

    if (cs->enemy_banner_timer > 0.0f)
    {
        float a = cs->enemy_banner_timer / 0.55f;
        DrawRectangleRec((Rectangle){ (float)(VIRT_W / 2 - 62), 106.0f, 124.0f, 20.0f }, (Color){ 100, 35, 40, (unsigned char)(190 * a) });
        draw_text_box((Rectangle){ (float)(VIRT_W / 2 - 58), 111.0f, 116.0f, 13.0f },
            "ENEMY ACTIONS", 10, 0, (Color){ 255, 180, 170, (unsigned char)(255 * a) }, TEXT_ALIGN_CENTER);
    }

    if (cs->turn_banner_timer > 0.0f && cs->turn_banner_text[0])
    {
        float a = cs->turn_banner_timer / 0.9f;
        if (a > 1.0f) a = 1.0f;
        DrawRectangleRec((Rectangle){ (float)(VIRT_W / 2 - 54), 58.0f, 108.0f, 17.0f }, (Color){ 35, 70, 105, (unsigned char)(175 * a) });
        draw_text_box((Rectangle){ (float)(VIRT_W / 2 - 50), 62.0f, 100.0f, 13.0f },
            cs->turn_banner_text, 10, 0, (Color){ 210, 235, 255, (unsigned char)(255 * a) }, TEXT_ALIGN_CENTER);
    }

    if (cs->play_flash_timer > 0.0f && cs->play_flash_text[0])
    {
        float t = 1.0f - cs->play_flash_timer / 0.55f;
        float y = cs->play_flash_y - t * 10.0f;
        unsigned char a = (unsigned char)((1.0f - t) * 230);
        DrawRectangleRec((Rectangle){ cs->play_flash_x, y, 84, 13 }, (Color){ 30, 30, 44, a });
        DrawRectangleLinesEx((Rectangle){ cs->play_flash_x, y, 84, 13 }, 1.0f, (Color){ 230, 215, 120, a });
        draw_text_box((Rectangle){ cs->play_flash_x + 4.0f, y + 3.0f, 76.0f, 12.0f },
            cs->play_flash_text, 10, 0, (Color){ 245, 235, 190, a }, TEXT_ALIGN_LEFT);
    }

    if (cs->synergy_banner_timer > 0.0f && cs->synergy_banner_title[0])
    {
        float a = cs->synergy_banner_timer / 1.35f;
        if (a > 1.0f) a = 1.0f;
        float scale = cs->combo_scale <= 0.01f ? 1.0f : cs->combo_scale;
        Rectangle r = { (float)(VIRT_W / 2 - 118), 84.0f, 236.0f, 38.0f };
        DrawRectangleRec(r, (Color){ 18, 15, 26, (unsigned char)(210 * a) });
        DrawRectangleLinesEx(r, 1.0f, (Color){ 245, 220, 90, (unsigned char)(220 * a) });
        int title_size = 18;
        int tw = MeasureText(cs->synergy_banner_title, title_size);
        (void)tw;
        (void)scale;
        draw_text_box((Rectangle){ r.x + 5.0f, 90.0f, r.width - 10.0f, 20.0f },
            cs->synergy_banner_title, title_size, 0, (Color){ 255, 235, 120, (unsigned char)(255 * a) }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ r.x + 5.0f, 110.0f, r.width - 10.0f, 12.0f },
            cs->synergy_banner_subtitle, 10, 0, (Color){ 210, 220, 245, (unsigned char)(235 * a) }, TEXT_ALIGN_CENTER);
    }

    Rectangle feed = layout_action_feed_panel();
    draw_panel(feed, "ACTIONS", (Color){ 130, 145, 185, 220 });
    int feed_x = (int)feed.x + 7;
    int feed_y = (int)feed.y + 18;
    for (int i = 0; i < 5; i++)
    {
        if (cs->action_feed_timer[i] <= 0.0f || !cs->action_feed[i][0]) continue;
        float a = cs->action_feed_timer[i] > 0.5f ? 1.0f : cs->action_feed_timer[i] / 0.5f;
        draw_text_box((Rectangle){ (float)feed_x, (float)(feed_y + i * 14), feed.width - 14.0f, 13.0f },
            cs->action_feed[i], 10, 0, (Color){ 175, 180, 205, (unsigned char)(210 * a) }, TEXT_ALIGN_LEFT);
    }
}

void run_screen_update(void)
{
    if (!combat_initialized)
    {
        combat_start(&g_state.combat, &g_state.run_party, g_state.encounter);
        combat_initialized = true;
    }

    combat_update(&g_state.combat);

    if (IsKeyPressed(KEY_D))
    {
        game_change_screen(SCREEN_DECK);
        return;
    }

    Rectangle deck_btn = { 552.0f, 220.0f, 76.0f, 16.0f };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), deck_btn))
    {
        game_change_screen(SCREEN_DECK);
        return;
    }

    if (g_state.combat.phase == COMBAT_TURN_START)
    {
        combat_initialized = false;

        bool was_victory = (strstr(g_state.combat.result_message, "VICTORY") != 0);
        memcpy(&g_state.run_party, &g_state.combat.party, sizeof(Party));
        g_state.run_party_active = true;

        if (!was_victory)
        {
            g_state.run_won = false;
            g_state.result_area = g_state.current_area;
            g_state.result_floor = g_state.map.floor + 1;
            const AreaDef *area = area_def(g_state.current_area);
            snprintf(g_state.result_reason, sizeof(g_state.result_reason), "The party wiped in %s, Floor %d.", area ? area->name : "the area", g_state.result_floor);
            game_change_screen(SCREEN_GAME_OVER);
            return;
        }

        int gold_gain = g_state.encounter_is_boss ? 50 : (g_state.encounter_is_elite ? 25 : 10);
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_GILDED_CHARM))
            gold_gain += 8;
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_RABBIT_FOOT) && (rand() % 10) == 0)
            gold_gain *= 2;
        g_state.gold += gold_gain;
        ft_spawn_gold(gold_gain);

        // Bandage Roll: heal lowest HP ally for 8
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_BANDAGE_ROLL))
        {
            int lowest = party_lowest_hp(&g_state.run_party);
            if (lowest >= 0)
            {
                g_state.run_party.members[lowest].hp += 8;
                if (g_state.run_party.members[lowest].hp > g_state.run_party.members[lowest].max_hp)
                    g_state.run_party.members[lowest].hp = g_state.run_party.members[lowest].max_hp;
            }
        }

        // Reroll tokens from elites and bosses
        if (g_state.encounter_is_boss)
            g_state.reroll_tokens++;
        else if (g_state.encounter_is_elite && (rand() % 2) == 0)
            g_state.reroll_tokens++;

        g_state.relic_reward_pending = g_state.encounter_is_boss;
        g_state.relic_reward_count = 0;

        int ci = g_state.map.current_index;
        if (ci >= 0)
        {
            g_state.map.nodes[ci].completed = true;
            g_state.map.current_index = -1;
            map_unlock_next(&g_state.map);
        }

        if (g_state.encounter_is_boss && ci >= 0 && g_state.map.nodes[ci].type == NODE_BOSS)
        {
            g_state.result_bosses_defeated++;
            g_state.map.floor++;
            g_state.frugal_used_this_floor = false;
            int area_floors = area_floor_count(g_state.current_area);
            const AreaDef *current_area = area_def(g_state.current_area);
            int loaded_floors = map_loaded_floor_count_for_area(current_area ? current_area->id : NULL);
            if (loaded_floors > 0 && area_floors > loaded_floors)
                area_floors = loaded_floors;
            if (g_state.map.floor >= area_floors)
            {
                g_state.run_won = true;
                g_state.result_area = g_state.current_area;
                g_state.result_floor = area_floors;
                g_state.relic_reward_pending = false;
                const AreaDef *area = area_def(g_state.current_area);
                snprintf(g_state.result_reason, sizeof(g_state.result_reason), "%s cleared.", area ? area->name : "Area");
                game_change_screen(SCREEN_GAME_OVER);
                return;
            }
        }

        game_change_screen(SCREEN_REWARD);
    }
}

void run_screen_draw(void)
{
    CombatState *cs = &g_state.combat;

    theme_draw_background();

    if (cs->phase == COMBAT_VICTORY || cs->phase == COMBAT_DEFEAT)
    {
        Color bg = { 0, 0, 0, 180 };
        DrawRectangle(0, 0, VIRT_W, VIRT_H, bg);
        Color msg_col = cs->phase == COMBAT_VICTORY ? (Color){ 70, 220, 120, 255 } : (Color){ 220, 60, 60, 255 };
        DrawText(cs->result_message, VIRT_W / 2 - MeasureText(cs->result_message, 18) / 2, 170, 18, msg_col);
        return;
    }

    party_frames_draw(&cs->party);

    bool targeting = cs->target_mode != TGT_NONE;

    if (targeting && cs->target_mode == TGT_SELECT_ENEMY)
    {
        draw_text_box((Rectangle){ 12.0f, 154.0f, 160.0f, 12.0f }, "Select a target", 10, 0, (Color){ 255, 255, 100, 220 }, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ 12.0f, 166.0f, 160.0f, 12.0f }, "Right-click to cancel", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_LEFT);
    }
    else if (targeting && cs->target_mode == TGT_SELECT_ALLY)
    {
        draw_text_box((Rectangle){ 12.0f, 154.0f, 160.0f, 12.0f }, "Select an ally", 10, 0, (Color){ 100, 255, 150, 220 }, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ 12.0f, 166.0f, 160.0f, 12.0f }, "Right-click to cancel", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_LEFT);
    }

    for (int i = 0; i < cs->enemy_count; i++)
    {
        if (cs->enemies[i].def && cs->enemies[i].hp > 0)
        {
            bool hl = (cs->target_mode == TGT_SELECT_ENEMY && cs->hovered_enemy == i);
            enemy_render_draw(&cs->enemies[i], hl, targeting);

            if (cs->enemies[i].intent.ability_idx >= 0)
            {
                int ab_idx = cs->enemies[i].intent.ability_idx;
                Rectangle bar = layout_enemy_cast_bar_rect((Vector2){ (float)cs->enemies[i].pos_x, (float)cs->enemies[i].pos_y }, cs->enemy_count, i);
                cast_bar_draw_ability(
                    &cs->enemies[i].def->abilities[ab_idx],
                    cs->enemies[i].intent.remaining_turns,
                    cs->enemies[i].def->abilities[ab_idx].cast_time,
                    (int)bar.x, (int)bar.y
                );
            }
        }
    }

    if (cs->combo_prime != COMBO_PRIME_NONE)
    {
        char cb[64];
        snprintf(cb, sizeof(cb), "PRIMED: %s", cs->synergy_banner_title[0] ? cs->synergy_banner_title : "COMBO");
        float scale = cs->combo_scale <= 0.01f ? 1.0f : cs->combo_scale;
        int font = 10;
        int tw = MeasureText(cb, font);
        int badge_w = tw + 14;
        if (badge_w > 300) badge_w = 300;
        Rectangle badge = { (float)(VIRT_W / 2) - badge_w / 2.0f, 58.0f - (scale - 1.0f) * 3.0f, (float)badge_w, 15.0f * scale };
        DrawRectangleRec(badge, (Color){ 70, 50, 18, 220 });
        DrawRectangleLinesEx(badge, 1.0f, (Color){ 255, 220, 80, 220 });
        draw_text_box((Rectangle){ badge.x + 5.0f, badge.y + 3.0f, badge.width - 10.0f, badge.height - 4.0f },
            cb, font, 0, (Color){ 255, 225, 95, 240 }, TEXT_ALIGN_CENTER);
    }

    if (cs->channel_card && cs->channel_remaining > 0)
    {
        char ch_text[64];
        snprintf(ch_text, sizeof(ch_text), "%s channeling: %d turns", cs->channel_card->name, cs->channel_remaining);
        draw_text_box((Rectangle){ 170.0f, 78.0f, 300.0f, 14.0f }, ch_text, 10, 0, (Color){ 200, 180, 100, 220 }, TEXT_ALIGN_CENTER);

        // Draw channel cast bar on the channeling party member
        for (int i = 0; i < cs->party.count; i++)
        {
            if (cs->party.members[i].class == cs->channel_class && cs->party.members[i].alive)
            {
                Rectangle frame = layout_party_frame_rect(cs->party.count, i);
                int bar_x = (int)frame.x + 5;
                int bar_y = snap_i(frame.y + frame.height + 3);
                int bar_w = (int)frame.width - 10;
                int bar_h = 5;

                DrawRectangleRec((Rectangle){ (float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h }, (Color){ 40, 30, 20, 255 });

                float progress = 1.0f - (float)cs->channel_remaining / (float)cs->channel_card->channel_turns;
                int fill = (int)((bar_w - 4) * progress);
                if (fill > 0)
                    DrawRectangleRec((Rectangle){ (float)(bar_x + 1), (float)(bar_y + 1), (float)fill, (float)(bar_h - 2) }, (Color){ 220, 200, 80, 255 });

                char turns[16];
                snprintf(turns, sizeof(turns), "%d", cs->channel_remaining);
                DrawText(turns, bar_x + bar_w / 2 - MeasureText(turns, 10) / 2, bar_y - 1, 10, (Color){ 220, 200, 80, 220 });
                break;
            }
        }
    }

    draw_discard_pile(cs);
    draw_hand_synergy_glows(cs);
    hand_render_draw(&cs->deck, &cs->energy, cs->hovered_card, cs->channel_class, cs->target_hand_idx, cs->target_offset, cs->combo_prime);
    combat_draw_card_throws(cs);
    draw_combat_feedback(cs);

    int inspector_idx = targeting ? cs->target_hand_idx : cs->hovered_card;
    Rectangle inspector = layout_card_inspector_panel();
    if (inspector_idx >= 0 && inspector_idx < cs->deck.hand_count)
    {
        CardInstance *inst = &cs->deck.cards[cs->deck.hand[inspector_idx]];
        if (inst->def)
            theme_draw_card_tooltip(inspector, inst->def, inst->upgraded);
    }
    else
    {
        draw_panel(inspector, "CARD", (Color){ 130, 145, 185, 220 });
    }

    draw_text_box((Rectangle){ 12.0f, (float)(VIRT_H - 14), 76.0f, 12.0f },
        TextFormat("Turn %d", cs->turn), 10, 0, (Color){ 100, 100, 130, 200 }, TEXT_ALIGN_LEFT);

    Rectangle energy_panel = layout_energy_panel();
    draw_panel(energy_panel, "ENERGY", (Color){ 230, 205, 70, 220 });
    char energy_big[8];
    snprintf(energy_big, sizeof(energy_big), "%d/%d", cs->energy.current, cs->energy.max);
    draw_text_box((Rectangle){ energy_panel.x + 8.0f, energy_panel.y + 18.0f, energy_panel.width - 16.0f, 20.0f },
        energy_big, 18, 0, (Color){ 255, 225, 95, 245 }, TEXT_ALIGN_LEFT);
    char regen_text[16];
    snprintf(regen_text, sizeof(regen_text), "+%d/turn", cs->energy.regen);
    draw_text_box((Rectangle){ energy_panel.x + 10.0f, energy_panel.y + 36.0f, energy_panel.width - 20.0f, 12.0f },
        regen_text, 10, 0, (Color){ 180, 180, 205, 220 }, TEXT_ALIGN_LEFT);

    // Deck button
    Vector2 mouse = GetMousePosition();
    Rectangle deck_btn = { 552.0f, 220.0f, 76.0f, 16.0f };
    Color deck_col = CheckCollisionPointRec(mouse, deck_btn) ? (Color){ 80, 80, 120, 255 } : (Color){ 50, 50, 80, 255 };
    DrawRectangleRec(deck_btn, deck_col);
    DrawRectangleLinesEx(deck_btn, 1.0f, (Color){ 100, 100, 140, 200 });
    draw_text_box((Rectangle){ deck_btn.x + 4.0f, deck_btn.y + 3.0f, deck_btn.width - 8.0f, deck_btn.height - 6.0f },
        "DECK", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    Rectangle end_turn_btn = layout_end_turn_button();
    bool hover = CheckCollisionPointRec(mouse, end_turn_btn);
    Color btn_col = hover ? (Color){ 80, 80, 120, 255 } : (Color){ 50, 50, 80, 255 };
    DrawRectangleRec(end_turn_btn, btn_col);
    DrawRectangleLinesEx(end_turn_btn, 1.0f, (Color){ 100, 100, 140, 200 });
    draw_text_box((Rectangle){ end_turn_btn.x + 4.0f, end_turn_btn.y + 5.0f, end_turn_btn.width - 8.0f, end_turn_btn.height - 8.0f },
        "End Turn", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    draw_pair_passives(cs);
    draw_target_preview(cs);
    relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){ 12.0f, 184.0f, 76.0f, 66.0f });

    for (int i = 0; i < cs->enemy_count; i++)
    {
        if (!cs->enemies[i].def || cs->enemies[i].hp <= 0 || cs->enemies[i].intent.ability_idx < 0) continue;
        int ab_idx = cs->enemies[i].intent.ability_idx;
        Rectangle bar = layout_enemy_cast_bar_rect((Vector2){ (float)cs->enemies[i].pos_x, (float)cs->enemies[i].pos_y }, cs->enemy_count, i);
        cast_bar_draw_ability_tooltip(&cs->enemies[i].def->abilities[ab_idx], bar);
    }
    for (int i = 0; i < cs->enemy_count; i++)
        enemy_render_draw_status_tooltip(&cs->enemies[i]);
    party_frames_draw_tooltips(&cs->party);

    // PRIMED combo hover tooltip
    if (cs->combo_prime != COMBO_PRIME_NONE)
    {
        char cb[64];
        snprintf(cb, sizeof(cb), "PRIMED: %s", cs->synergy_banner_title[0] ? cs->synergy_banner_title : "COMBO");
        int tw = MeasureText(cb, 10);
        int badge_w = tw + 14;
        if (badge_w > 300) badge_w = 300;
        Rectangle badge = { (float)(VIRT_W / 2) - badge_w / 2.0f, 58.0f, (float)badge_w, 15.0f };
        if (CheckCollisionPointRec(mouse, badge) && cs->synergy_banner_subtitle[0])
        {
            int tip_w = 236;
            int tip_h = 42;
            int tip_x = VIRT_W / 2 - tip_w / 2;
            int tip_y = (int)(badge.y + badge.height + 4);
            Rectangle tip = { (float)tip_x, (float)tip_y, (float)tip_w, (float)tip_h };
            DrawRectangleRec(tip, (Color){ 10, 11, 18, 245 });
            DrawRectangleLinesEx(tip, 1.0f, (Color){ 255, 220, 80, 230 });
            draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, 12.0f },
                cs->synergy_banner_title, 10, 0, (Color){ 255, 220, 80, 245 }, TEXT_ALIGN_LEFT);
            draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 18.0f, tip.width - 12.0f, 20.0f },
                cs->synergy_banner_subtitle, 10, 0, (Color){ 210, 214, 235, 235 }, TEXT_ALIGN_LEFT);
        }
    }

}





