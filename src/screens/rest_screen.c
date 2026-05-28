#include "screens.h"
#include "game.h"
#include "constants.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "util/math_utils.h"
#include "util/text.h"
#include "raylib.h"
#include <stdio.h>

typedef enum {
    REST_CHOICE,
    REST_HEAL_DONE,
    REST_UPGRADE,
} RestMode;

static RestMode mode = REST_CHOICE;
static int hovered_upgrade = -1;
static DeckBrowser rest_browser;
static char rest_msg[96] = "";
static bool rest_healed = false;
static bool rest_upgraded = false;
static bool rest_frugal_shown = false;

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

static void rest_leave(void)
{
    mode = REST_CHOICE;
    rest_healed = false;
    rest_upgraded = false;
    rest_frugal_shown = false;
    g_state.map.nodes[g_state.map.current_index].completed = true;
    g_state.map.current_index = -1;
    map_unlock_next(&g_state.map);
    game_change_screen(SCREEN_MAP);
}

void rest_screen_update(void)
{
    if (!g_state.tutorial_active)
        game_start_tutorial_once(&g_state.meta.tutorial_seen_rest, TUTORIAL_STEP_REST);
    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REST)
    {
        if (game_tutorial_handle_close())
            return;
    }

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
                rest_healed = true;
                // Frugal Tome: allow upgrade too
                if (!g_state.frugal_used_this_floor && relic_has(g_state.relics, g_state.relic_count, RELIC_FRUGAL_TOME)
                    && deck_browser_has_upgradeable(&g_state.run_deck))
                {
                    mode = REST_UPGRADE;
                    deck_browser_reset(&rest_browser);
                    rest_msg[0] = '\0';
                }
                else
                {
                    mode = REST_HEAL_DONE;
                }
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
        int selected = deck_browser_update(&rest_browser, &g_state.run_deck, rest_browser_bounds(), 1);
        hovered_upgrade = rest_browser.hovered_deck_index;

        if (selected >= 0)
        {
            g_state.run_deck.cards[selected].upgrade_level = 1;
            LOG_I(CAT_SCREEN, "Rest: upgraded %s", g_state.run_deck.cards[selected].def->name);
            rest_upgraded = true;
            // Frugal Tome: auto-heal if not healed yet
            if (!rest_healed && relic_has(g_state.relics, g_state.relic_count, RELIC_FRUGAL_TOME))
            {
                do_heal();
                rest_healed = true;
                g_state.frugal_used_this_floor = true;
            }
            else if (relic_has(g_state.relics, g_state.relic_count, RELIC_FRUGAL_TOME))
            {
                g_state.frugal_used_this_floor = true;
            }
            mode = REST_HEAL_DONE;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            // If we already healed via Frugal Tome, go to done
            if (rest_healed)
            {
                mode = REST_HEAL_DONE;
            }
            else
            {
                mode = REST_CHOICE;
                rest_msg[0] = '\0';
            }
        }
    }
    else if (mode == REST_HEAL_DONE)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            rest_leave();
    }
}

