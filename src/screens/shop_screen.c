#include "screens.h"
#include "game.h"
#include "assets.h"
#include "constants.h"
#include "data/card_defs.h"
#include "systems/relic.h"
#include "systems/telemetry.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "ui/ui.h"
#include "util/text.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define UPGRADE_COST 30
#define SUPER_UPGRADE_COST 60
#define REMOVE_COST 20
#define CARD_SALE_COST 25
#define CARD_REROLL_COST 5
#define HP_BOOST_COST 15
#define BOON_COST 20
#define FATE_INTEREST_BOON_COST 30

typedef enum {
    SHOP_MAIN,
    SHOP_UPGRADE_1,
    SHOP_UPGRADE_2,
    SHOP_REMOVE,
    SHOP_HP_BOOST,
} ShopMode;

static ShopMode mode = SHOP_MAIN;
static int hovered_deck = -1;
static DeckBrowser shop_browser;
static char msg[128] = "";
static bool lucky_coin_given = false;
static int active_shop_key = -99999;
static const CardDef *shop_card = NULL;

static void log_shop_metric(const char *action, int cost, int gold_before, bool success, const char *card_id)
{
    char run_id[16], area[16], floor[16], cost_text[16], before[16], after[16], success_text[8];
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(area, sizeof(area), "%d", g_state.current_area);
    snprintf(floor, sizeof(floor), "%d", g_state.map.floor + 1);
    snprintf(cost_text, sizeof(cost_text), "%d", cost);
    snprintf(before, sizeof(before), "%d", gold_before);
    snprintf(after, sizeof(after), "%d", g_state.gold);
    snprintf(success_text, sizeof(success_text), "%d", success ? 1 : 0);
    const char *fields[] = {
        run_id,
        area,
        floor,
        action ? action : "",
        cost_text,
        before,
        after,
        success_text,
        card_id ? card_id : ""
    };
    telemetry_csv_append(
        "shop_metrics.csv",
        "timestamp,run_id,area,floor,action,cost,gold_before,gold_after,success,card_id",
        fields,
        9);

    char json[512];
    snprintf(json, sizeof(json),
        "\"area\":%d,\"floor\":%d,\"action\":\"%s\",\"cost\":%d,\"gold_before\":%d,\"gold_after\":%d,\"success\":%s,\"card_id\":\"%s\"",
        g_state.current_area,
        g_state.map.floor + 1,
        action ? action : "",
        cost,
        gold_before,
        g_state.gold,
        success ? "true" : "false",
        card_id ? card_id : "");
    telemetry_push_json("shop_purchase", json);
}

static Rectangle shop_browser_bounds(void)
{
    return layout_deck_browser_viewport();
}

static int valid_card_count(const Deck *deck)
{
    int count = 0;
    if (!deck) return 0;
    for (int i = 0; i < deck->card_count; i++)
        if (deck->cards[i].def)
            count++;
    return count;
}

static const CardDef *random_party_card(void)
{
    const CardDef *pool[96];
    int count = 0;

    for (int i = 0; i < g_state.selected_count; i++)
    {
        ClassType ct = (ClassType)g_state.selected_classes[i];
        if (ct < 0 || ct >= CLASS_COUNT || !class_card_sets[ct]) continue;
        for (int c = 0; c < class_card_counts[ct] && count < 96; c++)
            pool[count++] = &class_card_sets[ct][c];
    }

    for (int c = 0; c < utility_card_count && count < 96; c++)
        pool[count++] = &utility_cards[c];

    if (count <= 0)
        return utility_card_count > 0 ? &utility_cards[0] : card_def_by_id("util_prep");
    return pool[rand() % count];
}

static void reset_shop_for_visit(void)
{
    int key = g_state.map.floor * 1000 + g_state.map.current_index;
    if (key == active_shop_key) return;
    active_shop_key = key;
    mode = SHOP_MAIN;
    hovered_deck = -1;
    deck_browser_reset(&shop_browser);
    msg[0] = '\0';
    lucky_coin_given = false;
    shop_card = random_party_card();
}

static void complete_shop(void)
{
    int ci = g_state.map.current_index;
    if (ci >= 0 && ci < g_state.map.node_count)
        g_state.map.nodes[ci].completed = true;
    g_state.map.current_index = -1;
    map_unlock_next(&g_state.map);
    active_shop_key = -99999;
    lucky_coin_given = false;
    mode = SHOP_MAIN;
    game_change_screen(SCREEN_MAP);
}

