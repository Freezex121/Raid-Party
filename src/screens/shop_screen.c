#include "screens.h"
#include "game.h"
#include "constants.h"
#include "systems/relic.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "util/math_utils.h"
#include "util/text.h"
#include "raylib.h"
#include <stdio.h>

#define UPGRADE_COST 30
#define REMOVE_COST 20

typedef enum {
    SHOP_MAIN,
    SHOP_UPGRADE,
    SHOP_REMOVE,
    SHOP_DONE,
} ShopMode;

static ShopMode mode = SHOP_MAIN;
static int hovered_deck = -1;
static DeckBrowser shop_browser;
static char msg[128] = "";
static bool lucky_coin_given = false;

static Rectangle shop_upgrade_button(void)
{
    return (Rectangle){ (float)((VIRT_W / 2) - 190), 154.0f, 170.0f, 44.0f };
}

static Rectangle shop_remove_button(void)
{
    return (Rectangle){ (float)((VIRT_W / 2) + 20), 154.0f, 170.0f, 44.0f };
}

static Rectangle shop_browser_bounds(void)
{
    return layout_deck_browser_viewport();
}

void shop_screen_update(void)
{
    // Lucky Coin: gain 10g once per shop visit
    if (!lucky_coin_given && relic_has(g_state.relics, g_state.relic_count, RELIC_LUCKY_COIN))
    {
        g_state.gold += 10;
        lucky_coin_given = true;
    }

    if (mode == SHOP_MAIN)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 m = GetMousePosition();
            Rectangle upg_btn = shop_upgrade_button();
            Rectangle rem_btn = shop_remove_button();

            if (CheckCollisionPointRec(m, upg_btn) && g_state.gold >= UPGRADE_COST && deck_browser_has_upgradeable(&g_state.run_deck))
            {
                mode = SHOP_UPGRADE;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
            else if (CheckCollisionPointRec(m, upg_btn) && g_state.gold < UPGRADE_COST)
            {
                snprintf(msg, sizeof(msg), "Not enough gold! Need %dg", UPGRADE_COST);
            }
            else if (CheckCollisionPointRec(m, upg_btn))
            {
                snprintf(msg, sizeof(msg), "No cards left to upgrade!");
            }

            if (CheckCollisionPointRec(m, rem_btn) && g_state.gold >= REMOVE_COST && g_state.run_deck.card_count > 3)
            {
                mode = SHOP_REMOVE;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
            else if (CheckCollisionPointRec(m, rem_btn) && g_state.gold < REMOVE_COST)
            {
                snprintf(msg, sizeof(msg), "Not enough gold! Need %dg", REMOVE_COST);
            }
            else if (CheckCollisionPointRec(m, rem_btn) && g_state.run_deck.card_count <= 3)
            {
                snprintf(msg, sizeof(msg), "Deck too small to remove!");
            }

            // Leave button
            Rectangle leave_btn = { (float)(VIRT_W / 2 - 40), 214.0f, 80.0f, 22.0f };
            if (CheckCollisionPointRec(m, leave_btn))
            {
                g_state.map.nodes[g_state.map.current_index].completed = true;
                g_state.map.current_index = -1;
                map_unlock_next(&g_state.map);
                lucky_coin_given = false;
                game_change_screen(SCREEN_MAP);
            }
        }
    }
    else if (mode == SHOP_UPGRADE || mode == SHOP_REMOVE)
    {
        int selected = deck_browser_update(&shop_browser, &g_state.run_deck, shop_browser_bounds(), mode == SHOP_UPGRADE);
        hovered_deck = shop_browser.hovered_deck_index;

        if (selected >= 0)
        {
            if (mode == SHOP_UPGRADE)
            {
                g_state.gold -= UPGRADE_COST;
                g_state.run_deck.cards[selected].upgraded = true;
                LOG_I(CAT_SCREEN, "Shop: upgraded %s", g_state.run_deck.cards[selected].def->name);
                snprintf(msg, sizeof(msg), "Card upgraded!");
            }
            else
            {
                g_state.gold -= REMOVE_COST;
                for (int j = selected; j < g_state.run_deck.card_count - 1; j++)
                    g_state.run_deck.cards[j] = g_state.run_deck.cards[j + 1];
                g_state.run_deck.card_count--;
                LOG_I(CAT_SCREEN, "Shop: removed card at index %d", selected);
                snprintf(msg, sizeof(msg), "Card removed!");
            }
            mode = SHOP_DONE;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            mode = SHOP_MAIN;
            msg[0] = '\0';
        }
    }
    else if (mode == SHOP_DONE)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            mode = SHOP_MAIN;
            g_state.map.nodes[g_state.map.current_index].completed = true;
            g_state.map.current_index = -1;
            map_unlock_next(&g_state.map);
            lucky_coin_given = false;
            game_change_screen(SCREEN_MAP);
        }
    }
}

