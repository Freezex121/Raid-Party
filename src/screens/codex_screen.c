#include "screens.h"
#include "game.h"
#include "constants.h"
#include "raylib.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "util/text.h"
#include "util/math_utils.h"

static Button back_btn;
static bool initialized = false;
static int codex_tab = 0;

static const char *tab_names[5] = {
    "STATUSES",
    "SYNERGIES",
    "COMBOS",
    "CLASSES",
    "PASSIVES",
};

static Rectangle tab_rect(int index)
{
    return (Rectangle){ 54.0f + index * 106.0f, 66.0f, 96.0f, 20.0f };
}

static void init_if_needed(void)
{
    if (initialized) return;
    back_btn = button_create(
        (Rectangle){ (float)(VIRT_W / 2 - 64), 322.0f, 128.0f, (float)BTN_H },
        "BACK",
        (Color){ 42, 48, 70, 255 },
        (Color){ 70, 78, 110, 255 },
        RAYWHITE);
    initialized = true;
}

void codex_screen_update(void)
{
    init_if_needed();

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        for (int i = 0; i < 5; i++)
            if (CheckCollisionPointRec(mouse, tab_rect(i)))
                codex_tab = i;
    }

    button_update(&back_btn);
    if (back_btn.pressed_this_frame || IsKeyPressed(KEY_ESCAPE))
    {
        initialized = false;
        game_change_screen(SCREEN_TITLE);
    }
}

static void draw_line(int *y, const char *title, const char *body, Color accent)
{
    DrawText(title, 54, *y, 10, accent);
    draw_text_wrapped(body, 164, *y, 420, 10, 2, (Color){ 205, 210, 230, 230 });
    *y += 30;
}

static void draw_compact_line(int *y, const char *title, const char *body, Color accent)
{
    DrawText(title, 54, *y, 10, accent);
    draw_text_wrapped(body, 154, *y, 430, 10, 1, (Color){ 205, 210, 230, 230 });
    *y += 20;
}

static void draw_statuses(int *y)
{
    draw_line(y, "MARKED", "Ranger applies it. Rogue, Mage, and Cleric exploit it; some payoffs consume it.", (Color){ 245, 220, 75, 255 });
    draw_line(y, "CONDUCTIVE", "Shaman applies it. Mage and Rogue turn it into chain damage and big AoE turns.", (Color){ 95, 185, 255, 255 });
    draw_line(y, "BLIGHT", "Rogue and Warlock apply it. Guardian, Cleric, Paladin, and Warlock cash it out.", (Color){ 190, 95, 230, 255 });
    draw_line(y, "BURNING", "Damages enemies at the start of turns.", (Color){ 245, 120, 55, 255 });
    draw_line(y, "BLEED", "Damages allies at the start of turns.", (Color){ 210, 65, 80, 255 });
}

static void draw_synergies(int *y)
{
    draw_line(y, "Ranger", "Snare Mark and Pounce apply MARKED. Eviscerate consumes it; Backstab and Missiles keep it alive.", (Color){ 245, 220, 75, 255 });
    draw_line(y, "Shaman", "Lightning Bolt, Windfury, and Chain Lightning apply CONDUCTIVE for Mage and Rogue payoffs.", (Color){ 95, 185, 255, 255 });
    draw_line(y, "Warlock", "Corruption and Poison Blade apply BLIGHT. Dark Harvest rewards wide BLIGHT setup.", (Color){ 190, 95, 230, 255 });
    draw_line(y, "Paladin", "Holy Strike consumes MARKED; Aegis Aura and Judgment turn BLIGHT into defense/control.", (Color){ 230, 205, 95, 255 });
    draw_line(y, "Bard", "Power Chord marks, Dissonance conducts, Battle Hymn extends, Finale scales with setup.", (Color){ 235, 95, 155, 255 });
    draw_line(y, "Support", "Shield Slam, Holy Fire, Judgment, and Smite become active payoff cards when setup exists.", (Color){ 235, 205, 95, 255 });
}

