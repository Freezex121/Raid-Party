#include "game.h"
#include "assets.h"
#include "data/area_defs.h"
#include "systems/economy_metrics.h"
#include "util/tween.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GameState g_state;

static float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static int clamp_scale(int scale)
{
    if (scale < 1) return 1;
    if (scale > 4) return 4;
    return scale;
}

static void game_settings_load(void)
{
    FILE *f = fopen("settings.cfg", "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        char key[64];
        char value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) != 2)
            continue;
        if (strcmp(key, "window_scale") == 0)
            g_state.window_scale = clamp_scale(atoi(value));
        else if (strcmp(key, "fullscreen") == 0)
            g_state.fullscreen = atoi(value) != 0;
        else if (strcmp(key, "master_volume") == 0)
            g_state.master_volume = clamp01((float)atof(value));
        else if (strcmp(key, "music_volume") == 0)
            g_state.music_volume = clamp01((float)atof(value));
        else if (strcmp(key, "sfx_volume") == 0)
            g_state.sfx_volume = clamp01((float)atof(value));
    }
    fclose(f);
}

void game_init(void)
{
    memset(&g_state, 0, sizeof(GameState));
    meta_load(&g_state.meta);
    g_state.screen = SCREEN_TITLE;
    g_state.pending_screen = SCREEN_TITLE;
    g_state.settings_return_screen = SCREEN_TITLE;
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    g_state.selected_area = area_clamp_index(g_state.meta.highest_area_unlocked);
    g_state.current_area = g_state.selected_area;
    g_state.window_scale = SCALE;
    g_state.fullscreen = false;
    g_state.master_volume = 1.0f;
    g_state.music_volume = 1.0f;
    g_state.sfx_volume = 1.0f;
    game_settings_load();
    game_set_window_scale(g_state.window_scale);
    game_set_fullscreen(g_state.fullscreen);
    game_set_master_volume(g_state.master_volume);
    game_set_music_volume(g_state.music_volume);
    game_set_sfx_volume(g_state.sfx_volume);
    g_state.result_area = g_state.current_area;
    g_state.result_unlocked_area = -1;
    g_state.result_floor = 1;
}

void game_change_screen(GameScreen screen)
{
    if (g_state.transition_active && g_state.pending_screen == screen)
        return;

    if (!g_state.transition_active && g_state.screen == screen)
        return;

    g_state.pending_screen = screen;
    g_state.transition_active = true;
    g_state.transition_switched = false;
    g_state.transition_alpha = 0.0f;
    g_state.transition_timer = 0.0f;
    tween_kill_all();
}

void game_update_transition(float dt)
{
    if (!g_state.transition_active) return;

    if (!g_state.transition_switched)
    {
        g_state.transition_timer += dt;
        g_state.transition_alpha = g_state.transition_timer / 0.18f;
        if (g_state.transition_alpha >= 1.0f)
        {
            g_state.transition_alpha = 1.0f;
            g_state.screen = g_state.pending_screen;
            g_state.transition_switched = true;
            g_state.transition_timer = 0.0f;
        }
    }
    else
    {
        g_state.transition_timer += dt;
        g_state.transition_alpha = 1.0f - g_state.transition_timer / 0.24f;
        if (g_state.transition_alpha <= 0.0f)
        {
            g_state.transition_alpha = 0.0f;
            g_state.transition_active = false;
            g_state.transition_switched = false;
        }
    }
}

void game_draw_transition(void)
{
    if (!g_state.transition_active && g_state.transition_alpha <= 0.0f) return;
    unsigned char a = (unsigned char)(g_state.transition_alpha * 255.0f);
    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){ 0, 0, 0, a });
}

bool game_transition_allows_update(void)
{
    return !g_state.transition_active || g_state.transition_switched;
}

