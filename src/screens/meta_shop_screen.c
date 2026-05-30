#include "screens.h"
#include "game.h"
#include "assets.h"
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
static float shop_scroll_y = 0.0f;

#define META_ITEM_COUNT 15
#define META_ITEM_W 292.0f
#define META_ITEM_H 76.0f
#define META_ITEM_GAP_X 12.0f
#define META_ITEM_GAP_Y 8.0f

static Rectangle shop_viewport(void)
{
    return (Rectangle){ 18.0f, 80.0f, 604.0f, 228.0f };
}

static float shop_content_height(void)
{
    int rows = (META_ITEM_COUNT + 1) / 2;
    return rows * META_ITEM_H + (rows - 1) * META_ITEM_GAP_Y;
}

static float shop_max_scroll(void)
{
    Rectangle view = shop_viewport();
    float max_scroll = shop_content_height() - view.height;
    return max_scroll > 0.0f ? max_scroll : 0.0f;
}

static void clamp_shop_scroll(void)
{
    float max_scroll = shop_max_scroll();
    if (shop_scroll_y < 0.0f) shop_scroll_y = 0.0f;
    if (shop_scroll_y > max_scroll) shop_scroll_y = max_scroll;
}

static Rectangle item_rect(int index)
{
    Rectangle view = shop_viewport();
    int col = index % 2;
    int row = index / 2;
    float total_w = 2.0f * META_ITEM_W + META_ITEM_GAP_X;
    float start_x = view.x + (view.width - total_w) * 0.5f;
    return (Rectangle){
        start_x + col * (META_ITEM_W + META_ITEM_GAP_X),
        view.y + row * (META_ITEM_H + META_ITEM_GAP_Y) - shop_scroll_y,
        META_ITEM_W,
        META_ITEM_H
    };
}

static bool item_hit(int index, Vector2 mouse)
{
    Rectangle view = shop_viewport();
    Rectangle r = item_rect(index);
    Rectangle buy = { r.x + 10.0f, r.y + r.height - (float)BTN_H - 4.0f, r.width - 20.0f, (float)BTN_H };
    return CheckCollisionPointRec(mouse, view) &&
           buy.y >= view.y &&
           buy.y + buy.height <= view.y + view.height &&
           CheckCollisionPointRec(mouse, buy);
}

static void init_if_needed(void)
{
    if (initialized) return;
    back_btn = button_create(
        (Rectangle){ (float)(VIRT_W / 2 - BTN_MED / 2), 330.0f, (float)BTN_MED, (float)BTN_H },
        "BACK",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);
    shop_msg[0] = '\0';
    shop_scroll_y = 0.0f;
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

static void try_buy_legacy(void)
{
    if (g_state.meta.starting_relic_rank >= META_LEGACY_MAX_RANK)
    {
        snprintf(shop_msg, sizeof(shop_msg), "Legacy is maxed.");
        return;
    }
    int cost = meta_next_legacy_cost(&g_state.meta);
    if (!spend_renown(cost))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", cost);
        return;
    }
    g_state.meta.starting_relic_rank++;
    meta_save(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "Legacy rank %d unlocked.", g_state.meta.starting_relic_rank);
}

static void try_buy_class(int class_index)
{
    if (meta_class_unlocked(&g_state.meta, class_index))
    {
        snprintf(shop_msg, sizeof(shop_msg), "%s is already unlocked.", class_name((ClassType)class_index));
        return;
    }
    if (!spend_renown(META_CLASS_UNLOCK_COST))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", META_CLASS_UNLOCK_COST);
        return;
    }
    meta_unlock_class(&g_state.meta, class_index);
    meta_save(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "%s unlocked.", class_name((ClassType)class_index));
}

static void try_buy_start_bool(bool *flag, const char *name, int cost)
{
    if (*flag)
    {
        snprintf(shop_msg, sizeof(shop_msg), "%s is already unlocked.", name);
        return;
    }
    if (!spend_renown(cost))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", cost);
        return;
    }
    *flag = true;
    meta_save(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "%s unlocked!", name);
}

