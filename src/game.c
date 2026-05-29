#include "game.h"
#include "assets.h"
#include "data/area_defs.h"
#include "systems/economy_metrics.h"
#include "systems/telemetry.h"
#include "ui/ui.h"
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
        else if (strcmp(key, "telemetry_opt_in") == 0)
            g_state.telemetry_opt_in = atoi(value) != 0;
        else if (strcmp(key, "telemetry_prompt_seen") == 0)
            g_state.telemetry_prompt_seen = atoi(value) != 0;
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
    g_state.post_combat_destination = SCREEN_MAP;
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    g_state.selected_area = area_clamp_index(g_state.meta.highest_area_unlocked);
    g_state.current_area = g_state.selected_area;
    g_state.window_scale = SCALE;
    g_state.fullscreen = false;
    g_state.telemetry_opt_in = false;
    g_state.telemetry_prompt_seen = false;
    g_state.master_volume = 1.0f;
    g_state.music_volume = 1.0f;
    g_state.sfx_volume = 1.0f;
    game_settings_load();
    game_set_window_scale(g_state.window_scale);
    game_set_fullscreen(g_state.fullscreen);
    game_set_master_volume(g_state.master_volume);
    game_set_music_volume(g_state.music_volume);
    game_set_sfx_volume(g_state.sfx_volume);
    telemetry_init();
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

void game_go_to_level_up_or(GameScreen destination)
{
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        if (g_state.run_party.members[i].pending_levels > 0)
        {
            g_state.post_combat_destination = destination;
            game_change_screen(SCREEN_LEVEL_UP);
            return;
        }
    }
    game_change_screen(destination);
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

static void draw_overlay_tooltip(Rectangle anchor, const char *title, const char *body, Color accent)
{
    int w = 224;
    int body_h = measure_text_box(body, w - 12, 10, 0);
    if (body_h < ui_line_height(10)) body_h = ui_line_height(10);
    int h = 26 + body_h;
    int x = (int)(anchor.x + anchor.width * 0.5f - w * 0.5f);
    int y = (int)(anchor.y + anchor.height + 5.0f);
    if (x < 4) x = 4;
    if (x + w > VIRT_W - 4) x = VIRT_W - w - 4;
    if (y + h > VIRT_H - 4) y = (int)(anchor.y - h - 5.0f);
    if (y < 4) y = 4;

    Rectangle tip = { (float)x, (float)y, (float)w, (float)h };
    DrawRectangleRec(tip, (Color){ 10, 11, 18, 248 });
    DrawRectangleLinesEx(tip, 1.0f, (Color){ accent.r, accent.g, accent.b, 230 });
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, 12.0f },
        title, 10, 0, accent, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 18.0f, tip.width - 12.0f, tip.height - 22.0f },
        body, 10, 0, (Color){ 215, 218, 238, 238 }, TEXT_ALIGN_LEFT);
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
        g_state.screen == SCREEN_LEVEL_UP ||
        g_state.screen == SCREEN_SETTINGS)
        return;

    Rectangle r = { 10.0f, 34.0f, (float)BTN_NARROW, 18.0f };
    DrawRectangleRec(r, (Color){ 12, 10, 18, 225 });
    DrawRectangleLinesEx(r, 1.0f, (Color){ 220, 185, 70, 190 });

    char text[32];
    snprintf(text, sizeof(text), "Gold %d", g_state.gold);
    draw_text_box((Rectangle){ r.x + 4.0f, r.y + 3.0f, r.width - 8.0f, 12.0f },
        text, 10, 0, (Color){ 245, 218, 105, 245 }, TEXT_ALIGN_CENTER);

    if (CheckCollisionPointRec(GetMousePosition(), r))
    {
        draw_overlay_tooltip(r,
            "Gold",
            "Run currency. Spend it in shops, events, reward rerolls, and extra reward choices. Unspent gold can convert to renown when the run ends.",
            (Color){ 245, 218, 105, 255 });
    }
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
    fprintf(f, "telemetry_opt_in=%d\n", g_state.telemetry_opt_in ? 1 : 0);
    fprintf(f, "telemetry_prompt_seen=%d\n", g_state.telemetry_prompt_seen ? 1 : 0);
    fprintf(f, "master_volume=%.3f\n", g_state.master_volume);
    fprintf(f, "music_volume=%.3f\n", g_state.music_volume);
    fprintf(f, "sfx_volume=%.3f\n", g_state.sfx_volume);
    fclose(f);
}

