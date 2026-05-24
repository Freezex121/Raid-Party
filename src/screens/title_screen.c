#include "screens.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "game.h"
#include "data/area_defs.h"
#include "util/tween.h"
#include "util/text.h"
#include "util/math_utils.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

static Button start_btn;
static Button shop_btn;
static Button codex_btn;
static Button prev_area_btn;
static Button next_area_btn;
static Button asc_down_btn;
static Button asc_up_btn;
static float title_y = -24.0f;
static float subtitle_alpha = 0.0f;
static float button_y = 0.0f;
static int title_tween, subtitle_tween, btn_tween;

static Rectangle area_panel_rect(void)
{
    return (Rectangle){ 150.0f, 90.0f, 340.0f, 92.0f };
}

static const char *ascension_effect_text(int level)
{
    switch (level)
    {
        case 1: return "A1: enemies deal +10% damage";
        case 2: return "A2: start combats with -1 energy";
        case 3: return "A3: enemies have +15% HP";
        case 4: return "A4: enemy casts are 1 turn faster";
        case 5: return "A5: add 1 Doubt curse";
        case 6: return "A6: enemies deal another +15% damage";
        case 7: return "A7: bosses gain a bonus ability";
        case 8: return "A8: draw 1 fewer card each turn";
        case 9: return "A9: add a second Doubt curse";
        case 10: return "A10: enemies deal another +25% damage";
        default: return "A0: no ascension modifiers";
    }
}

