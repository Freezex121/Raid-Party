#include "screens.h"
#include "game.h"
#include "data/card_defs.h"
#include "systems/relic.h"
#include "systems/telemetry.h"
#include "util/text.h"
#include "util/log.h"
#include "ui/floating_text.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "constants.h"
#include "assets.h"
#include "raylib.h"
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hovered_reward = -1;
static bool generated = false;
static int extra_choices = 0;

static int reward_keyword_options(const CardDef *card, int *out, int max_options)
{
    if (!card || !out || max_options <= 0) return 0;

    int count = 0;
    if (!card->echo && (card->damage > 0 || card->heal > 0 || card->shield > 0) && count < max_options)
        out[count++] = KW_ECHO;
    if (card->lifesteal <= 0 && card->damage > 0 && card->class != CLASS_NONE && count < max_options)
        out[count++] = KW_LIFESTEAL;
    if (!card->retain && count < max_options)
        out[count++] = KW_RETAIN;
    if (!card->interrupt && card->target == TARGET_ENEMY && count < max_options)
        out[count++] = KW_INTERRUPT;
    if (!card->taunt && card->class != CLASS_NONE && count < max_options)
        out[count++] = KW_TAUNT;
    return count;
}

static int random_reward_keyword(const CardDef *card)
{
    int options[KW_TAUNT + 1];
    int count = reward_keyword_options(card, options, KW_TAUNT + 1);
    return count > 0 ? options[rand() % count] : -1;
}

static const char *reward_encounter_type(void)
{
    return g_state.encounter_is_boss ? "boss" : (g_state.encounter_is_elite ? "elite" : "normal");
}

static void reward_offered_string(char *out, int out_size, bool json_array)
{
    if (!out || out_size <= 0) return;
    out[0] = '\0';
    for (int i = 0; i < g_state.reward_count; i++)
    {
        const CardDef *card = g_state.reward_cards[i];
        const char *id = card && card->id ? card->id : "";
        char piece[96];
        if (json_array)
            snprintf(piece, sizeof(piece), "%s\"%s\"", i > 0 ? "," : "", id);
        else
            snprintf(piece, sizeof(piece), "%s%s", i > 0 ? "|" : "", id);
        strncat(out, piece, out_size - strlen(out) - 1);
    }
}

static void log_card_reward_metric(const char *action, const CardDef *picked, int pick_number)
{
    char run_id[16], area[16], floor[16], pick_no[16], picks_remaining[16], gold[16];
    char offered[256];
    reward_offered_string(offered, sizeof(offered), false);
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(area, sizeof(area), "%d", g_state.current_area);
    snprintf(floor, sizeof(floor), "%d", g_state.map.floor + 1);
    snprintf(pick_no, sizeof(pick_no), "%d", pick_number);
    snprintf(picks_remaining, sizeof(picks_remaining), "%d", g_state.reward_picks_remaining);
    snprintf(gold, sizeof(gold), "%d", g_state.gold);
    const char *fields[] = {
        run_id,
        area,
        floor,
        reward_encounter_type(),
        action ? action : "",
        offered,
        picked && picked->id ? picked->id : "",
        pick_no,
        picks_remaining,
        gold
    };
    telemetry_csv_append(
        "card_reward_metrics.csv",
        "timestamp,run_id,area,floor,encounter,action,offered,picked,pick_number,picks_remaining,gold",
        fields,
        10);

    char offered_json[320];
    reward_offered_string(offered_json, sizeof(offered_json), true);
    char json[640];
    snprintf(json, sizeof(json),
        "\"area\":%d,\"floor\":%d,\"encounter\":\"%s\",\"action\":\"%s\",\"offered\":[%s],\"picked\":\"%s\",\"pick_number\":%d,\"picks_remaining\":%d,\"gold\":%d",
        g_state.current_area,
        g_state.map.floor + 1,
        reward_encounter_type(),
        action ? action : "",
        offered_json,
        picked && picked->id ? picked->id : "",
        pick_number,
        g_state.reward_picks_remaining,
        g_state.gold);
    telemetry_push_json("card_pick", json);
}