static int shop_boon_cost(void)
{
    return relic_has(g_state.relics, g_state.relic_count, RELIC_FATES_INTEREST) ? FATE_INTEREST_BOON_COST : BOON_COST;
}

static int shop_boon_turns(void)
{
    return relic_has(g_state.relics, g_state.relic_count, RELIC_FATES_INTEREST) ? 3 : 1;
}

static void buy_boon(bool energy)
{
    int cost = shop_boon_cost();
    int gold_before = g_state.gold;
    if (!game_spend_gold(cost, energy ? "shop_boon_energy" : "shop_boon_draw"))
    {
        snprintf(msg, sizeof(msg), "Need %dg.", cost);
        log_shop_metric(energy ? "energy_boon" : "draw_boon", cost, gold_before, false, "");
        return;
    }

    if (energy)
    {
        g_state.next_combat_energy_bonus += 1;
        snprintf(msg, sizeof(msg), "Next combat: +1 energy.");
    }
    else
    {
        g_state.next_combat_draw_bonus += 2;
        snprintf(msg, sizeof(msg), "Next combat: +2 draw.");
    }

    int turns = shop_boon_turns();
    if (g_state.next_combat_boon_turns < turns)
        g_state.next_combat_boon_turns = turns;
    log_shop_metric(energy ? "energy_boon" : "draw_boon", cost, gold_before, true, "");
}

static Rectangle sale_card_rect(void)
{
    return (Rectangle){ 60.0f, 86.0f, 96.0f, 120.0f };
}

static Rectangle sale_buy_button(void)
{
    return (Rectangle){ 28.0f, 214.0f, (float)BTN_WIDE, (float)BTN_H };
}

static Rectangle sale_reroll_button(void)
{
    return (Rectangle){ 48.0f, 244.0f, (float)BTN_MED, (float)BTN_H };
}

static Rectangle option_rect(int col, int row)
{
    return (Rectangle){ 212.0f + col * 204.0f, 74.0f + row * 58.0f, (float)BTN_FULL, 44.0f };
}

static Rectangle leave_button(void)
{
    return (Rectangle){ 520.0f, 286.0f, (float)BTN_NARROW, (float)BTN_H };
}

static bool clicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r);
}

