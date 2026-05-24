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
static Button prev_area_btn;
static Button next_area_btn;
static float title_y = -24.0f;
static float subtitle_alpha = 0.0f;
static float button_y = 0.0f;
static int title_tween, subtitle_tween, btn_tween;

static Rectangle area_panel_rect(void)
{
    return (Rectangle){ 150.0f, 90.0f, 340.0f, 92.0f };
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
            (Rectangle){ (float)(VIRT_W / 2 - 72), button_y + 28.0f, 144, (float)BTN_H },
            "META SHOP",
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

        title_tween = tween_create(&title_y, 20.0f, 0.6f, EASE_OUT_BACK);
        subtitle_tween = tween_create(&subtitle_alpha, 1.0f, 0.5f, EASE_OUT_QUAD);
        btn_tween = tween_create(&button_y, 264.0f, 0.6f, EASE_OUT_ELASTIC);

        initialized = true;
    }

    int count = area_defs_count();
    if (count <= 0) count = 1;
    if (g_state.selected_area < 0) g_state.selected_area = 0;
    if (g_state.selected_area >= count) g_state.selected_area = count - 1;

    start_btn.bounds.y = button_y;
    shop_btn.bounds.y = button_y + 28.0f;

    bool selected_unlocked = meta_area_unlocked(&g_state.meta, g_state.selected_area);
    if (selected_unlocked)
        button_update(&start_btn);
    else
        start_btn.pressed_this_frame = false;
    button_update(&shop_btn);
    button_update(&prev_area_btn);
    button_update(&next_area_btn);

    if (prev_area_btn.pressed_this_frame && g_state.selected_area > 0)
        g_state.selected_area--;
    if (next_area_btn.pressed_this_frame && g_state.selected_area < count - 1)
        g_state.selected_area++;

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
}

void title_screen_draw(void)
{
    theme_draw_background();

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        int x = VIRT_W / 2 - 150 + i * 60;
        int y = 232 + (i % 2) * 7;
        theme_draw_class_portrait((ClassType)(i % CLASS_COUNT), x, y, 14, true);
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

    Color credit_color = { 100, 100, 120, 180 };
    game_draw_text("devlog v0.1", VIRT_W/2 - game_measure_text("devlog v0.1", 10) / 2, VIRT_H - 20, 10, credit_color);
}