void rest_screen_draw(void)
{
    theme_draw_background();

    if (mode == REST_CHOICE)
    {
        draw_text_box((Rectangle){ 80.0f, 76.0f, 480.0f, 22.0f }, "REST SITE", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 104.0f, 480.0f, 14.0f }, "Choose:", 10, 0, (Color){ 160, 160, 180, 200 }, TEXT_ALIGN_CENTER);

        Vector2 m = GetMousePosition();
        Rectangle heal_btn = rest_heal_button();
        Rectangle upg_btn  = rest_upgrade_button();

        Color hc = CheckCollisionPointRec(m, heal_btn) ? (Color){ 60, 180, 80, 255 } : (Color){ 40, 120, 60, 255 };
        Color uc = CheckCollisionPointRec(m, upg_btn)  ? (Color){ 80, 140, 220, 255 } : (Color){ 50, 80, 140, 255 };

        DrawRectangleRec(heal_btn, hc);
        draw_text_box((Rectangle){ heal_btn.x + 6.0f, heal_btn.y + 8.0f, heal_btn.width - 12.0f, 14.0f },
            "HEAL PARTY", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

        DrawRectangleRec(upg_btn, uc);
        draw_text_box((Rectangle){ upg_btn.x + 6.0f, upg_btn.y + 8.0f, upg_btn.width - 12.0f, 14.0f },
            "UPGRADE A CARD", 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

        draw_text_box((Rectangle){ heal_btn.x + 6.0f, heal_btn.y + 28.0f, heal_btn.width - 12.0f, 14.0f },
            "Fully restore HP", 10, 0, (Color){ 160, 220, 170, 200 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ upg_btn.x + 6.0f, upg_btn.y + 28.0f, upg_btn.width - 12.0f, 14.0f },
            "Boost one card's power", 10, 0, (Color){ 160, 180, 220, 200 }, TEXT_ALIGN_CENTER);

        if (rest_msg[0])
            draw_text_box((Rectangle){ 96.0f, 228.0f, 448.0f, 26.0f }, rest_msg, 10, 0, (Color){ 200, 150, 100, 220 }, TEXT_ALIGN_CENTER);
    }
    else if (mode == REST_UPGRADE)
    {
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f }, "PICK A CARD TO UPGRADE", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        char hint[80];
        snprintf(hint, sizeof(hint), "%d cards  |  wheel scroll  |  right-click cancel", g_state.run_deck.card_count);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f }, hint, 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);

        deck_browser_draw(&rest_browser, &g_state.run_deck, 1, RAYWHITE);

        if (hovered_upgrade >= 0)
        {
            const CardDef *cd = g_state.run_deck.cards[hovered_upgrade].def;
            int level = g_state.run_deck.cards[hovered_upgrade].upgrade_level;
            Rectangle tip = theme_draw_card_tooltip_limited(layout_deck_inspector_panel(), cd, level, 268);
            int preview_y = (int)(tip.y + tip.height + 5.0f);
            if (level > 0)
            {
                draw_text_box((Rectangle){ tip.x, (float)preview_y, tip.width, 24.0f },
                    "Rest sites only upgrade base cards", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_LEFT);
            }
            else if (!card_upgrade_changes_values_at(cd, level))
            {
                draw_text_box((Rectangle){ tip.x, (float)preview_y, tip.width, 24.0f },
                    "Upgrade has no value changes", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_LEFT);
            }
            else
            {
                int od = card_damage(cd, level), oh = card_heal(cd, level), os = card_shield(cd, level);
                int nd = card_damage(cd, level + 1), nh = card_heal(cd, level + 1), ns = card_shield(cd, level + 1);

                int y = preview_y;
                if (nd != od) { char b[32]; snprintf(b, sizeof(b), "DMG %d>%d", od, nd); DrawText(b, (int)tip.x, y, 10, (Color){ 220, 110, 100, 255 }); y += 14; }
                if (nh != oh) { char b[32]; snprintf(b, sizeof(b), "HEAL %d>%d", oh, nh); DrawText(b, (int)tip.x, y, 10, (Color){ 105, 220, 125, 255 }); y += 14; }
                if (ns != os) { char b[32]; snprintf(b, sizeof(b), "SHIELD %d>%d", os, ns); DrawText(b, (int)tip.x, y, 10, (Color){ 125, 190, 255, 255 }); }
            }
        }
    }
    else if (mode == REST_HEAL_DONE)
    {
        draw_text_box((Rectangle){ 80.0f, 164.0f, 480.0f, 22.0f }, "PARTY RESTED", 18, 0, (Color){ 100, 220, 120, 255 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 190.0f, 480.0f, 14.0f }, "Click to continue.", 10, 0, (Color){ 160, 160, 180, 200 }, TEXT_ALIGN_CENTER);
    }

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_REST)
    {
        game_draw_tutorial_overlay_ex((Rectangle){ 128.0f, 150.0f, 384.0f, 60.0f },
            "Rest Sites",
            "Rest sites are recovery choices. Heal restores the party; Upgrade improves one card if that card has upgradeable values.",
            "Click to continue  |  Right-click/Esc: skip",
            0,
            0);
    }
}



