#include "screens.h"
#include "game.h"
#include "constants.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "raylib.h"
#include <stdio.h>

typedef enum {
    REST_CHOICE,
    REST_HEAL,
    REST_UPGRADE,
} RestMode;

static RestMode mode = REST_CHOICE;
static int hovered_upgrade = -1;
static DeckBrowser rest_browser;
static char rest_msg[96] = "";

static Rectangle rest_heal_button(void)
{
    return (Rectangle){ (float)((VIRT_W / 2) - 190), 158.0f, 170.0f, 44.0f };
}

static Rectangle rest_upgrade_button(void)
{
    return (Rectangle){ (float)((VIRT_W / 2) + 20), 158.0f, 170.0f, 44.0f };
}

static Rectangle rest_browser_bounds(void)
{
    return layout_deck_browser_viewport();
}

static void do_heal(void)
{
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        g_state.run_party.members[i].alive = true;
        g_state.run_party.members[i].hp = g_state.run_party.members[i].max_hp;
        g_state.run_party.members[i].shield = 0;
        g_state.run_party.members[i].aggro = 0;
        g_state.run_party.members[i].status_count = 0;
    }
    LOG_I(CAT_SCREEN, "Rest: party fully healed");
}

void rest_screen_update(void)
{
    if (mode == REST_CHOICE)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 m = GetMousePosition();
            Rectangle heal_btn = rest_heal_button();
            Rectangle upg_btn  = rest_upgrade_button();

            if (CheckCollisionPointRec(m, heal_btn))
            {
                do_heal();
                mode = REST_HEAL;
                rest_msg[0] = '\0';
            }
            else if (CheckCollisionPointRec(m, upg_btn))
            {
                if (deck_browser_has_upgradeable(&g_state.run_deck))
                {
                    mode = REST_UPGRADE;
                    deck_browser_reset(&rest_browser);
                    rest_msg[0] = '\0';
                }
                else
                {
                    snprintf(rest_msg, sizeof(rest_msg), "No cards left to upgrade.");
                }
            }
        }
    }
    else if (mode == REST_UPGRADE)
    {
        int selected = deck_browser_update(&rest_browser, &g_state.run_deck, rest_browser_bounds(), true);
        hovered_upgrade = rest_browser.hovered_deck_index;

        if (selected >= 0)
        {
            g_state.run_deck.cards[selected].upgraded = true;
            LOG_I(CAT_SCREEN, "Rest: upgraded %s", g_state.run_deck.cards[selected].def->name);
            mode = REST_HEAL;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            mode = REST_CHOICE;
            rest_msg[0] = '\0';
        }
    }
    else if (mode == REST_HEAL)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            mode = REST_CHOICE;
            g_state.map.nodes[g_state.map.current_index].completed = true;
            g_state.map.current_index = -1;
            map_unlock_next(&g_state.map);
            game_change_screen(SCREEN_MAP);
        }
    }
}

void rest_screen_draw(void)
{
    theme_draw_background();

    if (mode == REST_CHOICE)
    {
        DrawText("REST SITE", (VIRT_W / 2) - MeasureText("REST SITE", 18) / 2, 76, 18, RAYWHITE);
        DrawText("Choose:", (VIRT_W / 2) - MeasureText("Choose:", 9) / 2, 104, 9, (Color){ 160, 160, 180, 200 });

        Vector2 m = GetMousePosition();
        Rectangle heal_btn = rest_heal_button();
        Rectangle upg_btn  = rest_upgrade_button();

        Color hc = CheckCollisionPointRec(m, heal_btn) ? (Color){ 60, 180, 80, 255 } : (Color){ 40, 120, 60, 255 };
        Color uc = CheckCollisionPointRec(m, upg_btn)  ? (Color){ 80, 140, 220, 255 } : (Color){ 50, 80, 140, 255 };

        DrawRectangleRec(heal_btn, hc);
        DrawText("HEAL PARTY", (int)(heal_btn.x + heal_btn.width / 2 - MeasureText("HEAL PARTY", 8) / 2), (int)heal_btn.y + 10, 8, RAYWHITE);

        DrawRectangleRec(upg_btn, uc);
        DrawText("UPGRADE A CARD", (int)(upg_btn.x + upg_btn.width / 2 - MeasureText("UPGRADE A CARD", 8) / 2), (int)upg_btn.y + 10, 8, RAYWHITE);

        DrawText("Fully restore HP", (int)heal_btn.x + 16, (int)heal_btn.y + 29, 6, (Color){ 160, 220, 170, 200 });
        DrawText("Boost one card's power", (int)upg_btn.x + 12, (int)upg_btn.y + 29, 6, (Color){ 160, 180, 220, 200 });

        if (rest_msg[0])
            DrawText(rest_msg, (VIRT_W / 2) - MeasureText(rest_msg, 8) / 2, 228, 8, (Color){ 200, 150, 100, 220 });
    }
    else if (mode == REST_UPGRADE)
    {
        DrawText("PICK A CARD TO UPGRADE", (VIRT_W / 2) - MeasureText("PICK A CARD TO UPGRADE", 12) / 2, 16, 12, RAYWHITE);
        char hint[80];
        snprintf(hint, sizeof(hint), "%d cards  |  wheel scroll  |  right-click cancel", g_state.run_deck.card_count);
        DrawText(hint, (VIRT_W / 2) - MeasureText(hint, 6) / 2, 34, 6, (Color){ 160, 160, 180, 180 });

        deck_browser_draw(&rest_browser, &g_state.run_deck, true, RAYWHITE);

        if (hovered_upgrade >= 0)
        {
            const CardDef *cd = g_state.run_deck.cards[hovered_upgrade].def;
            theme_draw_card_tooltip(layout_deck_inspector_panel(), cd, g_state.run_deck.cards[hovered_upgrade].upgraded);
            if (g_state.run_deck.cards[hovered_upgrade].upgraded)
            {
                DrawText("Already upgraded", 448, 190, 7, (Color){ 160, 160, 180, 220 });
            }
            else
            {
                int od = cd->damage, oh = cd->heal, os = cd->shield;
                int nd = card_damage(cd, true), nh = card_heal(cd, true), ns = card_shield(cd, true);

                int y = 190;
                if (od > 0) { char b[32]; snprintf(b, sizeof(b), "DMG %d>%d", od, nd); DrawText(b, 448, y, 7, (Color){ 220, 110, 100, 255 }); y += 12; }
                if (oh > 0) { char b[32]; snprintf(b, sizeof(b), "HEAL %d>%d", oh, nh); DrawText(b, 448, y, 7, (Color){ 105, 220, 125, 255 }); y += 12; }
                if (os > 0) { char b[32]; snprintf(b, sizeof(b), "SHIELD %d>%d", os, ns); DrawText(b, 448, y, 7, (Color){ 125, 190, 255, 255 }); }
            }
        }
    }
    else if (mode == REST_HEAL)
    {
        DrawText("PARTY RESTED", (VIRT_W / 2) - MeasureText("PARTY RESTED", 18) / 2, 164, 18, (Color){ 100, 220, 120, 255 });
        DrawText("Click to continue.", (VIRT_W / 2) - MeasureText("Click to continue.", 8) / 2, 190, 8, (Color){ 160, 160, 180, 200 });
    }
}



