#include "screens.h"
#include "game.h"
#include "data/area_defs.h"
#include "systems/telemetry.h"
#include "ui/theme.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

static int final_deck_size(void)
{
    int valid_cards = 0;
    for (int i = 0; i < g_state.run_deck.card_count; i++)
        if (g_state.run_deck.cards[i].def)
            valid_cards++;
    return valid_cards;
}

static void log_run_summary_metric(void)
{
    char run_id[16], won[8], area[16], floor[16], bosses[16], deaths[16], interrupts[16], ascension[16];
    char renown[16], gold[16], deck_size[16], relic_count[16];
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(won, sizeof(won), "%d", g_state.run_won ? 1 : 0);
    snprintf(area, sizeof(area), "%d", g_state.result_area);
    snprintf(floor, sizeof(floor), "%d", g_state.result_floor);
    snprintf(bosses, sizeof(bosses), "%d", g_state.result_bosses_defeated);
    snprintf(deaths, sizeof(deaths), "%d", g_state.run_deaths);
    snprintf(interrupts, sizeof(interrupts), "%d", g_state.run_interrupts);
    snprintf(ascension, sizeof(ascension), "%d", g_state.meta.ascension_level);
    snprintf(renown, sizeof(renown), "%d", g_state.result_renown_gained + g_state.result_achievement_renown + g_state.result_gold_renown);
    snprintf(gold, sizeof(gold), "%d", g_state.gold);
    snprintf(deck_size, sizeof(deck_size), "%d", final_deck_size());
    snprintf(relic_count, sizeof(relic_count), "%d", g_state.relic_count);
    const char *fields[] = { run_id, won, area, floor, bosses, deaths, interrupts, ascension, renown, gold, deck_size, relic_count };
    telemetry_csv_append(
        "run_metrics.csv",
        "timestamp,run_id,won,area,floors_cleared,bosses_defeated,deaths,interrupts,ascension,renown_gained,gold_earned,deck_size,relic_count",
        fields, 12);

    char json[512];
    snprintf(json, sizeof(json),
        "\"won\":%s,\"area\":%d,\"floors_cleared\":%d,\"bosses_defeated\":%d,\"deaths\":%d,\"interrupts\":%d,\"ascension\":%d,\"renown_gained\":%d,\"gold_earned\":%d,\"deck_size\":%d,\"relic_count\":%d",
        g_state.run_won ? "true" : "false",
        g_state.result_area, g_state.result_floor, g_state.result_bosses_defeated,
        g_state.run_deaths, g_state.run_interrupts, g_state.meta.ascension_level,
        g_state.result_renown_gained + g_state.result_achievement_renown + g_state.result_gold_renown,
        g_state.gold, final_deck_size(), g_state.relic_count);
    telemetry_push_json("run_summary", json);
    telemetry_flush_upload();
}

