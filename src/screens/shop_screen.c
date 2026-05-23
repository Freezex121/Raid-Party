#include "screens.h"
#include "game.h"
#include "constants.h"
#include "systems/relic.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/relic_tray.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "raylib.h"
#include <stdio.h>

#define UPGRADE_COST 30
#define REMOVE_COST 20
#define RELIC_COST 45

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

static Rectangle shop_upgrade_button(void)
{
    return (Rectangle){ 66.0f, 154.0f, 160.0f, 44.0f };
}

static Rectangle shop_remove_button(void)
{
    return (Rectangle){ 240.0f, 154.0f, 160.0f, 44.0f };
}

static Rectangle shop_relic_button(void)
{
    return (Rectangle){ 414.0f, 154.0f, 160.0f, 44.0f };
}

static Rectangle shop_browser_bounds(void)
{
    return layout_deck_browser_viewport();
}

void shop_screen_update(void)
{
    if (mode == SHOP_MAIN)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 m = GetMousePosition();
            Rectangle upg_btn = shop_upgrade_button();
            Rectangle rem_btn = shop_remove_button();
            Rectangle relic_btn = shop_relic_button();

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

            if (CheckCollisionPointRec(m, relic_btn))
            {
                RelicId relic = relic_random_unowned(g_state.relics, g_state.relic_count);
                if (g_state.gold < RELIC_COST)
                {
                    snprintf(msg, sizeof(msg), "Not enough gold! Need %dg", RELIC_COST);
                }
                else if (relic == RELIC_NONE)
                {
                    snprintf(msg, sizeof(msg), "No relics left to buy!");
                }
                else
                {
                    const RelicDef *def = relic_def(relic);
                    g_state.gold -= RELIC_COST;
                    relic_add_unique(g_state.relics, &g_state.relic_count, relic);
                    snprintf(msg, sizeof(msg), "Bought %s!", def ? def->name : "a relic");
                    mode = SHOP_DONE;
                }
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
            game_change_screen(SCREEN_MAP);
        }
    }
}

void shop_screen_draw(void)
{
    theme_draw_background();

    DrawText("SHOP", (VIRT_W / 2) - MeasureText("SHOP", 18) / 2, 72, 18, (Color){ 220, 200, 60, 255 });

    char gold_text[32];
    snprintf(gold_text, sizeof(gold_text), "Gold: %d", g_state.gold);
    DrawText(gold_text, (VIRT_W / 2) - MeasureText(gold_text, 9) / 2, 100, 9, (Color){ 220, 200, 60, 220 });

    if (mode == SHOP_MAIN)
    {
        Vector2 m = GetMousePosition();
        Rectangle upg_btn = shop_upgrade_button();
        Rectangle rem_btn = shop_remove_button();
        Rectangle relic_btn = shop_relic_button();

        bool can_upg = g_state.gold >= UPGRADE_COST && deck_browser_has_upgradeable(&g_state.run_deck);
        bool can_rem = g_state.gold >= REMOVE_COST && g_state.run_deck.card_count > 3;
        bool can_relic = g_state.gold >= RELIC_COST && g_state.relic_count < RELIC_COUNT;

        bool hu = CheckCollisionPointRec(m, upg_btn);
        bool hr = CheckCollisionPointRec(m, rem_btn);
        bool hrel = CheckCollisionPointRec(m, relic_btn);

        Color uc = can_upg ? (hu ? (Color){ 80, 140, 220, 255 } : (Color){ 50, 80, 140, 255 }) : (Color){ 40, 40, 60, 255 };
        Color rc = can_rem ? (hr ? (Color){ 220, 100, 100, 255 } : (Color){ 160, 60, 60, 255 }) : (Color){ 40, 40, 60, 255 };
        Color relic_col = can_relic ? (hrel ? (Color){ 185, 155, 75, 255 } : (Color){ 130, 105, 45, 255 }) : (Color){ 40, 40, 60, 255 };

        DrawRectangleRec(upg_btn, uc);
        char ul[64]; snprintf(ul, sizeof(ul), "UPGRADE (%dg)", UPGRADE_COST);
        DrawText(ul, (int)(upg_btn.x + upg_btn.width / 2 - MeasureText(ul, 8) / 2), (int)upg_btn.y + 11, 8, can_upg ? RAYWHITE : (Color){ 100, 100, 120, 200 });

        DrawRectangleRec(rem_btn, rc);
        char rl[64]; snprintf(rl, sizeof(rl), "REMOVE (%dg)", REMOVE_COST);
        DrawText(rl, (int)(rem_btn.x + rem_btn.width / 2 - MeasureText(rl, 8) / 2), (int)rem_btn.y + 11, 8, can_rem ? RAYWHITE : (Color){ 100, 100, 120, 200 });

        DrawRectangleRec(relic_btn, relic_col);
        char rel[64]; snprintf(rel, sizeof(rel), "RELIC (%dg)", RELIC_COST);
        DrawText(rel, (int)(relic_btn.x + relic_btn.width / 2 - MeasureText(rel, 8) / 2), (int)relic_btn.y + 11, 8, can_relic ? RAYWHITE : (Color){ 100, 100, 120, 200 });

        if (msg[0])
            DrawText(msg, (VIRT_W / 2) - MeasureText(msg, 8) / 2, 224, 8, (Color){ 200, 150, 100, 220 });

        relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){ 206.0f, 236.0f, 228.0f, 44.0f });
    }
    else if (mode == SHOP_UPGRADE)
    {
        DrawText("PICK A CARD TO UPGRADE", (VIRT_W / 2) - MeasureText("PICK A CARD TO UPGRADE", 12) / 2, 16, 12, RAYWHITE);
        char hint[80];
        snprintf(hint, sizeof(hint), "%d cards  |  wheel scroll  |  right-click cancel", g_state.run_deck.card_count);
        DrawText(hint, (VIRT_W / 2) - MeasureText(hint, 6) / 2, 34, 6, (Color){ 160, 160, 180, 180 });
        deck_browser_draw(&shop_browser, &g_state.run_deck, true, RAYWHITE);
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgraded);
    }
    else if (mode == SHOP_REMOVE)
    {
        DrawText("PICK A CARD TO REMOVE", (VIRT_W / 2) - MeasureText("PICK A CARD TO REMOVE", 12) / 2, 16, 12, (Color){ 220, 120, 120, 255 });
        char hint[80];
        snprintf(hint, sizeof(hint), "%d cards  |  wheel scroll  |  right-click cancel", g_state.run_deck.card_count);
        DrawText(hint, (VIRT_W / 2) - MeasureText(hint, 6) / 2, 34, 6, (Color){ 160, 160, 180, 180 });
        deck_browser_draw(&shop_browser, &g_state.run_deck, false, (Color){ 255, 100, 100, 255 });
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgraded);
    }
    else if (mode == SHOP_DONE)
    {
        Color c = (Color){ 100, 220, 120, 255 };
        DrawText(msg, (VIRT_W / 2) - MeasureText(msg, 16) / 2, 164, 16, c);
        DrawText("Click to continue.", (VIRT_W / 2) - MeasureText("Click to continue.", 8) / 2, 190, 8, (Color){ 160, 160, 180, 200 });
    }
}