void title_screen_update(void)
{
    static bool initialized = false;
    if (!initialized)
    {
        start_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 - 72), button_y, 144, (float)BTN_H },
            "START AREA",
            (Color){ 46, 117, 182, 255 },
            (Color){ 80, 160, 230, 255 },
            WHITE
        );
        shop_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 - 118), button_y + 28.0f, 112, (float)BTN_H },
            "META SHOP",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        codex_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 + 6), button_y + 28.0f, 112, (float)BTN_H },
            "COLLECTIVE",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        prev_area_btn = button_create(
            (Rectangle){ 112.0f, 158.0f, 24.0f, 24.0f },
            "<",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        next_area_btn = button_create(
            (Rectangle){ 504.0f, 158.0f, 24.0f, 24.0f },
            ">",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        asc_down_btn = button_create(
            (Rectangle){ 214.0f, 206.0f, 24.0f, 20.0f },
            "-",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        asc_up_btn = button_create(
            (Rectangle){ 402.0f, 206.0f, 24.0f, 20.0f },
            "+",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );

        title_tween = tween_create(&title_y, 20.0f, 0.6f, EASE_OUT_BACK);
        subtitle_tween = tween_create(&subtitle_alpha, 1.0f, 0.5f, EASE_OUT_QUAD);
        btn_tween = tween_create(&button_y, 284.0f, 0.6f, EASE_OUT_ELASTIC);

        initialized = true;
    }

    int count = area_defs_count();
    if (count <= 0) count = 1;
    if (g_state.selected_area < 0) g_state.selected_area = 0;
    if (g_state.selected_area >= count) g_state.selected_area = count - 1;

    start_btn.bounds.y = button_y;
    shop_btn.bounds.y = button_y + 28.0f;
    codex_btn.bounds.y = button_y + 28.0f;

    bool selected_unlocked = meta_area_unlocked(&g_state.meta, g_state.selected_area);
    if (selected_unlocked)
        button_update(&start_btn);
    else
        start_btn.pressed_this_frame = false;
    button_update(&shop_btn);
    button_update(&codex_btn);
    button_update(&prev_area_btn);
    button_update(&next_area_btn);
    if (g_state.meta.max_ascension_unlocked > 0)
    {
        button_update(&asc_down_btn);
        button_update(&asc_up_btn);
    }

    if (prev_area_btn.pressed_this_frame && g_state.selected_area > 0)
        g_state.selected_area--;
    if (next_area_btn.pressed_this_frame && g_state.selected_area < count - 1)
        g_state.selected_area++;
    if (asc_down_btn.pressed_this_frame && g_state.meta.ascension_level > 0)
    {
        g_state.meta.ascension_level--;
        meta_save(&g_state.meta);
    }
    if (asc_up_btn.pressed_this_frame && g_state.meta.ascension_level < g_state.meta.max_ascension_unlocked)
    {
        g_state.meta.ascension_level++;
        meta_save(&g_state.meta);
    }

    if (start_btn.pressed_this_frame)
    {
        initialized = false;
        g_state.current_area = g_state.selected_area;
        g_state.selected_count = 0;
        game_change_screen(SCREEN_DRAFT);
    }
    else if (shop_btn.pressed_this_frame)
    {
        initialized = false;
        game_change_screen(SCREEN_META_SHOP);
    }
    else if (codex_btn.pressed_this_frame)
    {
        initialized = false;
        game_change_screen(SCREEN_CODEX);
    }
}

void title_screen_draw(void)
{
    theme_draw_background();

    int unlocked_class_count = 0;
    for (int i = 0; i < CLASS_COUNT; i++)
    {
        if (meta_class_unlocked(&g_state.meta, i))
            unlocked_class_count++;
    }

    int unlocked_idx = 0;
    for (int i = 0; i < CLASS_COUNT; i++)
    {
        if (!meta_class_unlocked(&g_state.meta, i))
            continue;

        int x = VIRT_W / 2 - (unlocked_class_count * 22) + unlocked_idx * 50;
        int y = 250 + (unlocked_idx % 2) * 10;
        theme_draw_class_portrait((ClassType)i, x, y, 14, true);
        unlocked_idx++;
    }

    game_draw_text("RAID PARTY", VIRT_W/2 - game_measure_text("RAID PARTY", 40) / 2, snap_i(title_y), 40, RAYWHITE);

    Color subtitle_color = { 180, 180, 200, (unsigned char)(subtitle_alpha * 180) };
    game_draw_text("A deck-building roguelite MMO", VIRT_W/2 - game_measure_text("A deck-building roguelite MMO", 24) / 2, snap_i(title_y + 38), 24, subtitle_color);

    Rectangle panel = area_panel_rect();
    const AreaDef *area = area_def(g_state.selected_area);
    bool selected_unlocked = meta_area_unlocked(&g_state.meta, g_state.selected_area);
    DrawRectangleRec(panel, (Color){ 10, 12, 22, 220 });
    DrawRectangleLinesEx(panel, 1.0f, selected_unlocked ? (Color){ 95, 150, 210, 200 } : (Color){ 80, 82, 98, 180 });

    int area_count = area_defs_count();
    if (area_count <= 0) area_count = 1;
    char area_num[48];
    snprintf(area_num, sizeof(area_num), "AREA %d/%d", g_state.selected_area + 1, area_count);
    game_draw_text(area_num, snap_i(panel.x + 10), snap_i(panel.y + 8), 10, (Color){ 145, 155, 190, 220 });

    const char *area_name = area ? area->name : "Unknown Area";
    Color name_col = selected_unlocked ? RAYWHITE : (Color){ 130, 132, 150, 230 };
    game_draw_text(area_name, (int)panel.x + 10, (int)panel.y + 20, 10, name_col);

    if (area)
        draw_text_wrapped(area->description, (int)panel.x + 10, (int)panel.y + 36, (int)panel.width - 25, 10, 0, (Color){ 170, 176, 205, 220 });

    char area_stats[96];
    snprintf(area_stats, sizeof(area_stats), "%d floors  Difficulty %d%%  %s",
        area ? area->floor_count : 3,
        area ? area->difficulty_percent : 100,
        selected_unlocked ? "Unlocked" : "Locked");
    Color stat_col = selected_unlocked ? (Color){ 230, 205, 95, 235 } : (Color){ 125, 128, 145, 220 };
    game_draw_text(area_stats, (int)panel.x + 10, (int)panel.y + 74, 10, stat_col);

    if (g_state.selected_area > 0)
        button_draw(&prev_area_btn);
    if (g_state.selected_area < area_count - 1)
        button_draw(&next_area_btn);

    char meta[128];
    snprintf(meta, sizeof(meta), "Runs: %d      |       Wins: %d        |       Renown: %d      |       Max Party: %d",
        g_state.meta.runs_completed,
        g_state.meta.wins,
        g_state.meta.renown,
        g_state.max_party_size);
    game_draw_text(meta, VIRT_W / 2 - game_measure_text(meta, 18) / 2, snap_i(panel.y + panel.height + 10), 18, (Color){ 150, 155, 180, 200 });

    char asc[96];
    snprintf(asc, sizeof(asc), "ASCENSION %d / %d", g_state.meta.ascension_level, g_state.meta.max_ascension_unlocked);
    DrawText(asc, VIRT_W / 2 - MeasureText(asc, 10) / 2, 220, 10,
        g_state.meta.max_ascension_unlocked > 0 ? (Color){ 190, 160, 255, 220 } : (Color){ 110, 112, 135, 180 });
    if (g_state.meta.max_ascension_unlocked > 0)
    {
        button_draw(&asc_down_btn);
        button_draw(&asc_up_btn);
    }

    Rectangle asc_panel = { 122.0f, 232.0f, 396.0f, 38.0f };
    DrawRectangleRec(asc_panel, (Color){ 12, 10, 22, 210 });
    DrawRectangleLinesEx(asc_panel, 1.0f, (Color){ 160, 120, 230, 165 });
    DrawText("Ascensions are additive: A5 includes A1-A5.", 132, 237, 10, (Color){ 205, 185, 245, 230 });
    char asc_effect[128];
    snprintf(asc_effect, sizeof(asc_effect), "%s", ascension_effect_text(g_state.meta.ascension_level));
    DrawText(asc_effect, 132, 250, 10, (Color){ 230, 225, 245, 235 });
    draw_text_wrapped("A1 dmg A2 energy A3 HP A4 casts A5/A9 Doubt A7 boss A8 draw A10 dmg",
        132, 262, 376, 10, 1, (Color){ 145, 150, 175, 215 });

    if (selected_unlocked)
    {
        button_draw(&start_btn);
    }
    else
    {
        Rectangle r = start_btn.bounds;
        DrawRectangleRec(r, (Color){ 35, 36, 48, 220 });
        DrawRectangleLinesEx(r, 1.0f, (Color){ 75, 78, 95, 180 });
        game_draw_text("LOCKED", snap_i(r.x + r.width / 2 - game_measure_text("LOCKED", 10) / 2), snap_i(r.y) + 7, 10, (Color){ 110, 113, 130, 230 });
    }
    button_draw(&shop_btn);
    button_draw(&codex_btn);

    Color credit_color = { 100, 100, 120, 180 };
    game_draw_text("devlog v0.1", VIRT_W/2 - game_measure_text("devlog v0.1", 10) / 2, VIRT_H - 20, 10, credit_color);
}
