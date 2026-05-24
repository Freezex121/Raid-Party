#include "screens.h"
#include "game.h"
#include "constants.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "util/text.h"
#include "util/math_utils.h"
#include "raylib.h"
#include <stdio.h>

static Button back_btn;
static bool initialized = false;
static char shop_msg[128] = "";

static Rectangle item_rect(int index)
{
    const float w = 178.0f;
    const float h = 118.0f;
    const float gap = 14.0f;
    float start_x = (VIRT_W - (3.0f * w + 2.0f * gap)) * 0.5f;
    return (Rectangle){ start_x + index * (w + gap), 102.0f, w, h };
}

static void init_if_needed(void)
{
    if (initialized) return;
    back_btn = button_create(
        (Rectangle){ (float)(VIRT_W / 2 - 64), 316.0f, 128.0f, (float)BTN_H },
        "BACK",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);
    shop_msg[0] = '\0';
    initialized = true;
}

static bool spend_renown(int cost)
{
    if (g_state.meta.renown < cost)
        return false;
    g_state.meta.renown -= cost;
    meta_save(&g_state.meta);
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    return true;
}

static void try_buy_slot4(void)
{
    if (g_state.meta.slot4_unlocked)
    {
        snprintf(shop_msg, sizeof(shop_msg), "Party Slot IV is already unlocked.");
        return;
    }
    if (!spend_renown(META_SLOT4_COST))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", META_SLOT4_COST);
        return;
    }
    g_state.meta.slot4_unlocked = true;
    meta_save(&g_state.meta);
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "Party Slot IV unlocked.");
}

static void try_buy_slot5(void)
{
    if (!g_state.meta.slot4_unlocked)
    {
        snprintf(shop_msg, sizeof(shop_msg), "Unlock Party Slot IV first.");
        return;
    }
    if (g_state.meta.slot5_unlocked)
    {
        snprintf(shop_msg, sizeof(shop_msg), "Party Slot V is already unlocked.");
        return;
    }
    if (!spend_renown(META_SLOT5_COST))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", META_SLOT5_COST);
        return;
    }
    g_state.meta.slot5_unlocked = true;
    meta_save(&g_state.meta);
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "Party Slot V unlocked.");
}

static void try_buy_travel_fund(void)
{
    if (g_state.meta.starting_gold_rank >= META_TRAVEL_FUND_MAX_RANK)
    {
        snprintf(shop_msg, sizeof(shop_msg), "Travel Fund is maxed.");
        return;
    }

    int cost = meta_next_travel_fund_cost(&g_state.meta);
    if (!spend_renown(cost))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", cost);
        return;
    }
    g_state.meta.starting_gold_rank++;
    meta_save(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "Runs now start with %dg.", meta_starting_gold(&g_state.meta));
}

void meta_shop_screen_update(void)
{
    init_if_needed();

    button_update(&back_btn);
    if (back_btn.pressed_this_frame || IsKeyPressed(KEY_ESCAPE))
    {
        initialized = false;
        game_change_screen(SCREEN_TITLE);
        return;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, item_rect(0))) try_buy_slot4();
        else if (CheckCollisionPointRec(mouse, item_rect(1))) try_buy_slot5();
        else if (CheckCollisionPointRec(mouse, item_rect(2))) try_buy_travel_fund();
    }
}

