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

static void generate_rewards(void)
{
    int count = g_state.encounter_is_elite ? 4 : (g_state.encounter_is_boss ? 4 : 3);
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_EXPLORER_LANTERN))
        count++;
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
        g_state.reward_upgraded[i] = false;

        // Boss: 50% chance this card is upgraded
        if (g_state.encounter_is_boss && card_upgrade_changes_values(pool[idx]) && (rand() % 2) == 0)
            g_state.reward_upgraded[i] = true;
        if (g_state.encounter_is_boss && i == 0 && card_upgrade_changes_values(pool[idx]) && relic_has(g_state.relics, g_state.relic_count, RELIC_VETERAN_SIGIL))
            g_state.reward_upgraded[i] = true;

        LOG_I(CAT_CARD, "Reward[%d]: %s (%s)%s", i, pool[idx]->name,
            class_name(pool[idx]->class),
            g_state.reward_upgraded[i] ? " UPGRADED" : "");
    }

    vfx_spawn_burst((float)(VIRT_W / 2), 74.0f, (Color){ 255, 220, 90, 255 }, 7);
}

void reward_screen_update(void)
{
    static bool generated = false;
    if (!generated)
    {
        generate_rewards();
        generated = true;
    }
    ft_update(GetFrameTime());

    if (g_state.reward_count == 0) return;

    Vector2 mouse = GetMousePosition();
    hovered_reward = -1;

    for (int i = 0; i < g_state.reward_count; i++)
    {
        Rectangle r = layout_reward_card_rect(g_state.reward_count, i);
        if (CheckCollisionPointRec(mouse, r) && !g_state.reward_picked[i])
        {
            hovered_reward = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                const CardDef *chosen = g_state.reward_cards[i];
                deck_add_card_upgraded(&g_state.run_deck, chosen, g_state.reward_upgraded[i]);
                LOG_I(CAT_CARD, "Reward chosen: %s (%s)%s", chosen->name, class_name(chosen->class),
                    g_state.reward_upgraded[i] ? " UPGRADED" : "");
                g_state.reward_picked[i] = true;
                g_state.reward_picks_remaining--;

                // Scholar's Notes: 5g per unpicked card on final pick
                if (g_state.reward_picks_remaining <= 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_SCHOLAR_NOTES))
                {
                    int unpicked = 0;
                    for (int j = 0; j < g_state.reward_count; j++)
                        if (!g_state.reward_picked[j]) unpicked++;
                    int gold_gain = unpicked * 5;
                    g_state.gold += gold_gain;
                }

                if (g_state.reward_picks_remaining <= 0)
                {
                    generated = false;
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

    for (int i = 0; i < g_state.reward_count; i++)
    {
        if (g_state.reward_picked[i]) continue;

        const CardDef *card = g_state.reward_cards[i];
        Rectangle card_rect = layout_reward_card_rect(g_state.reward_count, i);
        theme_draw_card_art(card_rect, card, g_state.reward_upgraded[i]);
    }

    if (hovered_reward >= 0 && hovered_reward < g_state.reward_count && !g_state.reward_picked[hovered_reward])
    {
        const CardDef *card = g_state.reward_cards[hovered_reward];
        theme_draw_card_tooltip(layout_reward_inspector_panel(), card, g_state.reward_upgraded[hovered_reward]);
    }

}



