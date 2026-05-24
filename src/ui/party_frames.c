#include "party_frames.h"
#include "raylib.h"
#include "util/tween.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "constants.h"
#include <stdio.h>
#include <string.h>

static float shown_hp[MAX_PARTY_SIZE];
static float shown_shield[MAX_PARTY_SIZE];
static ClassType shown_class[MAX_PARTY_SIZE];
static int shown_count = -1;

int party_frame_hit_test(Party *party, Vector2 mouse)
{
    for (int i = 0; i < party->count; i++)
    {
        Rectangle r = layout_party_frame_rect(party->count, i);
        if (CheckCollisionPointRec(mouse, r)) return i;
    }
    return -1;
}

static Color class_tint(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return (Color){ 74, 144, 217, 255 };
        case CLASS_CLERIC:   return (Color){ 225, 170, 50, 255 };
        case CLASS_MAGE:     return (Color){ 155, 89, 182, 255 };
        case CLASS_ROGUE:    return (Color){ 39, 174, 96, 255 };
        case CLASS_SHAMAN:   return (Color){ 230, 126, 34, 255 };
        case CLASS_RANGER:   return (Color){ 70, 190, 120, 255 };
        default:             return (Color){ 100, 100, 120, 255 };
    }
}

static const char *status_label(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING:    return "BRN";
        case STATUS_RENEW:      return "REN";
        case STATUS_TRAP:       return "TRP";
        case STATUS_TOTEM_HEAL: return "TOT";
    }
    return "???";
}

static Color status_color(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING:    return (Color){ 230, 100, 45, 255 };
        case STATUS_RENEW:      return (Color){ 90, 220, 125, 255 };
        case STATUS_TRAP:       return (Color){ 170, 125, 230, 255 };
        case STATUS_TOTEM_HEAL: return (Color){ 90, 190, 230, 255 };
    }
    return (Color){ 150, 150, 170, 255 };
}

static int highest_aggro_index(Party *party)
{
    int best = -1;
    int highest = -1;
    for (int i = 0; i < party->count; i++)
    {
        if (!party->members[i].alive) continue;
        if (party->members[i].aggro > highest)
        {
            highest = party->members[i].aggro;
            best = i;
        }
    }
    return best;
}

void party_frames_draw(Party *party)
{
    float dt = GetFrameTime();
    int target_idx = highest_aggro_index(party);

    if (shown_count != party->count)
    {
        shown_count = party->count;
        for (int i = 0; i < MAX_PARTY_SIZE; i++)
            shown_class[i] = CLASS_NONE;
    }

    for (int i = 0; i < party->count; i++)
    {
        PartyMember *m = &party->members[i];
        Rectangle frame_rect = layout_party_frame_rect(party->count, i);
        int x = (int)frame_rect.x;
        int y = (int)frame_rect.y;
        int frame_w = (int)frame_rect.width;
        int frame_h = (int)frame_rect.height;

        Color tint = theme_class_color(m->class);

        if (shown_class[i] != m->class)
        {
            shown_class[i] = m->class;
            shown_hp[i] = (float)m->hp;
            shown_shield[i] = (float)m->shield;
        }

        float speed = dt * 10.0f;
        if (speed > 1.0f) speed = 1.0f;
        shown_hp[i] += ((float)m->hp - shown_hp[i]) * speed;
        shown_shield[i] += ((float)m->shield - shown_shield[i]) * speed;

        Color bg = m->alive ? (Color){ 30, 30, 50, 255 } : (Color){ 40, 30, 30, 180 };
        DrawRectangleRec((Rectangle){ (float)x, (float)y, (float)frame_w, (float)frame_h }, bg);
        DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)frame_w, (float)frame_h }, 1.0f, (Color){ tint.r, tint.g, tint.b, 135 });

        if (i == target_idx && m->alive)
        {
            DrawRectangleLinesEx((Rectangle){ (float)(x - 1), (float)(y - 1), (float)(frame_w + 2), (float)(frame_h + 2) },
                1.0f, (Color){ 245, 165, 65, 220 });
            DrawText("T", x + frame_w - 8, y + 2, 10, (Color){ 245, 165, 65, 230 });
        }

        Color bar_bg = (Color){ 20, 20, 30, 255 };
        int portrait_x = x + 17;
        int portrait_y = y + 17;
        int bar_x = x + 36;
        int bar_y = y + 7;
        int bar_w = frame_w - 44;
        int bar_h = 7;
        DrawRectangleRec((Rectangle){ (float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h }, bar_bg);

        float hp_ratio = shown_hp[i] / (float)m->max_hp;
        if (hp_ratio < 0.0f) hp_ratio = 0.0f;
        if (hp_ratio > 1.0f) hp_ratio = 1.0f;
        Color hp_color;
        if (hp_ratio > 0.6f) hp_color = (Color){ 70, 220, 120, 255 };
        else if (hp_ratio > 0.3f) hp_color = (Color){ 240, 200, 50, 255 };
        else hp_color = (Color){ 220, 60, 60, 255 };

        int hp_fill_w = (int)(bar_w * hp_ratio);
        if (hp_fill_w > 0)
            DrawRectangleRec((Rectangle){ (float)bar_x, (float)bar_y, (float)hp_fill_w, (float)bar_h }, hp_color);

        if (shown_shield[i] > 0.5f)
        {
            Color shield_col = { 100, 180, 255, 120 };
            int shield_fill = (int)(bar_w * shown_shield[i] / (m->max_hp / 2));
            if (shield_fill > bar_w) shield_fill = bar_w;
            DrawRectangleRec((Rectangle){ (float)bar_x, (float)bar_y, (float)shield_fill, (float)bar_h }, shield_col);
        }

        theme_draw_class_portrait(m->class, portrait_x, portrait_y, 10, m->alive);

        char hp_text[32];
        snprintf(hp_text, sizeof(hp_text), "%d / %d", m->hp, m->max_hp);
        DrawText(hp_text, x + 36, y + 17, 10, (Color){ 200, 200, 220, 210 });

        Color aggro_col = (Color){ 220, 160, 60, 200 };
        char agg_text[16];
        snprintf(agg_text, sizeof(agg_text), "A:%d", m->aggro);
        DrawText(agg_text, x + 36, y + 25, 10, aggro_col);

        if (!m->alive)
        {
            DrawRectangleRec((Rectangle){ (float)(x + 2), (float)(y + 2), (float)(frame_w - 4), (float)(frame_h - 4) }, (Color){ 80, 10, 18, 120 });
            DrawText("DOWNED", x + frame_w / 2 - MeasureText("DOWNED", 10) / 2, y + 13, 10, (Color){ 255, 95, 95, 230 });
        }

        int sx = x + frame_w - 54;
        int sy = y + 24;
        for (int s = 0; s < m->status_count && s < 3; s++)
        {
            StatusEffect *st = &m->statuses[s];
            Color sc = status_color(st->type);
            Rectangle pill = { (float)(sx + s * 18), (float)sy, 16.0f, 8.0f };
            DrawRectangleRec(pill, (Color){ sc.r, sc.g, sc.b, 70 });
            DrawRectangleLinesEx(pill, 1.0f, (Color){ sc.r, sc.g, sc.b, 180 });
            DrawText(status_label(st->type), (int)pill.x + 1, (int)pill.y + 2, 10, (Color){ 235, 235, 245, 230 });
            DrawText(TextFormat("%d", st->turns), (int)pill.x + 11, (int)pill.y + 1, 10, RAYWHITE);
        }
    }
}



