#include "party_frames.h"
#include "raylib.h"
#include "util/tween.h"
#include "util/text.h"
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
        case STATUS_BLEED:      return "BLD";
        case STATUS_WEAKNESS:   return "WEK";
        case STATUS_ENERGY_DRAIN: return "DRN";
        case STATUS_MARKED:     return "MRK";
        case STATUS_CONDUCTIVE: return "CND";
        case STATUS_BLIGHT:     return "BLT";
    }
    return "???";
}

static const char *status_icon(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING: return "B";
        case STATUS_RENEW: return "R";
        case STATUS_TRAP: return "T";
        case STATUS_TOTEM_HEAL: return "O";
        case STATUS_BLEED: return "L";
        case STATUS_WEAKNESS: return "W";
        case STATUS_ENERGY_DRAIN: return "D";
        case STATUS_MARKED: return "M";
        case STATUS_CONDUCTIVE: return "C";
        case STATUS_BLIGHT: return "X";
    }
    return "?";
}

static const char *status_description(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING: return "Burning: takes damage at the start of turns.";
        case STATUS_RENEW: return "Renew: heals at the start of turns.";
        case STATUS_TRAP: return "Trap: reduces outgoing damage.";
        case STATUS_TOTEM_HEAL: return "Totem: party healing over time.";
        case STATUS_BLEED: return "Bleed: takes damage at the start of turns.";
        case STATUS_WEAKNESS: return "Weakness: outgoing damage is reduced.";
        case STATUS_ENERGY_DRAIN: return "Energy Drain: lowers available energy.";
        case STATUS_MARKED: return "Marked: enables Ranger/Rogue/Mage/Cleric payoffs.";
        case STATUS_CONDUCTIVE: return "Conductive: enables Shaman/Mage chain effects.";
        case STATUS_BLIGHT: return "Blight: enables Warlock, Guardian, Cleric, and Paladin payoffs.";
    }
    return "Status effect.";
}

static Color status_color(StatusType type)
{
    switch (type)
    {
        case STATUS_BURNING:    return (Color){ 230, 100, 45, 255 };
        case STATUS_RENEW:      return (Color){ 90, 220, 125, 255 };
        case STATUS_TRAP:       return (Color){ 170, 125, 230, 255 };
        case STATUS_TOTEM_HEAL: return (Color){ 90, 190, 230, 255 };
        case STATUS_BLEED:      return (Color){ 210, 65, 80, 255 };
        case STATUS_WEAKNESS:   return (Color){ 165, 170, 195, 255 };
        case STATUS_ENERGY_DRAIN: return (Color){ 245, 210, 85, 255 };
        case STATUS_MARKED:     return (Color){ 245, 220, 75, 255 };
        case STATUS_CONDUCTIVE: return (Color){ 95, 185, 255, 255 };
        case STATUS_BLIGHT:     return (Color){ 190, 95, 230, 255 };
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

static Rectangle party_status_rect(Rectangle frame_rect, int status_index)
{
    return (Rectangle){
        frame_rect.x + frame_rect.width - 52.0f + status_index * 17.0f,
        frame_rect.y + frame_rect.height - 16.0f,
        15.0f,
        11.0f
    };
}

static Rectangle party_xp_bar_rect(Rectangle frame_rect)
{
    return (Rectangle){
        frame_rect.x + 4.0f,
        frame_rect.y + frame_rect.height - 5.0f,
        frame_rect.width - 8.0f,
        3.0f
    };
}

static void draw_status_tooltip(Rectangle anchor, StatusType status)
{
    const char *title = status_label(status);
    const char *body = status_description(status);
    Color accent = status_color(status);
    int w = 184;
    int body_h = measure_text_box(body, w - 12, 10, 0);
    if (body_h < ui_line_height(10)) body_h = ui_line_height(10);
    int h = 24 + body_h;
    int x = (int)(anchor.x + anchor.width * 0.5f - w * 0.5f);
    int y = (int)(anchor.y + anchor.height + 5.0f);
    if (x < 4) x = 4;
    if (x + w > VIRT_W - 4) x = VIRT_W - w - 4;
    if (y + h > HAND_Y - 4) y = (int)(anchor.y - h - 5.0f);
    if (y < 4) y = 4;

    Rectangle tip = { (float)x, (float)y, (float)w, (float)h };
    DrawRectangleRec(tip, (Color){ 10, 11, 18, 245 });
    DrawRectangleLinesEx(tip, 1.0f, (Color){ accent.r, accent.g, accent.b, 220 });
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, 12.0f },
        title, 10, 0, accent, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 18.0f, tip.width - 12.0f, tip.height - 22.0f },
        body, 10, 0, (Color){ 210, 214, 235, 235 }, TEXT_ALIGN_LEFT);
}