static void draw_item(Rectangle r, const char *title, const char *body, const char *status, int cost, bool can_buy, bool complete)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);
    Color bg = complete ? (Color){ 25, 44, 35, 238 } :
               can_buy && hover ? (Color){ 38, 55, 78, 245 } :
               can_buy ? (Color){ 24, 33, 50, 238 } :
               (Color){ 20, 21, 30, 225 };
    Color border = complete ? (Color){ 95, 210, 130, 210 } :
                   can_buy && hover ? RAYWHITE :
                   can_buy ? (Color){ 105, 150, 210, 190 } :
                   (Color){ 70, 72, 88, 170 };
    Color title_col = can_buy || complete ? RAYWHITE : (Color){ 112, 116, 135, 230 };
    Color body_col = can_buy || complete ? (Color){ 175, 184, 210, 225 } : (Color){ 95, 98, 116, 205 };

    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, hover && can_buy ? 2.0f : 1.0f, border);
    DrawText(title, (int)r.x + 9, (int)r.y + 9, 10, title_col);
    draw_text_wrapped(body, (int)r.x + 9, (int)r.y + 29, (int)r.width - 18, 10, 2, body_col);

    Rectangle buy = { r.x + 9.0f, r.y + r.height - 30.0f, r.width - 18.0f, 20.0f };
    Color buy_bg = complete ? (Color){ 42, 95, 58, 255 } :
                   can_buy ? (Color){ 46, 117, 182, 255 } :
                   (Color){ 38, 39, 50, 255 };
    DrawRectangleRec(buy, buy_bg);

    char label[48];
    if (complete || cost <= 0)
        snprintf(label, sizeof(label), "%s", status);
    else
        snprintf(label, sizeof(label), "%s  %dR", status, cost);
    Color label_col = can_buy || complete ? RAYWHITE : (Color){ 105, 108, 125, 220 };
    DrawText(label, snap_i(buy.x + buy.width / 2 - MeasureText(label, 10) / 2), snap_i(buy.y) + 7, 10, label_col);
}

void meta_shop_screen_draw(void)
{
    theme_draw_background();

    DrawText("META SHOP", VIRT_W / 2 - MeasureText("META SHOP", 18) / 2, 34, 18, RAYWHITE);

    char line[96];
    snprintf(line, sizeof(line), "Renown: %d    Party max: %d/5    Starting gold: %d",
        g_state.meta.renown,
        meta_party_slots(&g_state.meta),
        meta_starting_gold(&g_state.meta));
    DrawText(line, VIRT_W / 2 - MeasureText(line, 10) / 2, 62, 10, (Color){ 190, 195, 220, 230 });

    bool can_slot4 = !g_state.meta.slot4_unlocked && g_state.meta.renown >= META_SLOT4_COST;
    bool can_slot5 = g_state.meta.slot4_unlocked && !g_state.meta.slot5_unlocked && g_state.meta.renown >= META_SLOT5_COST;
    int fund_cost = meta_next_travel_fund_cost(&g_state.meta);
    bool fund_done = g_state.meta.starting_gold_rank >= META_TRAVEL_FUND_MAX_RANK;
    bool can_fund = !fund_done && g_state.meta.renown >= fund_cost;

    draw_item(item_rect(0), "Party Slot IV", "Raise the draft cap to four heroes. You can still begin with fewer.", g_state.meta.slot4_unlocked ? "OWNED" : "BUY", META_SLOT4_COST, can_slot4, g_state.meta.slot4_unlocked);
    draw_item(item_rect(1), "Party Slot V", "Raise the draft cap to five heroes for full raid-party chaos.", g_state.meta.slot5_unlocked ? "OWNED" : (g_state.meta.slot4_unlocked ? "BUY" : "LOCKED"), META_SLOT5_COST, can_slot5, g_state.meta.slot5_unlocked);

    char fund_body[128];
    snprintf(fund_body, sizeof(fund_body), "Start each run with +%dg. Current rank %d/%d.",
        META_TRAVEL_FUND_GOLD_PER_RANK,
        g_state.meta.starting_gold_rank,
        META_TRAVEL_FUND_MAX_RANK);
    draw_item(item_rect(2), "Travel Fund", fund_body, fund_done ? "MAXED" : "BUY", fund_cost, can_fund, fund_done);

    if (shop_msg[0])
        DrawText(shop_msg, VIRT_W / 2 - MeasureText(shop_msg, 10) / 2, 258, 10, (Color){ 230, 205, 115, 240 });

    button_draw(&back_btn);
}
