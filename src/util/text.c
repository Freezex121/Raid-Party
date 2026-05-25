#include "text.h"
#include <stdio.h>
#include <string.h>

#define TEXT_MAX_LINES 160
#define TEXT_LINE_MAX 256

typedef struct {
    char text[TEXT_LINE_MAX];
    int width;
} WrappedLine;

static int clamp_i(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int ui_font_size(int font_size)
{
    return font_size >= 18 ? 18 : 10;
}

int ui_line_height(int font_size)
{
    return ui_font_size(font_size) == 18 ? 20 : 12;
}

static int text_line_height(int font_size, int line_spacing)
{
    int h = ui_line_height(font_size) + line_spacing;
    return h < ui_line_height(font_size) ? ui_line_height(font_size) : h;
}

static void append_line(WrappedLine *lines, int *line_count, const char *text, int font_size)
{
    if (!lines || !line_count || *line_count >= TEXT_MAX_LINES) return;
    WrappedLine *line = &lines[*line_count];
    if (!text) text = "";
    strncpy(line->text, text, TEXT_LINE_MAX - 1);
    line->text[TEXT_LINE_MAX - 1] = '\0';
    line->width = MeasureText(line->text, font_size);
    (*line_count)++;
}

static void append_word_part(WrappedLine *lines, int *line_count, char *current, int *current_w,
    const char *word, int max_width, int font_size)
{
    if (!word || !word[0]) return;

    char candidate[TEXT_LINE_MAX];
    int has_current = current[0] != '\0';
    snprintf(candidate, sizeof(candidate), "%s%s%s", current, has_current ? " " : "", word);
    int candidate_w = MeasureText(candidate, font_size);
    if (candidate_w <= max_width)
    {
        strncpy(current, candidate, TEXT_LINE_MAX - 1);
        current[TEXT_LINE_MAX - 1] = '\0';
        *current_w = candidate_w;
        return;
    }

    if (has_current)
    {
        append_line(lines, line_count, current, font_size);
        current[0] = '\0';
        *current_w = 0;
    }

    if (MeasureText(word, font_size) <= max_width)
    {
        strncpy(current, word, TEXT_LINE_MAX - 1);
        current[TEXT_LINE_MAX - 1] = '\0';
        *current_w = MeasureText(current, font_size);
        return;
    }

    char part[TEXT_LINE_MAX] = "";
    int part_len = 0;
    for (const char *p = word; *p; p++)
    {
        if (part_len >= TEXT_LINE_MAX - 2)
        {
            append_line(lines, line_count, part, font_size);
            part[0] = '\0';
            part_len = 0;
        }

        char next[TEXT_LINE_MAX];
        memcpy(next, part, (size_t)part_len);
        next[part_len] = *p;
        next[part_len + 1] = '\0';

        if (part_len > 0 && MeasureText(next, font_size) > max_width)
        {
            append_line(lines, line_count, part, font_size);
            part[0] = *p;
            part[1] = '\0';
            part_len = 1;
        }
        else
        {
            part[part_len++] = *p;
            part[part_len] = '\0';
        }
    }

    strncpy(current, part, TEXT_LINE_MAX - 1);
    current[TEXT_LINE_MAX - 1] = '\0';
    *current_w = MeasureText(current, font_size);
}

static int wrap_text(const char *text, int max_width, int font_size, WrappedLine *lines)
{
    if (!lines || max_width <= 0) return 0;
    if (!text || !*text) return 0;

    font_size = ui_font_size(font_size);
    int line_count = 0;
    char current[TEXT_LINE_MAX] = "";
    int current_w = 0;

    const char *p = text;
    while (*p && line_count < TEXT_MAX_LINES)
    {
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\n')
        {
            append_line(lines, &line_count, current, font_size);
            current[0] = '\0';
            current_w = 0;
            p++;
            continue;
        }

        char word[TEXT_LINE_MAX];
        int wi = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n')
        {
            if (wi < TEXT_LINE_MAX - 1)
                word[wi++] = *p;
            p++;
        }
        word[wi] = '\0';
        append_word_part(lines, &line_count, current, &current_w, word, max_width, font_size);
    }

    if (current[0] && line_count < TEXT_MAX_LINES)
        append_line(lines, &line_count, current, font_size);

    return line_count;
}

int measure_text_box(const char *text, int max_width, int font_size, int line_spacing)
{
    WrappedLine lines[TEXT_MAX_LINES];
    int line_count = wrap_text(text, max_width, font_size, lines);
    if (line_count <= 0) return 0;
    return line_count * text_line_height(font_size, line_spacing);
}

void text_scroll_update(TextScroll *scroll, Rectangle bounds, int content_height)
{
    if (!scroll) return;

    int max_scroll = content_height - (int)bounds.height;
    if (max_scroll < 0) max_scroll = 0;

    Vector2 mouse = GetMousePosition();
    if (max_scroll > 0 && CheckCollisionPointRec(mouse, bounds))
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
            scroll->offset_y -= (int)(wheel * ui_line_height(10) * 2);
    }

    scroll->offset_y = clamp_i(scroll->offset_y, 0, max_scroll);
}

