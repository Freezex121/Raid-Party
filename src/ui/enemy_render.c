#include "enemy_render.h"
#include "ui/theme.h"
#include "constants.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Color enemy_accent(const char *id)
{
    if (!id) return (Color){ 190, 70, 70, 255 };
    if (strcmp(id, "flame_imp") == 0) return (Color){ 235, 105, 55, 255 };
    if (strcmp(id, "rage_knight") == 0) return (Color){ 210, 70, 82, 255 };
    if (strcmp(id, "cult_healer") == 0) return (Color){ 95, 210, 125, 255 };
    if (strcmp(id, "berserker") == 0) return (Color){ 230, 120, 60, 255 };
    if (strcmp(id, "living_armor") == 0) return (Color){ 110, 160, 220, 255 };
    if (strcmp(id, "venom_stalker") == 0) return (Color){ 120, 210, 90, 255 };
    if (strcmp(id, "arcane_wisp") == 0) return (Color){ 165, 105, 235, 255 };
    return (Color){ 190, 70, 70, 255 };
}

static const char *enemy_mark(const char *id)
{
    if (!id) return "EN";
    if (strcmp(id, "flame_imp") == 0) return "FI";
    if (strcmp(id, "rage_knight") == 0) return "RK";
    if (strcmp(id, "cult_healer") == 0) return "CH";
    if (strcmp(id, "berserker") == 0) return "BR";
    if (strcmp(id, "living_armor") == 0) return "LA";
    if (strcmp(id, "venom_stalker") == 0) return "VS";
    if (strcmp(id, "arcane_wisp") == 0) return "AW";
    return "EN";
}

static Color darken(Color c, float m)
{
    c.r = (unsigned char)(c.r * m);
    c.g = (unsigned char)(c.g * m);
    c.b = (unsigned char)(c.b * m);
    return c;
}

void enemy_render_draw(EnemyState *enemy, bool highlighted, bool targeting)
{
    if (!enemy->def || enemy->hp <= 0) return;

    int cx = enemy->pos_x;
    int cy = enemy->pos_y;
    int size = ENEMY_SIZE;
    float bob = sinf((float)GetTime() * 2.0f + cx * 0.01f) * 1.2f;
    int draw_y = cy + (int)bob;

    Color accent = enemy_accent(enemy->def->id);
    Color shadow = (Color){ 0, 0, 0, 80 };
    Color body = darken(accent, 0.42f);

    if (targeting)
    {
        Color highlight = highlighted ? (Color){ 255, 245, 120, 80 } : (Color){ 120, 120, 70, 25 };
        DrawCircle(cx, draw_y, size / 2 + (highlighted ? 6 : 3), highlight);
    }

    DrawEllipse(cx, cy + size / 2 + 5, size / 2, 3, shadow);

    Rectangle body_rect = { (float)(cx - 13), (float)(draw_y - 3), 26.0f, 23.0f };
    DrawRectangleRec(body_rect, body);
    DrawRectangleLinesEx(body_rect, highlighted ? 2.0f : 1.0f, highlighted ? (Color){ 255, 245, 150, 255 } : accent);

    DrawCircle(cx, draw_y - 11, 10, darken(accent, 0.55f));
    DrawCircleLines(cx, draw_y - 11, 10, (Color){ accent.r, accent.g, accent.b, 220 });

    DrawTriangle((Vector2){ (float)(cx - 8), (float)(draw_y - 19) },
                 (Vector2){ (float)(cx - 4), (float)(draw_y - 29) },
                 (Vector2){ (float)(cx - 1), (float)(draw_y - 18) },
                 darken(accent, 0.70f));
    DrawTriangle((Vector2){ (float)(cx + 8), (float)(draw_y - 19) },
                 (Vector2){ (float)(cx + 4), (float)(draw_y - 29) },
                 (Vector2){ (float)(cx + 1), (float)(draw_y - 18) },
                 darken(accent, 0.70f));

    DrawCircle(cx - 3, draw_y - 12, 1, (Color){ 255, 235, 170, 255 });
    DrawCircle(cx + 3, draw_y - 12, 1, (Color){ 255, 235, 170, 255 });

    const char *mark = enemy_mark(enemy->def->id);
    DrawText(mark, cx - MeasureText(mark, 6) / 2, draw_y + 4, 6, RAYWHITE);

    Rectangle nameplate = { (float)(cx - 33), (float)(draw_y - size / 2 - 15), 66.0f, 10.0f };
    DrawRectangleRec(nameplate, (Color){ 18, 18, 28, 220 });
    DrawRectangleLinesEx(nameplate, 1.0f, (Color){ accent.r, accent.g, accent.b, 170 });
    int name_size = 6;
    while (name_size > 4 && MeasureText(enemy->def->name, name_size) > (int)nameplate.width - 4)
        name_size--;
    DrawText(enemy->def->name, cx - MeasureText(enemy->def->name, name_size) / 2, (int)nameplate.y + 2, name_size, RAYWHITE);

    int hp_bar_w = 46;
    int hp_bar_h = 6;
    int hp_x = cx - hp_bar_w / 2;
    int hp_y = cy + size / 2 + 8;

    DrawRectangleRec((Rectangle){ (float)hp_x, (float)hp_y, (float)hp_bar_w, (float)hp_bar_h }, (Color){ 42, 20, 25, 245 });

    float hp_ratio = (float)enemy->hp / (float)enemy->def->max_hp;
    if (hp_ratio < 0.0f) hp_ratio = 0.0f;
    if (hp_ratio > 1.0f) hp_ratio = 1.0f;
    int hp_fill_w = (int)((hp_bar_w - 4) * hp_ratio);
    if (hp_fill_w > 0)
    {
        Color hp_col = hp_ratio > 0.5f ? (Color){ 205, 70, 78, 255 } : (Color){ 230, 135, 55, 255 };
        DrawRectangleRec((Rectangle){ (float)(hp_x + 1), (float)(hp_y + 1), (float)hp_fill_w, (float)(hp_bar_h - 2) }, hp_col);
    }

    char hp_text[32];
    snprintf(hp_text, sizeof(hp_text), "%d / %d", enemy->hp, enemy->def->max_hp);
    DrawText(hp_text, cx - MeasureText(hp_text, 5) / 2, hp_y + 1, 5, RAYWHITE);

    if (enemy->shield > 0)
    {
        char shield_text[24];
        snprintf(shield_text, sizeof(shield_text), "S%d", enemy->shield);
        DrawText(shield_text, cx - MeasureText(shield_text, 5) / 2, hp_y + 8, 5, (Color){ 130, 200, 255, 235 });
    }
}

