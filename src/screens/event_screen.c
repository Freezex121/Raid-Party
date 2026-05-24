#include "screens.h"
#include "game.h"
#include "constants.h"
#include "data/card_defs.h"
#include "data/event_defs.h"
#include "ui/deck_browser.h"
#include "ui/layout.h"
#include "ui/theme.h"
#include "util/log.h"
#include "util/text.h"
#include "raylib.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    EVENT_CHOICE,
    EVENT_REMOVE_CARD,
    EVENT_DONE,
} EventMode;

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

static const EventDef *active_event_def(void)
{
    return event_def_by_index(active_event);
}

static bool choice_available(const EventChoiceDef *choice)
{
    if (!choice) return false;
    switch (choice->effect)
    {
        case EVENT_EFFECT_PAY_GOLD_GAIN_RELIC:
        case EVENT_EFFECT_PAY_GOLD_ADD_CARD:
            return g_state.gold >= choice->gold;
        case EVENT_EFFECT_REMOVE_CARD:
            return g_state.run_deck.card_count > 3;
        case EVENT_EFFECT_UPGRADE_RANDOM_CARD_HURT_PARTY:
            return deck_browser_has_upgradeable(&g_state.run_deck);
        default:
            return true;
    }
}

static void describe_unavailable(const EventChoiceDef *choice)
{
    if (!choice)
    {
        event_msg[0] = '\0';
        return;
    }

    switch (choice->effect)
    {
        case EVENT_EFFECT_PAY_GOLD_GAIN_RELIC:
        case EVENT_EFFECT_PAY_GOLD_ADD_CARD:
            snprintf(event_msg, sizeof(event_msg), "Need %dg.", choice->gold);
            break;
        case EVENT_EFFECT_REMOVE_CARD:
            snprintf(event_msg, sizeof(event_msg), "Deck is too small.");
            break;
        case EVENT_EFFECT_UPGRADE_RANDOM_CARD_HURT_PARTY:
            snprintf(event_msg, sizeof(event_msg), "No cards left to upgrade.");
            break;
        default:
            event_msg[0] = '\0';
            break;
    }
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
    const CardDef *pool[80];
    int count = 0;

    for (int i = 0; i < g_state.selected_count; i++)
    {
        ClassType ct = (ClassType)g_state.selected_classes[i];
        if (ct < 0 || ct >= CLASS_COUNT || !class_card_sets[ct]) continue;
        for (int c = 0; c < class_card_counts[ct]; c++)
            pool[count++] = &class_card_sets[ct][c];
    }

    for (int c = 0; c < utility_card_count; c++)
        pool[count++] = &utility_cards[c];

    if (count <= 0)
        return utility_card_count > 0 ? &utility_cards[0] : card_def_by_id("util_prep");
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
    const EventDef *event = active_event_def();
    if (!event || choice < 0 || choice >= event->choice_count)
    {
        finish_event("The road is quiet.");
        return;
    }

    const EventChoiceDef *choice_def = &event->choices[choice];
    switch (choice_def->effect)
    {
        case EVENT_EFFECT_HEAL_PARTY:
        {
            heal_party(choice_def->amount);
            finish_event("The party shares supplies and recovers.");
            break;
        }
        case EVENT_EFFECT_GAIN_GOLD_ADD_CURSE:
        {
            const CardDef *curse = card_def_by_id(choice_def->curse);
            g_state.gold += choice_def->gold;
            if (curse)
                deck_add_card(&g_state.run_deck, curse);
            finish_event("Found %dg%s.", choice_def->gold, curse ? ", but Doubt enters the deck" : "");
            break;
        }
        case EVENT_EFFECT_REMOVE_CARD:
        {
            if (g_state.run_deck.card_count <= 3)
                finish_event("The deck is too small to trim.");
            else
            {
                mode = EVENT_REMOVE_CARD;
                deck_browser_reset(&event_browser);
                event_msg[0] = '\0';
            }
            break;
        }
        case EVENT_EFFECT_UPGRADE_RANDOM_CARD_HURT_PARTY:
        {
            bool upgraded = upgrade_random_card(detail, sizeof(detail));
            if (upgraded)
            {
                bool downed = hurt_party(choice_def->hp_loss);
                finish_event("%s The shrine takes blood.", detail);
                if (downed)
                    LOG_I(CAT_SCREEN, "Event shrine downed at least one party member");
            }
            else
            {
                finish_event("The shrine finds nothing to improve.");
            }
            break;
        }
        case EVENT_EFFECT_PAY_GOLD_GAIN_RELIC:
        {
            RelicId relic = relic_random_unowned(g_state.relics, g_state.relic_count);
            if (g_state.gold < choice_def->gold)
                finish_event("Not enough gold for the offering.");
            else if (relic == RELIC_NONE)
                finish_event("The shrine is quiet. No relic remains.");
            else
            {
                const RelicDef *def = relic_def(relic);
                g_state.gold -= choice_def->gold;
                relic_add_unique(g_state.relics, &g_state.relic_count, relic);
                finish_event("The shrine grants %s.", def ? def->name : "a relic");
            }
            break;
        }
        case EVENT_EFFECT_PAY_GOLD_ADD_CARD:
        {
            if (g_state.gold < choice_def->gold)
            {
                finish_event("Not enough gold for the scroll.");
            }
            else
            {
                const CardDef *card = random_party_card();
                if (card)
                {
                    g_state.gold -= choice_def->gold;
                    deck_add_card(&g_state.run_deck, card);
                    finish_event("Bought %s for %dg.", card->name, choice_def->gold);
                }
                else
                {
                    finish_event("The merchant has nothing useful.");
                }
            }
            break;
        }
        case EVENT_EFFECT_GAIN_GOLD:
        {
            g_state.gold += choice_def->gold;
            finish_event("Sold odd trinkets for %dg.", choice_def->gold);
            break;
        }
        case EVENT_EFFECT_NONE:
        default:
        {
            finish_event("The party moves on.");
            break;
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
    int event_count = event_defs_count();
    active_event = event_count > 0 ? (floor * 2 + node + g_state.result_bosses_defeated) % event_count : 0;
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
        const EventDef *event = active_event_def();
        int choice_count = event ? event->choice_count : 0;
        for (int i = 0; i < choice_count; i++)
        {
            Rectangle r = event_choice_rect(i);
            if (CheckCollisionPointRec(mouse, r))
            {
                hovered_choice = i;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    if (choice_available(&event->choices[i]))
                    {
                        apply_choice(i);
                    }
                    else
                    {
                        describe_unavailable(&event->choices[i]);
                    }
                }
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
        DrawText("TRAVEL LIGHT", (VIRT_W / 2) - MeasureText("TRAVEL LIGHT", 18) / 2, 16, 18, RAYWHITE);
        DrawText("Pick a card to remove. Right-click cancels.", (VIRT_W / 2) - MeasureText("Pick a card to remove. Right-click cancels.", 10) / 2, 34, 10, (Color){ 160, 160, 180, 180 });
        deck_browser_draw(&event_browser, &g_state.run_deck, false, (Color){ 255, 115, 115, 255 });
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgraded);
        return;
    }

    const EventDef *event = active_event_def();
    const char *title = event ? event->name : "EVENT";
    DrawText(title, (VIRT_W / 2) - MeasureText(title, 18) / 2, 66, 18, (Color){ 130, 225, 235, 255 });
    const char *body = event ? event->body : "";
    DrawText(body, (VIRT_W / 2) - MeasureText(body, 10) / 2, 98, 10, (Color){ 185, 190, 215, 225 });

    if (mode == EVENT_CHOICE)
    {
        Vector2 mouse = GetMousePosition();
        int choice_count = event ? event->choice_count : 0;
        for (int i = 0; i < choice_count; i++)
        {
            Rectangle r = event_choice_rect(i);
            bool hover = CheckCollisionPointRec(mouse, r);
            bool available = choice_available(&event->choices[i]);
            Color bg = !available ? (Color){ 22, 23, 31, 220 } :
                       hover ? (Color){ 38, 70, 82, 245 } : (Color){ 18, 25, 38, 235 };
            Color border = !available ? (Color){ 70, 72, 88, 170 } :
                           hover ? RAYWHITE : (Color){ 85, 185, 205, 180 };
            Color title_col = available ? RAYWHITE : (Color){ 105, 108, 125, 220 };
            Color desc_col = available ? (Color){ 180, 190, 210, 225 } : (Color){ 100, 102, 120, 205 };
            DrawRectangleRec(r, bg);
            DrawRectangleLinesEx(r, hover && available ? 2.0f : 1.0f, border);
            DrawText(event->choices[i].label, (int)r.x + 9, (int)r.y + 10, 10, title_col);
            draw_text_wrapped(event->choices[i].description, (int)r.x + 9, (int)r.y + 31, (int)r.width - 18, 10, 2, desc_col);
        }

        if (event_msg[0])
            DrawText(event_msg, (VIRT_W / 2) - MeasureText(event_msg, 10) / 2, 238, 10, (Color){ 210, 165, 105, 230 });
    }
    else
    {
        DrawText(event_msg, (VIRT_W / 2) - MeasureText(event_msg, 10) / 2, 158, 10, RAYWHITE);
        DrawText("Click to continue.", (VIRT_W / 2) - MeasureText("Click to continue.", 10) / 2, 186, 10, (Color){ 160, 160, 190, 220 });
    }
}
