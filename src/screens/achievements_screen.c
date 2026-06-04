#include "screens.h"
#include "game.h"
#include "systems/achievements.h"
#include "util/text.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include "ui/ui.h"
#include <stdio.h>
#include <string.h>

static int scroll_offset = 0;
static int hovered_ach = -1;

static const char *class_short_name(ClassType ct)
{
    return class_name(ct);
}

void achievements_screen_update(void)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        scroll_offset = 0;
        hovered_ach = -1;
        game_change_screen(SCREEN_TITLE);
        return;
    }

    int wheel = GetMouseWheelMove();
    scroll_offset -= wheel * 16;
    if (scroll_offset < 0) scroll_offset = 0;

    hovered_ach = -1;
    Vector2 mouse = GetMousePosition();

    int box_w = 120;
    int box_h = 28;
    int gap_x = 10;
    int gap_y = 8;
    int cols = 5;
    int total_w = cols * box_w + (cols - 1) * gap_x;
    int start_x = (VIRT_W - total_w) / 2;
    int start_y = 66;

    int achievement_total = achievement_count();
    int rows = (achievement_total + cols - 1) / cols;
    int max_scroll = rows * (box_h + gap_y) - gap_y - (VIRT_H - start_y - 42);
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;

    for (int i = 0; i < achievement_total; i++)
    {
        AchievementId id = achievement_at(i);
        int col = i % cols;
        int row = i / cols;
        int cx = start_x + col * (box_w + gap_x);
        int cy = start_y + row * (box_h + gap_y) - scroll_offset;

        if (cy + box_h < 56 || cy > VIRT_H) continue;

        Rectangle r = { (float)cx, (float)cy, (float)box_w, (float)box_h };
        if (CheckCollisionPointRec(mouse, r)) hovered_ach = id;
    }

    // Back button
    Rectangle back_btn = { (float)(VIRT_W / 2 - 40), (float)(VIRT_H - 29), (float)BTN_NARROW, 22.0f };
    if (CheckCollisionPointRec(mouse, back_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        scroll_offset = 0;
        hovered_ach = -1;
        game_change_screen(SCREEN_TITLE);
        return;
    }
}