static void generate_rewards(void)
{
    int count = g_state.encounter_is_elite ? 4 : (g_state.encounter_is_boss ? 4 : 3);
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_EXPLORER_LANTERN))
        count++;
    count += meta_reward_choice_bonus(&g_state.meta);
    count += extra_choices;
    if (count > MAX_REWARD_CARDS) count = MAX_REWARD_CARDS;
    g_state.reward_count = count;
    g_state.reward_picks_remaining = g_state.encounter_is_boss ? 2 : 1;
    for (int i = 0; i < MAX_REWARD_CARDS; i++)
    {
        g_state.reward_picked[i] = false;
        g_state.reward_keyword[i] = -1;
    }

    const CardDef *pool[128];
    int pool_count = card_reward_pool_for_party(
        g_state.selected_classes,
        g_state.selected_count,
        &g_state.meta,
        pool,
        128);
    if (pool_count <= 0)
    {
        const CardDef *fallback = card_def_by_id("util_prep");
        if (!fallback && utility_card_count > 0)
            fallback = &utility_cards[0];
        if (!fallback)
        {
            g_state.reward_count = 0;
            return;
        }
        pool[pool_count++] = fallback;
    }

    // Pick random cards, no duplicates
    int used_indices[128] = {0};

    for (int i = 0; i < count; i++)
    {
        int idx;
        int attempts = 0;
        do {
            idx = rand() % pool_count;
            attempts++;
        } while (used_indices[idx] && attempts < 100);
        used_indices[idx] = 1;

        g_state.reward_cards[i] = pool[idx];
        g_state.reward_upgrade_level[i] = 0;

        int upgrade_chance = meta_reward_upgrade_chance_percent(&g_state.meta);
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_CHRONICLE_QUILL))
            upgrade_chance += 10;
        if (g_state.encounter_is_boss)
            upgrade_chance += 50;
        if (upgrade_chance > 95)
            upgrade_chance = 95;
        if (card_upgrade_changes_values(pool[idx]) && upgrade_chance > 0 && (rand() % 100) < upgrade_chance)
            g_state.reward_upgrade_level[i] = 1;
        if (g_state.encounter_is_boss && i == 0 && card_upgrade_changes_values(pool[idx]) && relic_has(g_state.relics, g_state.relic_count, RELIC_VETERAN_SIGIL))
            g_state.reward_upgrade_level[i] = 1;

        // Boss: 20% chance this card gets a keyword effect
        if (g_state.encounter_is_boss && (rand() % 5) == 0)
            g_state.reward_keyword[i] = random_reward_keyword(pool[idx]);

        LOG_I(CAT_CARD, "Reward[%d]: %s (%s)%s", i, pool[idx]->name,
            class_name(pool[idx]->class),
            g_state.reward_upgrade_level[i] > 0 ? " UPGRADED" : "");
    }

    if (g_state.encounter_is_boss)
    {
        bool has_keyword = false;
        for (int i = 0; i < count; i++)
            if (g_state.reward_keyword[i] >= 0)
                has_keyword = true;
        if (!has_keyword && count > 0)
        {
            int candidates[MAX_REWARD_CARDS];
            int candidate_count = 0;
            for (int i = 0; i < count; i++)
            {
                int options[KW_TAUNT + 1];
                if (reward_keyword_options(g_state.reward_cards[i], options, KW_TAUNT + 1) > 0)
                    candidates[candidate_count++] = i;
            }
            if (candidate_count > 0)
            {
                int reward_idx = candidates[rand() % candidate_count];
                g_state.reward_keyword[reward_idx] = random_reward_keyword(g_state.reward_cards[reward_idx]);
            }
        }
    }

    vfx_spawn_burst((float)(VIRT_W / 2), 74.0f, (Color){ 255, 220, 90, 255 }, 7);
}

