#include "screens.h"
#include "game.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>

static DeckBrowser deck_browser;
static bool initialized = false;

void deck_screen_update(void)
{
    if (!initialized) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        initialized = false;
        game_change_screen(SCREEN_MAP);
        return;
    }

    int selected = deck_browser_update(&deck_browser, &g_state.run_deck, layout_deck_browser_viewport(), 0);
    (void)selected;
}

void deck_screen_draw(void)
{
    if (!initialized)
    {
        deck_browser_reset(&deck_browser);
        initialized = true;
    }

    theme_draw_background();

    char title[48];
    snprintf(title, sizeof(title), "DECK  (%d cards)", g_state.run_deck.card_count);
    draw_text_box((Rectangle){ 80.0f, 14.0f, 480.0f, 22.0f }, title, 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f }, "Right-click to go back", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);

    deck_browser_draw(&deck_browser, &g_state.run_deck, 0, RAYWHITE);

    if (deck_browser.hovered_deck_index >= 0 && g_state.run_deck.cards[deck_browser.hovered_deck_index].def)
        theme_draw_card_tooltip(layout_deck_inspector_panel(),
            g_state.run_deck.cards[deck_browser.hovered_deck_index].def,
            g_state.run_deck.cards[deck_browser.hovered_deck_index].upgrade_level);
}
