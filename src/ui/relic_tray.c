#include "relic_tray.h"
#include "constants.h"
#include "util/text.h"
#include "util/math_utils.h"

static void draw_tooltip(Rectangle icon_rect, const RelicDef *def)
{
    if (!def) return;

    static TextScroll relic_scroll = { 0 };
    static const RelicDef *last_relic = NULL;
    if (last_relic != def)
    {
        relic_scroll.offset_y = 0;
        last_relic = def;
    }

    int title_h = measure_text_box(def->name, 158, 10, 0);
    if (title_h < ui_line_height(10)) title_h = ui_line_height(10);
    int body_h = measure_text_box(def->description, 155, 10, 0);
    int needed_h = title_h + body_h + 18;
    int tip_h = needed_h < 96 ? needed_h : 96;
    if (tip_h < 46) tip_h = 46;

    Rectangle tip = {
        icon_rect.x,
        icon_rect.y + icon_rect.height + 5.0f,
        170.0f,
        (float)tip_h
    };

    if (tip.x + tip.width > VIRT_W - 4) tip.x = VIRT_W - tip.width - 4;
    if (tip.y + tip.height > VIRT_H - 4) tip.y = icon_rect.y - tip.height - 5.0f;
    if (tip.x < 4) tip.x = 4;
    if (tip.y < 4) tip.y = 4;

    DrawRectangleRec(tip, (Color){ 9, 10, 17, 245 });
    DrawRectangleLinesEx(tip, 1.0f, (Color){ 210, 190, 95, 230 });
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, (float)title_h },
        def->name, 10, 0, (Color){ 235, 215, 120, 255 }, TEXT_ALIGN_LEFT);
    draw_text_box_scrolled((Rectangle){ tip.x + 6.0f, tip.y + 8.0f + title_h, tip.width - 15.0f, tip.height - title_h - 12.0f },
        def->description, 10, 0, (Color){ 190, 194, 215, 235 }, TEXT_ALIGN_LEFT, &relic_scroll, true);
}

void relic_tray_draw(const RelicId *relics, int count, Rectangle bounds)
{
    DrawRectangleRec(bounds, (Color){ 9, 10, 17, 205 });
    DrawRectangleLinesEx(bounds, 1.0f, (Color){ 120, 118, 82, 175 });
    draw_text_box((Rectangle){ bounds.x + 5.0f, bounds.y + 4.0f, bounds.width - 10.0f, 12.0f },
        "RELICS", 10, 0, (Color){ 210, 190, 95, 225 }, TEXT_ALIGN_LEFT);

    if (count <= 0)
    {
        draw_text_box((Rectangle){ bounds.x + 6.0f, bounds.y + 17.0f, bounds.width - 12.0f, 12.0f },
            "none", 10, 0, (Color){ 120, 124, 150, 190 }, TEXT_ALIGN_LEFT);
        return;
    }

    Vector2 mouse = GetMousePosition();
    const RelicDef *hovered = NULL;
    Rectangle hovered_rect = { 0 };

    int icon_size = 18;
    int gap = 4;
    int start_x = (int)bounds.x + 6;
    int start_y = (int)bounds.y + 16;
    int cols = (int)((bounds.width - 12.0f + gap) / (icon_size + gap));
    if (cols < 1) cols = 1;

    for (int i = 0; i < count && i < MAX_RUN_RELICS; i++)
    {
        const RelicDef *def = relic_def(relics[i]);
        if (!def) continue;

        int col = i % cols;
        int row = i / cols;
        Rectangle r = {
            (float)(start_x + col * (icon_size + gap)),
            (float)(start_y + row * (icon_size + gap)),
            (float)icon_size,
            (float)icon_size
        };
        if (r.y + r.height > bounds.y + bounds.height - 3.0f) break;

        bool hover = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hover ? (Color){ 90, 82, 42, 255 } : (Color){ 48, 42, 28, 255 });
        DrawRectangleLinesEx(r, hover ? 2.0f : 1.0f, (Color){ 225, 205, 105, hover ? 255 : 185 });
        DrawText(def->icon, snap_i(r.x + r.width * 0.5f - MeasureText(def->icon, 10) / 2), snap_i(r.y) + 6, 10, RAYWHITE);

        if (hover)
        {
            hovered = def;
            hovered_rect = r;
        }
    }

    if (hovered)
        draw_tooltip(hovered_rect, hovered);
}
