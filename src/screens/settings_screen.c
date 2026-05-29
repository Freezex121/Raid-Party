#include "screens.h"
#include "game.h"
#include "assets.h"
#include "constants.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "util/text.h"
#include "raylib.h"
#include <stdio.h>

static Button back_btn;
static Button fullscreen_btn;
static Button scale_down_btn;
static Button scale_up_btn;
static Button telemetry_btn;
static bool initialized = false;

static Rectangle visual_panel_rect(void)
{
    return (Rectangle){ 54.0f, 78.0f, 246.0f, 178.0f };
}

static Rectangle audio_panel_rect(void)
{
    return (Rectangle){ 340.0f, 78.0f, 246.0f, 178.0f };
}

static void init_if_needed(void)
{
    if (initialized) return;

    back_btn = button_create(
        (Rectangle){ (float)(VIRT_W / 2 - BTN_MED / 2), 328.0f, (float)BTN_MED, (float)BTN_H },
        "BACK",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);

    Rectangle v = visual_panel_rect();
    const float scale_group_w = (float)BTN_ICON + 12.0f + 52.0f + 12.0f + (float)BTN_ICON;
    const float scale_group_x = v.x + (v.width - scale_group_w) * 0.5f;
    fullscreen_btn = button_create(
        (Rectangle){ v.x + (v.width - BTN_WIDE) * 0.5f, v.y + 52.0f, (float)BTN_WIDE, (float)BTN_H },
        "FULLSCREEN",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);
    scale_down_btn = button_create(
        (Rectangle){ scale_group_x, v.y + 112.0f, (float)BTN_ICON, (float)BTN_H },
        "-",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);
    scale_up_btn = button_create(
        (Rectangle){ scale_group_x + BTN_ICON + 12.0f + 52.0f + 12.0f, v.y + 112.0f, (float)BTN_ICON, (float)BTN_H },
        "+",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);
    telemetry_btn = button_create(
        (Rectangle){ (float)(VIRT_W / 2 - BTN_WIDE / 2), 266.0f, (float)BTN_WIDE, (float)BTN_H },
        "TELEMETRY: OFF",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);

    initialized = true;
}

static Rectangle slider_track(int index)
{
    Rectangle a = audio_panel_rect();
    return (Rectangle){ a.x + 34.0f, a.y + 58.0f + index * 42.0f, a.width - 68.0f, 8.0f };
}

static float slider_value_from_mouse(Rectangle track)
{
    float value = (GetMousePosition().x - track.x) / track.width;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return value;
}

static void update_slider(Rectangle track, void (*setter)(float))
{
    Vector2 mouse = GetMousePosition();
    Rectangle hit = { track.x - 6.0f, track.y - 8.0f, track.width + 12.0f, track.height + 16.0f };
    if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) &&
        CheckCollisionPointRec(mouse, hit))
    {
        setter(slider_value_from_mouse(track));
    }
}

void settings_screen_update(void)
{
    init_if_needed();

    button_update(&back_btn);
    button_update(&fullscreen_btn);
    button_update(&scale_down_btn);
    button_update(&scale_up_btn);
    button_update(&telemetry_btn);

    if (back_btn.pressed_this_frame || IsKeyPressed(KEY_ESCAPE))
    {
        game_settings_save();
        initialized = false;
        game_change_screen(g_state.settings_return_screen);
        return;
    }

    if (fullscreen_btn.pressed_this_frame)
        game_set_fullscreen(!g_state.fullscreen);

    if (scale_down_btn.pressed_this_frame)
        game_set_window_scale(g_state.window_scale - 1);
    if (scale_up_btn.pressed_this_frame)
        game_set_window_scale(g_state.window_scale + 1);
    if (telemetry_btn.pressed_this_frame)
    {
        g_state.telemetry_opt_in = !g_state.telemetry_opt_in;
        g_state.telemetry_prompt_seen = true;
        game_settings_save();
    }

    update_slider(slider_track(0), game_set_master_volume);
    update_slider(slider_track(1), game_set_music_volume);
    update_slider(slider_track(2), game_set_sfx_volume);
}

