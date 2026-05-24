#include "screens.h"
#include "game.h"
#include "constants.h"
#include "systems/relic.h"
#include "ui/relic_tray.h"
#include "ui/theme.h"
#include "util/text.h"
#include "util/math_utils.h"
#include "raylib.h"
#include <stdio.h>

static bool generated = false;
static int hovered_relic = -1;
static char fallback_msg[96] = "";

static Rectangle relic_reward_rect(int count, int index)
{
    const float w = 150.0f;
    const float h = 94.0f;
    const float gap = 16.0f;
    float total = count * w + (count - 1) * gap;
    float start_x = (VIRT_W - total) * 0.5f;
    return (Rectangle){ start_x + index * (w + gap), 122.0f, w, h };
}

static void generate_relic_rewards(void)
{
    g_state.relic_reward_count = relic_generate_choices(
        g_state.relics,
        g_state.relic_count,
        g_state.relic_reward_choices,
        RELIC_REWARD_CHOICES
    );

    fallback_msg[0] = '\0';
    if (g_state.relic_reward_count <= 0)
    {
        g_state.gold += 25;
        snprintf(fallback_msg, sizeof(fallback_msg), "All relics owned. Gained 25g instead.");
    }
    generated = true;
}

void relic_reward_screen_update(void)
{
    if (!generated)
        generate_relic_rewards();

    Vector2 mouse = GetMousePosition();
    hovered_relic = -1;

    if (g_state.relic_reward_count <= 0)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            g_state.relic_reward_pending = false;
            generated = false;
            game_change_screen(SCREEN_MAP);
        }
        return;
    }

    for (int i = 0; i < g_state.relic_reward_count; i++)
    {
        Rectangle r = relic_reward_rect(g_state.relic_reward_count, i);
        if (CheckCollisionPointRec(mouse, r))
        {
            hovered_relic = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                RelicId id = g_state.relic_reward_choices[i];
                relic_add_unique(g_state.relics, &g_state.relic_count, id);
                g_state.relic_reward_pending = false;
                generated = false;
                game_change_screen(SCREEN_MAP);
            }
            break;
        }
    }
}

void relic_reward_screen_draw(void)
{
    theme_draw_background();

    DrawText("RELIC REWARD", (VIRT_W / 2) - MeasureText("RELIC REWARD", 18) / 2, 54, 18, (Color){ 230, 205, 95, 255 });
    DrawText("Choose one passive for this run", (VIRT_W / 2) - MeasureText("Choose one passive for this run", 10) / 2, 82, 10, (Color){ 170, 174, 200, 220 });

    if (g_state.relic_reward_count <= 0)
    {
        DrawText(fallback_msg, (VIRT_W / 2) - MeasureText(fallback_msg, 10) / 2, 160, 10, RAYWHITE);
        DrawText("Click to continue.", (VIRT_W / 2) - MeasureText("Click to continue.", 10) / 2, 188, 10, (Color){ 160, 160, 190, 220 });
        return;
    }

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < g_state.relic_reward_count; i++)
    {
        const RelicDef *def = relic_def(g_state.relic_reward_choices[i]);
        if (!def) continue;

        Rectangle r = relic_reward_rect(g_state.relic_reward_count, i);
        bool hover = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hover ? (Color){ 70, 60, 36, 245 } : (Color){ 22, 20, 17, 238 });
        DrawRectangleLinesEx(r, hover ? 2.0f : 1.0f, hover ? RAYWHITE : (Color){ 225, 205, 105, 190 });
        DrawRectangleRec((Rectangle){ r.x + 12.0f, r.y + 12.0f, 30.0f, 30.0f }, (Color){ 55, 47, 28, 255 });
        DrawRectangleLinesEx((Rectangle){ r.x + 12.0f, r.y + 12.0f, 30.0f, 30.0f }, 1.0f, (Color){ 225, 205, 105, 220 });
        DrawText(def->icon, snap_i(r.x + 27.0f - MeasureText(def->icon, 10) / 2), snap_i(r.y) + 23, 10, RAYWHITE);
        DrawText(def->name, (int)r.x + 52, (int)r.y + 14, 10, RAYWHITE);
        draw_text_wrapped(def->description, (int)r.x + 12, (int)r.y + 54, (int)r.width - 24, 10, 2, (Color){ 190, 194, 215, 235 });
    }

    relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){ 206.0f, 258.0f, 228.0f, 44.0f });
}
