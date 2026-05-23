#include "screens.h"
#include "game.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

void game_over_screen_update(void)
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        g_state.run_party_active = false;
        g_state.encounter = NULL;
        g_state.encounter_is_elite = false;
        g_state.encounter_is_boss = false;
        game_change_screen(SCREEN_TITLE);
    }
}

void game_over_screen_draw(void)
{
    theme_draw_background();

    const char *title = g_state.run_won ? "VICTORY" : "PARTY WIPED";
    Color title_col = g_state.run_won ? (Color){ 90, 230, 140, 255 } : (Color){ 230, 80, 80, 255 };

    DrawText(title, (VIRT_W / 2) - MeasureText(title, 18) / 2, 24, 18, title_col);

    const char *reason = g_state.result_reason[0] ? g_state.result_reason :
        (g_state.run_won ? "The raid is complete." : "The run has ended.");
    DrawText(reason, (VIRT_W / 2) - MeasureText(reason, 7) / 2, 50, 7, (Color){ 190, 190, 215, 230 });

    char line[96];
    snprintf(line, sizeof(line), "Floor reached: %d", g_state.result_floor);
    int stats_x = (VIRT_W / 2) - 58;
    DrawText(line, stats_x, 78, 7, RAYWHITE);

    snprintf(line, sizeof(line), "Bosses defeated: %d", g_state.result_bosses_defeated);
    DrawText(line, stats_x, 91, 7, RAYWHITE);

    snprintf(line, sizeof(line), "Gold earned: %d", g_state.gold);
    DrawText(line, stats_x, 104, 7, (Color){ 230, 200, 80, 255 });

    int valid_cards = 0;
    for (int i = 0; i < g_state.run_deck.card_count; i++)
        if (g_state.run_deck.cards[i].def)
            valid_cards++;
    snprintf(line, sizeof(line), "Deck size: %d", valid_cards);
    DrawText(line, stats_x, 117, 7, RAYWHITE);

    int y = 145;
    DrawText("Final party", stats_x, y, 6, (Color){ 160, 160, 190, 220 });
    y += 12;

    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        snprintf(line, sizeof(line), "%s  %d/%d HP%s", pm->name, pm->hp, pm->max_hp, pm->alive ? "" : "  DOWNED");
        Color c = pm->alive ? RAYWHITE : (Color){ 230, 90, 90, 230 };
        DrawText(line, stats_x, y, 6, c);
        y += 10;
    }

    const char *hint = "Click to return to title";
    DrawText(hint, (VIRT_W / 2) - MeasureText(hint, 6) / 2, VIRT_H - 20, 6, (Color){ 160, 160, 190, 220 });
}


