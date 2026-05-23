#include "screens.h"
#include "game.h"
#include "constants.h"
#include "data/card_defs.h"
#include "ui/deck_browser.h"
#include "ui/layout.h"
#include "ui/theme.h"
#include "util/log.h"
#include "util/text.h"
#include "raylib.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    EVENT_CHOICE,
    EVENT_REMOVE_CARD,
    EVENT_DONE,
} EventMode;

static const CardDef curse_doubt = {
    "curse_doubt",
    "Doubt",
    CARD_SKILL,
    CLASS_NONE,
    1,
    0,
    0,
    0,
    false,
    0,
    0,
    false,
    0,
    false,
    0,
    false,
    false,
    0,
    TARGET_SELF,
    1,
    NULL,
    0,
    "No effect. Spend energy to clear it from hand."
};

static EventMode mode = EVENT_CHOICE;
static int active_node = -999;
static int active_floor = -999;
static int active_event = 0;
static int hovered_choice = -1;
static int hovered_deck = -1;
static DeckBrowser event_browser;
static char event_msg[128] = "";

static Rectangle event_choice_rect(int index)
{
    const float w = 156.0f;
    const float h = 72.0f;
    const float gap = 14.0f;
    float start_x = (VIRT_W - (3.0f * w + 2.0f * gap)) * 0.5f;
    return (Rectangle){ start_x + index * (w + gap), 154.0f, w, h };
}

static const char *event_title(int event_id)
{
    switch (event_id)
    {
        case 0: return "ABANDONED CAMP";
        case 1: return "ANCIENT SHRINE";
        default: return "STRANGE MERCHANT";
    }
}

static const char *event_body(int event_id)
{
    switch (event_id)
    {
        case 0: return "A cold campfire and three untouched packs wait beside the road.";
        case 1: return "A stone shrine hums softly. The air tastes like lightning.";
        default: return "A masked merchant offers useful things at suspicious prices.";
    }
}

static const char *choice_title(int event_id, int choice)
{
    static const char *titles[3][3] = {
        { "Share Supplies", "Search Packs", "Travel Light" },
        { "Take Blessing", "Donate Gold", "Leave It" },
        { "Buy Scroll", "Sell Trinkets", "Patch Wounds" },
    };
    return titles[event_id][choice];
}

static const char *choice_desc(int event_id, int choice)
{
    static const char *descs[3][3] = {
        { "Heal all allies for 12.", "Gain 35g. Add Doubt.", "Remove a card." },
        { "Upgrade a card. Lose 6 HP each.", "Pay 20g. Gain a relic.", "No effect." },
        { "Pay 25g. Add a card.", "Gain 20g.", "Heal all allies for 8." },
    };
    return descs[event_id][choice];
}

static void finish_event(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(event_msg, sizeof(event_msg), fmt, args);
    va_end(args);
    mode = EVENT_DONE;
}

static void heal_party(int amount)
{
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        if (!pm->alive)
        {
            pm->alive = true;
            pm->hp = amount;
        }
        else
        {
            pm->hp += amount;
        }
        if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;
        if (pm->hp < 1) pm->hp = 1;
    }
}

static bool hurt_party(int amount)
{
    bool any_down = false;
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        if (!pm->alive) continue;
        pm->hp -= amount;
        if (pm->hp <= 0)
        {
            pm->hp = 0;
            pm->alive = false;
            pm->aggro = 0;
            pm->shield = 0;
            any_down = true;
        }
    }
    return any_down;
}

static bool upgrade_random_card(char *out, int out_size)
{
    int candidates[MAX_DECK_SIZE];
    int count = 0;
    for (int i = 0; i < g_state.run_deck.card_count; i++)
        if (g_state.run_deck.cards[i].def && !g_state.run_deck.cards[i].upgraded)
            candidates[count++] = i;

    if (count <= 0)
        return false;

    int idx = candidates[rand() % count];
    g_state.run_deck.cards[idx].upgraded = true;
    if (out && out_size > 0)
        snprintf(out, out_size, "%s upgraded.", g_state.run_deck.cards[idx].def->name);
    return true;
}

static const CardDef *random_party_card(void)
{
    const CardDef *pool[48];
    int count = 0;

    for (int i = 0; i < g_state.selected_count; i++)
    {
        ClassType ct = (ClassType)g_state.selected_classes[i];
        if (ct < 0 || ct >= CLASS_COUNT || !class_card_sets[ct]) continue;
        for (int c = 0; c < class_card_counts[ct]; c++)
            pool[count++] = &class_card_sets[ct][c];
    }

    for (int c = 0; c < UTILITY_CARD_COUNT; c++)
        pool[count++] = &utility_cards[c];

    if (count <= 0) return &utility_cards[0];
    return pool[rand() % count];
}

static void complete_map_node(void)
{
    int ci = g_state.map.current_index;
    if (ci >= 0 && ci < g_state.map.node_count)
        g_state.map.nodes[ci].completed = true;
    g_state.map.current_index = -1;
    map_unlock_next(&g_state.map);
}

