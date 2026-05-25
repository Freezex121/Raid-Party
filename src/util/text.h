#ifndef TEXT_H
#define TEXT_H

#include "raylib.h"
#include "ui/game_text.h"
#include <stdbool.h>

typedef struct {
    const char *text;
    int count;
    Color color;
} StyledSegment;

typedef enum {
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
} TextAlign;

typedef struct {
    int offset_y;
} TextScroll;

typedef struct {
    int content_height;
    int visible_height;
    int line_count;
    bool clipped;
} TextBoxResult;

int ui_font_size(int font_size);
int ui_line_height(int font_size);
TextBoxResult draw_text_box(Rectangle bounds, const char *text, int font_size, int line_spacing, Color color, TextAlign align);
TextBoxResult draw_text_box_scrolled(Rectangle bounds, const char *text, int font_size, int line_spacing, Color color, TextAlign align, TextScroll *scroll, bool show_scrollbar);
int  measure_text_box(const char *text, int max_width, int font_size, int line_spacing);
void text_scroll_update(TextScroll *scroll, Rectangle bounds, int content_height);

void draw_text_wrapped(const char *text, int x, int y, int max_width, int font_size, int line_spacing, Color color);
int  measure_text_wrapped(const char *text, int max_width, int font_size);

#endif
