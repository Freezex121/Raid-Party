#include "screens.h"
#include "game.h"
#include "constants.h"
#include "systems/telemetry.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "util/math_utils.h"
#include "util/text.h"
#include "raylib.h"
#include "ui/ui.h"
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

static int party_missing_hp(void)
{
    int missing = 0;
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        if (pm->max_hp > pm->hp)
            missing += pm->max_hp - pm->hp;
    }
    return missing;
}

static void log_rest_metric(const char *choice, const char *card_id)
{
    char run_id[16], area[16], floor[16], missing[16], upgradeable[16];
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(area, sizeof(area), "%d", g_state.current_area);
    snprintf(floor, sizeof(floor), "%d", g_state.map.floor + 1);
    snprintf(missing, sizeof(missing), "%d", party_missing_hp());
    snprintf(upgradeable, sizeof(upgradeable), "%d", deck_browser_has_upgradeable(&g_state.run_deck) ? 1 : 0);
    const char *fields[] = {
        run_id,
        area,
        floor,
        choice ? choice : "",
        card_id ? card_id : "",
        missing,
        upgradeable
    };
    telemetry_csv_append(
        "rest_metrics.csv",
        "timestamp,run_id,area,floor,choice,card_id,party_missing_hp,has_upgradeable_cards",
        fields,
        7);

    char json[384];
    snprintf(json, sizeof(json),
        "\"area\":%d,\"floor\":%d,\"choice\":\"%s\",\"card_id\":\"%s\",\"party_missing_hp\":%d,\"upgradeable_cards\":%s",
        g_state.current_area,
        g_state.map.floor + 1,
        choice ? choice : "",
        card_id ? card_id : "",
        party_missing_hp(),
        deck_browser_has_upgradeable(&g_state.run_deck) ? "true" : "false");
    telemetry_push_json("rest_choice", json);
}

static Rectangle rest_heal_button(void)
{
    return (Rectangle){ (float)((VIRT_W / 2) - 185), 158.0f, (float)BTN_WIDE, 44.0f };
}

static Rectangle rest_upgrade_button(void)
{
    return (Rectangle){ (float)((VIRT_W / 2) + 25), 158.0f, (float)BTN_WIDE, 44.0f };
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
                log_rest_metric("heal", "");
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
                    log_rest_metric("upgrade_select", "");
                    mode = REST_UPGRADE;
                    deck_browser_reset(&rest_browser);
                    rest_msg[0] = '\0';
                }
                else
                {
                    snprintf(rest_msg, sizeof(rest_msg), "No cards left to upgrade.");
                    log_rest_metric("upgrade_unavailable", "");
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
            const CardDef *upgraded = g_state.run_deck.cards[selected].def;
            g_state.run_deck.cards[selected].upgrade_level = 1;
            LOG_I(CAT_SCREEN, "Rest: upgraded %s", g_state.run_deck.cards[selected].def->name);
            log_rest_metric("upgrade", upgraded && upgraded->id ? upgraded->id : "");
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

        Rectangle heal_btn = rest_heal_button();
        Rectangle upg_btn  = rest_upgrade_button();

        draw_btn_large(heal_btn, (Color){ 40, 120, 60, 255 }, (Color){ 60, 180, 80, 255 }, "HEAL PARTY", "Fully restore HP");
        draw_btn_large(upg_btn, (Color){ 50, 80, 140, 255 }, (Color){ 80, 140, 220, 255 }, "UPGRADE A CARD", "Boost one card's power");

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