static TextBoxResult draw_text_box_internal(Rectangle bounds, const char *text, int font_size, int line_spacing,
    Color color, TextAlign align, int scroll_y, bool show_scrollbar)
{
    TextBoxResult result = { 0 };
    if (!text || !*text || bounds.width <= 0 || bounds.height <= 0) return result;

    font_size = ui_font_size(font_size);
    int line_h = text_line_height(font_size, line_spacing);
    WrappedLine lines[TEXT_MAX_LINES];
    int line_count = wrap_text(text, (int)bounds.width, font_size, lines);
    result.line_count = line_count;
    result.content_height = line_count * line_h;
    result.visible_height = (int)bounds.height;
    result.clipped = result.content_height > result.visible_height;

    scroll_y = clamp_i(scroll_y, 0, result.content_height > result.visible_height ? result.content_height - result.visible_height : 0);

    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
    for (int i = 0; i < line_count; i++)
    {
        int y = (int)bounds.y + i * line_h - scroll_y;
        if (y + line_h < (int)bounds.y) continue;
        if (y > (int)(bounds.y + bounds.height)) break;

        int x = (int)bounds.x;
        if (align == TEXT_ALIGN_CENTER)
            x = (int)(bounds.x + (bounds.width - lines[i].width) * 0.5f);
        else if (align == TEXT_ALIGN_RIGHT)
            x = (int)(bounds.x + bounds.width - lines[i].width);

        DrawText(lines[i].text, x, y, font_size, color);
    }
    EndScissorMode();

    if (show_scrollbar && result.clipped)
    {
        int track_x = (int)(bounds.x + bounds.width - 3);
        int track_y = (int)bounds.y;
        int track_h = (int)bounds.height;
        DrawRectangle(track_x, track_y, 2, track_h, (Color){ 40, 42, 56, 210 });

        float visible_ratio = bounds.height / (float)result.content_height;
        int thumb_h = (int)(track_h * visible_ratio);
        if (thumb_h < 8) thumb_h = 8;
        int max_scroll = result.content_height - result.visible_height;
        float scroll_t = max_scroll > 0 ? (float)scroll_y / (float)max_scroll : 0.0f;
        int thumb_y = track_y + (int)((track_h - thumb_h) * scroll_t);
        DrawRectangle(track_x, thumb_y, 2, thumb_h, (Color){ 135, 145, 185, 235 });
    }

    return result;
}

TextBoxResult draw_text_box(Rectangle bounds, const char *text, int font_size, int line_spacing, Color color, TextAlign align)
{
    return draw_text_box_internal(bounds, text, font_size, line_spacing, color, align, 0, false);
}

TextBoxResult draw_text_box_scrolled(Rectangle bounds, const char *text, int font_size, int line_spacing,
    Color color, TextAlign align, TextScroll *scroll, bool show_scrollbar)
{
    int content_height = measure_text_box(text, (int)bounds.width, font_size, line_spacing);
    text_scroll_update(scroll, bounds, content_height);
    return draw_text_box_internal(bounds, text, font_size, line_spacing, color, align,
        scroll ? scroll->offset_y : 0, show_scrollbar);
}

void draw_text_wrapped(const char *text, int x, int y, int max_width, int font_size, int line_spacing, Color color)
{
    Rectangle bounds = { (float)x, (float)y, (float)max_width, (float)measure_text_box(text, max_width, font_size, line_spacing) };
    draw_text_box(bounds, text, font_size, line_spacing, color, TEXT_ALIGN_LEFT);
}

int measure_text_wrapped(const char *text, int max_width, int font_size)
{
    return measure_text_box(text, max_width, font_size, 0);
}