static void draw_member_tooltip(Rectangle anchor, const PartyMember *member)
{
    if (!member) return;
    int w = 200;
    int h = 88;
    int x = (int)(anchor.x + anchor.width * 0.5f - w * 0.5f);
    int y = (int)(anchor.y + anchor.height + 5.0f);
    if (x < 4) x = 4;
    if (x + w > VIRT_W - 4) x = VIRT_W - w - 4;
    if (y + h > HAND_Y - 4) y = (int)(anchor.y - h - 5.0f);
    if (y < 4) y = 4;

    Color accent = theme_class_color(member->class);
    Rectangle tip = { (float)x, (float)y, (float)w, (float)h };
    DrawRectangleRec(tip, (Color){ 10, 11, 18, 245 });
    DrawRectangleLinesEx(tip, 1.0f, (Color){ accent.r, accent.g, accent.b, 220 });

    char title[64];
    snprintf(title, sizeof(title), "%s  LV %d", member->name, member->level);
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 5.0f, tip.width - 12.0f, 12.0f },
        title, 10, 0, accent, TEXT_ALIGN_LEFT);

    char body[160];
    snprintf(body, sizeof(body), "HP %d/%d  Shield %d  Aggro %d",
        member->hp, member->max_hp, member->shield, member->aggro);
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 19.0f, tip.width - 12.0f, 12.0f },
        body, 10, 0, (Color){ 210, 214, 235, 235 }, TEXT_ALIGN_LEFT);

    // XP progress
    char xp_line[64];
    if (member->level >= MAX_LEVEL)
        snprintf(xp_line, sizeof(xp_line), "XP MAX    Combat %d/%d", member->combat_xp, MAX_COMBAT_XP);
    else
        snprintf(xp_line, sizeof(xp_line), "XP %d/%d    Combat %d/%d",
            party_member_xp_into_level(member), xp_for_level(member->level),
            member->combat_xp, MAX_COMBAT_XP);
    draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 33.0f, tip.width - 12.0f, 12.0f },
        xp_line, 10, 0, (Color){ 245, 205, 65, 235 }, TEXT_ALIGN_LEFT);

    // XP bar in tooltip
    if (member->level < MAX_LEVEL)
    {
        int need = xp_for_level(member->level);
        float xp_r = need > 0 ? (float)party_member_xp_into_level(member) / (float)need : 0.0f;
        if (xp_r < 0.0f) xp_r = 0.0f;
        if (xp_r > 1.0f) xp_r = 1.0f;
        int bar_w = (int)((w - 12) * xp_r);
        if (bar_w > 0)
        {
            DrawRectangleRec((Rectangle){ tip.x + 6.0f, tip.y + 47.0f, (float)(w - 12), 4.0f },
                (Color){ 40, 35, 20, 255 });
            DrawRectangleRec((Rectangle){ tip.x + 6.0f, tip.y + 47.0f, (float)bar_w, 4.0f },
                (Color){ 245, 205, 65, 245 });
        }
    }

    // Status effects list
    char status_line[96] = "";
    for (int s = 0; s < member->status_count && s < 3; s++)
    {
        if (s > 0) strcat(status_line, "  ");
        char buf[24];
        snprintf(buf, sizeof(buf), "%s(%d)", status_label(member->statuses[s].type), member->statuses[s].turns);
        strcat(status_line, buf);
    }
    if (status_line[0])
    {
        draw_text_box((Rectangle){ tip.x + 6.0f, tip.y + 55.0f, tip.width - 12.0f, 28.0f },
            status_line, 10, 0, (Color){ 180, 184, 210, 220 }, TEXT_ALIGN_LEFT);
    }
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

        Color bg = m->alive ? (Color){ 28, 29, 45, 255 } : (Color){ 40, 30, 30, 180 };
        DrawRectangleRec((Rectangle){ (float)x, (float)y, (float)frame_w, (float)frame_h }, bg);
        DrawRectangleRec((Rectangle){ (float)x, (float)y, 3.0f, (float)frame_h }, (Color){ tint.r, tint.g, tint.b, m->alive ? 210 : 120 });
        DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)frame_w, (float)frame_h }, 1.0f, (Color){ tint.r, tint.g, tint.b, 135 });

        if (i == target_idx && m->alive)
        {
            DrawRectangleLinesEx((Rectangle){ (float)(x - 1), (float)(y - 1), (float)(frame_w + 2), (float)(frame_h + 2) },
                2.0f, (Color){ 245, 165, 65, 255 });
            DrawText("", x + 6, y + 2, 10, (Color){ 245, 165, 65, 240 });
        }

        Color bar_bg = (Color){ 20, 20, 30, 255 };
        int portrait_x = x + 20;
        int portrait_y = y + 21;
        int bar_x = x + 39;
        int bar_y = y + 10;
        int bar_w = frame_w - 48;
        int bar_h = 8;
        char level_text[16];
        snprintf(level_text, sizeof(level_text), "LV %d%s", m->level, m->pending_levels > 0 ? "!" : "");
        draw_text_box((Rectangle){ (float)bar_x, (float)(y + 1), (float)bar_w, 11.0f },
            level_text, 10, 0,
            (Color){255, 255, 255, 255 },
            TEXT_ALIGN_RIGHT);
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

        theme_draw_class_portrait(m->class, portrait_x, portrait_y, 12, m->alive);

        char hp_text[32];
        snprintf(hp_text, sizeof(hp_text), "%d / %d", m->hp, m->max_hp);
        draw_text_box((Rectangle){ (float)bar_x, (float)(y + 18), (float)bar_w, 12.0f },
            hp_text, 10, 0, (Color){ 200, 200, 220, 215 }, TEXT_ALIGN_LEFT);

        if (!m->alive)
        {
            DrawRectangleRec((Rectangle){ (float)(x + 2), (float)(y + 2), (float)(frame_w - 4), (float)(frame_h - 4) }, (Color){ 80, 10, 18, 120 });
            DrawText("DOWNED", x + frame_w / 2 - MeasureText("DOWNED", 10) / 2, y + 13, 10, (Color){ 255, 95, 95, 230 });
        }

        for (int s = 0; s < m->status_count && s < 3; s++)
        {
            StatusEffect *st = &m->statuses[s];
            Color sc = status_color(st->type);
            Rectangle pill = party_status_rect(frame_rect, s);
            DrawRectangleRec(pill, (Color){ sc.r, sc.g, sc.b, 70 });
            DrawRectangleLinesEx(pill, 1.0f, (Color){ 255, 255, 255, 160 });
            char label[8];
            snprintf(label, sizeof(label), "%s%d", status_icon(st->type), st->turns);
            draw_text_box((Rectangle){ pill.x + 1.0f, pill.y, pill.width - 2.0f, pill.height },
                label, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        }

        Rectangle xp_bar = party_xp_bar_rect(frame_rect);
        DrawRectangleRec(xp_bar, (Color){ 48, 42, 24, 235 });
        float xp_ratio = 1.0f;
        if (m->level < MAX_LEVEL)
        {
            int need = xp_for_level(m->level);
            xp_ratio = need > 0 ? (float)party_member_xp_into_level(m) / (float)need : 0.0f;
            if (xp_ratio < 0.0f) xp_ratio = 0.0f;
            if (xp_ratio > 1.0f) xp_ratio = 1.0f;
        }
        int xp_fill = (int)(xp_bar.width * xp_ratio);
        if (xp_fill > 0)
            DrawRectangleRec((Rectangle){ xp_bar.x, xp_bar.y, (float)xp_fill, xp_bar.height },
                (Color){ 245, 205, 65, 245 });
        if (m->combat_xp >= MAX_COMBAT_XP)
            DrawRectangleLinesEx(xp_bar, 1.0f, (Color){ 255, 150, 45, 230 });
    }
}

void party_frames_draw_tooltips(Party *party)
{
    if (!party) return;
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < party->count; i++)
    {
        PartyMember *m = &party->members[i];
        Rectangle frame_rect = layout_party_frame_rect(party->count, i);
        for (int s = 0; s < m->status_count && s < 3; s++)
        {
            Rectangle r = party_status_rect(frame_rect, s);
            if (CheckCollisionPointRec(mouse, r))
            {
                draw_status_tooltip(r, m->statuses[s].type);
                return;
            }
        }
    }

    for (int i = 0; i < party->count; i++)
    {
        Rectangle frame_rect = layout_party_frame_rect(party->count, i);
        if (CheckCollisionPointRec(mouse, frame_rect))
        {
            draw_member_tooltip(frame_rect, &party->members[i]);
            return;
        }
    }
}