void shop_screen_update(void)
{
    reset_shop_for_visit();

    if (!g_state.tutorial_active)
        game_start_tutorial_once(&g_state.meta.tutorial_seen_shop, TUTORIAL_STEP_SHOP);
    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_SHOP)
    {
        if (game_tutorial_handle_close())
            return;
    }

    if (!lucky_coin_given && relic_has(g_state.relics, g_state.relic_count, RELIC_LUCKY_COIN))
    {
        game_gain_gold(10, "lucky_coin_shop");
        lucky_coin_given = true;
    }

    if (mode == SHOP_MAIN)
    {
        bool can_upg1 = deck_browser_has_upgradeable_at(&g_state.run_deck, 1);
        bool can_upg2 = deck_browser_has_upgradeable_at(&g_state.run_deck, 2);
        bool can_remove = valid_card_count(&g_state.run_deck) > 3;
        bool deck_space = g_state.run_deck.card_count < MAX_DECK_SIZE;

        if (clicked(sale_buy_button()))
        {
            int gold_before = g_state.gold;
            if (!deck_space)
            {
                snprintf(msg, sizeof(msg), "Deck is full.");
                log_shop_metric("buy_card", CARD_SALE_COST, gold_before, false, shop_card && shop_card->id ? shop_card->id : "");
            }
            else if (!game_spend_gold(CARD_SALE_COST, "shop_buy_card"))
            {
                snprintf(msg, sizeof(msg), "Need %dg.", CARD_SALE_COST);
                log_shop_metric("buy_card", CARD_SALE_COST, gold_before, false, shop_card && shop_card->id ? shop_card->id : "");
            }
            else
            {
                deck_add_card(&g_state.run_deck, shop_card);
                snprintf(msg, sizeof(msg), "Bought %s.", shop_card ? shop_card->name : "a card");
                log_shop_metric("buy_card", CARD_SALE_COST, gold_before, true, shop_card && shop_card->id ? shop_card->id : "");
                shop_card = random_party_card();
            }
        }
        else if (clicked(sale_reroll_button()))
        {
            int gold_before = g_state.gold;
            if (!game_spend_gold(CARD_REROLL_COST, "shop_reroll_card"))
            {
                snprintf(msg, sizeof(msg), "Need %dg.", CARD_REROLL_COST);
                log_shop_metric("reroll_card", CARD_REROLL_COST, gold_before, false, "");
            }
            else
            {
                shop_card = random_party_card();
                snprintf(msg, sizeof(msg), "Shop card rerolled.");
                log_shop_metric("reroll_card", CARD_REROLL_COST, gold_before, true, "");
            }
        }
        else if (clicked(option_rect(0, 0)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < UPGRADE_COST)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", UPGRADE_COST);
                log_shop_metric("upgrade_card_select", UPGRADE_COST, gold_before, false, "");
            }
            else if (!can_upg1)
            {
                snprintf(msg, sizeof(msg), "No base cards can improve.");
                log_shop_metric("upgrade_card_select", UPGRADE_COST, gold_before, false, "");
            }
            else
            {
                mode = SHOP_UPGRADE_1;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(1, 0)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < SUPER_UPGRADE_COST)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", SUPER_UPGRADE_COST);
                log_shop_metric("max_upgrade_select", SUPER_UPGRADE_COST, gold_before, false, "");
            }
            else if (!can_upg2)
            {
                snprintf(msg, sizeof(msg), "No upgraded cards can improve.");
                log_shop_metric("max_upgrade_select", SUPER_UPGRADE_COST, gold_before, false, "");
            }
            else
            {
                mode = SHOP_UPGRADE_2;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(0, 1)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < REMOVE_COST)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", REMOVE_COST);
                log_shop_metric("remove_card_select", REMOVE_COST, gold_before, false, "");
            }
            else if (!can_remove)
            {
                snprintf(msg, sizeof(msg), "Deck too small.");
                log_shop_metric("remove_card_select", REMOVE_COST, gold_before, false, "");
            }
            else
            {
                mode = SHOP_REMOVE;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(1, 1)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < HP_BOOST_COST)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", HP_BOOST_COST);
                log_shop_metric("train_hp_select", HP_BOOST_COST, gold_before, false, "");
            }
            else
            {
                mode = SHOP_HP_BOOST;
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(0, 2)))
        {
            buy_boon(true);
        }
        else if (clicked(option_rect(1, 2)))
        {
            buy_boon(false);
        }
        else if (clicked(leave_button()))
        {
            log_shop_metric("leave", 0, g_state.gold, true, "");
            complete_shop();
        }
    }
    else if (mode == SHOP_UPGRADE_1 || mode == SHOP_UPGRADE_2 || mode == SHOP_REMOVE)
    {
        int target_level = mode == SHOP_UPGRADE_1 ? 1 : (mode == SHOP_UPGRADE_2 ? 2 : 0);
        int selected = deck_browser_update(&shop_browser, &g_state.run_deck, shop_browser_bounds(), target_level);
        hovered_deck = shop_browser.hovered_deck_index;

        if (selected >= 0)
        {
            CardInstance *inst = &g_state.run_deck.cards[selected];
            if (mode == SHOP_UPGRADE_1)
            {
                int gold_before = g_state.gold;
                if (game_spend_gold(UPGRADE_COST, "shop_upgrade"))
                {
                    inst->upgrade_level = 1;
                    snprintf(msg, sizeof(msg), "%s upgraded.", inst->def ? inst->def->name : "Card");
                    log_shop_metric("upgrade_card", UPGRADE_COST, gold_before, true, inst->def && inst->def->id ? inst->def->id : "");
                    mode = SHOP_MAIN;
                }
                else
                    log_shop_metric("upgrade_card", UPGRADE_COST, gold_before, false, inst->def && inst->def->id ? inst->def->id : "");
            }
            else if (mode == SHOP_UPGRADE_2)
            {
                int gold_before = g_state.gold;
                if (game_spend_gold(SUPER_UPGRADE_COST, "shop_super_upgrade"))
                {
                    inst->upgrade_level = 2;
                    snprintf(msg, sizeof(msg), "%s maxed.", inst->def ? inst->def->name : "Card");
                    log_shop_metric("max_upgrade_card", SUPER_UPGRADE_COST, gold_before, true, inst->def && inst->def->id ? inst->def->id : "");
                    mode = SHOP_MAIN;
                }
                else
                    log_shop_metric("max_upgrade_card", SUPER_UPGRADE_COST, gold_before, false, inst->def && inst->def->id ? inst->def->id : "");
            }
            else
            {
                int gold_before = g_state.gold;
                if (game_spend_gold(REMOVE_COST, "shop_remove"))
                {
                    const CardDef *def = inst->def;
                    int uid = inst->uid;
                    deck_remove_card_by_uid(&g_state.run_deck, uid);
                    snprintf(msg, sizeof(msg), "Removed %s.", def ? def->name : "a card");
                    log_shop_metric("remove_card", REMOVE_COST, gold_before, true, def && def->id ? def->id : "");
                    mode = SHOP_MAIN;
                }
                else
                    log_shop_metric("remove_card", REMOVE_COST, gold_before, false, inst->def && inst->def->id ? inst->def->id : "");
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            mode = SHOP_MAIN;
            hovered_deck = -1;
        }
    }
    else if (mode == SHOP_HP_BOOST)
    {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < g_state.run_party.count; i++)
        {
            Rectangle r = { (float)(VIRT_W / 2 - BTN_FULL / 2), 86.0f + i * 38.0f, (float)BTN_FULL, 28.0f };
            if (!CheckCollisionPointRec(mouse, r) || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                continue;
            int gold_before = g_state.gold;
            if (game_spend_gold(HP_BOOST_COST, "shop_hp_boost"))
            {
                PartyMember *pm = &g_state.run_party.members[i];
                pm->max_hp += 5;
                pm->hp += 5;
                if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;
                pm->alive = true;
                snprintf(msg, sizeof(msg), "%s gained +5 max HP.", pm->name);
                log_shop_metric("train_hp", HP_BOOST_COST, gold_before, true, class_name(pm->class));
                mode = SHOP_MAIN;
            }
            else
                log_shop_metric("train_hp", HP_BOOST_COST, gold_before, false, "");
            break;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            mode = SHOP_MAIN;
    }
}

static void draw_shop_panel(Rectangle r, const char *title, Color accent)
{
    DrawRectangleRec(r, (Color){ 13, 14, 24, 230 });
    DrawRectangleLinesEx(r, 1.0f, (Color){ accent.r, accent.g, accent.b, 155 });
    if (title && title[0])
        draw_text_box((Rectangle){ r.x + 8.0f, r.y + 7.0f, r.width - 16.0f, 12.0f },
            title, 10, 0, accent, TEXT_ALIGN_CENTER);
}

static void draw_shop_button(Rectangle r, const char *label, const char *body, bool enabled, Color accent)
{
    Vector2 mouse = GetMousePosition();
    bool hover = enabled && CheckCollisionPointRec(mouse, r);
    Color bg = !enabled ? (Color){ 28, 29, 38, 230 } :
        hover ? (Color){ accent.r, accent.g, accent.b, 210 } : (Color){ accent.r / 3, accent.g / 3, accent.b / 3, 235 };
    Color title_col = enabled ? RAYWHITE : (Color){ 110, 112, 130, 215 };
    Color body_col = enabled ? (Color){ 185, 190, 215, 220 } : (Color){ 95, 98, 115, 200 };

    Texture2D tex = r.height >= 40.0f ? g_assets.btn_large : g_assets.btn_standard;
    draw_9slice(tex, r.height >= 40.0f ? 8 : 6, r.height >= 40.0f ? 8 : 6, r, bg);

    int label_h = label && label[0] ? measure_text_box(label, (int)r.width - 16, 10, 0) : 0;
    if (label_h <= 0 && label && label[0]) label_h = ui_line_height(10);
    int body_h = body && body[0] ? measure_text_box(body, (int)r.width - 16, 10, 0) : 0;
    if (body_h <= 0 && body && body[0]) body_h = ui_line_height(10);
    int gap = label_h > 0 && body_h > 0 ? 2 : 0;
    int total_h = label_h + gap + body_h;
    int y = (int)(r.y + (r.height - total_h) * 0.5f);

    draw_text_box((Rectangle){ r.x + 8.0f, (float)y, r.width - 16.0f, (float)label_h },
        label, 10, 0, title_col, TEXT_ALIGN_CENTER);
    if (body && body[0])
        draw_text_box((Rectangle){ r.x + 8.0f, (float)(y + label_h + gap), r.width - 16.0f, (float)body_h },
            body, 10, 0, body_col, TEXT_ALIGN_CENTER);
}

static void draw_upgrade_preview(const CardDef *cd, int level, Rectangle tip)
{
    int preview_y = (int)(tip.y + tip.height + 5.0f);
    if (!cd || !card_upgrade_changes_values_at(cd, level))
    {
        draw_text_box((Rectangle){ tip.x, (float)preview_y, tip.width, 24.0f },
            "Upgrade has no value changes", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_LEFT);
        return;
    }

    int od = card_damage(cd, level), oh = card_heal(cd, level), os = card_shield(cd, level);
    int nd = card_damage(cd, level + 1), nh = card_heal(cd, level + 1), ns = card_shield(cd, level + 1);
    int y = preview_y;
    if (nd != od) { char b[32]; snprintf(b, sizeof(b), "DMG %d>%d", od, nd); DrawText(b, (int)tip.x, y, 10, (Color){ 220, 110, 100, 255 }); y += 14; }
    if (nh != oh) { char b[32]; snprintf(b, sizeof(b), "HEAL %d>%d", oh, nh); DrawText(b, (int)tip.x, y, 10, (Color){ 105, 220, 125, 255 }); y += 14; }
    if (ns != os) { char b[32]; snprintf(b, sizeof(b), "SHIELD %d>%d", os, ns); DrawText(b, (int)tip.x, y, 10, (Color){ 125, 190, 255, 255 }); }
}

void shop_screen_draw(void)
{
    theme_draw_background();

    if (mode == SHOP_MAIN)
    {
        draw_text_box((Rectangle){ 80.0f, 18.0f, 480.0f, 22.0f }, "SHOP", 18, 0, (Color){ 220, 200, 60, 255 }, TEXT_ALIGN_CENTER);

        Rectangle sale_panel = { 24.0f, 58.0f, 168.0f, 220.0f };
        Rectangle service_panel = { 204.0f, 58.0f, 416.0f, 202.0f };
        draw_shop_panel(sale_panel, "CARD FOR SALE - 25g", (Color){ 90, 160, 230, 230 });
        draw_shop_panel(service_panel, "SERVICES", (Color){ 220, 200, 90, 230 });

        Rectangle card_rect = sale_card_rect();
        if (shop_card)
            theme_draw_card_art_seeded(card_rect, shop_card, 0, theme_card_seed_from_id(shop_card->id, 91u));

        bool deck_space = g_state.run_deck.card_count < MAX_DECK_SIZE;
        draw_shop_button(sale_buy_button(), "BUY CARD - 25g", "", g_state.gold >= CARD_SALE_COST && deck_space, (Color){ 80, 150, 220, 255 });
        draw_shop_button(sale_reroll_button(), "NEW CARD - 5g", "", g_state.gold >= CARD_REROLL_COST, (Color){ 95, 190, 110, 255 });

        bool can_upg1 = g_state.gold >= UPGRADE_COST && deck_browser_has_upgradeable_at(&g_state.run_deck, 1);
        bool can_upg2 = g_state.gold >= SUPER_UPGRADE_COST && deck_browser_has_upgradeable_at(&g_state.run_deck, 2);
        bool can_remove = g_state.gold >= REMOVE_COST && valid_card_count(&g_state.run_deck) > 3;
        int boon_cost = shop_boon_cost();
        bool can_boon = g_state.gold >= boon_cost;

        draw_shop_button(option_rect(0, 0), "UPGRADE - 30g", "base card -> upgraded", can_upg1, (Color){ 80, 140, 220, 255 });
        draw_shop_button(option_rect(1, 0), "MAX UPGRADE - 60g", "upgraded card -> max", can_upg2, (Color){ 205, 165, 70, 255 });
        draw_shop_button(option_rect(0, 1), "REMOVE CARD - 20g", "thin your deck", can_remove, (Color){ 220, 100, 100, 255 });
        draw_shop_button(option_rect(1, 1), "TRAIN HP - 15g", "+5 max HP to one PM", g_state.gold >= HP_BOOST_COST, (Color){ 90, 200, 120, 255 });

        char boon_label[32];
        snprintf(boon_label, sizeof(boon_label), "ENERGY BOON - %dg", boon_cost);
        draw_shop_button(option_rect(0, 2), boon_label, "+1 next combat", can_boon, (Color){ 230, 205, 70, 255 });
        snprintf(boon_label, sizeof(boon_label), "DRAW BOON - %dg", boon_cost);
        draw_shop_button(option_rect(1, 2), boon_label, "+2 next combat", can_boon, (Color){ 135, 190, 245, 255 });

        if (g_state.next_combat_energy_bonus > 0 || g_state.next_combat_draw_bonus > 0)
        {
            char boon[96];
            snprintf(boon, sizeof(boon), "Queued boon: +%d energy, +%d draw for %d turn%s",
                g_state.next_combat_energy_bonus,
                g_state.next_combat_draw_bonus,
                g_state.next_combat_boon_turns,
                g_state.next_combat_boon_turns == 1 ? "" : "s");
            draw_text_box((Rectangle){ 210.0f, 266.0f, 278.0f, 24.0f }, boon, 10, 0, (Color){ 230, 220, 140, 230 }, TEXT_ALIGN_LEFT);
        }

        draw_shop_button(leave_button(), "LEAVE", "", true, (Color){ 120, 120, 145, 255 });
        if (msg[0])
            draw_text_box((Rectangle){ 210.0f, 292.0f, 278.0f, 28.0f }, msg, 10, 0, (Color){ 230, 205, 115, 240 }, TEXT_ALIGN_LEFT);
    }
    else if (mode == SHOP_UPGRADE_1 || mode == SHOP_UPGRADE_2)
    {
        int target_level = mode == SHOP_UPGRADE_1 ? 1 : 2;
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f },
            target_level == 1 ? "PICK A CARD TO UPGRADE" : "PICK A CARD TO MAX",
            18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f },
            "wheel scroll  |  right-click cancel", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);
        deck_browser_draw(&shop_browser, &g_state.run_deck, target_level, RAYWHITE);
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
        {
            CardInstance *inst = &g_state.run_deck.cards[hovered_deck];
            Rectangle tip = theme_draw_card_tooltip_limited(layout_deck_inspector_panel(), inst->def, inst->upgrade_level, 268);
            draw_upgrade_preview(inst->def, inst->upgrade_level, tip);
        }
    }
    else if (mode == SHOP_REMOVE)
    {
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f }, "PICK A CARD TO REMOVE", 18, 0, (Color){ 220, 120, 120, 255 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f }, "wheel scroll  |  right-click cancel", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);
        deck_browser_draw(&shop_browser, &g_state.run_deck, 0, (Color){ 255, 100, 100, 255 });
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgrade_level);
    }
    else if (mode == SHOP_HP_BOOST)
    {
        draw_text_box((Rectangle){ 80.0f, 36.0f, 480.0f, 22.0f }, "TRAIN A HERO", 18, 0, (Color){ 120, 230, 150, 255 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 58.0f, 480.0f, 14.0f }, "Pick one party member. Right-click cancels.", 10, 0, (Color){ 170, 180, 205, 210 }, TEXT_ALIGN_CENTER);
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < g_state.run_party.count; i++)
        {
            PartyMember *pm = &g_state.run_party.members[i];
            Rectangle r = { (float)(VIRT_W / 2 - BTN_FULL / 2), 86.0f + i * 38.0f, (float)BTN_FULL, 28.0f };
            bool hover = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, hover ? (Color){ 36, 72, 48, 245 } : (Color){ 22, 36, 30, 235 });
            DrawRectangleLinesEx(r, 1.0f, hover ? RAYWHITE : (Color){ 95, 210, 130, 180 });
            char line[96];
            snprintf(line, sizeof(line), "%s  %d/%d HP  ->  max %d", pm->name, pm->hp, pm->max_hp, pm->max_hp + 5);
            draw_text_box((Rectangle){ r.x + 8.0f, r.y + 8.0f, r.width - 16.0f, 12.0f },
                line, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        }
    }

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_SHOP)
    {
        game_draw_tutorial_overlay_ex((Rectangle){ 24.0f, 58.0f, 596.0f, 220.0f },
            "Shops",
            "Spend gold on cards, upgrades, removals, HP training, or next-combat boons. Grey options are unaffordable or unavailable.",
            "Click to continue  |  Right-click/Esc: skip",
            0,
            0);
    }
}
