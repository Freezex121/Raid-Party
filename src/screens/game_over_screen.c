#include "screens.h"
#include "game.h"
#include "data/area_defs.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

void game_over_screen_update(void)
{
    if (!g_state.result_recorded)
    {
        int before_slots = meta_party_slots(&g_state.meta);
        int before_area = g_state.meta.highest_area_unlocked;
        g_state.result_renown_gained = meta_record_run(
            &g_state.meta,
            g_state.run_won,
            g_state.result_area,
            g_state.result_floor,
            g_state.result_bosses_defeated,
            g_state.run_party.count,
            g_state.run_deaths,
            g_state.relic_count,
            g_state.run_interrupts,
            g_state.run_best_combat_turns,
            area_defs_count(),
            &g_state.result_achievement_renown,
            g_state.result_achievement_names,
            sizeof(g_state.result_achievement_names));
        int area_count = area_defs_count();
        if (area_count > 0 && g_state.meta.highest_area_unlocked >= area_count)
            g_state.meta.highest_area_unlocked = area_count - 1;
        meta_save(&g_state.meta);
        g_state.max_party_size = meta_party_slots(&g_state.meta);
        g_state.result_unlocked_party_size = g_state.max_party_size > before_slots ? g_state.max_party_size : 0;
        g_state.result_unlocked_area = g_state.meta.highest_area_unlocked > before_area ? g_state.meta.highest_area_unlocked : -1;
        if (g_state.result_unlocked_area >= 0)
            g_state.selected_area = g_state.result_unlocked_area;
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

    DrawText(title, (VIRT_W / 2) - MeasureText(title, 18) / 2, 58, 18, title_col);

    const char *reason = g_state.result_reason[0] ? g_state.result_reason :
        (g_state.run_won ? "The raid is complete." : "The run has ended.");
    DrawText(reason, (VIRT_W / 2) - MeasureText(reason, 10) / 2, 92, 10, (Color){ 190, 190, 215, 230 });

    char line[96];
    const AreaDef *area = area_def(g_state.result_area);
    int area_floors = area_floor_count(g_state.result_area);
    int loaded_floors = map_loaded_floor_count_for_area(area ? area->id : NULL);
    if (loaded_floors > 0 && area_floors > loaded_floors)
        area_floors = loaded_floors;
    snprintf(line, sizeof(line), "Area: %s", area ? area->name : "Unknown Area");
    int stats_x = (VIRT_W / 2) - 86;
    DrawText(line, stats_x, 132, 10, RAYWHITE);

    snprintf(line, sizeof(line), "Floor reached: %d/%d", g_state.result_floor, area_floors);
    DrawText(line, stats_x, 148, 10, RAYWHITE);

    snprintf(line, sizeof(line), "Bosses defeated: %d", g_state.result_bosses_defeated);
    DrawText(line, stats_x, 164, 10, RAYWHITE);

    snprintf(line, sizeof(line), "Gold earned: %d", g_state.gold);
    DrawText(line, stats_x, 180, 10, (Color){ 230, 200, 80, 255 });

    int valid_cards = 0;
    for (int i = 0; i < g_state.run_deck.card_count; i++)
        if (g_state.run_deck.cards[i].def)
            valid_cards++;
    snprintf(line, sizeof(line), "Deck size: %d", valid_cards);
    DrawText(line, stats_x, 196, 10, RAYWHITE);

    snprintf(line, sizeof(line), "Renown: +%d  Total %d", g_state.result_renown_gained, g_state.meta.renown);
    DrawText(line, stats_x, 212, 10, (Color){ 230, 205, 95, 240 });

    int unlock_y = 226;
    if (g_state.result_achievement_renown > 0)
    {
        snprintf(line, sizeof(line), "Achievements: +%dR", g_state.result_achievement_renown);
        DrawText(line, stats_x, unlock_y, 10, (Color){ 230, 205, 95, 255 });
        unlock_y += 14;
        DrawText(g_state.result_achievement_names, stats_x, unlock_y, 10, (Color){ 170, 220, 255, 230 });
        unlock_y += 14;
    }

    if (g_state.run_won && g_state.meta.max_ascension_unlocked > 0)
    {
        snprintf(line, sizeof(line), "Ascension unlocked: %d", g_state.meta.max_ascension_unlocked);
        DrawText(line, stats_x, unlock_y, 10, (Color){ 185, 150, 255, 240 });
        unlock_y += 14;
    }

    if (g_state.result_unlocked_area >= 0)
    {
        const AreaDef *next = area_def(g_state.result_unlocked_area);
        snprintf(line, sizeof(line), "Unlocked area: %s", next ? next->name : "Next Area");
        DrawText(line, stats_x, unlock_y, 10, (Color){ 120, 220, 255, 245 });
        unlock_y += 14;
    }

    if (g_state.result_unlocked_party_size > 0)
    {
        snprintf(line, sizeof(line), "Unlocked %d-party drafts!", g_state.result_unlocked_party_size);
        DrawText(line, stats_x, unlock_y, 10, (Color){ 230, 205, 95, 255 });
        unlock_y += 14;
    }

    int y = unlock_y + 4;
    DrawText("Final party", stats_x, y, 10, (Color){ 160, 160, 190, 220 });
    y += 16;

    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        snprintf(line, sizeof(line), "%s  %d/%d HP%s", pm->name, pm->hp, pm->max_hp, pm->alive ? "" : "  DOWNED");
        Color c = pm->alive ? RAYWHITE : (Color){ 230, 90, 90, 230 };
        DrawText(line, stats_x, y, 10, c);
        y += 14;
    }

    const char *hint = "Click to return to title";
    DrawText(hint, (VIRT_W / 2) - MeasureText(hint, 10) / 2, VIRT_H - 28, 10, (Color){ 160, 160, 190, 220 });
}


