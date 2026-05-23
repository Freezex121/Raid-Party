#include "game.h"
#include "data/area_defs.h"
#include "util/tween.h"
#include "constants.h"
#include "raylib.h"
#include <string.h>

GameState g_state;

void game_init(void)
{
    memset(&g_state, 0, sizeof(GameState));
    meta_load(&g_state.meta);
    g_state.screen = SCREEN_TITLE;
    g_state.pending_screen = SCREEN_TITLE;
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    g_state.selected_area = area_clamp_index(g_state.meta.highest_area_unlocked);
    g_state.current_area = g_state.selected_area;
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