void game_draw_gold_overlay(void)
{
    if (!g_state.run_party_active)
        return;
    if (g_state.screen == SCREEN_TITLE ||
        g_state.screen == SCREEN_DRAFT ||
        g_state.screen == SCREEN_META_SHOP ||
        g_state.screen == SCREEN_CODEX ||
        g_state.screen == SCREEN_ACHIEVEMENTS ||
        g_state.screen == SCREEN_SETTINGS)
        return;

    Rectangle r = { 10.0f, 30.0f, 66.0f, 16.0f };
    DrawRectangleRec(r, (Color){ 12, 10, 18, 225 });
    DrawRectangleLinesEx(r, 1.0f, (Color){ 220, 185, 70, 190 });

    char text[32];
    snprintf(text, sizeof(text), "Gold %d", g_state.gold);
    draw_text_box((Rectangle){ r.x + 4.0f, r.y + 3.0f, r.width - 8.0f, 11.0f },
        text, 10, 0, (Color){ 245, 218, 105, 245 }, TEXT_ALIGN_CENTER);
}

void game_gain_gold(int amount, const char *context)
{
    if (amount <= 0) return;
    int before = g_state.gold;
    g_state.gold += amount;
    economy_metrics_log("gain", amount, before, g_state.gold, context);
}

bool game_spend_gold(int amount, const char *context)
{
    if (amount <= 0) return true;
    if (g_state.gold < amount) return false;
    int before = g_state.gold;
    g_state.gold -= amount;
    economy_metrics_log("spend", -amount, before, g_state.gold, context);
    return true;
}

void game_log_gold_conversion(int gold_amount, int renown_amount)
{
    economy_metrics_log("convert_gold_to_renown", renown_amount, gold_amount, gold_amount, "game_over");
}

void game_open_settings(GameScreen return_screen)
{
    if (return_screen == SCREEN_SETTINGS)
        return_screen = SCREEN_TITLE;
    g_state.settings_return_screen = return_screen;
    game_change_screen(SCREEN_SETTINGS);
}

void game_set_window_scale(int scale)
{
    g_state.window_scale = clamp_scale(scale);
    if (!IsWindowFullscreen())
        SetWindowSize(VIRT_W * g_state.window_scale, VIRT_H * g_state.window_scale);
}

void game_set_fullscreen(bool fullscreen)
{
    g_state.fullscreen = fullscreen;

    if (fullscreen && !IsWindowFullscreen())
    {
        int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        ToggleFullscreen();
    }
    else if (!fullscreen && IsWindowFullscreen())
    {
        ToggleFullscreen();
        SetWindowSize(VIRT_W * g_state.window_scale, VIRT_H * g_state.window_scale);
    }
    else if (!fullscreen)
    {
        SetWindowSize(VIRT_W * g_state.window_scale, VIRT_H * g_state.window_scale);
    }
}

void game_set_master_volume(float volume)
{
    g_state.master_volume = clamp01(volume);
    SetMasterVolume(g_state.master_volume);
}

void game_set_music_volume(float volume)
{
    g_state.music_volume = clamp01(volume);
    assets_set_music_volume(g_state.music_volume);
}

void game_set_sfx_volume(float volume)
{
    g_state.sfx_volume = clamp01(volume);
    assets_set_sfx_volume(g_state.sfx_volume);
}

Rectangle game_render_destination(void)
{
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int scale_x = screen_w / VIRT_W;
    int scale_y = screen_h / VIRT_H;
    int scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1) scale = 1;

    int draw_w = VIRT_W * scale;
    int draw_h = VIRT_H * scale;
    return (Rectangle){
        (float)((screen_w - draw_w) / 2),
        (float)((screen_h - draw_h) / 2),
        (float)draw_w,
        (float)draw_h
    };
}

void game_update_mouse_transform(void)
{
    Rectangle dst = game_render_destination();
    SetMouseOffset(-(int)dst.x, -(int)dst.y);
    SetMouseScale((float)VIRT_W / dst.width, (float)VIRT_H / dst.height);
}

void game_settings_save(void)
{
    FILE *f = fopen("settings.cfg", "w");
    if (!f) return;
    fprintf(f, "window_scale=%d\n", g_state.window_scale);
    fprintf(f, "fullscreen=%d\n", g_state.fullscreen ? 1 : 0);
    fprintf(f, "master_volume=%.3f\n", g_state.master_volume);
    fprintf(f, "music_volume=%.3f\n", g_state.music_volume);
    fprintf(f, "sfx_volume=%.3f\n", g_state.sfx_volume);
    fclose(f);
}
