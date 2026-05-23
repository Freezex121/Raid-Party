#include "theme.h"
#include "assets.h"
#include <string.h>
#include <stdio.h>

static Color color_fade_alpha(Color c, unsigned char a)
{
    c.a = a;
    return c;
}

static Rectangle fit_card_art_rect(Rectangle bounds)
{
    float sx = bounds.width / (float)CARD_ART_SOURCE_W;
    float sy = bounds.height / (float)CARD_ART_SOURCE_H;
    float scale = sx < sy ? sx : sy;

    float w = CARD_ART_SOURCE_W * scale;
    float h = CARD_ART_SOURCE_H * scale;
    return (Rectangle){
        bounds.x + (bounds.width - w) * 0.5f,
        bounds.y + (bounds.height - h) * 0.5f,
        w,
        h
    };
}

static int scaled_x(Rectangle r, float sx)
{
    return (int)(r.x + sx * (r.width / (float)CARD_ART_SOURCE_W));
}

static int scaled_y(Rectangle r, float sy)
{
    return (int)(r.y + sy * (r.height / (float)CARD_ART_SOURCE_H));
}

static int scaled_font(Rectangle r, int source_px)
{
    float s = r.width / (float)CARD_ART_SOURCE_W;
    int f = (int)(source_px * s);
    return f < 4 ? 4 : f;
}

static void draw_text_fit(const char *text, int x, int y, int max_w, int size, Color color)
{
    if (!text) return;
    while (size > 4 && MeasureText(text, size) > max_w)
        size--;
    DrawText(text, x, y, size, color);
}

static Texture2D class_icon_texture(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return (Texture2D){0};
    return g_assets.class_icons[ct];
}

Color theme_class_color(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return (Color){ 74, 144, 217, 255 };
        case CLASS_CLERIC:   return (Color){ 225, 170, 50, 255 };
        case CLASS_MAGE:     return (Color){ 155, 89, 182, 255 };
        case CLASS_ROGUE:    return (Color){ 39, 174, 96, 255 };
        case CLASS_SHAMAN:   return (Color){ 230, 126, 34, 255 };
        case CLASS_RANGER:   return (Color){ 70, 190, 120, 255 };
        default:             return (Color){ 130, 135, 160, 255 };
    }
}

Color theme_class_dark(ClassType ct)
{
    Color c = theme_class_color(ct);
    c.r = (unsigned char)(c.r * 0.20f);
    c.g = (unsigned char)(c.g * 0.20f);
    c.b = (unsigned char)(c.b * 0.24f);
    return c;
}

Color theme_card_type_color(CardType type)
{
    switch (type)
    {
        case CARD_ATTACK: return (Color){ 215, 82, 70, 255 };
        case CARD_SKILL:  return (Color){ 80, 155, 220, 255 };
        case CARD_POWER:  return (Color){ 175, 100, 225, 255 };
    }
    return (Color){ 130, 135, 160, 255 };
}

Color theme_node_color(NodeType type)
{
    switch (type)
    {
        case NODE_START:  return (Color){ 125, 130, 155, 255 };
        case NODE_COMBAT: return (Color){ 205, 92, 74, 255 };
        case NODE_ELITE:  return (Color){ 185, 86, 205, 255 };
        case NODE_REST:   return (Color){ 78, 190, 120, 255 };
        case NODE_SHOP:   return (Color){ 225, 185, 70, 255 };
        case NODE_BOSS:   return (Color){ 230, 65, 70, 255 };
    }
    return (Color){ 125, 130, 155, 255 };
}

const char *theme_class_abbrev(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return "GD";
        case CLASS_CLERIC:   return "CL";
        case CLASS_MAGE:     return "MG";
        case CLASS_ROGUE:    return "RG";
        case CLASS_SHAMAN:   return "SH";
        case CLASS_RANGER:   return "RA";
        default:             return "--";
    }
}

const char *theme_node_icon(NodeType type)
{
    switch (type)
    {
        case NODE_START:  return "S";
        case NODE_COMBAT: return "X";
        case NODE_ELITE:  return "!";
        case NODE_REST:   return "+";
        case NODE_SHOP:   return "$";
        case NODE_BOSS:   return "B";
    }
    return "?";
}

const char *theme_node_name(NodeType type)
{
    switch (type)
    {
        case NODE_START:  return "Start";
        case NODE_COMBAT: return "Combat";
        case NODE_ELITE:  return "Elite";
        case NODE_REST:   return "Rest";
        case NODE_SHOP:   return "Shop";
        case NODE_BOSS:   return "Boss";
    }
    return "Unknown";
}