static void try_buy_stat(int *stat, const char *name, int cost, int max)
{
    if (*stat >= max)
    {
        snprintf(shop_msg, sizeof(shop_msg), "%s is maxed.", name);
        return;
    }
    if (!spend_renown(cost))
    {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown.", cost);
        return;
    }
    (*stat)++;
    meta_save(&g_state.meta);
    snprintf(shop_msg, sizeof(shop_msg), "%s upgraded.", name);
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

    Vector2 mouse = GetMousePosition();
    Rectangle view = shop_viewport();
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && CheckCollisionPointRec(mouse, view))
        shop_scroll_y -= wheel * 28.0f;
    if (IsKeyDown(KEY_DOWN)) shop_scroll_y += 3.0f;
    if (IsKeyDown(KEY_UP)) shop_scroll_y -= 3.0f;
    if (IsKeyPressed(KEY_PAGE_DOWN)) shop_scroll_y += view.height * 0.85f;
    if (IsKeyPressed(KEY_PAGE_UP)) shop_scroll_y -= view.height * 0.85f;
    clamp_shop_scroll();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (!CheckCollisionPointRec(mouse, view))
            return;

        if (item_hit(0, mouse)) try_buy_slot4();
        else if (item_hit(1, mouse)) try_buy_slot5();
        else if (item_hit(2, mouse)) try_buy_travel_fund();
        else if (item_hit(3, mouse)) try_buy_legacy();
        else if (item_hit(4, mouse)) try_buy_class(CLASS_PALADIN);
        else if (item_hit(5, mouse)) try_buy_class(CLASS_WARLOCK);
        else if (item_hit(6, mouse)) try_buy_class(CLASS_BARD);
        else if (item_hit(7, mouse)) try_buy_start_bool(&g_state.meta.start_prep, "Scout's Kit", 10);
        else if (item_hit(8, mouse)) try_buy_start_bool(&g_state.meta.start_energize, "Mana Crystal", 10);
        else if (item_hit(9, mouse)) try_buy_start_bool(&g_state.meta.start_fortify, "Traveler's Pack", 15);
        else if (item_hit(10, mouse)) try_buy_start_bool(&g_state.meta.start_rejuv, "First Aid", 15);
        else if (item_hit(11, mouse)) try_buy_stat(&g_state.meta.dmg_bonus, "Sharpened Blades", 10, 3);
        else if (item_hit(12, mouse)) try_buy_stat(&g_state.meta.shield_bonus, "Reinforced Armor", 10, 3);
        else if (item_hit(13, mouse)) try_buy_start_bool(&g_state.meta.seasoned_adventurer, "Seasoned Adventurer", 15);
        else if (item_hit(14, mouse)) try_buy_start_bool(&g_state.meta.master_raider, "Master Raider", 25);
    }
}

static void draw_item(Rectangle r, const char *title, const char *body, const char *status, int cost, bool can_buy, bool complete)
{
    Vector2 mouse = GetMousePosition();
    Rectangle view = shop_viewport();

    Color bg = complete ? (Color){ 25, 44, 35, 238 } :
               can_buy ? (Color){ 24, 33, 50, 238 } :
               (Color){ 20, 21, 30, 225 };

    Color title_col = can_buy || complete ? RAYWHITE : (Color){ 112, 116, 135, 230 };
    Color body_col = can_buy || complete ? (Color){ 175, 184, 210, 225 } : (Color){ 95, 98, 116, 205 };

    DrawRectangleRec(r, bg);

    draw_text_box((Rectangle){ r.x + 10.0f, r.y + 7.0f, r.width - 20.0f, 14.0f },
        title, 10, 0, title_col, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ r.x + 10.0f, r.y + 23.0f, r.width - 20.0f, 28.0f },
        body, 10, 0, body_col, TEXT_ALIGN_LEFT);

    Rectangle buy = { r.x + 10.0f, r.y + r.height - (float)BTN_H - 4.0f, r.width - 20.0f, (float)BTN_H };
    Color buy_bg = complete ? (Color){ 42, 95, 58, 255 } :
                   can_buy ? (Color){ 46, 117, 182, 255 } :
                   (Color){ 38, 39, 50, 255 };

    char label[48];
    if (complete || cost <= 0)
        snprintf(label, sizeof(label), "%s", status);
    else
        snprintf(label, sizeof(label), "%s  %dR", status, cost);
    draw_btn_standard(buy, buy_bg, can_buy ? (Color){ 80, 160, 230, 255 } : buy_bg, label);
}

