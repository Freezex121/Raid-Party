#include "screens.h"
#include "game.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

void game_over_screen_update(void)
{
    if (!g_state.result_recorded)
    {
        int before_slots = meta_party_slots(&g_state.meta);
        meta_record_run(&g_state.meta, g_state.run_won, g_state.result_floor, g_state.result_bosses_defeated);
        meta_save(&g_state.meta);
        g_state.max_party_size = meta_party_slots(&g_state.meta);
        g_state.result_unlocked_party_size = g_state.max_party_size > before_slots ? g_state.max_party_size : 0;
        g_state.result_recorded = true;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        g_state.run_party_active = false;
        g_state.encounter = NULL;
        g_state.encounter_is_elite = false;
        g_state.encounter_is_boss = false;
        g_state.relic_reward_pending = false;
        game_change_screen(SCREEN_TITLE);
    }
}

void game_over_screen_draw(void)
{
    theme_draw_background();

    const char *title = g_state.run_won ? "VICTORY" : "PARTY WIPED";
    Color title_col = g_state.run_won ? (Color){ 90, 230, 140, 255 } : (Color){ 230, 80, 80, 255 };

    DrawText(title, (VIRT_W / 2) - MeasureText(title, 26) / 2, 58, 26, title_col);

    const char *reason = g_state.result_reason[0] ? g_state.result_reason :
        (g_state.run_won ? "The raid is complete." : "The run has ended.");
    DrawText(reason, (VIRT_W / 2) - MeasureText(reason, 9) / 2, 92, 9, (Color){ 190, 190, 215, 230 });

    char line[96];
    snprintf(line, sizeof(line), "Floor reached: %d", g_state.result_floor);
    int stats_x = (VIRT_W / 2) - 86;
    DrawText(line, stats_x, 132, 9, RAYWHITE);

    snprintf(line, sizeof(line), "Bosses defeated: %d", g_state.result_bosses_defeated);
    DrawText(line, stats_x, 148, 9, RAYWHITE);

    snprintf(line, sizeof(line), "Gold earned: %d", g_state.gold);
    DrawText(line, stats_x, 164, 9, (Color){ 230, 200, 80, 255 });

    int valid_cards = 0;
    for (int i = 0; i < g_state.run_deck.card_count; i++)
        if (g_state.run_deck.cards[i].def)
            valid_cards++;
    snprintf(line, sizeof(line), "Deck size: %d", valid_cards);
    DrawText(line, stats_x, 180, 9, RAYWHITE);

    snprintf(line, sizeof(line), "Meta: %d runs  %d wins  slots %d/5", g_state.meta.runs_completed, g_state.meta.wins, g_state.max_party_size);
    DrawText(line, stats_x, 196, 8, (Color){ 170, 175, 205, 230 });

    if (g_state.result_unlocked_party_size > 0)
    {
        snprintf(line, sizeof(line), "Unlocked %d-party drafts!", g_state.result_unlocked_party_size);
        DrawText(line, stats_x, 210, 8, (Color){ 230, 205, 95, 255 });
    }

    int y = 220;
    DrawText("Final party", stats_x, y, 8, (Color){ 160, 160, 190, 220 });
    y += 16;

    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        snprintf(line, sizeof(line), "%s  %d/%d HP%s", pm->name, pm->hp, pm->max_hp, pm->alive ? "" : "  DOWNED");
        Color c = pm->alive ? RAYWHITE : (Color){ 230, 90, 90, 230 };
        DrawText(line, stats_x, y, 7, c);
        y += 13;
    }

    const char *hint = "Click to return to title";
    DrawText(hint, (VIRT_W / 2) - MeasureText(hint, 8) / 2, VIRT_H - 28, 8, (Color){ 160, 160, 190, 220 });
}