static void apply_choice(int choice)
{
    char detail[96] = "";

    if (active_event == 0)
    {
        if (choice == 0)
        {
            heal_party(12);
            finish_event("The party shares supplies and recovers.");
        }
        else if (choice == 1)
        {
            g_state.gold += 35;
            deck_add_card(&g_state.run_deck, &curse_doubt);
            finish_event("Found 35g, but Doubt enters the deck.");
        }
        else
        {
            if (g_state.run_deck.card_count <= 3)
                finish_event("The deck is too small to trim.");
            else
            {
                mode = EVENT_REMOVE_CARD;
                deck_browser_reset(&event_browser);
                event_msg[0] = '\0';
            }
        }
    }
    else if (active_event == 1)
    {
        if (choice == 0)
        {
            bool upgraded = upgrade_random_card(detail, sizeof(detail));
            if (upgraded)
            {
                bool downed = hurt_party(6);
                finish_event("%s The shrine takes blood.", detail);
                if (downed)
                    LOG_I(CAT_SCREEN, "Event shrine downed at least one party member");
            }
            else
            {
                finish_event("The shrine finds nothing to improve.");
            }
        }
        else if (choice == 1)
        {
            RelicId relic = relic_random_unowned(g_state.relics, g_state.relic_count);
            if (g_state.gold < 20)
                finish_event("Not enough gold for the offering.");
            else if (relic == RELIC_NONE)
                finish_event("The shrine is quiet. No relic remains.");
            else
            {
                const RelicDef *def = relic_def(relic);
                g_state.gold -= 20;
                relic_add_unique(g_state.relics, &g_state.relic_count, relic);
                finish_event("The shrine grants %s.", def ? def->name : "a relic");
            }
        }
        else
        {
            finish_event("The party leaves the shrine untouched.");
        }
    }
    else
    {
        if (choice == 0)
        {
            if (g_state.gold < 25)
            {
                finish_event("Not enough gold for the scroll.");
            }
            else
            {
                const CardDef *card = random_party_card();
                g_state.gold -= 25;
                deck_add_card(&g_state.run_deck, card);
                finish_event("Bought %s for 25g.", card->name);
            }
        }
        else if (choice == 1)
        {
            g_state.gold += 20;
            finish_event("Sold odd trinkets for 20g.");
        }
        else
        {
            heal_party(8);
            finish_event("The merchant's tonic restores the party.");
        }
    }
}

static void init_event_if_needed(void)
{
    int node = g_state.map.current_index;
    int floor = g_state.map.floor;
    if (node == active_node && floor == active_floor) return;

    active_node = node;
    active_floor = floor;
    active_event = (floor * 2 + node + g_state.result_bosses_defeated) % 3;
    if (active_event < 0) active_event = 0;
    mode = EVENT_CHOICE;
    hovered_choice = -1;
    hovered_deck = -1;
    event_msg[0] = '\0';
}

void event_screen_update(void)
{
    init_event_if_needed();

    Vector2 mouse = GetMousePosition();
    hovered_choice = -1;

    if (mode == EVENT_CHOICE)
    {
        for (int i = 0; i < 3; i++)
        {
            Rectangle r = event_choice_rect(i);
            if (CheckCollisionPointRec(mouse, r))
            {
                hovered_choice = i;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    apply_choice(i);
                break;
            }
        }
    }
    else if (mode == EVENT_REMOVE_CARD)
    {
        int selected = deck_browser_update(&event_browser, &g_state.run_deck, layout_deck_browser_viewport(), false);
        hovered_deck = event_browser.hovered_deck_index;
        if (selected >= 0)
        {
            const CardDef *def = g_state.run_deck.cards[selected].def;
            int uid = g_state.run_deck.cards[selected].uid;
            deck_remove_card_by_uid(&g_state.run_deck, uid);
            finish_event("Removed %s from the deck.", def ? def->name : "a card");
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            mode = EVENT_CHOICE;
    }
    else if (mode == EVENT_DONE)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        {
            complete_map_node();
            active_node = -999;
            game_change_screen(SCREEN_MAP);
        }
    }
}

void event_screen_draw(void)
{
    theme_draw_background();

    if (mode == EVENT_REMOVE_CARD)
    {
        DrawText("TRAVEL LIGHT", (VIRT_W / 2) - MeasureText("TRAVEL LIGHT", 12) / 2, 16, 12, RAYWHITE);
        DrawText("Pick a card to remove. Right-click cancels.", (VIRT_W / 2) - MeasureText("Pick a card to remove. Right-click cancels.", 6) / 2, 34, 6, (Color){ 160, 160, 180, 180 });
        deck_browser_draw(&event_browser, &g_state.run_deck, false, (Color){ 255, 115, 115, 255 });
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgraded);
        return;
    }

    const char *title = event_title(active_event);
    DrawText(title, (VIRT_W / 2) - MeasureText(title, 17) / 2, 66, 17, (Color){ 130, 225, 235, 255 });
    const char *body = event_body(active_event);
    DrawText(body, (VIRT_W / 2) - MeasureText(body, 8) / 2, 98, 8, (Color){ 185, 190, 215, 225 });

    if (mode == EVENT_CHOICE)
    {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < 3; i++)
        {
            Rectangle r = event_choice_rect(i);
            bool hover = CheckCollisionPointRec(mouse, r);
            Color bg = hover ? (Color){ 38, 70, 82, 245 } : (Color){ 18, 25, 38, 235 };
            DrawRectangleRec(r, bg);
            DrawRectangleLinesEx(r, hover ? 2.0f : 1.0f, hover ? RAYWHITE : (Color){ 85, 185, 205, 180 });
            DrawText(choice_title(active_event, i), (int)r.x + 9, (int)r.y + 10, 9, RAYWHITE);
            draw_text_wrapped(choice_desc(active_event, i), (int)r.x + 9, (int)r.y + 31, (int)r.width - 18, 7, 2, (Color){ 180, 190, 210, 225 });
        }
    }
    else
    {
        DrawText(event_msg, (VIRT_W / 2) - MeasureText(event_msg, 10) / 2, 158, 10, RAYWHITE);
        DrawText("Click to continue.", (VIRT_W / 2) - MeasureText("Click to continue.", 8) / 2, 186, 8, (Color){ 160, 160, 190, 220 });
    }
}
