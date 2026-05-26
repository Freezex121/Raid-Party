#include "deck_browser.h"
#include "constants.h"
#include "ui/theme.h"
#include "util/math_utils.h"
#include "util/text.h"
#include <string.h>

#define BROWSER_GAP_X DECK_GAP
#define BROWSER_GAP_Y DECK_GAP
#define BROWSER_SCROLL_W 4

static int class_sort_key(ClassType ct)
{
    return ct == CLASS_NONE ? 99 : (int)ct;
}

static int card_compare(Deck *deck, int a, int b)
{
    const CardDef *ca = deck->cards[a].def;
    const CardDef *cb = deck->cards[b].def;
    if (!ca || !cb) return 0;

    int ac = class_sort_key(ca->class);
    int bc = class_sort_key(cb->class);
    if (ac != bc) return ac - bc;
    if (ca->type != cb->type) return (int)ca->type - (int)cb->type;
    if (ca->cost != cb->cost) return ca->cost - cb->cost;
    return strcmp(ca->name, cb->name);
}

static int build_sorted_indices(Deck *deck, int *indices, int max_indices)
{
    int count = 0;
    for (int i = 0; i < deck->card_count && count < max_indices; i++)
    {
        if (!deck->cards[i].def) continue;

        int pos = count++;
        indices[pos] = i;
        while (pos > 0 && card_compare(deck, indices[pos], indices[pos - 1]) < 0)
        {
            int tmp = indices[pos];
            indices[pos] = indices[pos - 1];
            indices[pos - 1] = tmp;
            pos--;
        }
    }
    return count;
}

static void update_layout(DeckBrowser *browser, int count)
{
    int content_w = (int)browser->viewport.width - BROWSER_SCROLL_W - 2;
    int step_x = DECK_CARD_W + BROWSER_GAP_X;
    int step_y = DECK_CARD_H + BROWSER_GAP_Y;
    int columns = (content_w + BROWSER_GAP_X) / step_x;
    if (columns < 1) columns = 1;

    browser->columns = columns;
    browser->visible_rows = ((int)browser->viewport.height + BROWSER_GAP_Y) / step_y;
    if (browser->visible_rows < 1) browser->visible_rows = 1;
    browser->total_rows = (count + columns - 1) / columns;

    int max_scroll = browser->total_rows - browser->visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (browser->scroll_row < 0) browser->scroll_row = 0;
    if (browser->scroll_row > max_scroll) browser->scroll_row = max_scroll;
}

static Rectangle card_rect_for(DeckBrowser *browser, int visible_pos)
{
    int col = visible_pos % browser->columns;
    int row = visible_pos / browser->columns;
    int content_w = (int)browser->viewport.width - BROWSER_SCROLL_W - 2;
    int grid_w = browser->columns * DECK_CARD_W + (browser->columns - 1) * BROWSER_GAP_X;
    int start_x = (int)browser->viewport.x + (content_w - grid_w) / 2;

    return (Rectangle){
        (float)(start_x + col * (DECK_CARD_W + BROWSER_GAP_X)),
        browser->viewport.y + row * (DECK_CARD_H + BROWSER_GAP_Y),
        (float)DECK_CARD_W,
        (float)DECK_CARD_H
    };
}

static void draw_scrollbar(DeckBrowser *browser)
{
    if (browser->total_rows <= browser->visible_rows) return;

    int track_x = (int)(browser->viewport.x + browser->viewport.width - BROWSER_SCROLL_W);
    int track_y = (int)browser->viewport.y;
    int track_h = (int)browser->viewport.height;
    DrawRectangle(track_x, track_y, BROWSER_SCROLL_W, track_h, (Color){ 18, 18, 28, 220 });

    float visible_ratio = (float)browser->visible_rows / (float)browser->total_rows;
    int thumb_h = (int)(track_h * visible_ratio);
    if (thumb_h < 10) thumb_h = 10;

    int max_scroll = browser->total_rows - browser->visible_rows;
    float scroll_t = max_scroll > 0 ? (float)browser->scroll_row / (float)max_scroll : 0.0f;
    int thumb_y = track_y + (int)((track_h - thumb_h) * scroll_t);
    DrawRectangle(track_x + 1, thumb_y, BROWSER_SCROLL_W - 2, thumb_h, (Color){ 110, 120, 155, 230 });
}

