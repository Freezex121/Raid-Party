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
static Button ach_btn;
static Button settings_btn;
static Button prev_area_btn;
static Button next_area_btn;
static Button asc_down_btn;
static Button asc_up_btn;
static Button telemetry_allow_btn;
static Button telemetry_decline_btn;
static float title_y = -24.0f;
static float subtitle_alpha = 0.0f;
static float button_y = 0.0f;
static int title_tween, subtitle_tween, btn_tween;

static Rectangle area_panel_rect(void)
{
    return (Rectangle){ 126.0f, 81.0f, 388.0f, 134.0f };
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
            (Rectangle){ (float)(VIRT_W / 2 - BTN_WIDE / 2), button_y, BTN_WIDE, (float)BTN_H },
            "START AREA",
            (Color){ 46, 117, 182, 255 },
            (Color){ 80, 160, 230, 255 },
            WHITE
        );
        shop_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 - BTN_MED - 8), button_y + 28.0f, BTN_MED, (float)BTN_H },
            "META SHOP",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        codex_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 + 8), button_y + 28.0f, BTN_MED, (float)BTN_H },
            "COLLECTIVE",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        ach_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 - BTN_MED - 8), button_y + 56.0f, BTN_MED, (float)BTN_H },
            "ACHIEVEMENTS",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        settings_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 + 8), button_y + 56.0f, BTN_MED, (float)BTN_H },
            "SETTINGS",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        Rectangle area_panel = area_panel_rect();
        prev_area_btn = button_create(
            (Rectangle){ area_panel.x - BTN_ICON - 8.0f, area_panel.y + 55.0f, BTN_ICON, (float)BTN_H },
            "<",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        next_area_btn = button_create(
            (Rectangle){ area_panel.x + area_panel.width + 8.0f, area_panel.y + 55.0f, BTN_ICON, (float)BTN_H },
            ">",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        asc_down_btn = button_create(
            (Rectangle){ area_panel.x + 94.0f, area_panel.y + 96.0f, BTN_ICON, (float)BTN_H },
            "-",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        asc_up_btn = button_create(
            (Rectangle){ area_panel.x + area_panel.width - 116.0f, area_panel.y + 96.0f, BTN_ICON, (float)BTN_H },
            "+",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );
        telemetry_allow_btn = button_create(
            (Rectangle){ 222.0f, 236.0f, BTN_MED, (float)BTN_H },
            "ALLOW",
            (Color){ 46, 117, 182, 255 },
            (Color){ 80, 160, 230, 255 },
            WHITE
        );
        telemetry_decline_btn = button_create(
            (Rectangle){ 222.0f + BTN_MED + 10.0f, 236.0f, BTN_MED, (float)BTN_H },
            "NO THANKS",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
            WHITE
        );

        title_tween = tween_create(&title_y, 15.0f, 0.6f, EASE_OUT_BACK);
        subtitle_tween = tween_create(&subtitle_alpha, 1.0f, 0.5f, EASE_OUT_QUAD);
        btn_tween = tween_create(&button_y, 279.0f, 0.6f, EASE_OUT_ELASTIC);

        initialized = true;
    }

    int count = area_defs_count();
    if (count <= 0) count = 1;
    if (g_state.selected_area < 0) g_state.selected_area = 0;
    if (g_state.selected_area >= count) g_state.selected_area = count - 1;

    start_btn.bounds.y = button_y;
    shop_btn.bounds.y = button_y + 28.0f;
    codex_btn.bounds.y = button_y + 28.0f;
    ach_btn.bounds.y = button_y + 56.0f;
    settings_btn.bounds.y = button_y + 56.0f;

    if (!g_state.telemetry_prompt_seen)
    {
        button_update(&telemetry_allow_btn);
        button_update(&telemetry_decline_btn);
        if (telemetry_allow_btn.pressed_this_frame || telemetry_decline_btn.pressed_this_frame)
        {
            g_state.telemetry_opt_in = telemetry_allow_btn.pressed_this_frame;
            g_state.telemetry_prompt_seen = true;
            game_settings_save();
        }
        return;
    }

    if (!g_state.tutorial_active && g_state.meta.runs_completed > 0)
        game_start_tutorial_once(&g_state.meta.tutorial_seen_meta_shop, TUTORIAL_STEP_META_SHOP);
    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_META_SHOP)
    {
        game_tutorial_handle_close();
        return;
    }

    bool selected_unlocked = meta_area_unlocked(&g_state.meta, g_state.selected_area);
    if (selected_unlocked)
        button_update(&start_btn);
    else
        start_btn.pressed_this_frame = false;
    button_update(&shop_btn);
    button_update(&codex_btn);
    button_update(&ach_btn);
    button_update(&settings_btn);
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
    else if (ach_btn.pressed_this_frame)
    {
        initialized = false;
        game_change_screen(SCREEN_ACHIEVEMENTS);
    }
    else if (settings_btn.pressed_this_frame)
    {
        initialized = false;
        game_open_settings(SCREEN_TITLE);
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
        int y = 255;
        theme_draw_class_portrait((ClassType)i, x, y, 14, true);
        unlocked_idx++;
    }

    game_draw_text("RAID PARTY", VIRT_W/2 - game_measure_text("RAID PARTY", 40) / 2, snap_i(title_y), 40, RAYWHITE);

    Color subtitle_color = { 180, 180, 200, (unsigned char)(subtitle_alpha * 180) };
    game_draw_text("A deck-building roguelite MMO", VIRT_W/2 - game_measure_text("A deck-building roguelite MMO", 24) / 2, snap_i(title_y + 33), 24, subtitle_color);

    Rectangle panel = area_panel_rect();
    const AreaDef *area = area_def(g_state.selected_area);
    bool selected_unlocked = meta_area_unlocked(&g_state.meta, g_state.selected_area);
    DrawRectangleRec(panel, (Color){ 10, 12, 22, 220 });
    DrawRectangleLinesEx(panel, 1.0f, selected_unlocked ? (Color){ 95, 150, 210, 200 } : (Color){ 80, 82, 98, 180 });

    int area_count = area_defs_count();
    if (area_count <= 0) area_count = 1;
    char area_num[48];
    snprintf(area_num, sizeof(area_num), "AREA %d/%d", g_state.selected_area + 1, area_count);
    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 8.0f, panel.width - 20.0f, 12.0f },
        area_num, 10, 0, (Color){ 145, 155, 190, 220 }, TEXT_ALIGN_LEFT);

    const char *area_name = area ? area->name : "Unknown Area";
    Color name_col = selected_unlocked ? RAYWHITE : (Color){ 130, 132, 150, 230 };
    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 20.0f, panel.width - 20.0f, 14.0f },
        area_name, 10, 0, name_col, TEXT_ALIGN_LEFT);

    if (area)
        draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 36.0f, panel.width - 25.0f, 34.0f },
            area->description, 10, 0, (Color){ 170, 176, 205, 220 }, TEXT_ALIGN_LEFT);

    char area_stats[96];
    snprintf(area_stats, sizeof(area_stats), "%d floors  Difficulty %d%%  %s",
        area ? area->floor_count : 3,
        area ? area->difficulty_percent : 100,
        selected_unlocked ? "Unlocked" : "Locked");
    Color stat_col = selected_unlocked ? (Color){ 230, 205, 95, 235 } : (Color){ 125, 128, 145, 220 };
    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 74.0f, panel.width - 20.0f, 14.0f },
        area_stats, 10, 0, stat_col, TEXT_ALIGN_LEFT);

    DrawRectangle((int)panel.x + 10, (int)panel.y + 91, (int)panel.width - 20, 1, (Color){ 60, 65, 92, 190 });

    char asc[96];
    snprintf(asc, sizeof(asc), "ASCENSION %d / %d", g_state.meta.ascension_level, g_state.meta.max_ascension_unlocked);
    Color asc_col = g_state.meta.max_ascension_unlocked > 0 ? (Color){ 190, 160, 255, 230 } : (Color){ 110, 112, 135, 180 };
    draw_text_box((Rectangle){ panel.x + 124.0f, panel.y + 96.0f, panel.width - 248.0f, 14.0f },
        asc, 10, 0, asc_col, TEXT_ALIGN_CENTER);
    char asc_effect[128];
    snprintf(asc_effect, sizeof(asc_effect), "%s", ascension_effect_text(g_state.meta.ascension_level));
    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 120.0f, panel.width - 20.0f, 12.0f },
        asc_effect, 10, 0, (Color){ 230, 225, 245, 235 }, TEXT_ALIGN_CENTER);

    if (g_state.meta.max_ascension_unlocked > 0)
    {
        button_draw_9slice(&asc_down_btn);
        button_draw_9slice(&asc_up_btn);
    }

    if (g_state.selected_area > 0)
        button_draw_9slice(&prev_area_btn);
    if (g_state.selected_area < area_count - 1)
        button_draw_9slice(&next_area_btn);

    char meta[128];
    snprintf(meta, sizeof(meta), "Runs: %d      |       Wins: %d        |       Renown: %d      |       Max Party: %d",
        g_state.meta.runs_completed,
        g_state.meta.wins,
        g_state.meta.renown,
        g_state.max_party_size);
    draw_text_box((Rectangle){ 70.0f, panel.y + panel.height + 6.0f, 500.0f, 14.0f },
        meta, 10, 0, (Color){ 150, 155, 180, 200 }, TEXT_ALIGN_CENTER);

    if (selected_unlocked)
    {
        button_draw_9slice(&start_btn);
    }
    else
    {
        Rectangle r = start_btn.bounds;
        draw_9slice(g_assets.btn_standard, 6, 6, r, (Color){ 35, 36, 48, 220 });
        game_draw_text("LOCKED", snap_i(r.x + r.width / 2 - game_measure_text("LOCKED", 10) / 2), snap_i(r.y) + 7, 10, (Color){ 110, 113, 130, 230 });
    }
    button_draw_9slice(&shop_btn);
    button_draw_9slice(&codex_btn);
    button_draw_9slice(&ach_btn);
    button_draw_9slice(&settings_btn);

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_META_SHOP)
    {
        game_draw_tutorial_overlay_ex(shop_btn.bounds,
            "Meta Shop",
            "Runs earn renown. Spend it here on permanent unlocks like party slots, classes, starting gold, and opening-run bonuses.",
            "Click to continue  |  Right-click/Esc: skip",
            0,
            0);
    }

    if (!g_state.telemetry_prompt_seen)
    {
        DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){ 0, 0, 0, 145 });
        Rectangle modal = { 166.0f, 106.0f, 308.0f, 160.0f };
        DrawRectangleRec(modal, (Color){ 12, 14, 24, 245 });
        DrawRectangleLinesEx(modal, 1.0f, (Color){ 95, 150, 210, 220 });
        draw_text_box((Rectangle){ modal.x + 14.0f, modal.y + 12.0f, modal.width - 28.0f, 22.0f },
            "Help improve Raid Paper Legends?", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ modal.x + 16.0f, modal.y + 42.0f, modal.width - 32.0f, 70.0f },
            "Send anonymous gameplay metrics like card choices, combat outcomes, shop purchases, and run results. No names, Steam IDs, chat, or personal information are collected.",
            10, 0, (Color){ 190, 196, 220, 235 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ modal.x + 20.0f, modal.y + 116.0f, modal.width - 40.0f, 12.0f },
            "You can change this later in Settings.", 10, 0, (Color){ 145, 155, 190, 220 }, TEXT_ALIGN_CENTER);
        button_draw_9slice(&telemetry_allow_btn);
        button_draw_9slice(&telemetry_decline_btn);
    }

    Color credit_color = { 100, 100, 120, 180 };
    const char *version = "v0.1";
    game_draw_text(version, VIRT_W - game_measure_text(version, 10) / 2 - 25, VIRT_H - 30, 10, credit_color);
}
