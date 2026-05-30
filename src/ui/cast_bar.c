#include "cast_bar.h"
#include "constants.h"
#include "util/text.h"
#include "raylib.h"
#include <stdio.h>

static Color intent_color(IntentType intent, bool is_wipe, int remaining_turns, int total_turns)
{
    if (is_wipe || intent == INTENT_WIPE)
        return (Color){ 210, 45, 65, 255 };
    if (remaining_turns <= 1 && total_turns > 1)
        return (Color){ 230, 165, 45, 255 };

    switch (intent)
    {
        case INTENT_AOE:         return (Color){ 210, 90, 70, 255 };
        case INTENT_TANK_BUSTER: return (Color){ 230, 115, 45, 255 };
        case INTENT_HEAL:        return (Color){ 75, 205, 115, 255 };
        case INTENT_SHIELD:      return (Color){ 90, 160, 235, 255 };
        case INTENT_BUFF:        return (Color){ 170, 110, 230, 255 };
        default:                 return (Color){ 70, 145, 215, 255 };
    }
}

static const char *intent_icon(IntentType intent, bool is_wipe)
{
    if (is_wipe || intent == INTENT_WIPE) return "!!";
    switch (intent)
    {
        case INTENT_AOE:         return "AOE";
        case INTENT_TANK_BUSTER: return "TB";
        case INTENT_HEAL:        return "HEAL";
        case INTENT_SHIELD:      return "SHLD";
        case INTENT_BUFF:        return "BUFF";
        default:                 return "ATK";
    }
}

void cast_bar_draw_ability_tooltip(const EnemyCardDef *ability, Rectangle bounds)
{
    if (!ability) return;

    Vector2 mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, bounds)) return;

    static TextScroll tooltip_scroll = { 0 };
    static const EnemyCardDef *last_ability = NULL;
    if (last_ability != ability)
    {
        tooltip_scroll.offset_y = 0;
        last_ability = ability;
    }

    int w = 196;
    int body_w = w - 10;
    char details[512];
    const char *intent_name = "ATTACK";
    switch (ability->intent)
    {
        case INTENT_ATTACK:    intent_name = "ATTACK — standard damage"; break;
        case INTENT_TANK_BUSTER: intent_name = "TANK BUSTER — high damage to the current tank"; break;
        case INTENT_AOE:       intent_name = "AOE — damages all party members"; break;
        case INTENT_WIPE:      intent_name = "WIPE — extremely high damage to all"; break;
        case INTENT_HEAL:      intent_name = "HEAL — restores HP to enemies"; break;
        case INTENT_SHIELD:    intent_name = "SHIELD — grants protection to enemies"; break;
        case INTENT_BUFF:      intent_name = "BUFF — strengthens enemies"; break;
        default:               intent_name = "UNKNOWN"; break;
    }
    snprintf(details, sizeof(details), "%s\n\n%s\n\nDmg %d   Heal %d   Shield %d\n%s",
        ability->description,
        intent_name,
        ability->base_damage,
        ability->heal_amount,
        ability->shield_amount,
        (ability->is_wipe || ability->intent == INTENT_WIPE) ? "Uninterruptible" : "Interruptible");

    int title_h = measure_text_box(ability->name, body_w - 4, 10, 0);
    if (title_h < ui_line_height(10)) title_h = ui_line_height(10);
    int detail_h = measure_text_box(details, body_w - 4, 10, 0);
    int needed_h = 10 + title_h + detail_h + 10;
    int max_h = (int)(HAND_Y - (FRAME_Y + FRAME_H + 12));
    if (max_h > 128) max_h = 128;
    if (max_h < 68) max_h = 68;
    int h = needed_h < max_h ? needed_h : max_h;

    int x = (int)(bounds.x + bounds.width / 2.0f - w / 2.0f);
    int y = (int)(bounds.y - h - 5);
    if (x + w > VIRT_W - 2) x = VIRT_W - w - 2;
    if (x < 2) x = 2;
    if (y < FRAME_Y + FRAME_H + 8) y = (int)(bounds.y + bounds.height + 4);
    if (y + h > HAND_Y - 4) y = HAND_Y - h - 4;
    DrawRectangleRec((Rectangle){ (float)x, (float)y, (float)w, (float)h }, (Color){ 18, 18, 28, 245 });
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 1.0f, (Color){ 100, 100, 130, 220 });

    draw_text_box((Rectangle){ (float)(x + 5), (float)(y + 5), (float)(body_w - 4), (float)title_h },
        ability->name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);

    Rectangle detail_box = {
        (float)(x + 5),
        (float)(y + 7 + title_h),
        (float)(body_w - 7),
        (float)(h - title_h - 12)
    };
    draw_text_box_scrolled(detail_box, details, 10, 0, (Color){ 220, 220, 235, 240 },
        TEXT_ALIGN_LEFT, &tooltip_scroll, true);
}