void shop_screen_draw(void)
{
    theme_draw_background();

    draw_text_box((Rectangle){ 80.0f, 72.0f, 480.0f, 22.0f }, "SHOP", 18, 0, (Color){ 220, 200, 60, 255 }, TEXT_ALIGN_CENTER);

    char gold_text[32];
    snprintf(gold_text, sizeof(gold_text), "Gold: %d", g_state.gold);
    draw_text_box((Rectangle){ 80.0f, 100.0f, 480.0f, 14.0f }, gold_text, 10, 0, (Color){ 220, 200, 60, 220 }, TEXT_ALIGN_CENTER);

    if (mode == SHOP_MAIN)
    {
        Vector2 m = GetMousePosition();
        Rectangle upg_btn = shop_upgrade_button();
        Rectangle rem_btn = shop_remove_button();

        bool can_upg = g_state.gold >= UPGRADE_COST && deck_browser_has_upgradeable(&g_state.run_deck);
        bool can_rem = g_state.gold >= REMOVE_COST && g_state.run_deck.card_count > 3;

        bool hu = CheckCollisionPointRec(m, upg_btn);
        bool hr = CheckCollisionPointRec(m, rem_btn);

        Color uc = can_upg ? (hu ? (Color){ 80, 140, 220, 255 } : (Color){ 50, 80, 140, 255 }) : (Color){ 40, 40, 60, 255 };
        Color rc = can_rem ? (hr ? (Color){ 220, 100, 100, 255 } : (Color){ 160, 60, 60, 255 }) : (Color){ 40, 40, 60, 255 };

        DrawRectangleRec(upg_btn, uc);
        char ul[64]; snprintf(ul, sizeof(ul), "UPGRADE (%dg)", UPGRADE_COST);
        draw_text_box((Rectangle){ upg_btn.x + 6.0f, upg_btn.y + 8.0f, upg_btn.width - 12.0f, upg_btn.height - 14.0f },
            ul, 10, 0, can_upg ? RAYWHITE : (Color){ 100, 100, 120, 200 }, TEXT_ALIGN_CENTER);

        DrawRectangleRec(rem_btn, rc);
        char rl[64]; snprintf(rl, sizeof(rl), "REMOVE (%dg)", REMOVE_COST);
        draw_text_box((Rectangle){ rem_btn.x + 6.0f, rem_btn.y + 8.0f, rem_btn.width - 12.0f, rem_btn.height - 14.0f },
            rl, 10, 0, can_rem ? RAYWHITE : (Color){ 100, 100, 120, 200 }, TEXT_ALIGN_CENTER);

        if (msg[0])
            draw_text_box((Rectangle){ 96.0f, 224.0f, 448.0f, 28.0f }, msg, 10, 0, (Color){ 200, 150, 100, 220 }, TEXT_ALIGN_CENTER);

        // Leave button
        Rectangle leave_btn = { (float)(VIRT_W / 2 - 40), 214.0f, 80.0f, 22.0f };
        bool leave_hover = CheckCollisionPointRec(m, leave_btn);
        Color leave_col = leave_hover ? (Color){ 100, 100, 100, 255 } : (Color){ 60, 60, 60, 255 };
        DrawRectangleRec(leave_btn, leave_col);
        DrawRectangleLinesEx(leave_btn, 1.0f, (Color){ 120, 120, 120, 200 });
        draw_text_box((Rectangle){ leave_btn.x + 4.0f, leave_btn.y + 4.0f, leave_btn.width - 8.0f, leave_btn.height - 8.0f },
            "Leave", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    }
    else if (mode == SHOP_UPGRADE)
    {
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f }, "PICK A CARD TO UPGRADE", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        char hint[80];
        snprintf(hint, sizeof(hint), "%d cards  |  wheel scroll  |  right-click cancel", g_state.run_deck.card_count);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f }, hint, 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);
        deck_browser_draw(&shop_browser, &g_state.run_deck, true, RAYWHITE);
        if (hovered_deck >= 0)
        {
            const CardDef *cd = g_state.run_deck.cards[hovered_deck].def;
            if (cd)
            {
                Rectangle tip = theme_draw_card_tooltip_limited(layout_deck_inspector_panel(), cd, g_state.run_deck.cards[hovered_deck].upgraded, 268);
                int preview_y = (int)(tip.y + tip.height + 5.0f);
                if (g_state.run_deck.cards[hovered_deck].upgraded)
                {
                    draw_text_box((Rectangle){ tip.x, (float)preview_y, tip.width, 24.0f },
                        "Already upgraded", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_LEFT);
                }
                else if (!card_upgrade_changes_values(cd))
                {
                    draw_text_box((Rectangle){ tip.x, (float)preview_y, tip.width, 24.0f },
                        "Upgrade has no value changes", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_LEFT);
                }
                else
                {
                    int od = cd->damage, oh = cd->heal, os = cd->shield;
                    int nd = card_damage(cd, true), nh = card_heal(cd, true), ns = card_shield(cd, true);
                    int y = preview_y;
                    if (nd != od) { char b[32]; snprintf(b, sizeof(b), "DMG %d>%d", od, nd); DrawText(b, (int)tip.x, y, 10, (Color){ 220, 110, 100, 255 }); y += 14; }
                    if (nh != oh) { char b[32]; snprintf(b, sizeof(b), "HEAL %d>%d", oh, nh); DrawText(b, (int)tip.x, y, 10, (Color){ 105, 220, 125, 255 }); y += 14; }
                    if (ns != os) { char b[32]; snprintf(b, sizeof(b), "SHIELD %d>%d", os, ns); DrawText(b, (int)tip.x, y, 10, (Color){ 125, 190, 255, 255 }); }
                }
            }
        }
    }
    else if (mode == SHOP_REMOVE)
    {
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f }, "PICK A CARD TO REMOVE", 18, 0, (Color){ 220, 120, 120, 255 }, TEXT_ALIGN_CENTER);
        char hint[80];
        snprintf(hint, sizeof(hint), "%d cards  |  wheel scroll  |  right-click cancel", g_state.run_deck.card_count);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f }, hint, 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);
        deck_browser_draw(&shop_browser, &g_state.run_deck, false, (Color){ 255, 100, 100, 255 });
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgraded);
    }
    else if (mode == SHOP_DONE)
    {
        Color c = (Color){ 100, 220, 120, 255 };
        draw_text_box((Rectangle){ 80.0f, 164.0f, 480.0f, 22.0f }, msg, 18, 0, c, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 190.0f, 480.0f, 14.0f }, "Click to continue.", 10, 0, (Color){ 160, 160, 180, 200 }, TEXT_ALIGN_CENTER);
    }
}



