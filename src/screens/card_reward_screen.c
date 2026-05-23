#include "screens.h"
#include "game.h"
#include "data/card_defs.h"
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
    g_state.reward_count = count;
    g_state.reward_picks_remaining = g_state.encounter_is_boss ? 2 : 1;
    for (int i = 0; i < MAX_REWARD_CARDS; i++)
        g_state.reward_picked[i] = false;

    // Collect all eligible cards from party classes + utilities
    const CardDef *pool[48];
    int pool_count = 0;

    for (int i = 0; i < g_state.selected_count; i++)
    {
        ClassType ct = (ClassType)g_state.selected_classes[i];
        const CardDef *set = class_card_sets[ct];
        for (int c = 0; c < class_card_counts[ct]; c++)
            pool[pool_count++] = &set[c];
    }
    // Also include utility cards
    for (int c = 0; c < UTILITY_CARD_COUNT; c++)
        pool[pool_count++] = &utility_cards[c];

    // Pick random cards, no duplicates
    int used_indices[36] = {0};

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
        if (g_state.encounter_is_boss && (rand() % 2) == 0)
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
    DrawText(title, (VIRT_W / 2) - MeasureText(title, 16) / 2, 18, 16, title_col);
    char pick_label[48];
    snprintf(pick_label, sizeof(pick_label), "Choose %d card%s", g_state.reward_picks_remaining, g_state.reward_picks_remaining > 1 ? "s" : "");
    DrawText(pick_label, (VIRT_W / 2) - MeasureText(pick_label, 8) / 2, 40, 8, (Color){ 160, 160, 180, 200 });

    for (int i = 0; i < g_state.reward_count; i++)
    {
        if (g_state.reward_picked[i]) continue;

        const CardDef *card = g_state.reward_cards[i];
        Rectangle card_rect = layout_reward_card_rect(g_state.reward_count, i);
        if (i == hovered_reward)
            DrawRectangleRec((Rectangle){ card_rect.x - 2, card_rect.y - 2, card_rect.width + 4, card_rect.height + 4 },
                (Color){ 255, 255, 255, 35 });
        theme_draw_card_art(card_rect, card, g_state.reward_upgraded[i]);
        if (i == hovered_reward)
            DrawRectangleLinesEx(card_rect, 2.0f, RAYWHITE);
    }

    if (hovered_reward >= 0 && hovered_reward < g_state.reward_count && !g_state.reward_picked[hovered_reward])
    {
        const CardDef *card = g_state.reward_cards[hovered_reward];
        theme_draw_card_tooltip(layout_reward_inspector_panel(), card, g_state.reward_upgraded[hovered_reward]);
    }

}