static float tutorial_clampf(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static bool tutorial_rect_valid(Rectangle rect)
{
    return rect.width > 1.0f && rect.height > 1.0f;
}

static Rectangle tutorial_expand_highlight(Rectangle rect, float pad)
{
    float x = tutorial_clampf(rect.x - pad, 0.0f, (float)VIRT_W);
    float y = tutorial_clampf(rect.y - pad, 0.0f, (float)VIRT_H);
    float right = tutorial_clampf(rect.x + rect.width + pad, 0.0f, (float)VIRT_W);
    float bottom = tutorial_clampf(rect.y + rect.height + pad, 0.0f, (float)VIRT_H);
    return (Rectangle){ x, y, right - x, bottom - y };
}

static Rectangle tutorial_panel_rect(Rectangle highlight, int panel_w, int panel_h)
{
    const float margin = 8.0f;
    float x = ((float)VIRT_W - (float)panel_w) * 0.5f;
    float y = (float)VIRT_H - (float)panel_h - 12.0f;

    if (tutorial_rect_valid(highlight))
    {
        float below = highlight.y + highlight.height + 10.0f;
        float above = highlight.y - (float)panel_h - 10.0f;
        bool room_below = below + (float)panel_h <= (float)VIRT_H - margin;
        bool room_above = above >= margin;

        if (room_below || room_above)
        {
            y = room_below ? below : above;
            x = highlight.x + highlight.width * 0.5f - (float)panel_w * 0.5f;
        }
        else
        {
            bool place_right = highlight.x + highlight.width * 0.5f < (float)VIRT_W * 0.5f;
            x = place_right ? highlight.x + highlight.width + 10.0f : highlight.x - (float)panel_w - 10.0f;
            y = highlight.y + highlight.height * 0.5f - (float)panel_h * 0.5f;
        }
    }

    x = tutorial_clampf(x, margin, (float)VIRT_W - (float)panel_w - margin);
    y = tutorial_clampf(y, margin, (float)VIRT_H - (float)panel_h - margin);
    return (Rectangle){ x, y, (float)panel_w, (float)panel_h };
}

static const char *tutorial_step_id(int step)
{
    switch (step)
    {
        case TUTORIAL_STEP_MAP: return "map_route";
        case TUTORIAL_STEP_COMBAT_PARTY: return "combat_party";
        case TUTORIAL_STEP_COMBAT_THREAT: return "combat_threat";
        case TUTORIAL_STEP_COMBAT_INTENT: return "combat_intent";
        case TUTORIAL_STEP_COMBAT_CARDS: return "combat_cards";
        case TUTORIAL_STEP_COMBAT_END: return "combat_end_turn";
        case TUTORIAL_STEP_REWARD: return "card_rewards";
        case TUTORIAL_STEP_ELITE: return "first_elite";
        case TUTORIAL_STEP_BOSS: return "first_boss";
        case TUTORIAL_STEP_SHOP: return "first_shop";
        case TUTORIAL_STEP_EVENT: return "first_event";
        case TUTORIAL_STEP_REST: return "first_rest";
        case TUTORIAL_STEP_LEVEL_UP: return "first_level_up";
        case TUTORIAL_STEP_DISCARD: return "first_discard";
        case TUTORIAL_STEP_GAME_OVER: return "game_over";
        case TUTORIAL_STEP_META_SHOP: return "meta_shop";
    }
    return "tutorial";
}

static const char *game_screen_id(GameScreen screen)
{
    switch (screen)
    {
        case SCREEN_TITLE: return "title";
        case SCREEN_CODEX: return "codex";
        case SCREEN_META_SHOP: return "meta_shop";
        case SCREEN_DRAFT: return "draft";
        case SCREEN_MAP: return "map";
        case SCREEN_RUN: return "run";
        case SCREEN_REST: return "rest";
        case SCREEN_SHOP: return "shop";
        case SCREEN_EVENT: return "event";
        case SCREEN_REWARD: return "reward";
        case SCREEN_RELIC_REWARD: return "relic_reward";
        case SCREEN_DISCARD: return "discard";
        case SCREEN_GAME_OVER: return "game_over";
        case SCREEN_DECK: return "deck";
        case SCREEN_ACHIEVEMENTS: return "achievements";
        case SCREEN_LEVEL_UP: return "level_up";
        case SCREEN_SETTINGS: return "settings";
    }
    return "unknown";
}

void game_skip_tutorial(void)
{
    if (g_state.tutorial_active)
        telemetry_log_tutorial(tutorial_step_id(g_state.tutorial_step), "closed", game_screen_id(g_state.screen));
    g_state.tutorial_active = false;
    g_state.tutorial_step = TUTORIAL_STEP_DONE;
}

bool game_tutorial_handle_skip(void)
{
    if (!g_state.tutorial_active) return false;
    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        telemetry_log_tutorial(tutorial_step_id(g_state.tutorial_step), "skipped", game_screen_id(g_state.screen));
        g_state.tutorial_active = false;
        g_state.tutorial_step = TUTORIAL_STEP_DONE;
        return true;
    }
    return false;
}