void achievements_screen_draw(void)
{
    theme_draw_background();

    draw_text_box((Rectangle){ 80.0f, 10.0f, 480.0f, 20.0f }, "ACHIEVEMENTS", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    int unlocked_count = 0;
    int achievement_total = achievement_count();
    for (int i = 0; i < achievement_total; i++)
    {
        AchievementId id = achievement_at(i);
        if (id >= 0 && id < ACH_COUNT && g_state.meta.achievements[id]) unlocked_count++;
    }
    char stats[64];
    snprintf(stats, sizeof(stats), "%d / %d unlocked", unlocked_count, achievement_total);
    draw_text_box((Rectangle){ 80.0f, 30.0f, 480.0f, 12.0f }, stats, 10, 0, (Color){ 190, 195, 220, 230 }, TEXT_ALIGN_CENTER);

    int box_w = 120;
    int box_h = 28;
    int gap_x = 10;
    int gap_y = 8;
    int cols = 5;
    int total_w = cols * box_w + (cols - 1) * gap_x;
    int start_x = (VIRT_W - total_w) / 2;
    int start_y = 66;

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < achievement_total; i++)
    {
        AchievementId id = achievement_at(i);
        int col = i % cols;
        int row = i / cols;
        int cx = start_x + col * (box_w + gap_x);
        int cy = start_y + row * (box_h + gap_y) - scroll_offset;

        if (cy + box_h < 56 || cy > VIRT_H) continue;

        Rectangle r = { (float)cx, (float)cy, (float)box_w, (float)box_h };
        bool unlocked = id >= 0 && id < ACH_COUNT && g_state.meta.achievements[id];
        bool hover = (id == hovered_ach) || CheckCollisionPointRec(mouse, r);

        Color bg = unlocked ? (Color){ 25, 44, 35, 238 } : (Color){ 20, 21, 30, 225 };
        Color border = unlocked ? (Color){ 95, 210, 130, 210 } : (Color){ 70, 72, 88, 170 };
        if (hover)
        {
            bg.r += 10; bg.g += 10; bg.b += 14;
            if (unlocked) border = (Color){ 130, 240, 160, 255 };
        }

        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, hover ? 2.0f : 1.0f, border);

        const char *name = achievement_name(id);
        Color name_col = unlocked ? RAYWHITE : (Color){ 150, 154, 175, 230 };
        draw_text_box((Rectangle){ r.x + 5.0f, r.y + 2.0f, r.width - 10.0f, 13.0f },
            name, 10, 0, name_col, TEXT_ALIGN_LEFT);

        if (unlocked)
        {
            char run_label[16];
            snprintf(run_label, sizeof(run_label), "Run #%d", g_state.meta.achievement_times[id]);
            draw_text_box((Rectangle){ r.x + 5.0f, r.y + 14.0f, r.width - 10.0f, 12.0f },
                run_label, 10, 0, (Color){ 140, 200, 160, 220 }, TEXT_ALIGN_LEFT);
        }
        else
        {
            draw_text_box((Rectangle){ r.x + 5.0f, r.y + 14.0f, r.width - 10.0f, 12.0f },
                "LOCKED", 10, 0, (Color){ 95, 98, 116, 200 }, TEXT_ALIGN_LEFT);
        }
    }

    // Hover tooltip
    if (hovered_ach >= 0 && hovered_ach < ACH_COUNT)
    {
        AchievementId id = (AchievementId)hovered_ach;
        bool unlocked = g_state.meta.achievements[id];

        int tip_w = 220;
        int body_h = 0;

        const char *desc = achievement_desc(id);
        int desc_h = measure_text_box(desc, tip_w - 12, 10, 0);
        if (desc_h < ui_line_height(10)) desc_h = ui_line_height(10);
        body_h += desc_h + 4;

        char reward[48];
        snprintf(reward, sizeof(reward), "Reward: +%d Renown", achievement_reward(id));
        body_h += ui_line_height(10) + 4;
        char unlocks[192];
        char unlock_line[224] = "";
        meta_content_achievement_rewards(id, unlocked, unlocks, sizeof(unlocks));
        int unlock_h = 0;
        if (unlocks[0])
        {
            snprintf(unlock_line, sizeof(unlock_line), "Enables: %s", unlocks);
            unlock_h = measure_text_box(unlock_line, tip_w - 12, 10, 0);
            if (unlock_h < ui_line_height(10)) unlock_h = ui_line_height(10);
            body_h += unlock_h + 4;
        }

        if (unlocked)
        {
            char run[32];
            snprintf(run, sizeof(run), "Achieved on run #%d", g_state.meta.achievement_times[id]);
            body_h += ui_line_height(10) + 4;

            int party_mask = g_state.meta.achievement_party[id];
            if (party_mask != 0)
            {
                char party_str[64] = "";
                int class_count = party_class_count();
                for (int i = 0; i < class_count; i++)
                {
                    ClassType c = party_class_at(i);
                    if (c >= 0 && c < 31 && (party_mask & (1 << c)))
                    {
                        if (party_str[0]) strcat(party_str, ", ");
                        strcat(party_str, class_short_name(c));
                    }
                }
                if (party_str[0])
                {
                    int party_h = measure_text_box(party_str, tip_w - 12, 10, 0);
                    if (party_h < ui_line_height(10)) party_h = ui_line_height(10);
                    body_h += party_h + 4;
                }
            }
        }

        int tip_h = 24 + body_h + 8;
        int tip_x = (int)GetMousePosition().x + 12;
        int tip_y = (int)GetMousePosition().y - tip_h / 2;
        if (tip_x + tip_w > VIRT_W - 4) tip_x = VIRT_W - tip_w - 4;
        if (tip_x < 4) tip_x = 4;
        if (tip_y < 4) tip_y = 4;
        if (tip_y + tip_h > VIRT_H - 4) tip_y = VIRT_H - tip_h - 4;

        Rectangle tip = { (float)tip_x, (float)tip_y, (float)tip_w, (float)tip_h };
        DrawRectangleRec(tip, (Color){ 10, 11, 18, 245 });
        DrawRectangleLinesEx(tip, 1.0f, (Color){ 230, 205, 95, 230 });

        int ly = (int)tip.y + 5;
        Color title_col = unlocked ? (Color){ 255, 220, 80, 245 } : (Color){ 180, 180, 200, 220 };
        draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, 14.0f },
            achievement_name(id), 10, 0, title_col, TEXT_ALIGN_LEFT);
        ly += 16;

        draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, (float)desc_h },
            desc, 10, 0, (Color){ 200, 204, 225, 235 }, TEXT_ALIGN_LEFT);
        ly += desc_h + 4;

        draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, 12.0f },
            reward, 10, 0, (Color){ 230, 205, 95, 240 }, TEXT_ALIGN_LEFT);
        ly += ui_line_height(10) + 4;

        if (unlocks[0])
        {
            draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, (float)unlock_h },
                unlock_line, 10, 0, (Color){ 150, 210, 240, 230 }, TEXT_ALIGN_LEFT);
            ly += unlock_h + 4;
        }

        if (unlocked)
        {
            char run[32];
            snprintf(run, sizeof(run), "Achieved on run #%d", g_state.meta.achievement_times[id]);
            draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, 12.0f },
                run, 10, 0, (Color){ 175, 184, 210, 225 }, TEXT_ALIGN_LEFT);
            ly += ui_line_height(10) + 4;

            int party_mask = g_state.meta.achievement_party[id];
            if (party_mask != 0)
            {
                char party_str[64] = "";
                int class_count = party_class_count();
                for (int i = 0; i < class_count; i++)
                {
                    ClassType c = party_class_at(i);
                    if (c >= 0 && c < 31 && (party_mask & (1 << c)))
                    {
                        if (party_str[0]) strcat(party_str, ", ");
                        strcat(party_str, class_short_name(c));
                    }
                }
                if (party_str[0])
                {
                    draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, 12.0f },
                        party_str, 10, 0, (Color){ 175, 184, 210, 225 }, TEXT_ALIGN_LEFT);
                }
            }
        }
        else
        {
            draw_text_box((Rectangle){ tip.x + 6.0f, (float)ly, tip.width - 12.0f, 12.0f },
                "Not yet unlocked", 10, 0, (Color){ 120, 124, 150, 200 }, TEXT_ALIGN_LEFT);
        }
    }

    Rectangle back_btn = { (float)(VIRT_W / 2 - 40), (float)(VIRT_H - 29), (float)BTN_NARROW, 22.0f };
    draw_btn_standard(back_btn, (Color){ 60, 60, 85, 255 }, (Color){ 100, 100, 130, 255 }, "BACK", BTN_ID_ACHIEVEMENTS_BACK);
}