void reward_screen_update(void)
{
    if (!generated)
    {
        generate_rewards();
        generated = true;
    }

    if (!g_state.tutorial_active && g_state.tutorial_reward_pending)
    {
        g_state.tutorial_reward_pending = false;
        g_state.tutorial_active = true;
        g_state.tutorial_step = TUTORIAL_STEP_REWARD;
    }

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REWARD)
    {
        if (game_tutorial_handle_skip())
            return;
    }

    if (g_state.reward_count == 0) return;

    Vector2 mouse = GetMousePosition();
    hovered_reward = -1;

    // Skip button
    Rectangle skip_btn = { 172.0f, 206.0f, (float)BTN_NARROW, (float)BTN_H };
    if (CheckCollisionPointRec(mouse, skip_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REWARD)
            game_skip_tutorial();
        log_card_reward_metric("skip", NULL, 0);
        generated = false;
        extra_choices = 0;
        if (g_state.encounter_is_elite)
        {
            g_state.discard_count = 1;
            g_state.discard_selected = 0;
            game_change_screen(SCREEN_DISCARD);
        }
        else if (g_state.encounter_is_boss)
        {
            g_state.discard_count = 2;
            g_state.discard_selected = 0;
            game_change_screen(SCREEN_DISCARD);
        }
        else
        {
            game_change_screen(SCREEN_MAP);
        }
        return;
    }

    Rectangle reroll_btn = { 260.0f, 206.0f, (float)BTN_NARROW, (float)BTN_H };
    if (CheckCollisionPointRec(mouse, reroll_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        int reroll_cost = meta_reward_reroll_cost(&g_state.meta, 10);
        if (game_spend_gold(reroll_cost, "reward_reroll"))
        {
            log_card_reward_metric("reroll", NULL, 0);
            generated = false;
        }
        return;
    }

    Rectangle extra_btn = { 348.0f, 206.0f, (float)BTN_MED, (float)BTN_H };
    if (CheckCollisionPointRec(mouse, extra_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (g_state.reward_count >= MAX_REWARD_CARDS)
            return;
        if (game_spend_gold(15, "reward_extra_choice"))
        {
            log_card_reward_metric("extra_choice", NULL, 0);
            extra_choices++;
            generated = false;
        }
        return;
    }

    for (int i = 0; i < g_state.reward_count; i++)
    {
        Rectangle r = layout_reward_card_rect(g_state.reward_count, i);
        if (CheckCollisionPointRec(mouse, r) && !g_state.reward_picked[i])
        {
            hovered_reward = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REWARD)
                    game_skip_tutorial();
                const CardDef *chosen = g_state.reward_cards[i];
                deck_add_card_with_level(&g_state.run_deck, chosen, g_state.reward_upgrade_level[i]);
                if (g_state.reward_keyword[i] >= 0)
                {
                    CardInstance *ci = &g_state.run_deck.cards[g_state.run_deck.card_count - 1];
                    int kw = g_state.reward_keyword[i];
                    if (kw == 0) ci->echo_override = 1;
                    else if (kw == 1) ci->lifesteal_override = 1;
                    else if (kw == 2) ci->retain_override = 1;
                    else if (kw == 3) ci->interrupt_override = 1;
                    else if (kw == 4) ci->taunt_override = 1;
                }
                assets_play_sfx(SFX_REWARD_PICKUP);
                int pick_number = g_state.encounter_is_boss ? (2 - g_state.reward_picks_remaining + 1) : 1;
                LOG_I(CAT_CARD, "Reward chosen: %s (%s)%s", chosen->name, class_name(chosen->class),
                    g_state.reward_upgrade_level[i] > 0 ? " UPGRADED" : "");
                g_state.reward_picked[i] = true;
                g_state.reward_picks_remaining--;
                log_card_reward_metric("pick", chosen, pick_number);

                // Scholar's Notes: 5g per unpicked card on final pick
                if (g_state.reward_picks_remaining <= 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_SCHOLAR_NOTES))
                {
                    int unpicked = 0;
                    for (int j = 0; j < g_state.reward_count; j++)
                        if (!g_state.reward_picked[j]) unpicked++;
                    int gold_gain = unpicked * 5;
                    game_gain_gold(gold_gain, "scholar_notes");
                }

                if (g_state.reward_picks_remaining <= 0)
                {
                    generated = false;
                    extra_choices = 0;
                    if (g_state.encounter_is_elite)
                    {
                        g_state.discard_count = 1;
                        g_state.discard_selected = 0;
                        game_change_screen(SCREEN_DISCARD);
                    }
                    else if (g_state.encounter_is_boss)
                    {
                        g_state.discard_count = 2;
                        g_state.discard_selected = 0;
                        game_change_screen(SCREEN_DISCARD);
                    }
                    else
                    {
                        game_change_screen(SCREEN_MAP);
                    }
                }
            }
            break;
        }
    }
}

