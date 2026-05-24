#include "relic_tray.h"
#include "constants.h"
#include "util/text.h"
#include "util/math_utils.h"

static void draw_tooltip(Rectangle icon_rect, const RelicDef *def)
{
    if (!def) return;

    Rectangle tip = {
        icon_rect.x,
        icon_rect.y + icon_rect.height + 5.0f,
        170.0f,
        46.0f
    };

    if (tip.x + tip.width > VIRT_W - 4) tip.x = VIRT_W - tip.width - 4;
    if (tip.y + tip.height > VIRT_H - 4) tip.y = icon_rect.y - tip.height - 5.0f;
    if (tip.x < 4) tip.x = 4;
    if (tip.y < 4) tip.y = 4;

    DrawRectangleRec(tip, (Color){ 9, 10, 17, 245 });
    DrawRectangleLinesEx(tip, 1.0f, (Color){ 210, 190, 95, 230 });
    DrawText(def->name, (int)tip.x + 6, (int)tip.y + 5, 10, (Color){ 235, 215, 120, 255 });
    draw_text_wrapped(def->description, (int)tip.x + 6, (int)tip.y + 20, (int)tip.width - 12, 10, 1, (Color){ 190, 194, 215, 235 });
}

void relic_tray_draw(const RelicId *relics, int count, Rectangle bounds)
{
    DrawRectangleRec(bounds, (Color){ 9, 10, 17, 205 });
    DrawRectangleLinesEx(bounds, 1.0f, (Color){ 120, 118, 82, 175 });
    DrawText("RELICS", (int)bounds.x + 5, (int)bounds.y + 4, 10, (Color){ 210, 190, 95, 225 });

    if (count <= 0)
    {
        DrawText("none", (int)bounds.x + 6, (int)bounds.y + 17, 10, (Color){ 120, 124, 150, 190 });
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
