#include "cast_bar.h"
#include "constants.h"
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
        case INTENT_TANK_BUSTER: return "TNK";
        case INTENT_HEAL:        return "HEAL";
        case INTENT_SHIELD:      return "SHLD";
        case INTENT_BUFF:        return "BUFF";
        default:                 return "ATK";
    }
}

static void draw_tooltip(Rectangle bounds, const EnemyAbility *ability)
{
    if (!ability) return;

    Vector2 mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, bounds)) return;

    int x = (int)bounds.x;
    int y = (int)(bounds.y + bounds.height + 2);
    int w = 160;
    int h = 46;
    if (x + w > VIRT_W - 2) x = VIRT_W - w - 2;
    if (y + h > VIRT_H - 2) y = (int)bounds.y - h - 2;
    DrawRectangleRec((Rectangle){ (float)x, (float)y, (float)w, (float)h }, (Color){ 18, 18, 28, 245 });
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 1.0f, (Color){ 100, 100, 130, 220 });

    DrawText(ability->description, x + 5, y + 5, 6, (Color){ 220, 220, 235, 240 });

    char details[96];
    snprintf(details, sizeof(details), "Damage %d   Heal %d   Shield %d", ability->base_damage, ability->heal_amount, ability->shield_amount);
    DrawText(details, x + 5, y + 20, 6, (Color){ 170, 170, 200, 230 });
    DrawText((ability->is_wipe || ability->intent == INTENT_WIPE) ? "Uninterruptible" : "Interruptible", x + 5, y + 33, 6,
        (ability->is_wipe || ability->intent == INTENT_WIPE) ? (Color){ 230, 110, 80, 240 } : (Color){ 140, 220, 160, 240 });
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

    DrawText(ability_name, bar_x + 4, bar_y + 3, 6, RAYWHITE);

    char turns_text[16];
    snprintf(turns_text, sizeof(turns_text), "%dT", remaining_turns);
    int tw = MeasureText(turns_text, 6);
    DrawText(turns_text, bar_x + bar_w - tw - 4, bar_y + 3, 6, (Color){ 200, 200, 220, 200 });

    if (is_wipe)
    {
        DrawText("WIPE!", bar_x + bar_w / 2 - MeasureText("WIPE!", 7) / 2, bar_y - 9, 7, (Color){ 255, 60, 60, 255 });
    }
}

void cast_bar_draw_ability(const EnemyAbility *ability, int remaining_turns, int total_turns, int bar_x, int bar_y)
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
    DrawText(icon, bar_x + 4, bar_y + 4, 5, RAYWHITE);
    int name_size = 6;
    while (name_size > 5 && MeasureText(ability->name, name_size) > 54)
        name_size--;
    DrawText(ability->name, bar_x + 24, bar_y + 4, name_size, RAYWHITE);

    char amount[48];
    if (ability->heal_amount > 0)
        snprintf(amount, sizeof(amount), "+%d", ability->heal_amount);
    else if (ability->shield_amount > 0 && ability->base_damage <= 0)
        snprintf(amount, sizeof(amount), "+%dS", ability->shield_amount);
    else
        snprintf(amount, sizeof(amount), "%dD", ability->base_damage);
    DrawText(amount, bar_x + 84, bar_y + 4, 5, (Color){ 220, 220, 235, 230 });

    char turns_text[20];
    snprintf(turns_text, sizeof(turns_text), "%dT", remaining_turns);
    DrawText(turns_text, bar_x + bar_w - 17, bar_y + 3, 6, (Color){ 230, 230, 245, 235 });

    DrawText(locked ? "LOCK" : "INT", bar_x + bar_w - 40, bar_y + 5, 4,
        locked ? (Color){ 240, 120, 90, 230 } : (Color){ 130, 220, 160, 230 });

    draw_tooltip(bounds, ability);
}



