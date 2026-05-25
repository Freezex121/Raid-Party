#include "screens.h"
#include "game.h"
#include "ui/theme.h"
#include "util/log.h"
#include "util/math_utils.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

static int hovered_card = -1;
static int scroll_offset = 0;

static int sorted_indices[MAX_DECK_SIZE];
static int sorted_count = 0;

static int card_sort_key(const CardDef *a)
{
    if (!a) return 999;
    return a->class * 100 + a->cost;
}

static void build_sorted_index(Deck *deck)
{
    sorted_count = 0;
    for (int i = 0; i < deck->card_count && sorted_count < MAX_DECK_SIZE; i++)
    {
        if (!deck->cards[i].def) continue;
        int pos = sorted_count++;
        sorted_indices[pos] = i;
        while (pos > 0 && card_sort_key(deck->cards[sorted_indices[pos]].def) < card_sort_key(deck->cards[sorted_indices[pos - 1]].def))
        {
            int tmp = sorted_indices[pos];
            sorted_indices[pos] = sorted_indices[pos - 1];
            sorted_indices[pos - 1] = tmp;
            pos--;
        }
    }
}

static void finish_discard_screen(void)
{
    scroll_offset = 0;
    if (g_state.relic_reward_pending)
        game_change_screen(SCREEN_RELIC_REWARD);
    else
        game_change_screen(SCREEN_MAP);
}