bool game_tutorial_handle_close(void)
{
    if (!g_state.tutorial_active) return false;
    if (game_tutorial_handle_skip())
        return true;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        telemetry_log_tutorial(tutorial_step_id(g_state.tutorial_step), "closed", game_screen_id(g_state.screen));
        g_state.tutorial_active = false;
        g_state.tutorial_step = TUTORIAL_STEP_DONE;
        return true;
    }
    return false;
}

bool game_start_tutorial_once(bool *seen_flag, int step)
{
    if (!seen_flag || *seen_flag || g_state.tutorial_active)
        return false;
    *seen_flag = true;
    meta_save(&g_state.meta);
    g_state.tutorial_active = true;
    g_state.tutorial_step = step;
    telemetry_log_tutorial(tutorial_step_id(step), "shown", game_screen_id(g_state.screen));
    return true;
}

void game_draw_tutorial_overlay_ex(Rectangle highlight, const char *title, const char *body, const char *footer, int step, int total_steps)
{
    if (!g_state.tutorial_active) return;

    const char *safe_title = title && title[0] ? title : "Tutorial";
    const char *safe_body = body ? body : "";
    const char *safe_footer = footer && footer[0] ? footer : "Click to continue";

    Color accent = (Color){ 255, 218, 92, 255 };
    Color dim = (Color){ 0, 0, 0, 88 };
    if (tutorial_rect_valid(highlight))
    {
        Rectangle focus = tutorial_expand_highlight(highlight, 10.0f);
        DrawRectangleRec((Rectangle){ 0.0f, 0.0f, (float)VIRT_W, focus.y }, dim);
        DrawRectangleRec((Rectangle){ 0.0f, focus.y + focus.height, (float)VIRT_W, (float)VIRT_H - focus.y - focus.height }, dim);
        DrawRectangleRec((Rectangle){ 0.0f, focus.y, focus.x, focus.height }, dim);
        DrawRectangleRec((Rectangle){ focus.x + focus.width, focus.y, (float)VIRT_W - focus.x - focus.width, focus.height }, dim);

        Rectangle halo = tutorial_expand_highlight(highlight, 5.0f);
        DrawRectangleRec(halo, (Color){ 255, 218, 92, 24 });
        DrawRectangleLinesEx(halo, 2.0f, accent);
        DrawRectangleLinesEx(tutorial_expand_highlight(highlight, 9.0f), 1.0f, (Color){ 255, 218, 92, 92 });
    }
    else
    {
        DrawRectangle(0, 0, VIRT_W, VIRT_H, dim);
    }

    int panel_w = 292;
    int body_w = panel_w - 20;
    int body_h = measure_text_box(safe_body, body_w, 10, 0);
    if (body_h < 26) body_h = 26;
    int panel_h = 60 + body_h;
    if (panel_h < 92) panel_h = 92;
    if (panel_h > 150) panel_h = 150;

    Rectangle panel = tutorial_panel_rect(highlight, panel_w, panel_h);
    Rectangle shadow = { panel.x + 2.0f, panel.y + 2.0f, panel.width, panel.height };
    DrawRectangleRec(shadow, (Color){ 0, 0, 0, 110 });
    DrawRectangleRec(panel, (Color){ 10, 11, 18, 246 });
    DrawRectangleRec((Rectangle){ panel.x, panel.y, 4.0f, panel.height }, (Color){ 255, 218, 92, 205 });
    DrawRectangleLinesEx(panel, 1.0f, (Color){ 255, 218, 92, 230 });

    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 8.0f, panel.width - 20.0f, 22.0f },
        safe_title, 18, 0, accent, TEXT_ALIGN_LEFT);

    if (step > 0 && total_steps > 0)
    {
        char count[16];
        snprintf(count, sizeof(count), "%d/%d", step, total_steps);
        draw_text_box((Rectangle){ panel.x + panel.width - 48.0f, panel.y + 10.0f, 36.0f, 12.0f },
            count, 10, 0, (Color){ 180, 184, 210, 220 }, TEXT_ALIGN_RIGHT);
    }

    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + 34.0f, panel.width - 20.0f, panel.height - 54.0f },
        safe_body, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ panel.x + 10.0f, panel.y + panel.height - 16.0f, panel.width - 20.0f, 12.0f },
        safe_footer, 10, 0, (Color){ 178, 184, 210, 225 }, TEXT_ALIGN_CENTER);
}

void game_draw_tutorial_overlay(Rectangle highlight, const char *text)
{
    game_draw_tutorial_overlay_ex(highlight, "Tutorial", text, "Click to continue", 0, 0);
}
