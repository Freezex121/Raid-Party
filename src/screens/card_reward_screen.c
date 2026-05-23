#include "screens.h"
#include "game.h"
#include "data/card_defs.h"
#include "util/text.h"
#include "util/log.h"
#include "ui/floating_text.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

static int hovered_reward = -1;

static void generate_rewards(void)
{
    int count = g_state.encounter_is_elite ? 4 : (g_state.encounter_is_boss ? 4 : 3);
    g_state.reward_count = count;

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

    int card_w = REWARD_CARD_W, card_h = REWARD_CARD_H;
    int gap = REWARD_GAP;
    int total_w = g_state.reward_count * (card_w + gap) - gap;
    int start_x = (VIRT_W - total_w) / 2;
    int card_y = REWARD_Y;

    for (int i = 0; i < g_state.reward_count; i++)
    {
        Rectangle r = { (float)(start_x + i * (card_w + gap)), (float)card_y, (float)card_w, (float)card_h };
        if (CheckCollisionPointRec(mouse, r))
        {
            hovered_reward = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                const CardDef *chosen = g_state.reward_cards[i];
                deck_add_card_upgraded(&g_state.run_deck, chosen, g_state.reward_upgraded[i]);
                LOG_I(CAT_CARD, "Reward chosen: %s (%s)%s", chosen->name, class_name(chosen->class),
                    g_state.reward_upgraded[i] ? " UPGRADED" : "");
                generated = false;
                game_change_screen(SCREEN_MAP);
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
    DrawText(title, (VIRT_W / 2) - MeasureText(title, 12) / 2, 12, 12, title_col);
    DrawText("Choose a card to add to your deck", (VIRT_W / 2) - MeasureText("Choose a card to add to your deck", 6) / 2, 29, 6, (Color){ 160, 160, 180, 200 });

    int card_w = REWARD_CARD_W, card_h = REWARD_CARD_H;
    int gap = REWARD_GAP;
    int total_w = g_state.reward_count * (card_w + gap) - gap;
    int start_x = (VIRT_W - total_w) / 2;
    int card_y = REWARD_Y;

    for (int i = 0; i < g_state.reward_count; i++)
    {
        const CardDef *card = g_state.reward_cards[i];
        int x = start_x + i * (card_w + gap);
        Rectangle card_rect = { (float)x, (float)card_y, (float)card_w, (float)card_h };
        if (i == hovered_reward)
            DrawRectangleRec((Rectangle){ card_rect.x - 2, card_rect.y - 2, card_rect.width + 4, card_rect.height + 4 },
                (Color){ 255, 255, 255, 35 });
        theme_draw_card_art(card_rect, card, g_state.reward_upgraded[i]);
        if (i == hovered_reward)
            DrawRectangleLinesEx(card_rect, 2.0f, RAYWHITE);
    }

}