void discard_screen_update(void)
{
    if (g_state.discard_count <= 0)
    {
        g_state.discard_selected = 0;
        finish_discard_screen();
        return;
    }

    Deck *deck = &g_state.run_deck;
    build_sorted_index(deck);

    Vector2 mouse = GetMousePosition();
    hovered_card = -1;

    int wheel = GetMouseWheelMove();
    scroll_offset -= wheel * 16;
    if (scroll_offset < 0) scroll_offset = 0;

    int cards_per_row = 4;
    int card_w = DISCARD_CARD_W;
    int card_h = DISCARD_CARD_H;
    int gap = DISCARD_GAP;
    int total_w = cards_per_row * (card_w + gap) - gap;
    int start_x = (VIRT_W - total_w) / 2;
    int start_y = DISCARD_Y;

    for (int pos = 0; pos < sorted_count; pos++)
    {
        int di = sorted_indices[pos];
        int row = pos / cards_per_row;
        int col = pos % cards_per_row;
        int cx = start_x + col * (card_w + gap);
        int cy = start_y + row * (card_h + gap) - scroll_offset;

        if (cy + card_h < 60 || cy > VIRT_H) continue;

        Rectangle r = { (float)cx, (float)cy, (float)card_w, (float)card_h };
        if (CheckCollisionPointRec(mouse, r))
        {
            hovered_card = di;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                int uid = deck->cards[di].uid;
                int already = -1;
                for (int j = 0; j < g_state.discard_selected; j++)
                    if (g_state.discard_uids[j] == uid) { already = j; break; }

                if (already >= 0)
                {
                    for (int j = already; j < g_state.discard_selected - 1; j++)
                        g_state.discard_uids[j] = g_state.discard_uids[j + 1];
                    g_state.discard_selected--;
                }
                else if (g_state.discard_selected < g_state.discard_count)
                {
                    g_state.discard_uids[g_state.discard_selected++] = uid;
                }
            }
            break;
        }
    }

    Rectangle skip_btn = { (float)(VIRT_W / 2 + 10), (float)(VIRT_H - 40), 80.0f, 26.0f };
    if (CheckCollisionPointRec(mouse, skip_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        LOG_I(CAT_SCREEN, "Skipped card discard");
        g_state.discard_count = 0;
        g_state.discard_selected = 0;
        finish_discard_screen();
    }

    if (g_state.discard_selected >= g_state.discard_count)
    {
        Rectangle confirm_btn = { (float)(VIRT_W / 2 - 90), (float)(VIRT_H - 40), 100.0f, 26.0f };
        if (CheckCollisionPointRec(mouse, confirm_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            for (int i = 0; i < g_state.discard_selected; i++)
                deck_remove_card_by_uid(deck, g_state.discard_uids[i]);

            LOG_I(CAT_SCREEN, "Discarded %d card(s)", g_state.discard_selected);
            g_state.discard_count = 0;
            g_state.discard_selected = 0;
            finish_discard_screen();
        }
    }
}

void discard_screen_draw(void)
{
    theme_draw_background();

    const char *title = g_state.discard_count == 2 ? "BOSS REWARD: Discard 2 Cards" : "ELITE REWARD: Discard 1 Card";
    Color title_col = g_state.discard_count == 2 ? (Color){ 220, 180, 50, 255 } : (Color){ 200, 100, 220, 255 };
    draw_text_box((Rectangle){ 80.0f, 18.0f, 480.0f, 22.0f }, title, 18, 0, title_col, TEXT_ALIGN_CENTER);

    char hint[64];
    snprintf(hint, sizeof(hint), "Select %d card%s to remove from your deck", g_state.discard_count, g_state.discard_count > 1 ? "s" : "");
    draw_text_box((Rectangle){ 80.0f, 40.0f, 480.0f, 14.0f }, hint, 10, 0, (Color){ 160, 160, 180, 200 }, TEXT_ALIGN_CENTER);

    char sel[32];
    snprintf(sel, sizeof(sel), "Selected: %d / %d", g_state.discard_selected, g_state.discard_count);
    Color sel_col = g_state.discard_selected >= g_state.discard_count ? (Color){ 100, 255, 130, 255 } : (Color){ 200, 200, 220, 200 };
    draw_text_box((Rectangle){ 80.0f, 56.0f, 480.0f, 14.0f }, sel, 10, 0, sel_col, TEXT_ALIGN_CENTER);

    Deck *deck = &g_state.run_deck;
    if (sorted_count == 0) build_sorted_index(deck);

    int cards_per_row = 4;
    int card_w = DISCARD_CARD_W;
    int card_h = DISCARD_CARD_H;
    int gap = DISCARD_GAP;
    int total_w = cards_per_row * (card_w + gap) - gap;
    int start_x = (VIRT_W - total_w) / 2;
    int start_y = DISCARD_Y;

    for (int pos = 0; pos < sorted_count; pos++)
    {
        int di = sorted_indices[pos];
        int row = pos / cards_per_row;
        int col = pos % cards_per_row;
        int cx = start_x + col * (card_w + gap);
        int cy = start_y + row * (card_h + gap) - scroll_offset;

        if (cy + card_h < 60 || cy > VIRT_H) continue;

        Rectangle r = { (float)cx, (float)cy, (float)card_w, (float)card_h };
        bool selected = false;
        for (int j = 0; j < g_state.discard_selected; j++)
            if (g_state.discard_uids[j] == deck->cards[di].uid) { selected = true; break; }

        theme_draw_card_art(r, deck->cards[di].def, deck->cards[di].upgraded);

        if (selected)
            DrawRectangleRec(r, (Color){ 255, 80, 80, 50 });
    }

    Rectangle skip_btn = { (float)(VIRT_W / 2 + 10), (float)(VIRT_H - 40), 80.0f, 26.0f };
    Vector2 mouse = GetMousePosition();
    bool skip_hover = CheckCollisionPointRec(mouse, skip_btn);
    Color skip_col = skip_hover ? (Color){ 100, 100, 130, 255 } : (Color){ 60, 60, 85, 255 };
    DrawRectangleRec(skip_btn, skip_col);
    DrawRectangleLinesEx(skip_btn, 1.0f, (Color){ 110, 110, 140, 200 });
    draw_text_box((Rectangle){ skip_btn.x + 5.0f, skip_btn.y + 5.0f, skip_btn.width - 10.0f, skip_btn.height - 8.0f },
        "Skip", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    if (g_state.discard_selected >= g_state.discard_count)
    {
        Rectangle confirm_btn = { (float)(VIRT_W / 2 - 90), (float)(VIRT_H - 40), 100.0f, 26.0f };
        bool hover = CheckCollisionPointRec(mouse, confirm_btn);
        Color btn_col = hover ? (Color){ 70, 180, 90, 255 } : (Color){ 45, 120, 60, 255 };
        DrawRectangleRec(confirm_btn, btn_col);
        DrawRectangleLinesEx(confirm_btn, 1.0f, (Color){ 100, 220, 120, 220 });
        draw_text_box((Rectangle){ confirm_btn.x + 5.0f, confirm_btn.y + 5.0f, confirm_btn.width - 10.0f, confirm_btn.height - 8.0f },
            "Confirm", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    }
}