void reward_screen_draw(void)
{
    theme_draw_background();

    const char *title = g_state.encounter_is_boss ? "BOSS REWARD" :
                        g_state.encounter_is_elite ? "ELITE REWARD" : "CARD REWARD";
    Color title_col = g_state.encounter_is_boss ? (Color){ 220, 180, 50, 255 } :
                      g_state.encounter_is_elite ? (Color){ 200, 100, 220, 255 } : RAYWHITE;
    draw_text_box((Rectangle){ 80.0f, 18.0f, 480.0f, 22.0f }, title, 18, 0, title_col, TEXT_ALIGN_CENTER);
    char pick_label[48];
    snprintf(pick_label, sizeof(pick_label), "Choose %d card%s", g_state.reward_picks_remaining, g_state.reward_picks_remaining > 1 ? "s" : "");
    draw_text_box((Rectangle){ 80.0f, 40.0f, 480.0f, 14.0f }, pick_label, 10, 0, (Color){ 160, 160, 180, 200 }, TEXT_ALIGN_CENTER);

    // Skip and Reroll buttons
    Rectangle skip_btn = { 172.0f, 206.0f, (float)BTN_NARROW, (float)BTN_H };
    draw_btn_standard(skip_btn, (Color){ 60, 60, 60, 255 }, (Color){ 100, 100, 100, 255 }, "Skip", BTN_ID_REWARD_SKIP);

    Rectangle reroll_btn = { 260.0f, 206.0f, (float)BTN_NARROW, (float)BTN_H };
    int reroll_cost = meta_reward_reroll_cost(&g_state.meta, 10);
    bool can_reroll = g_state.gold >= reroll_cost;
    char reroll_label[24];
    snprintf(reroll_label, sizeof(reroll_label), "Reroll %dg", reroll_cost);
    draw_btn_standard(reroll_btn,
        can_reroll ? (Color){ 45, 120, 60, 255 } : (Color){ 40, 40, 60, 255 },
        can_reroll ? (Color){ 70, 180, 90, 255 } : (Color){ 40, 40, 60, 255 },
        reroll_label, BTN_ID_REWARD_REROLL);

    Rectangle extra_btn = { 348.0f, 206.0f, (float)BTN_MED, (float)BTN_H };
    bool can_extra = g_state.gold >= 15 && g_state.reward_count < MAX_REWARD_CARDS;
    char extra_label[24];
    snprintf(extra_label, sizeof(extra_label), "Extra +1 15g");
    draw_btn_standard(extra_btn,
        can_extra ? (Color){ 45, 88, 150, 255 } : (Color){ 40, 40, 60, 255 },
        can_extra ? (Color){ 80, 150, 210, 255 } : (Color){ 40, 40, 60, 255 },
        extra_label, BTN_ID_REWARD_EXTRA);

    for (int i = 0; i < g_state.reward_count; i++)
    {
        if (g_state.reward_picked[i]) continue;

        const CardDef *card = g_state.reward_cards[i];
        Rectangle card_rect = layout_reward_card_rect(g_state.reward_count, i);
        unsigned int seed = theme_card_seed_from_id(card && card->id ? card->id : "reward", (unsigned int)(i + 1));
        theme_draw_card_art_seeded(card_rect, card, g_state.reward_upgrade_level[i], seed, -1);
        if (g_state.reward_keyword[i] >= 0 && g_state.reward_keyword[i] < KW_COUNT)
            theme_draw_keyword_icon(card_rect, (KeywordIcon)g_state.reward_keyword[i]);
    }

    if (hovered_reward >= 0 && hovered_reward < g_state.reward_count && !g_state.reward_picked[hovered_reward])
    {
        const CardDef *card = g_state.reward_cards[hovered_reward];
        theme_draw_card_tooltip(layout_reward_inspector_panel(), card, g_state.reward_upgrade_level[hovered_reward]);
    }

    // Tutorial overlay
    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REWARD)
    {
        Rectangle hl = { 100.0f, 60.0f, 440.0f, 152.0f };
        game_draw_tutorial_overlay_ex(hl,
            "Card Rewards",
            "Pick a card to add it to your deck. Reroll costs gold, Extra Choice adds another option, and Skip keeps your deck lean.",
            "Choose a reward  |  Right-click/Esc: skip tutorial",
            TUTORIAL_STEP_REWARD,
            TUTORIAL_STEP_REWARD);
    }

}



