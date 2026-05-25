#include "enemy_render.h"
#include "ui/theme.h"
#include "constants.h"
#include "util/text.h"
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

static const char *status_icon(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING:    return "B";
        case STATUS_TRAP:       return "T";
        case STATUS_BLEED:      return "L";
        case STATUS_WEAKNESS:   return "W";
        case STATUS_MARKED:     return "M";
        case STATUS_CONDUCTIVE: return "C";
        case STATUS_BLIGHT:     return "X";
        default:                return "?";
    }
}

static const char *status_name(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING: return "BURNING";
        case STATUS_TRAP: return "TRAP";
        case STATUS_BLEED: return "BLEED";
        case STATUS_WEAKNESS: return "WEAKNESS";
        case STATUS_MARKED: return "MARKED";
        case STATUS_CONDUCTIVE: return "CONDUCTIVE";
        case STATUS_BLIGHT: return "BLIGHT";
        case STATUS_RENEW: return "RENEW";
        case STATUS_TOTEM_HEAL: return "TOTEM";
        case STATUS_ENERGY_DRAIN: return "ENERGY DRAIN";
        default: return "STATUS";
    }
}

static const char *status_description(StatusType type)
{
    switch (type)
    {
        case STATUS_MARKED: return "Marked: enables Ranger setup and Rogue, Mage, Cleric, Paladin, and Warlock payoffs.";
        case STATUS_CONDUCTIVE: return "Conductive: enables Shaman and Mage chain effects, plus Rogue and Warlock payoffs.";
        case STATUS_BLIGHT: return "Blight: enables Guardian, Cleric, Paladin, and Warlock cash-out effects.";
        case STATUS_BURNING: return "Burning: takes damage at the start of turns.";
        case STATUS_TRAP: return "Trap: deals less damage while active.";
        case STATUS_BLEED: return "Bleed: takes damage at the start of turns.";
        case STATUS_WEAKNESS: return "Weakness: outgoing damage is reduced.";
        case STATUS_RENEW: return "Renew: heals over time.";
        case STATUS_TOTEM_HEAL: return "Totem: healing over time.";
        case STATUS_ENERGY_DRAIN: return "Energy Drain: reduces available energy.";
        default: return "Status effect.";
    }
}

static Color status_icon_color(StatusType type)
{
    switch (type)
    {
        case STATUS_MARKED:     return (Color){ 245, 220, 75, 255 };
        case STATUS_CONDUCTIVE: return (Color){ 95, 185, 255, 255 };
        case STATUS_BLIGHT:     return (Color){ 190, 95, 230, 255 };
        case STATUS_BURNING:    return (Color){ 245, 120, 55, 255 };
        case STATUS_TRAP:       return (Color){ 180, 125, 230, 255 };
        case STATUS_BLEED:      return (Color){ 210, 65, 80, 255 };
        case STATUS_WEAKNESS:   return (Color){ 165, 170, 195, 255 };
        default:                return (Color){ 155, 160, 180, 255 };
    }
}

static bool status_pulses(StatusType type)
{
    return type == STATUS_MARKED || type == STATUS_CONDUCTIVE || type == STATUS_BLIGHT;
}

static void draw_enemy_statuses(EnemyState *enemy, int cx, int top_y)
{
    int count = enemy->status_count;
    if (count > 5) count = 5;
    int start_x = cx - (count * 13) / 2;
    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 7.0f);
    for (int i = 0; i < count; i++)
    {
        StatusEffect *st = &enemy->statuses[i];
        Color c = status_icon_color(st->type);
        unsigned char fill_a = status_pulses(st->type) ? (unsigned char)(65 + 75 * pulse) : 70;
        Rectangle r = { (float)(start_x + i * 13), (float)top_y, 11.0f, 11.0f };
        DrawRectangleRec(r, (Color){ c.r, c.g, c.b, fill_a });
        DrawRectangleLinesEx(r, 1.0f, (Color){ c.r, c.g, c.b, 220 });
        const char *icon = status_icon(st->type);
        DrawText(icon, (int)r.x + 3, (int)r.y + 2, 10, RAYWHITE);
        DrawText(TextFormat("%d", st->turns), (int)r.x + 7, (int)r.y + 6, 10, RAYWHITE);
    }
}

static Rectangle enemy_status_rect(EnemyState *enemy, int cx, int top_y, int index)
{
    int count = enemy->status_count;
    if (count > 5) count = 5;
    int start_x = cx - (count * 13) / 2;
    return (Rectangle){ (float)(start_x + index * 13), (float)top_y, 11.0f, 11.0f };
}

static void draw_status_tooltip(Rectangle anchor, StatusType status)
{
    Color accent = status_icon_color(status);
    const char *title = status_name(status);
    const char *body = status_description(status);
    int w = 196;
    int body_h = measure_text_box(body, w - 12, 10, 0);
    if (body_h < ui_line_height(10)) body_h = ui_line_height(10);
    int h = 24 + body_h;
    int x = (int)(anchor.x + anchor.width * 0.5f - w * 0.5f);
    int y = (int)(anchor.y - h - 5.0f);
    if (x < 4) x = 4;
    if (x + w > VIRT_W - 4) x = VIRT_W - w - 4;
    if (y < FRAME_Y + FRAME_H + 3) y = (int)(anchor.y + anchor.height + 5.0f);
    if (y + h > HAND_Y - 4) y = HAND_Y - h - 4;
    if (y < 4) y = 4;

    Rectangle tip = { (float)x, (float)y, (float)w, (float)h };
    DrawRectangleRec(tip, (Color){ 10, 11, 18, 245 });
    DrawRectangleLinesEx(tip, 1.0f, (Color){ accent.r, accent.g, accent.b, 225 });
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, 12.0f },
        title, 10, 0, accent, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 18.0f, tip.width - 12.0f, tip.height - 22.0f },
        body, 10, 0, (Color){ 210, 214, 235, 235 }, TEXT_ALIGN_LEFT);
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
    draw_text_box((Rectangle){ nameplate.x + 3.0f, nameplate.y + 2.0f, nameplate.width - 6.0f, nameplate.height - 3.0f },
        enemy->def->name, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    if (enemy->status_count > 0)
        draw_enemy_statuses(enemy, cx, (int)nameplate.y - 13);

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

void enemy_render_draw_status_tooltip(EnemyState *enemy)
{
    if (!enemy || !enemy->def || enemy->hp <= 0 || enemy->status_count <= 0) return;

    int cx = enemy->pos_x;
    int cy = enemy->pos_y;
    int size = ENEMY_SIZE;
    float bob = sinf((float)GetTime() * 2.0f + cx * 0.01f) * 1.2f;
    int draw_y = cy + (int)bob;
    int top_y = draw_y - size / 2 - 22 - 13;
    int count = enemy->status_count;
    if (count > 5) count = 5;

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < count; i++)
    {
        Rectangle r = enemy_status_rect(enemy, cx, top_y, i);
        if (CheckCollisionPointRec(mouse, r))
        {
            draw_status_tooltip(r, enemy->statuses[i].type);
            return;
        }
    }
}

