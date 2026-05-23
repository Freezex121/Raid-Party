#ifndef DECK_BROWSER_H
#define DECK_BROWSER_H

#include "raylib.h"
#include "systems/deck.h"
#include <stdbool.h>

typedef struct {
    Rectangle viewport;
    int scroll_row;
    int hovered_deck_index;
    int columns;
    int visible_rows;
    int total_rows;
} DeckBrowser;

void deck_browser_reset(DeckBrowser *browser);
int  deck_browser_update(DeckBrowser *browser, Deck *deck, Rectangle viewport, bool require_unupgraded);
void deck_browser_draw(DeckBrowser *browser, Deck *deck, bool require_unupgraded, Color highlight);
bool deck_browser_has_upgradeable(Deck *deck);

#endif