void game_over_screen_update(void)
{
    if (!g_state.result_recorded)
    {
        int before_slots = meta_party_slots(&g_state.meta);
        int before_area = g_state.meta.highest_area_unlocked;
        g_state.result_renown_gained = meta_record_run(
            &g_state.meta, g_state.run_won, g_state.result_area, g_state.result_floor,
            g_state.result_bosses_defeated, g_state.run_party.count, g_state.selected_classes,
            g_state.run_deaths, g_state.relic_count, g_state.run_interrupts,
            g_state.run_best_combat_turns, area_defs_count(),
            &g_state.result_achievement_renown, g_state.result_achievement_names,
            sizeof(g_state.result_achievement_names));
        g_state.result_gold_renown = g_state.gold / 50;
        if (g_state.result_gold_renown > 0)
        {
            g_state.meta.renown += g_state.result_gold_renown;
            game_log_gold_conversion(g_state.gold, g_state.result_gold_renown);
        }
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
        log_run_summary_metric();
    }

    if (!g_state.tutorial_active)
        game_start_tutorial_once(&g_state.meta.tutorial_seen_game_over, TUTORIAL_STEP_GAME_OVER);
    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_GAME_OVER)
    {
        if (game_tutorial_handle_close())
            return;
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
    draw_text_box((Rectangle){ 80.0f, 44.0f, 480.0f, 18.0f }, title, 18, 0, title_col, TEXT_ALIGN_CENTER);

    const char *reason = g_state.result_reason[0] ? g_state.result_reason :
        (g_state.run_won ? "The raid is complete." : "The run has ended.");
    draw_text_box((Rectangle){ 100.0f, 66.0f, 440.0f, 12.0f }, reason, 10, 0, (Color){ 190, 190, 215, 230 }, TEXT_ALIGN_CENTER);

    const AreaDef *area = area_def(g_state.result_area);
    int area_floors = area_floor_count(g_state.result_area);
    char line[128];

    // ── Compact single-line header stats ─────────────────────
    int hx = 60;
    int hy = 84;
    snprintf(line, sizeof(line), "%s  |  Floor %d/%d  |  Asc %d",
        area ? area->name : "Unknown", g_state.result_floor, area_floors, g_state.meta.ascension_level);
    DrawText(line, hx, hy, 10, RAYWHITE);

    // ── Left column: Stats (starts at y=104) ────────────────
    int lx = 60;
    int ly = 104;
    int ls = 12;
    Color dim = (Color){ 200, 205, 225, 240 };
    Color gold_c = (Color){ 230, 200, 80, 255 };
    Color ren_c = (Color){ 230, 205, 95, 240 };

    snprintf(line, sizeof(line), "Gold: %d", g_state.gold);
    DrawText(line, lx, ly, 10, gold_c); ly += ls;

    snprintf(line, sizeof(line), "Bosses defeated: %d", g_state.result_bosses_defeated);
    DrawText(line, lx, ly, 10, dim); ly += ls;

    int valid_cards = final_deck_size();
    snprintf(line, sizeof(line), "Deck: %d  |  Relics: %d", valid_cards, g_state.relic_count);
    DrawText(line, lx, ly, 10, dim); ly += ls;

    snprintf(line, sizeof(line), "Deaths: %d  |  Interrupts: %d", g_state.run_deaths, g_state.run_interrupts);
    DrawText(line, lx, ly, 10, dim); ly += ls + 2;

    // Renown section header
    DrawText("RENOWN", lx, ly, 10, ren_c); ly += ls;

    int ren_total = g_state.result_renown_gained + g_state.result_gold_renown + g_state.result_achievement_renown;
    snprintf(line, sizeof(line), "Run: +%d", g_state.result_renown_gained);
    DrawText(line, lx, ly, 10, ren_c); ly += ls;

    if (g_state.result_gold_renown > 0)
    {
        snprintf(line, sizeof(line), "Gold: +%d", g_state.result_gold_renown);
        DrawText(line, lx, ly, 10, ren_c); ly += ls;
    }
    if (g_state.result_achievement_renown > 0)
    {
        snprintf(line, sizeof(line), "Achievements: +%d", g_state.result_achievement_renown);
        DrawText(line, lx, ly, 10, ren_c); ly += ls;
    }

    snprintf(line, sizeof(line), "Total: %d", g_state.meta.renown);
    DrawText(line, lx, ly, 10, ren_c); ly += ls;

    // ── Right column: Unlocks + Party (starts at y=104) ─────
    int rx = 340;
    int ry = 104;

    // Party members first (top right)
    DrawText("PARTY", rx, ry, 10, (Color){ 160, 160, 190, 220 }); ry += ls;
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        snprintf(line, sizeof(line), "%s  %d/%d  Lv%d%s",
            pm->name, pm->hp, pm->max_hp, pm->level,
            pm->alive ? "" : "  DOWN");
        Color c = pm->alive ? RAYWHITE : (Color){ 230, 90, 90, 230 };
        DrawText(line, rx, ry, 10, c);
        ry += ls;
    }

    ry += 2;

    // Unlocks
    bool asc_unlocked = (g_state.run_won && g_state.meta.max_ascension_unlocked > 0);
    bool area_unlocked = g_state.result_unlocked_area >= 0;
    bool party_unlocked = g_state.result_unlocked_party_size > 0;

    if (asc_unlocked || area_unlocked || party_unlocked || g_state.result_achievement_renown > 0)
    {
        DrawText("UNLOCKS", rx, ry, 10, (Color){ 170, 220, 255, 220 }); ry += ls;

        if (asc_unlocked)
        {
            snprintf(line, sizeof(line), "Ascension %d unlocked!", g_state.meta.max_ascension_unlocked);
            DrawText(line, rx, ry, 10, (Color){ 185, 150, 255, 240 }); ry += ls;

            if (g_state.meta.ascension_level >= META_ASCENSION_MAX && g_state.run_won)
            {
                DrawText("MAX ASCENSION CONQUERED!", rx, ry, 10, (Color){ 255, 200, 50, 255 }); ry += ls;
            }
        }

        if (area_unlocked)
        {
            const AreaDef *next = area_def(g_state.result_unlocked_area);
            snprintf(line, sizeof(line), "New area: %s", next ? next->name : "???");
            DrawText(line, rx, ry, 10, (Color){ 120, 220, 255, 245 }); ry += ls;
        }

        if (party_unlocked)
        {
            snprintf(line, sizeof(line), "%d-party drafts!", g_state.result_unlocked_party_size);
            DrawText(line, rx, ry, 10, (Color){ 230, 205, 95, 255 }); ry += ls;
        }

        if (g_state.result_achievement_renown > 0)
        {
            draw_text_box((Rectangle){ (float)rx, (float)ry, 240.0f, 40.0f },
                g_state.result_achievement_names, 10, 0, (Color){ 170, 220, 255, 230 }, TEXT_ALIGN_LEFT);
        }
    }

    // ── Hint ─────────────────────────────────────────────────
    const char *hint = "Click to return to title";
    draw_text_box((Rectangle){ 80.0f, (float)(VIRT_H - 22), 480.0f, 12.0f },
        hint, 10, 0, (Color){ 160, 160, 190, 220 }, TEXT_ALIGN_CENTER);

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_GAME_OVER)
    {
        game_draw_tutorial_overlay_ex((Rectangle){ 210.0f, 128.0f, 220.0f, 150.0f },
            "Run Results",
            "Renown is permanent progression currency. Gold can convert into renown here, then you can spend renown in the Meta Shop from the title screen.",
            "Click to continue  |  Right-click/Esc: skip", 0, 0);
    }
}