static void draw_combos(int *y)
{
    draw_compact_line(y, "Guardian > Cleric", "Shield of Faith: next heal this turn is 50% stronger.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Mage > Rogue", "Arcane Assault: next attack applies 2 Burning.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Shaman > Ranger", "Storm Volley: next AoE this turn deals 50% more damage.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Rogue > Cleric", "Shadow Dance: next heal costs 0.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Shaman > Mage", "Elemental Fury: next Mage damage card costs 0.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Rogue > Mage", "Backdraft: next damage card this turn deals 25% more damage.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Paladin > Bard", "Sacred Chorus: next group heal/shield is 50% stronger.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Bard > Warlock", "Dark Refrain: next Warlock damage applies BLIGHT.", (Color){ 235, 205, 95, 255 });
    draw_compact_line(y, "Warlock > Paladin", "Absolution: next Paladin card consumes BLIGHT to heal.", (Color){ 235, 205, 95, 255 });
}

static void draw_classes(int *y)
{
    draw_compact_line(y, "Guardian", "Shield pressure; consumes BLIGHT with Shield Slam.", theme_class_color(CLASS_GUARDIAN));
    draw_compact_line(y, "Cleric", "Consumes MARKED/BLIGHT for recovery.", theme_class_color(CLASS_CLERIC));
    draw_compact_line(y, "Mage", "Detonates CONDUCTIVE with arcs and Meteor.", theme_class_color(CLASS_MAGE));
    draw_compact_line(y, "Rogue", "Consumes MARKED and applies BLIGHT.", theme_class_color(CLASS_ROGUE));
    draw_compact_line(y, "Shaman", "Applies CONDUCTIVE and extends totems.", theme_class_color(CLASS_SHAMAN));
    draw_compact_line(y, "Ranger", "Applies MARKED and enables Ambush.", theme_class_color(CLASS_RANGER));
    draw_compact_line(y, "Paladin", "Consumes MARKED/BLIGHT; chains with Bard/Warlock.", theme_class_color(CLASS_PALADIN));
    draw_compact_line(y, "Warlock", "Turns CONDUCTIVE into BLIGHT; cashes out BLIGHT.", theme_class_color(CLASS_WARLOCK));
    draw_compact_line(y, "Bard", "Marks, conducts, extends, and amplifies setup.", theme_class_color(CLASS_BARD));
}

static void draw_passives(int *y)
{
    draw_line(y, "Molten Armor", "Guardian + Mage: when Guardian gains Shield, a random enemy takes 2 damage.", (Color){ 245, 105, 70, 255 });
    draw_line(y, "Ambush", "Rogue + Ranger: first damaging card each combat deals double damage.", (Color){ 245, 220, 75, 255 });
    draw_line(y, "Shadow Mend", "Cleric + Rogue: Rogue aggro drops heal the Rogue for 8.", (Color){ 120, 245, 170, 255 });
    draw_line(y, "Earthen Bulwark", "Guardian + Shaman: Healing Totem lasts 2 extra turns.", (Color){ 95, 185, 255, 255 });
    draw_line(y, "Absolution", "Paladin + Warlock: consuming BLIGHT heals the lowest HP ally.", (Color){ 230, 205, 95, 255 });
}

void codex_screen_draw(void)
{
    theme_draw_background();
    DrawText("COLLECTIVE", VIRT_W / 2 - MeasureText("COLLECTIVE", 18) / 2, 30, 18, RAYWHITE);

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 5; i++)
    {
        Rectangle r = tab_rect(i);
        bool active = i == codex_tab;
        bool hover = CheckCollisionPointRec(mouse, r);
        Color bg = active ? (Color){ 56, 48, 82, 245 } : hover ? (Color){ 38, 44, 66, 245 } : (Color){ 22, 25, 38, 230 };
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1.0f, active ? (Color){ 230, 205, 95, 240 } : (Color){ 95, 105, 135, 190 });
        DrawText(tab_names[i], snap_i(r.x + r.width / 2 - MeasureText(tab_names[i], 10) / 2), snap_i(r.y + 6), 10, RAYWHITE);
    }

    Rectangle panel = { 42.0f, 98.0f, 556.0f, 206.0f };
    DrawRectangleRec(panel, (Color){ 9, 10, 17, 225 });
    DrawRectangleLinesEx(panel, 1.0f, (Color){ 105, 115, 150, 180 });

    int y = 112;
    if (codex_tab == 0) draw_statuses(&y);
    else if (codex_tab == 1) draw_synergies(&y);
    else if (codex_tab == 2) draw_combos(&y);
    else if (codex_tab == 3) draw_classes(&y);
    else draw_passives(&y);

    button_draw(&back_btn);
}