void cast_bar_draw_ex(const char *ability_name, int remaining_turns, int total_turns, bool is_wipe, int bar_x, int bar_y)
{
    int bar_w = CAST_BAR_W, bar_h = CAST_BAR_HEIGHT;

    Color bar_color = intent_color(INTENT_ATTACK, is_wipe, remaining_turns, total_turns);

    Color bg = (Color){ 30, 30, 50, 200 };
    DrawRectangleRec((Rectangle){ (float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h }, bg);

    float fill = total_turns > 0 ? 1.0f - ((float)remaining_turns / (float)total_turns) : 0.0f;
    int fill_w = (int)((bar_w - 4) * fill);
    if (fill_w > 0)
        DrawRectangleRec((Rectangle){ (float)(bar_x + 1), (float)(bar_y + 1), (float)fill_w, (float)(bar_h - 2) }, bar_color);

    DrawRectangleLinesEx((Rectangle){ (float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h }, 1.0f, (Color){ 60, 60, 80, 200 });

    char turns_text[16];
    snprintf(turns_text, sizeof(turns_text), "%dT", remaining_turns);
    int tw = MeasureText(turns_text, 10);
    draw_text_box((Rectangle){ (float)(bar_x + 4), (float)(bar_y + 3), (float)(bar_w - tw - 12), (float)(bar_h - 4) },
        ability_name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
    DrawText(turns_text, bar_x + bar_w - tw - 4, bar_y + 3, 10, (Color){ 200, 200, 220, 200 });

    if (is_wipe)
    {
        DrawText("WIPE!", bar_x + bar_w / 2 - MeasureText("WIPE!", 10) / 2, bar_y - 11, 10, (Color){ 255, 60, 60, 255 });
    }
}

void cast_bar_draw_ability(const EnemyCardDef *ability, int remaining_turns, int total_turns, int bar_x, int bar_y)
{
    if (!ability) return;

    int bar_w = CAST_BAR_W, bar_h = CAST_BAR_HEIGHT;
    bool locked = ability->is_wipe || ability->intent == INTENT_WIPE;
    Color bar_color = intent_color(ability->intent, ability->is_wipe, remaining_turns, total_turns);
    Rectangle bounds = { (float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h };

    DrawRectangleRec(bounds, (Color){ 24, 24, 40, 230 });

    float fill = total_turns > 0 ? 1.0f - ((float)remaining_turns / (float)total_turns) : 0.0f;
    int fill_w = (int)((bar_w - 4) * fill);
    if (fill_w > 0)
        DrawRectangleRec((Rectangle){ (float)(bar_x + 1), (float)(bar_y + 1), (float)fill_w, (float)(bar_h - 2) }, bar_color);

    DrawRectangleLinesEx(bounds, 1.0f, locked ? (Color){ 230, 90, 80, 230 } : (Color){ 80, 90, 125, 230 });

    const char *icon = intent_icon(ability->intent, ability->is_wipe);
    int icon_w = 36;
    DrawRectangleRec((Rectangle){ (float)(bar_x + 1), (float)(bar_y + 1), (float)icon_w, (float)(bar_h - 2) }, (Color){ bar_color.r, bar_color.g, bar_color.b, 120 });
    draw_text_box((Rectangle){ (float)(bar_x + 2), (float)(bar_y + 4), (float)(icon_w - 2), (float)(bar_h - 6) },
        icon, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    char amount[48];
    if (ability->heal_amount > 0)
        snprintf(amount, sizeof(amount), "+%d", ability->heal_amount);
    else if (ability->shield_amount > 0 && ability->base_damage <= 0)
        snprintf(amount, sizeof(amount), "+%dS", ability->shield_amount);
    else
        snprintf(amount, sizeof(amount), "%dD", ability->base_damage);

    char turns_text[20];
    snprintf(turns_text, sizeof(turns_text), "%dT", remaining_turns);
    int turns_w = MeasureText(turns_text, 10);
    DrawText(turns_text, bar_x + bar_w - turns_w - 4, bar_y + 3, 10, (Color){ 230, 230, 245, 235 });

    int amount_w = MeasureText(amount, 10);
    int amount_x = bar_x + bar_w - amount_w - 4;
    draw_text_box((Rectangle){ (float)(bar_x + icon_w + 5), (float)(bar_y + bar_h - 12), (float)(bar_w - icon_w - 9), 12.0f },
        amount, 10, 0, (Color){ 220, 220, 235, 230 }, TEXT_ALIGN_RIGHT);

    int name_x = bar_x + icon_w + 5;
    int name_w = amount_x - name_x - 3;
    if (name_w < 0) name_w = bar_w - icon_w - 10;
    draw_text_box((Rectangle){ (float)name_x, (float)(bar_y + 4), (float)name_w, (float)(bar_h - 6) },
        ability->name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);

    if (locked)
        DrawText("!", bar_x + bar_w - turns_w - 38, bar_y + 14, 10, (Color){ 250, 105, 80, 240 });
}



