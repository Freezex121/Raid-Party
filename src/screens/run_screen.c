#include "screens.h"
#include "game.h"
#include "combat/combat.h"
#include "ui/party_frames.h"
#include "ui/hand_render.h"
#include "ui/cast_bar.h"
#include "ui/enemy_render.h"
#include "ui/floating_text.h"
#include "ui/theme.h"
#include "data/enemy_defs.h"
#include "data/encounter_defs.h"
#include "util/log.h"
#include "constants.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool combat_initialized = false;

static int party_frame_w(int count)
{
    if (count >= 5) return 80;
    if (count == 4) return 100;
    return FRAME_W;
}

static int party_frame_gap(int count)
{
    return count >= 4 ? 4 : FRAME_GAP;
}

static int party_start_x(Party *party)
{
    int fw = party_frame_w(party->count);
    int gap = party_frame_gap(party->count);
    int tw = party->count * fw + (party->count - 1) * gap;
    return (VIRT_W - tw) / 2;
}

static float preview_combo_mult(CombatState *cs, const CardDef *card)
{
    if (!card || card->class == CLASS_NONE) return 1.0f;
    if (cs->combo_class == card->class && card->cost > cs->combo_last_cost)
        return 1.0f + (float)(cs->combo_count + 1) * 0.10f;
    return 1.0f;
}

static int preview_hits(const CardDef *card)
{
    if (!card) return 1;
    if (strcmp(card->id, "mag_missiles") == 0) return 3;
    if (strcmp(card->id, "shm_fury") == 0) return 2;
    return 1;
}

