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
    if (strcmp(id, "giant_spider") == 0) return (Color){ 160, 200, 90, 255 };
    if (strcmp(id, "dire_wolf") == 0) return (Color){ 185, 160, 120, 255 };
    if (strcmp(id, "forest_boar") == 0) return (Color){ 140, 110, 80, 255 };
    if (strcmp(id, "alpha_wolf") == 0) return (Color){ 210, 180, 100, 255 };
    if (strcmp(id, "elder_treant") == 0) return (Color){ 100, 185, 95, 255 };
    if (strcmp(id, "manticore") == 0) return (Color){ 180, 130, 200, 255 };
    if (strcmp(id, "basilisk") == 0) return (Color){ 100, 200, 140, 255 };
    if (strcmp(id, "venom_stalker") == 0) return (Color){ 120, 210, 90, 255 };
    if (strcmp(id, "greater_manticore") == 0) return (Color){ 200, 100, 220, 255 };
    if (strcmp(id, "venom_hydra") == 0) return (Color){ 140, 220, 110, 255 };
    if (strcmp(id, "venom_priest") == 0) return (Color){ 155, 90, 185, 255 };
    if (strcmp(id, "flame_imp") == 0) return (Color){ 235, 105, 55, 255 };
    if (strcmp(id, "fire_drake") == 0) return (Color){ 230, 130, 50, 255 };
    if (strcmp(id, "cinder_warden") == 0) return (Color){ 235, 155, 65, 255 };
    if (strcmp(id, "living_armor") == 0) return (Color){ 110, 160, 220, 255 };
    if (strcmp(id, "fire_giant") == 0) return (Color){ 230, 90, 70, 255 };
    if (strcmp(id, "elder_dragon") == 0) return (Color){ 220, 70, 50, 255 };
    return (Color){ 190, 70, 70, 255 };
}

static const char *enemy_mark(const char *id)
{
    if (!id) return "EN";
    if (strcmp(id, "giant_spider") == 0) return "GS";
    if (strcmp(id, "dire_wolf") == 0) return "DW";
    if (strcmp(id, "forest_boar") == 0) return "FB";
    if (strcmp(id, "alpha_wolf") == 0) return "AW";
    if (strcmp(id, "elder_treant") == 0) return "ET";
    if (strcmp(id, "manticore") == 0) return "MC";
    if (strcmp(id, "basilisk") == 0) return "BS";
    if (strcmp(id, "venom_stalker") == 0) return "VS";
    if (strcmp(id, "greater_manticore") == 0) return "GM";
    if (strcmp(id, "venom_hydra") == 0) return "VH";
    if (strcmp(id, "venom_priest") == 0) return "VP";
    if (strcmp(id, "flame_imp") == 0) return "FI";
    if (strcmp(id, "fire_drake") == 0) return "FD";
    if (strcmp(id, "cinder_warden") == 0) return "CW";
    if (strcmp(id, "living_armor") == 0) return "LA";
    if (strcmp(id, "fire_giant") == 0) return "FG";
    if (strcmp(id, "elder_dragon") == 0) return "ED";
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

    DrawEllipse(cx, cy + size / 2 + 8, size / 2, 4, shadow);

    Rectangle body_rect = { (float)(cx - 18), (float)(draw_y - 4), 36.0f, 32.0f };
    DrawRectangleRec(body_rect, body);
    DrawRectangleLinesEx(body_rect, highlighted ? 2.0f : 1.0f, highlighted ? (Color){ 255, 245, 150, 255 } : accent);

    DrawCircle(cx, draw_y - 15, 14, darken(accent, 0.55f));
    DrawCircleLines(cx, draw_y - 15, 14, (Color){ accent.r, accent.g, accent.b, 220 });

    DrawTriangle((Vector2){ (float)(cx - 11), (float)(draw_y - 26) },
                 (Vector2){ (float)(cx - 6), (float)(draw_y - 39) },
                 (Vector2){ (float)(cx - 1), (float)(draw_y - 25) },
                 darken(accent, 0.70f));
    DrawTriangle((Vector2){ (float)(cx + 11), (float)(draw_y - 26) },
                 (Vector2){ (float)(cx + 6), (float)(draw_y - 39) },
                 (Vector2){ (float)(cx + 1), (float)(draw_y - 25) },
                 darken(accent, 0.70f));

    DrawCircle(cx - 4, draw_y - 16, 2, (Color){ 255, 235, 170, 255 });
    DrawCircle(cx + 4, draw_y - 16, 2, (Color){ 255, 235, 170, 255 });

    const char *mark = enemy_mark(enemy->def->id);
    DrawText(mark, cx - MeasureText(mark, 10) / 2, draw_y + 8, 10, RAYWHITE);

    Rectangle nameplate = { (float)(cx - 42), (float)(draw_y - size / 2 - 22), 84.0f, 14.0f };
    DrawRectangleRec(nameplate, (Color){ 18, 18, 28, 220 });
    DrawRectangleLinesEx(nameplate, 1.0f, (Color){ accent.r, accent.g, accent.b, 170 });
    int name_size = 10;
    DrawText(enemy->def->name, cx - MeasureText(enemy->def->name, name_size) / 2, (int)nameplate.y + 3, name_size, RAYWHITE);

    int hp_bar_w = 72;
    int hp_bar_h = 8;
    int hp_x = cx - hp_bar_w / 2;
    int hp_y = cy + size / 2 + 8;

    DrawRectangleRec((Rectangle){ (float)hp_x, (float)hp_y, (float)hp_bar_w, (float)hp_bar_h }, (Color){ 42, 20, 25, 245 });

    float hp_ratio = (float)enemy->hp / (float)enemy->max_hp;
    if (hp_ratio < 0.0f) hp_ratio = 0.0f;
    if (hp_ratio > 1.0f) hp_ratio = 1.0f;
    int hp_fill_w = (int)((hp_bar_w - 4) * hp_ratio);
    if (hp_fill_w > 0)
    {
        Color hp_col = hp_ratio > 0.5f ? (Color){ 205, 70, 78, 255 } : (Color){ 230, 135, 55, 255 };
        DrawRectangleRec((Rectangle){ (float)(hp_x + 1), (float)(hp_y + 1), (float)hp_fill_w, (float)(hp_bar_h - 2) }, hp_col);
    }

    char hp_text[32];
    snprintf(hp_text, sizeof(hp_text), "%d / %d", enemy->hp, enemy->max_hp);
    DrawText(hp_text, cx - MeasureText(hp_text, 10) / 2, hp_y + 1, 10, RAYWHITE);

    if (enemy->shield > 0)
    {
        char shield_text[24];
        snprintf(shield_text, sizeof(shield_text), "S%d", enemy->shield);
        DrawText(shield_text, cx - MeasureText(shield_text, 10) / 2, hp_y + 11, 10, (Color){ 130, 200, 255, 235 });
    }
}