static void draw_scrollbar(Rectangle view)
{
    float max_scroll = shop_max_scroll();
    if (max_scroll <= 0.0f) return;

    Rectangle track = { view.x + view.width - 5.0f, view.y + 2.0f, 3.0f, view.height - 4.0f };
    float thumb_h = (view.height / shop_content_height()) * track.height;
    if (thumb_h < 24.0f) thumb_h = 24.0f;
    float travel = track.height - thumb_h;
    float t = max_scroll > 0.0f ? shop_scroll_y / max_scroll : 0.0f;
    Rectangle thumb = { track.x, track.y + travel * t, track.width, thumb_h };

    DrawRectangleRec(track, (Color){ 26, 28, 40, 230 });
    DrawRectangleRec(thumb, (Color){ 120, 152, 210, 230 });
}

void meta_shop_screen_draw(void)
{
    theme_draw_background();

    draw_text_box((Rectangle){ 80.0f, 22.0f, 480.0f, 22.0f }, "META SHOP", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    char line[96];
    snprintf(line, sizeof(line), "Renown: %d    Party max: %d/5    Gold: %d    Asc max: %d",
        g_state.meta.renown,
        meta_party_slots(&g_state.meta),
        meta_starting_gold(&g_state.meta),
        g_state.meta.max_ascension_unlocked);
    Rectangle summary = { 84.0f, 50.0f, 472.0f, 25.0f };
    DrawRectangleRec(summary, (Color){ 10, 12, 21, 220 });
    DrawRectangleLinesEx(summary, 1.0f, (Color){ 86, 106, 150, 165 });
    draw_text_box((Rectangle){ summary.x + 8.0f, summary.y + 6.0f, summary.width - 16.0f, 14.0f },
        line, 10, 0, (Color){ 190, 195, 220, 230 }, TEXT_ALIGN_CENTER);

    bool can_slot4 = !g_state.meta.slot4_unlocked && g_state.meta.renown >= META_SLOT4_COST;
    bool can_slot5 = g_state.meta.slot4_unlocked && !g_state.meta.slot5_unlocked && g_state.meta.renown >= META_SLOT5_COST;
    int fund_cost = meta_next_travel_fund_cost(&g_state.meta);
    bool fund_done = g_state.meta.starting_gold_rank >= META_TRAVEL_FUND_MAX_RANK;
    bool can_fund = !fund_done && g_state.meta.renown >= fund_cost;
    int legacy_cost = meta_next_legacy_cost(&g_state.meta);
    bool legacy_done = g_state.meta.starting_relic_rank >= META_LEGACY_MAX_RANK;
    bool can_legacy = !legacy_done && g_state.meta.renown >= legacy_cost;

    Rectangle view = shop_viewport();
    DrawRectangleRec(view, (Color){ 7, 8, 14, 188 });
    DrawRectangleLinesEx(view, 1.0f, (Color){ 72, 86, 122, 150 });

    BeginScissorMode((int)view.x + 1, (int)view.y + 1, (int)view.width - 1, (int)view.height - 1);

    draw_item(item_rect(0), "Party Slot IV", "Raise the draft cap to four heroes. You can still begin with fewer.", g_state.meta.slot4_unlocked ? "OWNED" : "BUY", META_SLOT4_COST, can_slot4, g_state.meta.slot4_unlocked);
    draw_item(item_rect(1), "Party Slot V", "Raise the draft cap to five heroes for full raid-party chaos.", g_state.meta.slot5_unlocked ? "OWNED" : (g_state.meta.slot4_unlocked ? "BUY" : "LOCKED"), META_SLOT5_COST, can_slot5, g_state.meta.slot5_unlocked);

    char fund_body[128];
    snprintf(fund_body, sizeof(fund_body), "Start each run with +%dg. Current rank %d/%d.",
        META_TRAVEL_FUND_GOLD_PER_RANK,
        g_state.meta.starting_gold_rank,
        META_TRAVEL_FUND_MAX_RANK);
    draw_item(item_rect(2), "Travel Fund", fund_body, fund_done ? "MAXED" : "BUY", fund_cost, can_fund, fund_done);
    draw_item(item_rect(3), "Legacy", "Start runs with relic help. Rank III lets you choose from two.", legacy_done ? "MAXED" : "BUY", legacy_cost, can_legacy, legacy_done);
    draw_item(item_rect(4), "Paladin", "Unlock hybrid tank/healer class cards.", g_state.meta.paladin_unlocked ? "OWNED" : "BUY", META_CLASS_UNLOCK_COST, !g_state.meta.paladin_unlocked && g_state.meta.renown >= META_CLASS_UNLOCK_COST, g_state.meta.paladin_unlocked);
    draw_item(item_rect(5), "Warlock", "Unlock DOT and curse class cards.", g_state.meta.warlock_unlocked ? "OWNED" : "BUY", META_CLASS_UNLOCK_COST, !g_state.meta.warlock_unlocked && g_state.meta.renown >= META_CLASS_UNLOCK_COST, g_state.meta.warlock_unlocked);
    draw_item(item_rect(6), "Bard", "Unlock party buff and draw class cards.", g_state.meta.bard_unlocked ? "OWNED" : "BUY", META_CLASS_UNLOCK_COST, !g_state.meta.bard_unlocked && g_state.meta.renown >= META_CLASS_UNLOCK_COST, g_state.meta.bard_unlocked);

    draw_item(item_rect(7), "Scout's Kit", "Start each run with Preparation in your deck.", g_state.meta.start_prep ? "OWNED" : "BUY", 10, !g_state.meta.start_prep && g_state.meta.renown >= 10, g_state.meta.start_prep);
    draw_item(item_rect(8), "Mana Crystal", "Start each run with Energize in your deck.", g_state.meta.start_energize ? "OWNED" : "BUY", 10, !g_state.meta.start_energize && g_state.meta.renown >= 10, g_state.meta.start_energize);
    draw_item(item_rect(9), "Traveler's Pack", "Start each run with Fortify in your deck.", g_state.meta.start_fortify ? "OWNED" : "BUY", 15, !g_state.meta.start_fortify && g_state.meta.renown >= 15, g_state.meta.start_fortify);
    draw_item(item_rect(10), "First Aid", "Start each run with Rejuvenate in your deck.", g_state.meta.start_rejuv ? "OWNED" : "BUY", 15, !g_state.meta.start_rejuv && g_state.meta.renown >= 15, g_state.meta.start_rejuv);
    draw_item(item_rect(11), "Sharpened Blades", "All attacks deal +1 damage. Can stack 3 times.", g_state.meta.dmg_bonus >= 3 ? "MAXED" : "BUY", 10, g_state.meta.dmg_bonus < 3 && g_state.meta.renown >= 10, g_state.meta.dmg_bonus >= 3);
    draw_item(item_rect(12), "Reinforced Armor", "All shields gain +1. Can stack 3 times.", g_state.meta.shield_bonus >= 3 ? "MAXED" : "BUY", 10, g_state.meta.shield_bonus < 3 && g_state.meta.renown >= 10, g_state.meta.shield_bonus >= 3);
    draw_item(item_rect(13), "Seasoned Adventurer", "+1 renown per boss defeated.", g_state.meta.seasoned_adventurer ? "OWNED" : "BUY", 15, !g_state.meta.seasoned_adventurer && g_state.meta.renown >= 15, g_state.meta.seasoned_adventurer);
    draw_item(item_rect(14), "Master Raider", "+2 renown per area cleared.", g_state.meta.master_raider ? "OWNED" : "BUY", 25, !g_state.meta.master_raider && g_state.meta.renown >= 25, g_state.meta.master_raider);

    EndScissorMode();

    draw_scrollbar(view);

    if (shop_msg[0])
        draw_text_box((Rectangle){ 96.0f, 310.0f, 448.0f, 16.0f }, shop_msg, 10, 0, (Color){ 230, 205, 115, 240 }, TEXT_ALIGN_CENTER);

    button_draw_9slice(&back_btn);
}
