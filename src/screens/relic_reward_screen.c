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
    const float w = RELIC_REWARD_CARD_W;
    const float h = RELIC_REWARD_CARD_H;
    const float gap = RELIC_REWARD_CARD_GAP;
    float total = count * w + (count - 1) * gap;
    float start_x = (VIRT_W - total) * 0.5f;
    return (Rectangle){ start_x + index * (w + gap), RELIC_REWARD_CARD_Y, w, h };
}

static void generate_relic_rewards(void)
{
    if (g_state.relic_reward_count <= 0)
    {
        g_state.relic_reward_count = relic_generate_choices(
            g_state.relics,
            g_state.relic_count,
            g_state.relic_reward_choices,
            RELIC_REWARD_CHOICES
        );
    }

    fallback_msg[0] = '\0';
    if (g_state.relic_reward_count <= 0)
    {
        game_gain_gold(25, "relic_reward_fallback");
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

    Rectangle title_band = {
        RELIC_REWARD_TITLE_BAND_X,
        RELIC_REWARD_TITLE_BAND_Y,
        RELIC_REWARD_TITLE_BAND_W,
        RELIC_REWARD_TITLE_BAND_H
    };
    DrawRectangleRounded((Rectangle){ title_band.x + 3.0f, title_band.y + 4.0f, title_band.width, title_band.height }, 0.2f, 6, (Color){ 0, 0, 0, 92 });
    DrawRectangleRounded(title_band, 0.2f, 6, (Color){ 17, 18, 29, 222 });
    DrawRectangleRoundedLinesEx(title_band, 0.2f, 6, 1.0f, (Color){ 224, 204, 106, 212 });

    draw_text_box((Rectangle){ title_band.x + 8.0f, title_band.y + 8.0f, title_band.width - 16.0f, 22.0f },
        "RELIC REWARD", 18, 0, (Color){ 236, 214, 118, 255 }, TEXT_ALIGN_CENTER);
    draw_text_box((Rectangle){ title_band.x + 8.0f, title_band.y + 30.0f, title_band.width - 16.0f, 14.0f },
        "Choose one relic for a permanent run bonus", 10, 0, (Color){ 177, 181, 206, 230 }, TEXT_ALIGN_CENTER);

    if (g_state.relic_reward_count <= 0)
    {
        Rectangle empty_panel = {
            RELIC_REWARD_EMPTY_PANEL_X,
            RELIC_REWARD_EMPTY_PANEL_Y,
            RELIC_REWARD_EMPTY_PANEL_W,
            RELIC_REWARD_EMPTY_PANEL_H
        };
        DrawRectangleRounded((Rectangle){ empty_panel.x + 3.0f, empty_panel.y + 4.0f, empty_panel.width, empty_panel.height }, 0.14f, 6, (Color){ 0, 0, 0, 86 });
        DrawRectangleRounded(empty_panel, 0.14f, 6, (Color){ 20, 22, 33, 234 });
        DrawRectangleRoundedLinesEx(empty_panel, 0.14f, 6, 1.0f, (Color){ 134, 138, 166, 208 });

        draw_text_box((Rectangle){ empty_panel.x + 12.0f, empty_panel.y + 26.0f, empty_panel.width - 24.0f, 28.0f },
            fallback_msg, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ empty_panel.x + 12.0f, empty_panel.y + 52.0f, empty_panel.width - 24.0f, 14.0f },
            "Click anywhere to continue", 10, 0, (Color){ 164, 169, 196, 228 }, TEXT_ALIGN_CENTER);
        return;
    }

    Vector2 mouse = GetMousePosition();
    int pulse = ((int)(GetTime() * RELIC_REWARD_PULSE_SPEED)) % RELIC_REWARD_PULSE_MOD;
    unsigned char pulse_alpha = (unsigned char)(RELIC_REWARD_PULSE_BASE + pulse);

    for (int i = 0; i < g_state.relic_reward_count; i++)
    {
        const RelicDef *def = relic_def(g_state.relic_reward_choices[i]);
        if (!def) continue;

        Rectangle r = relic_reward_rect(g_state.relic_reward_count, i);
        bool hover = CheckCollisionPointRec(mouse, r);

        Rectangle card = r;
        if (hover) card.y -= RELIC_REWARD_CARD_HOVER_LIFT;

        DrawRectangleRounded((Rectangle){ card.x + 3.0f, card.y + 5.0f, card.width, card.height }, 0.08f, 6, (Color){ 0, 0, 0, 94 });
        DrawRectangleRounded(card, 0.08f, 6, hover ? (Color){ 76, 64, 38, 248 } : (Color){ 23, 21, 17, 240 });
        DrawRectangleRoundedLinesEx(card, 0.08f, 6, hover ? 2.0f : 1.0f, hover ? (Color){ 246, 232, 170, 255 } : (Color){ 222, 202, 102, 188 });

        Rectangle icon_box = {
            card.x + RELIC_REWARD_ICON_BOX_X,
            card.y + RELIC_REWARD_ICON_BOX_Y,
            RELIC_REWARD_ICON_BOX_SIZE,
            RELIC_REWARD_ICON_BOX_SIZE
        };
        DrawRectangleRounded(icon_box, 0.22f, 6, (Color){ 57, 48, 30, 255 });
        DrawRectangleRoundedLinesEx(icon_box, 0.22f, 6, 1.0f, (Color){ 232, 211, 112, 232 });
        Texture2D icon_tex = g_assets.relic_icons[g_state.relic_reward_choices[i]];
        if (icon_tex.id != 0)
        {
            float icon_draw = 32.0f;
            float off = (RELIC_REWARD_ICON_BOX_SIZE - icon_draw) * 0.5f;
            DrawTexturePro(icon_tex,
                (Rectangle){ 0.0f, 0.0f, (float)icon_tex.width, (float)icon_tex.height },
                (Rectangle){ icon_box.x + off, icon_box.y + off, icon_draw, icon_draw },
                (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
        else
        {
            DrawText(def->icon, snap_i(icon_box.x + icon_box.width * 0.5f - MeasureText(def->icon, 10) / 2), snap_i(icon_box.y) + 9, 10, RAYWHITE);
        }

        draw_text_box((Rectangle){ card.x + RELIC_REWARD_NAME_X, card.y + RELIC_REWARD_NAME_Y, card.width - RELIC_REWARD_NAME_X - 10.0f, 28.0f },
            def->name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ card.x + RELIC_REWARD_DESC_X, card.y + RELIC_REWARD_DESC_Y, card.width - RELIC_REWARD_DESC_INSET_W, 34.0f },
            def->description, 10, 0, (Color){ 198, 202, 220, 236 }, TEXT_ALIGN_LEFT);

        if (hover)
        {
            DrawText("CLICK TO CLAIM", (int)card.x + RELIC_REWARD_DESC_X, (int)card.y + RELIC_REWARD_CLAIM_Y, 10, (Color){ 250, 238, 176, pulse_alpha });
        }
    }

    Rectangle tray_frame = {
        RELIC_REWARD_TRAY_FRAME_X,
        RELIC_REWARD_TRAY_FRAME_Y,
        RELIC_REWARD_TRAY_FRAME_W,
        RELIC_REWARD_TRAY_FRAME_H
    };
    DrawRectangleRounded((Rectangle){ tray_frame.x + 2.0f, tray_frame.y + 3.0f, tray_frame.width, tray_frame.height }, 0.12f, 6, (Color){ 0, 0, 0, 80 });
    DrawRectangleRounded(tray_frame, 0.12f, 6, (Color){ 17, 18, 27, 182 });
    DrawRectangleRoundedLinesEx(tray_frame, 0.12f, 6, 1.0f, (Color){ 114, 118, 146, 176 });
    relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){
        tray_frame.x + RELIC_REWARD_TRAY_INSET_X,
        tray_frame.y + RELIC_REWARD_TRAY_INSET_Y,
        tray_frame.width - (RELIC_REWARD_TRAY_INSET_X * 2.0f),
        tray_frame.height - (RELIC_REWARD_TRAY_INSET_Y * 2.0f)
    });
}