static void draw_preview_box(int x, int y, const char *title, const char *line1, const char *line2, Color accent)
{
    int w = 92;
    int h = line2 && line2[0] ? 34 : 26;
    if (x < 2) x = 2;
    if (x + w > VIRT_W - 2) x = VIRT_W - w - 2;
    if (y + h > HAND_Y - 3) y = HAND_Y - h - 3;
    DrawRectangleRec((Rectangle){ (float)x, (float)y, (float)w, (float)h }, (Color){ 12, 12, 22, 235 });
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 1.0f, accent);
    DrawText(title, x + 4, y + 4, 5, accent);
    DrawText(line1, x + 4, y + 13, 6, RAYWHITE);
    if (line2 && line2[0])
        DrawText(line2, x + 4, y + 24, 5, (Color){ 180, 180, 205, 230 });
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
        bool revive = strcmp(card->id, "clr_revive") == 0;
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

        if (strcmp(card->id, "rog_smoke") == 0)
            snprintf(line2, sizeof(line2), "Moves aggro to Guardian");
        else if (strcmp(card->id, "clr_renew") == 0)
            snprintf(line2, sizeof(line2), "Renew: +5 HP for 3 turns");

        int fw = party_frame_w(cs->party.count);
        int gap = party_frame_gap(cs->party.count);
        int sx = party_start_x(&cs->party);
        draw_preview_box(sx + cs->hovered_ally * (fw + gap), 31, "Preview", line1, line2, (Color){ 120, 245, 165, 245 });
    }
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

    if (cs->enemy_banner_timer > 0.0f)
    {
        float a = cs->enemy_banner_timer / 0.55f;
        DrawRectangleRec((Rectangle){ (float)(VIRT_W / 2 - 48), 94.0f, 96.0f, 16.0f }, (Color){ 100, 35, 40, (unsigned char)(190 * a) });
        DrawText("ENEMY ACTIONS", VIRT_W / 2 - MeasureText("ENEMY ACTIONS", 8) / 2, 99, 8, (Color){ 255, 180, 170, (unsigned char)(255 * a) });
    }

    if (cs->turn_banner_timer > 0.0f && cs->turn_banner_text[0])
    {
        float a = cs->turn_banner_timer / 0.9f;
        if (a > 1.0f) a = 1.0f;
        DrawRectangleRec((Rectangle){ (float)(VIRT_W / 2 - 44), 42.0f, 88.0f, 14.0f }, (Color){ 35, 70, 105, (unsigned char)(175 * a) });
        DrawText(cs->turn_banner_text, VIRT_W / 2 - MeasureText(cs->turn_banner_text, 7) / 2, 46, 7, (Color){ 210, 235, 255, (unsigned char)(255 * a) });
    }

    if (cs->play_flash_timer > 0.0f && cs->play_flash_text[0])
    {
        float t = 1.0f - cs->play_flash_timer / 0.55f;
        float y = cs->play_flash_y - t * 10.0f;
        unsigned char a = (unsigned char)((1.0f - t) * 230);
        DrawRectangleRec((Rectangle){ cs->play_flash_x, y, 84, 13 }, (Color){ 30, 30, 44, a });
        DrawRectangleLinesEx((Rectangle){ cs->play_flash_x, y, 84, 13 }, 1.0f, (Color){ 230, 215, 120, a });
        DrawText(cs->play_flash_text, (int)cs->play_flash_x + 4, (int)y + 4, 5, (Color){ 245, 235, 190, a });
    }

    int feed_x = 4;
    int feed_y = 64;
    for (int i = 0; i < 5; i++)
    {
        if (cs->action_feed_timer[i] <= 0.0f || !cs->action_feed[i][0]) continue;
        float a = cs->action_feed_timer[i] > 0.5f ? 1.0f : cs->action_feed_timer[i] / 0.5f;
        DrawText(cs->action_feed[i], feed_x, feed_y + i * 7, 5, (Color){ 175, 180, 205, (unsigned char)(210 * a) });
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
    ft_update(GetFrameTime());

    if (g_state.combat.phase == COMBAT_TURN_START)
    {
        combat_initialized = false;

        bool was_victory = (strstr(g_state.combat.result_message, "VICTORY") != 0);
        memcpy(&g_state.run_party, &g_state.combat.party, sizeof(Party));
        g_state.run_party_active = true;

        if (!was_victory)
        {
            g_state.run_won = false;
            g_state.result_floor = g_state.map.floor + 1;
            snprintf(g_state.result_reason, sizeof(g_state.result_reason), "The party wiped on Floor %d.", g_state.result_floor);
            game_change_screen(SCREEN_GAME_OVER);
            return;
        }

        int gold_gain = g_state.encounter_is_boss ? 50 : (g_state.encounter_is_elite ? 25 : 10);
        g_state.gold += gold_gain;
        ft_spawn_gold(gold_gain);

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
            if (g_state.map.floor >= MAX_FLOORS)
            {
                g_state.run_won = true;
                g_state.result_floor = MAX_FLOORS;
                snprintf(g_state.result_reason, sizeof(g_state.result_reason), "Final boss defeated.");
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
        DrawText(cs->result_message, VIRT_W / 2 - MeasureText(cs->result_message, 12) / 2, 114, 12, msg_col);
        return;
    }

    party_frames_draw(&cs->party);

    bool targeting = cs->target_mode != TGT_NONE;

    if (targeting && cs->target_mode == TGT_SELECT_ENEMY)
    {
        DrawText("Select a target", 4, 34, 7, (Color){ 255, 255, 100, 220 });
        DrawText("Right-click to cancel", 4, 43, 5, (Color){ 160, 160, 180, 180 });
    }
    else if (targeting && cs->target_mode == TGT_SELECT_ALLY)
    {
        DrawText("Select an ally", 4, 34, 7, (Color){ 100, 255, 150, 220 });
        DrawText("Right-click to cancel", 4, 43, 5, (Color){ 160, 160, 180, 180 });
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
                cast_bar_draw_ability(
                    &cs->enemies[i].def->abilities[ab_idx],
                    cs->enemies[i].intent.remaining_turns,
                    cs->enemies[i].def->abilities[ab_idx].cast_time,
                    cs->enemies[i].pos_x - CAST_BAR_W / 2, cs->enemies[i].pos_y - ENEMY_SIZE / 2 - CAST_BAR_HEIGHT - 6
                );
            }
        }
    }

    draw_target_preview(cs);

    if (cs->combo_count > 0)
    {
        char cb[64];
        int pct = (cs->combo_count + 1) * 10;
        snprintf(cb, sizeof(cb), "COMBO x%d  +%d%%  %s", cs->combo_count + 1, pct, class_name(cs->combo_class));
        float scale = cs->combo_scale <= 0.01f ? 1.0f : cs->combo_scale;
        int font = (int)(7 * scale);
        if (font < 6) font = 6;
        int tw = MeasureText(cb, font);
        Rectangle badge = { (float)(VIRT_W / 2) - tw / 2.0f - 6.0f, 31.0f - (scale - 1.0f) * 3.0f, (float)tw + 12.0f, 13.0f * scale };
        DrawRectangleRec(badge, (Color){ 70, 50, 18, 220 });
        DrawRectangleLinesEx(badge, 1.0f, (Color){ 255, 220, 80, 220 });
        DrawText(cb, VIRT_W / 2 - tw / 2, (int)(badge.y + 4), font, (Color){ 255, 225, 95, 240 });
    }

    if (cs->channel_card && cs->channel_remaining > 0)
    {
        char ch_text[64];
        snprintf(ch_text, sizeof(ch_text), "%s channeling: %d turns", cs->channel_card->name, cs->channel_remaining);
        DrawText(ch_text, VIRT_W / 2 - MeasureText(ch_text, 6) / 2, 47, 6, (Color){ 200, 180, 100, 220 });

        // Draw channel cast bar on the channeling party member
        int fw = party_frame_w(cs->party.count);
        int fh = FRAME_H;
        int gap = party_frame_gap(cs->party.count);
        int tw = cs->party.count * fw + (cs->party.count - 1) * gap;
        int sx = (VIRT_W - tw) / 2;
        for (int i = 0; i < cs->party.count; i++)
        {
            if (cs->party.members[i].class == cs->channel_class && cs->party.members[i].alive)
            {
                int bar_x = sx + i * (fw + gap) + 4;
                int bar_y = FRAME_Y + fh + 2;
                int bar_w = fw - 8;
                int bar_h = 4;

                DrawRectangleRec((Rectangle){ (float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h }, (Color){ 40, 30, 20, 255 });

                float progress = 1.0f - (float)cs->channel_remaining / (float)cs->channel_card->channel_turns;
                int fill = (int)((bar_w - 4) * progress);
                if (fill > 0)
                    DrawRectangleRec((Rectangle){ (float)(bar_x + 1), (float)(bar_y + 1), (float)fill, (float)(bar_h - 2) }, (Color){ 220, 200, 80, 255 });

                char turns[16];
                snprintf(turns, sizeof(turns), "%d", cs->channel_remaining);
                DrawText(turns, bar_x + bar_w / 2 - MeasureText(turns, 5) / 2, bar_y - 1, 5, (Color){ 220, 200, 80, 220 });
                break;
            }
        }
    }

    hand_render_draw(&cs->deck, &cs->energy, cs->hovered_card, cs->channel_class, cs->target_hand_idx, cs->target_offset);
    draw_combat_feedback(cs);

    DrawText(TextFormat("Turn %d", cs->turn), 4, VIRT_H - 10, 5, (Color){ 100, 100, 130, 200 });

    char etext[48];
    snprintf(etext, sizeof(etext), "Energy: %d/%d (+%d/turn)", cs->energy.current, cs->energy.max, cs->energy.regen);
    DrawText(etext, 4, 3, 5, (Color){ 220, 200, 60, 200 });

    Rectangle end_turn_btn = { (float)(VIRT_W - 43), (float)(VIRT_H - BTN_H - 3), 40.0f, (float)BTN_H };
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, end_turn_btn);
    Color btn_col = hover ? (Color){ 80, 80, 120, 255 } : (Color){ 50, 50, 80, 255 };
    DrawRectangleRec(end_turn_btn, btn_col);
    DrawRectangleLinesEx(end_turn_btn, 1.0f, (Color){ 100, 100, 140, 200 });
    DrawText("End", (int)(end_turn_btn.x + 5), (int)(end_turn_btn.y + 5), 6, RAYWHITE);

}





