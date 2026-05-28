#include "screens.h"
#include "game.h"
#include "data/card_defs.h"
#include "systems/relic.h"
#include "util/text.h"
#include "util/log.h"
#include "ui/floating_text.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

static int hovered_reward = -1;
static bool generated = false;
static int extra_choices = 0;

static void generate_rewards(void)
{
    int count = g_state.encounter_is_elite ? 4 : (g_state.encounter_is_boss ? 4 : 3);
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_EXPLORER_LANTERN))
        count++;
    count += extra_choices;
    if (count > MAX_REWARD_CARDS) count = MAX_REWARD_CARDS;
    g_state.reward_count = count;
    g_state.reward_picks_remaining = g_state.encounter_is_boss ? 2 : 1;
    for (int i = 0; i < MAX_REWARD_CARDS; i++)
        g_state.reward_picked[i] = false;

    // Collect all eligible cards from party classes + utilities
    const CardDef *pool[80];
    int pool_count = 0;

    for (int i = 0; i < g_state.selected_count; i++)
    {
        ClassType ct = (ClassType)g_state.selected_classes[i];
        const CardDef *set = class_card_sets[ct];
        for (int c = 0; c < class_card_counts[ct]; c++)
            pool[pool_count++] = &set[c];
    }
    // Also include utility cards
    for (int c = 0; c < utility_card_count; c++)
        pool[pool_count++] = &utility_cards[c];

    // Pick random cards, no duplicates
    int used_indices[80] = {0};

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

        // Boss: 50% chance this card is upgraded
        if (g_state.encounter_is_boss && card_upgrade_changes_values(pool[idx]) && (rand() % 2) == 0)
            g_state.reward_upgrade_level[i] = 1;
        if (g_state.encounter_is_boss && i == 0 && card_upgrade_changes_values(pool[idx]) && relic_has(g_state.relics, g_state.relic_count, RELIC_VETERAN_SIGIL))
            g_state.reward_upgrade_level[i] = 1;

        LOG_I(CAT_CARD, "Reward[%d]: %s (%s)%s", i, pool[idx]->name,
            class_name(pool[idx]->class),
            g_state.reward_upgrade_level[i] > 0 ? " UPGRADED" : "");
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
    Rectangle skip_btn = { (float)(VIRT_W / 2 - 138), 206.0f, 84.0f, 22.0f };
    if (CheckCollisionPointRec(mouse, skip_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REWARD)
            game_skip_tutorial();
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

    Rectangle reroll_btn = { (float)(VIRT_W / 2 - 42), 206.0f, 84.0f, 22.0f };
    if (CheckCollisionPointRec(mouse, reroll_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (game_spend_gold(10, "reward_reroll"))
            generated = false;
        return;
    }

    Rectangle extra_btn = { (float)(VIRT_W / 2 + 50), 206.0f, 96.0f, 22.0f };
    if (CheckCollisionPointRec(mouse, extra_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (g_state.reward_count >= MAX_REWARD_CARDS)
            return;
        if (game_spend_gold(15, "reward_extra_choice"))
        {
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
                LOG_I(CAT_CARD, "Reward chosen: %s (%s)%s", chosen->name, class_name(chosen->class),
                    g_state.reward_upgrade_level[i] > 0 ? " UPGRADED" : "");
                g_state.reward_picked[i] = true;
                g_state.reward_picks_remaining--;

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
    Vector2 mouse = GetMousePosition();
    Rectangle skip_btn = { (float)(VIRT_W / 2 - 138), 206.0f, 84.0f, 22.0f };
    bool skip_hover = CheckCollisionPointRec(mouse, skip_btn);
    Color skip_col = skip_hover ? (Color){ 100, 100, 100, 255 } : (Color){ 60, 60, 60, 255 };
    DrawRectangleRec(skip_btn, skip_col);
    DrawRectangleLinesEx(skip_btn, 1.0f, (Color){ 120, 120, 120, 200 });
    draw_text_box((Rectangle){ skip_btn.x + 4.0f, skip_btn.y + 4.0f, skip_btn.width - 8.0f, skip_btn.height - 8.0f },
        "Skip", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    Rectangle reroll_btn = { (float)(VIRT_W / 2 - 42), 206.0f, 84.0f, 22.0f };
    bool can_reroll = g_state.gold >= 10;
    bool reroll_hover = can_reroll && CheckCollisionPointRec(mouse, reroll_btn);
    Color reroll_col = reroll_hover ? (Color){ 70, 180, 90, 255 } : (can_reroll ? (Color){ 45, 120, 60, 255 } : (Color){ 40, 40, 60, 255 });
    DrawRectangleRec(reroll_btn, reroll_col);
    DrawRectangleLinesEx(reroll_btn, 1.0f, can_reroll ? (Color){ 100, 220, 120, 220 } : (Color){ 80, 80, 100, 150 });
    char reroll_label[24];
    snprintf(reroll_label, sizeof(reroll_label), "Reroll 10g");
    draw_text_box((Rectangle){ reroll_btn.x + 4.0f, reroll_btn.y + 4.0f, reroll_btn.width - 8.0f, reroll_btn.height - 8.0f },
        reroll_label, 10, 0, can_reroll ? RAYWHITE : (Color){ 100, 100, 120, 180 }, TEXT_ALIGN_CENTER);

    Rectangle extra_btn = { (float)(VIRT_W / 2 + 50), 206.0f, 96.0f, 22.0f };
    bool can_extra = g_state.gold >= 15 && g_state.reward_count < MAX_REWARD_CARDS;
    bool extra_hover = can_extra && CheckCollisionPointRec(mouse, extra_btn);
    Color extra_col = extra_hover ? (Color){ 80, 150, 210, 255 } : (can_extra ? (Color){ 45, 88, 150, 255 } : (Color){ 40, 40, 60, 255 });
    DrawRectangleRec(extra_btn, extra_col);
    DrawRectangleLinesEx(extra_btn, 1.0f, can_extra ? (Color){ 120, 185, 245, 220 } : (Color){ 80, 80, 100, 150 });
    draw_text_box((Rectangle){ extra_btn.x + 4.0f, extra_btn.y + 4.0f, extra_btn.width - 8.0f, extra_btn.height - 8.0f },
        "Extra +1 15g", 10, 0, can_extra ? RAYWHITE : (Color){ 100, 100, 120, 180 }, TEXT_ALIGN_CENTER);

    for (int i = 0; i < g_state.reward_count; i++)
    {
        if (g_state.reward_picked[i]) continue;

        const CardDef *card = g_state.reward_cards[i];
        Rectangle card_rect = layout_reward_card_rect(g_state.reward_count, i);
        theme_draw_card_art(card_rect, card, g_state.reward_upgrade_level[i]);
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