void deck_browser_reset(DeckBrowser *browser)
{
    if (!browser) return;
    browser->scroll_row = 0;
    browser->hovered_deck_index = -1;
    browser->columns = 1;
    browser->visible_rows = 1;
    browser->total_rows = 0;
}

bool deck_browser_has_upgradeable(Deck *deck)
{
    return deck_browser_has_upgradeable_at(deck, 1);
}

bool deck_browser_has_upgradeable_at(Deck *deck, int upgrade_target_level)
{
    for (int i = 0; i < deck->card_count; i++)
        if (deck->cards[i].def &&
            upgrade_target_level > 0 &&
            deck->cards[i].upgrade_level == upgrade_target_level - 1 &&
            card_upgrade_changes_values_at(deck->cards[i].def, deck->cards[i].upgrade_level))
            return true;
    return false;
}

int deck_browser_update(DeckBrowser *browser, Deck *deck, Rectangle viewport, int upgrade_target_level)
{
    if (!browser || !deck) return -1;

    int indices[MAX_DECK_SIZE];
    int count = build_sorted_indices(deck, indices, MAX_DECK_SIZE);
    browser->viewport = viewport;
    browser->hovered_deck_index = -1;
    update_layout(browser, count);

    Vector2 mouse = GetMousePosition();
    bool in_view = CheckCollisionPointRec(mouse, viewport);
    if (in_view)
    {
        float wheel = GetMouseWheelMove();
        if (wheel > 0.0f) browser->scroll_row--;
        if (wheel < 0.0f) browser->scroll_row++;
        update_layout(browser, count);
    }

    int first_pos = browser->scroll_row * browser->columns;
    int last_pos = first_pos + browser->visible_rows * browser->columns;
    if (last_pos > count) last_pos = count;

    for (int pos = first_pos; pos < last_pos; pos++)
    {
        int deck_index = indices[pos];
        int visible_pos = pos - first_pos;
        Rectangle r = card_rect_for(browser, visible_pos);
        if (!CheckCollisionPointRec(mouse, r)) continue;

        browser->hovered_deck_index = deck_index;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (upgrade_target_level > 0 &&
                (deck->cards[deck_index].upgrade_level != upgrade_target_level - 1 ||
                 !card_upgrade_changes_values_at(deck->cards[deck_index].def, deck->cards[deck_index].upgrade_level)))
                return -1;
            return deck_index;
        }
        break;
    }

    return -1;
}

void deck_browser_draw(DeckBrowser *browser, Deck *deck, int upgrade_target_level, Color highlight)
{
    if (!browser || !deck) return;
    (void)highlight;

    int indices[MAX_DECK_SIZE];
    int count = build_sorted_indices(deck, indices, MAX_DECK_SIZE);
    update_layout(browser, count);

    DrawRectangleRec(browser->viewport, (Color){ 10, 11, 18, 150 });
    DrawRectangleLinesEx(browser->viewport, 1.0f, (Color){ 60, 64, 86, 180 });

    int first_pos = browser->scroll_row * browser->columns;
    int last_pos = first_pos + browser->visible_rows * browser->columns;
    if (last_pos > count) last_pos = count;

    for (int pos = first_pos; pos < last_pos; pos++)
    {
        int deck_index = indices[pos];
        int visible_pos = pos - first_pos;
        CardInstance *inst = &deck->cards[deck_index];
        Rectangle r = card_rect_for(browser, visible_pos);

        theme_draw_card_art(r, inst->def, inst->upgrade_level);
        if (upgrade_target_level > 0 &&
            (inst->upgrade_level != upgrade_target_level - 1 ||
             !card_upgrade_changes_values_at(inst->def, inst->upgrade_level)))
        {
            DrawRectangleRec(r, (Color){ 8, 8, 12, 155 });
            const char *label = inst->upgrade_level >= 2 ? "MAXED" :
                (inst->upgrade_level >= upgrade_target_level ? "UPGRADED" : "NO CHANGE");
            draw_text_box((Rectangle){ r.x + 5.0f, r.y + r.height * 0.5f - 7.0f, r.width - 10.0f, 16.0f },
                label, 10, 0, (Color){ 175, 178, 195, 230 }, TEXT_ALIGN_CENTER);
        }
    }

    draw_scrollbar(browser);

    if (count == 0)
        draw_text_box((Rectangle){ browser->viewport.x + 6.0f, browser->viewport.y + browser->viewport.height * 0.5f - 7.0f, browser->viewport.width - 12.0f, 14.0f },
            "No cards", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_CENTER);

}