const char *theme_card_type_label(CardType type)
{
    switch (type)
    {
        case CARD_ATTACK: return "ATK";
        case CARD_SKILL:  return "SKL";
        case CARD_POWER:  return "PWR";
    }
    return "---";
}

const char *theme_primary_effect_label(const CardDef *card)
{
    if (!card) return "---";
    if (card->interrupt) return "INT";
    if (card->taunt) return "TAUNT";
    if (card->burn_stacks > 0) return "BURN";
    if (card->channel) return "CHAN";
    if (card->damage > 0 && card->heal > 0) return "DRAIN";
    if (card->damage > 0) return "DMG";
    if (card->heal > 0) return "HEAL";
    if (card->shield > 0) return "SHLD";
    if (card->exhaust) return "EXH";
    return "UTIL";
}

void theme_draw_background(void)
{
    DrawRectangleGradientV(0, 0, VIRT_W, VIRT_H,
        (Color){ 18, 19, 31, 255 },
        (Color){ 10, 11, 18, 255 });

    if (g_assets.loaded)
    {
        DrawTexturePro(g_assets.paper_texture,
            (Rectangle){ 0, 0, VIRT_W, VIRT_H },
            (Rectangle){ 0, 0, VIRT_W, VIRT_H },
            (Vector2){ 0, 0 }, 0.0f,
            (Color){ 255, 255, 255, 20 });
    }

    DrawCircleGradient(60, 37, 86.0f, (Color){ 45, 75, 105, 55 }, (Color){ 45, 75, 105, 0 });
    DrawCircleGradient(347, 203, 100.0f, (Color){ 95, 58, 45, 45 }, (Color){ 95, 58, 45, 0 });
    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){ 0, 0, 0, 25 });
}

void theme_draw_cost_gem(int cx, int cy, int cost, bool enabled)
{
    Color fill = enabled ? (Color){ 235, 205, 65, 255 } : (Color){ 110, 95, 52, 255 };
    Color edge = enabled ? (Color){ 255, 235, 120, 255 } : (Color){ 80, 75, 68, 255 };

    DrawPoly((Vector2){ (float)cx, (float)cy }, 6, 15.0f, 30.0f, fill);
    DrawPolyLinesEx((Vector2){ (float)cx, (float)cy }, 6, 15.0f, 30.0f, 2.0f, edge);

    char text[4];
    snprintf(text, sizeof(text), "%d", cost);
    DrawText(text, cx - MeasureText(text, 14) / 2, cy - 7, 14, RAYWHITE);
}

void theme_draw_effect_badge(Rectangle bounds, const char *label, Color color)
{
    DrawRectangleRec(bounds, color_fade_alpha(color, 58));
    DrawRectangleLinesEx(bounds, 1.0f, color_fade_alpha(color, 185));
    DrawText(label, (int)(bounds.x + bounds.width / 2 - MeasureText(label, 9) / 2), (int)(bounds.y + 5), 9, RAYWHITE);
}

void theme_draw_class_portrait(ClassType ct, int cx, int cy, int radius, bool alive)
{
    Color c = theme_class_color(ct);
    Texture2D icon = class_icon_texture(ct);
    int size = CLASS_PORTRAIT_SIZE;
    if (size < radius * 2) size = radius * 2;

    if (!alive)
        c = (Color){ 95, 75, 78, 230 };

    Rectangle dest = {
        (float)(cx - size / 2),
        (float)(cy - size / 2),
        (float)size,
        (float)size
    };

    if (icon.id != 0)
    {
        Color tint = alive ? WHITE : (Color){ 135, 130, 140, 185 };
        DrawTexturePro(icon,
            (Rectangle){ 0.0f, 0.0f, (float)icon.width, (float)icon.height },
            dest,
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            tint);
    }
    else
    {
        const char *label = theme_class_abbrev(ct);
        int font_size = size / 3;
        if (font_size < 10) font_size = 10;
        DrawText(label,
            cx - MeasureText(label, font_size) / 2,
            cy - font_size / 2,
            font_size,
            color_fade_alpha(c, alive ? 245 : 155));
    }
}