static void draw_slider(Rectangle track, const char *label, float value)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    char text[48];
    snprintf(text, sizeof(text), "%s  %d%%", label, (int)(value * 100.0f + 0.5f));
    draw_text_box((Rectangle){ track.x, track.y - 18.0f, track.width, 12.0f },
        text, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);

    DrawRectangleRec(track, (Color){ 22, 24, 36, 255 });
    DrawRectangleRec((Rectangle){ track.x, track.y, track.width * value, track.height },
        (Color){ 95, 150, 220, 255 });
    DrawRectangleLinesEx(track, 1.0f, (Color){ 88, 96, 130, 210 });

    float knob_x = track.x + track.width * value;
    DrawRectangleRec((Rectangle){ knob_x - 3.0f, track.y - 4.0f, 6.0f, track.height + 8.0f },
        (Color){ 230, 235, 255, 245 });
}

void settings_screen_draw(void)
{
    init_if_needed();
    theme_draw_background();

    draw_text_box((Rectangle){ 80.0f, 32.0f, 480.0f, 22.0f },
        "SETTINGS", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    Rectangle visual = visual_panel_rect();
    Rectangle audio = audio_panel_rect();
    draw_panel(visual, (Color){ 9, 10, 18, 225 }, 0.0f, (Color){ 82, 96, 135, 180 });
    draw_panel(audio, (Color){ 9, 10, 18, 225 }, 0.0f, (Color){ 82, 96, 135, 180 });

    draw_text_box((Rectangle){ visual.x + 12.0f, visual.y + 12.0f, visual.width - 24.0f, 18.0f },
        "VISUAL", 18, 0, (Color){ 190, 210, 245, 245 }, TEXT_ALIGN_CENTER);
    draw_text_box((Rectangle){ audio.x + 12.0f, audio.y + 12.0f, audio.width - 24.0f, 18.0f },
        "AUDIO", 18, 0, (Color){ 190, 210, 245, 245 }, TEXT_ALIGN_CENTER);

    fullscreen_btn.text = g_state.fullscreen ? "FULLSCREEN: ON" : "FULLSCREEN: OFF";
    button_draw_9slice(&fullscreen_btn);

    // Scale buttons
    button_draw_9slice(&scale_down_btn);
    button_draw_9slice(&scale_up_btn);

    // ... (existing scale display code)
    char scale_text[32];
    snprintf(scale_text, sizeof(scale_text), "%dx", g_state.window_scale);
    const float scale_group_w = (float)BTN_ICON + 12.0f + 52.0f + 12.0f + (float)BTN_ICON;
    const float scale_group_x = visual.x + (visual.width - scale_group_w) * 0.5f;
    draw_text_box((Rectangle){ scale_group_x + BTN_ICON + 12.0f, visual.y + 116.0f, 52.0f, 14.0f },
        scale_text, 10, 0, (Color){ 245, 235, 160, 245 }, TEXT_ALIGN_CENTER);
    draw_text_box((Rectangle){ visual.x + 18.0f, visual.y + 144.0f, visual.width - 36.0f, 24.0f },
        "Fullscreen preserves 16:9 with letterboxing when needed.", 10, 0, (Color){ 150, 158, 185, 215 }, TEXT_ALIGN_CENTER);

    draw_slider(slider_track(0), "Master", g_state.master_volume);
    draw_slider(slider_track(1), "Music", g_state.music_volume);
    draw_slider(slider_track(2), "SFX", g_state.sfx_volume);

    telemetry_btn.text = g_state.telemetry_opt_in ? "TELEMETRY: ON" : "TELEMETRY: OFF";
    button_draw_9slice(&telemetry_btn);
    draw_text_box((Rectangle){ 120.0f, 292.0f, 400.0f, 28.0f },
        "Sends anonymous gameplay metrics for balance. No names, Steam IDs, or personal data.",
        10, 0, (Color){ 150, 158, 185, 215 }, TEXT_ALIGN_CENTER);

    button_draw_9slice(&back_btn);
}