void theme_draw_card_art(Rectangle bounds, const CardDef *card, bool upgraded)
{
    Color c = theme_class_color(card ? card->class : CLASS_NONE);
    Color type = card ? theme_card_type_color(card->type) : (Color){ 130, 135, 160, 255 };

    DrawRectangleRec(bounds, (Color){ 9, 10, 16, 245 });

    Rectangle dest = fit_card_art_rect(bounds);
    if (g_assets.loaded)
    {
        DrawTexturePro(g_assets.card_template,
            (Rectangle){ 0.0f, 0.0f, (float)CARD_ART_SOURCE_W, (float)CARD_ART_SOURCE_H },
            dest,
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE);
    }
    else
    {
        DrawRectangleRec(dest, (Color){ 8, 8, 10, 255 });
        DrawRectangleLinesEx(dest, 1.0f, c);
    }

    float s = dest.width / (float)CARD_ART_SOURCE_W;
    DrawRectangleLinesEx(dest, s >= 3.0f ? 3.0f : 1.0f, color_fade_alpha(c, upgraded ? 245 : 185));

    if (!card) return;

    int title_x = scaled_x(dest, 4);
    int title_y = scaled_y(dest, 3);
    int title_w = (int)(42 * s);
    draw_text_fit(card->name, title_x, title_y, title_w, scaled_font(dest, 5), RAYWHITE);

    char cost[4];
    snprintf(cost, sizeof(cost), "%d", card->cost);
    int cost_size = scaled_font(dest, 6);
    DrawText(cost,
        scaled_x(dest, 55) - MeasureText(cost, cost_size) / 2,
        scaled_y(dest, 2),
        cost_size,
        (Color){ 25, 25, 20, 255 });

    Rectangle art_box = {
        (float)scaled_x(dest, 5),
        (float)scaled_y(dest, 14),
        50.0f * s,
        22.0f * s
    };
    DrawRectangleRec(art_box, color_fade_alpha(theme_class_dark(card->class), 185));
    DrawCircleGradient((int)(art_box.x + art_box.width * 0.5f), (int)(art_box.y + art_box.height * 0.48f),
        art_box.height * 0.55f, color_fade_alpha(c, 170), color_fade_alpha(c, 0));
    Texture2D class_icon = class_icon_texture(card->class);
    if (class_icon.id != 0)
    {
        float icon_size = art_box.height < art_box.width ? art_box.height : art_box.width;
        icon_size *= 0.86f;
        Rectangle icon_dest = {
            art_box.x + (art_box.width - icon_size) * 0.5f,
            art_box.y + (art_box.height - icon_size) * 0.5f,
            icon_size,
            icon_size
        };
        DrawTexturePro(class_icon,
            (Rectangle){ 0.0f, 0.0f, (float)class_icon.width, (float)class_icon.height },
            icon_dest,
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE);
    }

    DrawText(theme_card_type_label(card->type), scaled_x(dest, 5), scaled_y(dest, 39), scaled_font(dest, 4), type);
    DrawText(theme_primary_effect_label(card), scaled_x(dest, 31), scaled_y(dest, 39), scaled_font(dest, 4), c);

    int stat_y = scaled_y(dest, 63);
    int stat_size = scaled_font(dest, 4);
    int stat_x = scaled_x(dest, 5);
    if (card->damage > 0)
    {
        char b[16]; snprintf(b, sizeof(b), "D%d", card->damage);
        DrawText(b, stat_x, stat_y, stat_size, (Color){ 230, 95, 85, 255 });
        stat_x += (int)(17 * s);
    }
    if (card->heal > 0)
    {
        char b[16]; snprintf(b, sizeof(b), "H%d", card->heal);
        DrawText(b, stat_x, stat_y, stat_size, (Color){ 105, 235, 130, 255 });
        stat_x += (int)(17 * s);
    }
    if (card->shield > 0)
    {
        char b[16]; snprintf(b, sizeof(b), "S%d", card->shield);
        DrawText(b, stat_x, stat_y, stat_size, (Color){ 125, 190, 255, 255 });
        stat_x += (int)(17 * s);
    }

    int desc_size = scaled_font(dest, 3);
    const char *desc = card->description ? card->description : "";
    char desc_buf[64];
    snprintf(desc_buf, sizeof(desc_buf), "%.24s", desc);
    DrawText(desc_buf, scaled_x(dest, 5), scaled_y(dest, 48), desc_size, (Color){ 205, 208, 226, 255 });

    if (upgraded)
        DrawText("*", scaled_x(dest, 45), scaled_y(dest, 2), scaled_font(dest, 5), (Color){ 255, 245, 120, 255 });
}



