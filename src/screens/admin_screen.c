#include "screens.h"
#include "game.h"
#include "data/area_defs.h"
#include "data/card_defs.h"
#include "data/enemy_defs.h"
#include "systems/deck.h"
#include "systems/party.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define ADMIN_ROW_H 14

static bool initialized = false;
static ClassType admin_team[MAX_PARTY_SIZE];
static int admin_levels[MAX_PARTY_SIZE];
static Deck admin_deck;
static EncounterDef admin_encounter;
static int card_scroll = 0;
static int deck_scroll = 0;
static int enemy_scroll = 0;
static int add_upgrade_level = 0;
static int encounter_mode = 0;
static int admin_area = 0;
static char admin_message[96];

static Rectangle team_rect(void) { return (Rectangle){ 8.0f, 34.0f, 624.0f, 48.0f }; }
static Rectangle deck_rect(void) { return (Rectangle){ 8.0f, 96.0f, 190.0f, 172.0f }; }
static Rectangle cards_rect(void) { return (Rectangle){ 206.0f, 96.0f, 218.0f, 172.0f }; }
static Rectangle enemies_rect(void) { return (Rectangle){ 432.0f, 96.0f, 200.0f, 172.0f }; }
static Rectangle controls_rect(void) { return (Rectangle){ 8.0f, 276.0f, 624.0f, 76.0f }; }

static bool clicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r);
}

static bool right_clicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(GetMousePosition(), r);
}

static int row_capacity(Rectangle panel, int top_pad)
{
    int rows = ((int)panel.height - top_pad - 6) / ADMIN_ROW_H;
    return rows < 1 ? 1 : rows;
}

static void clamp_scroll(int *scroll, int count, int capacity)
{
    if (!scroll) return;
    int max_scroll = count - capacity;
    if (max_scroll < 0) max_scroll = 0;
    if (*scroll < 0) *scroll = 0;
    if (*scroll > max_scroll) *scroll = max_scroll;
}

static ClassType next_loaded_class(ClassType current)
{
    int count = party_class_count();
    if (count <= 0) return CLASS_GUARDIAN;
    if (current == CLASS_NONE) return party_class_at(0);
    for (int i = 0; i < count; i++)
        if (party_class_at(i) == current)
            return party_class_at((i + 1) % count);
    return party_class_at(0);
}

static int admin_team_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_PARTY_SIZE; i++)
        if (admin_team[i] != CLASS_NONE)
            count++;
    return count;
}

static void admin_rebuild_draw_pile(void)
{
    admin_deck.draw_count = 0;
    admin_deck.hand_count = 0;
    admin_deck.discard_count = 0;
    admin_deck.exhaust_count = 0;
    for (int i = 0; i < admin_deck.card_count && i < MAX_DECK_SIZE; i++)
        if (admin_deck.cards[i].def)
            admin_deck.draw[admin_deck.draw_count++] = i;
}

static void admin_remove_deck_card(int index)
{
    if (index < 0 || index >= admin_deck.card_count) return;
    for (int i = index; i < admin_deck.card_count - 1; i++)
        admin_deck.cards[i] = admin_deck.cards[i + 1];
    admin_deck.card_count--;
    admin_rebuild_draw_pile();
}

static void admin_build_starter_deck(void)
{
    int classes[MAX_PARTY_SIZE];
    int count = 0;
    for (int i = 0; i < MAX_PARTY_SIZE; i++)
        if (admin_team[i] != CLASS_NONE && count < MAX_PARTY_SIZE)
            classes[count++] = (int)admin_team[i];
    if (count <= 0)
    {
        deck_init(&admin_deck);
        return;
    }
    deck_init_from_classes(&admin_deck, classes, count);
    snprintf(admin_message, sizeof(admin_message), "Starter deck rebuilt");
}

static void admin_init(void)
{
    for (int i = 0; i < MAX_PARTY_SIZE; i++)
    {
        admin_team[i] = CLASS_NONE;
        admin_levels[i] = 1;
    }

    int class_count = party_class_count();
    int starter_count = class_count < 3 ? class_count : 3;
    for (int i = 0; i < starter_count && i < MAX_PARTY_SIZE; i++)
        admin_team[i] = party_class_at(i);

    admin_area = g_state.selected_area;
    encounter_mode = 0;
    add_upgrade_level = 0;
    admin_encounter.count = 0;
    const EnemyDef *fallback = enemy_def_by_id("dummy");
    if (!fallback) fallback = enemy_def_by_index(0);
    if (fallback)
        admin_encounter.enemies[admin_encounter.count++] = fallback;

    deck_init(&admin_deck);
    admin_build_starter_deck();
    snprintf(admin_message, sizeof(admin_message), "Admin lab ready");
    initialized = true;
}

static void admin_apply_member_perks(PartyMember *pm)
{
    if (!pm) return;
    int choices = pm->level - 1;
    int class_perks = perk_class_count(pm->class);
    for (int i = 0; i < choices && pm->perk_count < MAX_MEMBER_PERKS; i++)
    {
        PerkId perk = class_perks > 0 ? perk_class_at(pm->class, i % class_perks) : perk_for_class(pm->class);
        if (perk_is_loaded(perk))
            party_member_add_perk(pm, perk);
    }
}

static bool admin_launch(void)
{
    int classes[MAX_PARTY_SIZE];
    int levels[MAX_PARTY_SIZE];
    int count = 0;
    for (int i = 0; i < MAX_PARTY_SIZE; i++)
    {
        if (admin_team[i] == CLASS_NONE) continue;
        classes[count] = (int)admin_team[i];
        levels[count] = admin_levels[i];
        count++;
    }
    if (count <= 0)
    {
        snprintf(admin_message, sizeof(admin_message), "Add at least one party member");
        return false;
    }
    if (admin_deck.card_count <= 0)
    {
        snprintf(admin_message, sizeof(admin_message), "Add at least one card");
        return false;
    }
    if (admin_encounter.count <= 0)
    {
        snprintf(admin_message, sizeof(admin_message), "Add at least one enemy");
        return false;
    }

    party_create(&g_state.run_party, classes, count);
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        PartyMember *pm = &g_state.run_party.members[i];
        int level = levels[i];
        if (level < 1) level = 1;
        if (level > MAX_LEVEL) level = MAX_LEVEL;
        pm->level = level;
        pm->xp = xp_total_for_level(level);
        pm->pending_levels = 0;
        pm->combat_xp = 0;
        pm->perk_count = 0;
        for (int p = 0; p < MAX_MEMBER_PERKS; p++)
            pm->perks[p] = -1;
        admin_apply_member_perks(pm);
    }

    memcpy(&g_state.run_deck, &admin_deck, sizeof(Deck));
    g_state.encounter = &admin_encounter;
    g_state.run_party_active = true;
    g_state.encounter_is_elite = encounter_mode == 1;
    g_state.encounter_is_boss = encounter_mode == 2;
    g_state.debug_admin_mode = true;
    g_state.current_area = admin_area;
    g_state.map.floor = 0;
    g_state.map.current_index = -1;
    g_state.relic_reward_pending = false;
    g_state.post_combat_destination = SCREEN_ADMIN;
    g_state.next_combat_energy_bonus = 0;
    g_state.next_combat_draw_bonus = 0;
    g_state.next_combat_boon_turns = 0;
    game_change_screen(SCREEN_RUN);
    return true;
}

static void update_team(void)
{
    Rectangle panel = team_rect();
    float slot_w = (panel.width - 16.0f) / (float)MAX_PARTY_SIZE;
    for (int i = 0; i < MAX_PARTY_SIZE; i++)
    {
        Rectangle slot = { panel.x + 8.0f + i * slot_w, panel.y + 18.0f, slot_w - 5.0f, 22.0f };
        Rectangle dec = { slot.x + slot.width - 45.0f, slot.y + 3.0f, 14.0f, 14.0f };
        Rectangle inc = { slot.x + slot.width - 28.0f, slot.y + 3.0f, 14.0f, 14.0f };
        if (admin_team[i] != CLASS_NONE && clicked(dec) && admin_levels[i] > 1)
            admin_levels[i]--;
        else if (admin_team[i] != CLASS_NONE && clicked(inc) && admin_levels[i] < MAX_LEVEL)
            admin_levels[i]++;
        else if (right_clicked(slot))
            admin_team[i] = CLASS_NONE;
        else if (clicked(slot))
            admin_team[i] = next_loaded_class(admin_team[i]);
    }
}

static void update_deck_list(Rectangle panel, float wheel)
{
    int capacity = row_capacity(panel, 20);
    if (CheckCollisionPointRec(GetMousePosition(), panel) && wheel != 0.0f)
        deck_scroll -= (int)wheel;
    clamp_scroll(&deck_scroll, admin_deck.card_count, capacity);

    int first = deck_scroll;
    int last = first + capacity;
    if (last > admin_deck.card_count) last = admin_deck.card_count;
    for (int i = first; i < last; i++)
    {
        Rectangle row = { panel.x + 6.0f, panel.y + 20.0f + (float)(i - first) * ADMIN_ROW_H, panel.width - 12.0f, 12.0f };
        if (clicked(row))
        {
            Rectangle remove = { row.x + row.width - 13.0f, row.y, 13.0f, row.height };
            if (CheckCollisionPointRec(GetMousePosition(), remove))
            {
                admin_remove_deck_card(i);
                snprintf(admin_message, sizeof(admin_message), "Removed deck card");
                break;
            }
            CardInstance *inst = &admin_deck.cards[i];
            int next = inst->upgrade_level + 1;
            if (next > 2) next = 0;
            inst->upgrade_level = card_clamp_upgrade_level(inst->def, next);
        }
        else if (right_clicked(row))
        {
            admin_remove_deck_card(i);
            snprintf(admin_message, sizeof(admin_message), "Removed deck card");
            break;
        }
    }
}

static void update_card_library(Rectangle panel, float wheel)
{
    int total = card_defs_loaded_count();
    int capacity = row_capacity(panel, 20);
    if (CheckCollisionPointRec(GetMousePosition(), panel) && wheel != 0.0f)
        card_scroll -= (int)wheel;
    clamp_scroll(&card_scroll, total, capacity);

    int first = card_scroll;
    int last = first + capacity;
    if (last > total) last = total;
    for (int i = first; i < last; i++)
    {
        Rectangle row = { panel.x + 6.0f, panel.y + 20.0f + (float)(i - first) * ADMIN_ROW_H, panel.width - 12.0f, 12.0f };
        if (!clicked(row)) continue;
        const CardDef *def = card_def_by_index(i);
        if (def && admin_deck.card_count < MAX_DECK_SIZE)
        {
            deck_add_card_with_level(&admin_deck, def, add_upgrade_level);
            snprintf(admin_message, sizeof(admin_message), "Added %s", def->name);
        }
    }
}

static void update_enemies(Rectangle panel, float wheel)
{
    Rectangle selected = { panel.x + 6.0f, panel.y + 20.0f, panel.width - 12.0f, 42.0f };
    for (int i = 0; i < admin_encounter.count; i++)
    {
        Rectangle slot = { selected.x, selected.y + i * 8.0f, selected.width, 8.0f };
        if (right_clicked(slot) || clicked(slot))
        {
            for (int j = i; j < admin_encounter.count - 1; j++)
                admin_encounter.enemies[j] = admin_encounter.enemies[j + 1];
            admin_encounter.count--;
            snprintf(admin_message, sizeof(admin_message), "Removed enemy");
            return;
        }
    }

    Rectangle library = { panel.x, panel.y + 68.0f, panel.width, panel.height - 68.0f };
    int total = enemy_defs_loaded_count();
    int capacity = row_capacity(library, 14);
    if (CheckCollisionPointRec(GetMousePosition(), library) && wheel != 0.0f)
        enemy_scroll -= (int)wheel;
    clamp_scroll(&enemy_scroll, total, capacity);

    int first = enemy_scroll;
    int last = first + capacity;
    if (last > total) last = total;
    for (int i = first; i < last; i++)
    {
        Rectangle row = { library.x + 6.0f, library.y + 14.0f + (float)(i - first) * ADMIN_ROW_H, library.width - 12.0f, 12.0f };
        if (!clicked(row)) continue;
        const EnemyDef *def = enemy_def_by_index(i);
        if (def && admin_encounter.count < MAX_ENEMIES)
        {
            admin_encounter.enemies[admin_encounter.count++] = def;
            snprintf(admin_message, sizeof(admin_message), "Added %s", def->name);
        }
    }
}

static void update_controls(void)
{
    Rectangle controls = controls_rect();
    Rectangle base = { controls.x + 56.0f, controls.y + 10.0f, 44.0f, 18.0f };
    Rectangle upg = { controls.x + 104.0f, controls.y + 10.0f, 44.0f, 18.0f };
    Rectangle max = { controls.x + 152.0f, controls.y + 10.0f, 44.0f, 18.0f };
    if (clicked(base)) add_upgrade_level = 0;
    if (clicked(upg)) add_upgrade_level = 1;
    if (clicked(max)) add_upgrade_level = 2;

    Rectangle starter = { controls.x + 210.0f, controls.y + 10.0f, 86.0f, 18.0f };
    Rectangle clear = { controls.x + 300.0f, controls.y + 10.0f, 74.0f, 18.0f };
    if (clicked(starter)) admin_build_starter_deck();
    if (clicked(clear))
    {
        deck_init(&admin_deck);
        snprintf(admin_message, sizeof(admin_message), "Deck cleared");
    }

    Rectangle normal = { controls.x + 56.0f, controls.y + 36.0f, 58.0f, 18.0f };
    Rectangle elite = { controls.x + 118.0f, controls.y + 36.0f, 54.0f, 18.0f };
    Rectangle boss = { controls.x + 176.0f, controls.y + 36.0f, 54.0f, 18.0f };
    if (clicked(normal)) encounter_mode = 0;
    if (clicked(elite)) encounter_mode = 1;
    if (clicked(boss)) encounter_mode = 2;

    Rectangle area_minus = { controls.x + 284.0f, controls.y + 36.0f, 22.0f, 18.0f };
    Rectangle area_plus = { controls.x + 392.0f, controls.y + 36.0f, 22.0f, 18.0f };
    int area_count = area_defs_count();
    if (area_count < 1) area_count = 1;
    if (clicked(area_minus) && admin_area > 0) admin_area--;
    if (clicked(area_plus) && admin_area < area_count - 1) admin_area++;

    Rectangle launch = { controls.x + controls.width - 108.0f, controls.y + 10.0f, 96.0f, 44.0f };
    if (clicked(launch))
        admin_launch();
}

void admin_screen_update(void)
{
    if (!initialized)
        admin_init();

    if (IsKeyPressed(KEY_ESCAPE))
    {
        g_state.debug_admin_mode = false;
        game_change_screen(SCREEN_TITLE);
        return;
    }

    float wheel = GetMouseWheelMove();
    update_team();
    update_deck_list(deck_rect(), wheel);
    update_card_library(cards_rect(), wheel);
    update_enemies(enemies_rect(), wheel);
    update_controls();
}

static void draw_panel_header(Rectangle panel, const char *title, const char *detail)
{
    DrawRectangleRec(panel, (Color){ 10, 11, 18, 210 });
    DrawRectangleLinesEx(panel, 1.0f, (Color){ 64, 70, 96, 210 });
    draw_text_box((Rectangle){ panel.x + 6.0f, panel.y + 5.0f, panel.width - 12.0f, 12.0f },
        title, 10, 0, (Color){ 220, 224, 245, 240 }, TEXT_ALIGN_LEFT);
    if (detail && detail[0])
        draw_text_box((Rectangle){ panel.x + 72.0f, panel.y + 5.0f, panel.width - 78.0f, 12.0f },
            detail, 10, 0, (Color){ 145, 153, 190, 220 }, TEXT_ALIGN_RIGHT);
}

static void draw_row(Rectangle row, bool active, Color accent)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, row);
    Color fill = hover ? (Color){ 38, 42, 62, 235 } : (Color){ 20, 22, 34, 210 };
    if (active)
        fill = (Color){ accent.r / 3, accent.g / 3, accent.b / 3, 235 };
    DrawRectangleRec(row, fill);
    DrawRectangleLinesEx(row, 1.0f, hover ? accent : (Color){ 48, 52, 72, 170 });
}

static const char *card_type_short(CardType type)
{
    switch (type)
    {
        case CARD_ATTACK: return "A";
        case CARD_POWER: return "P";
        default: return "S";
    }
}

static void draw_team(void)
{
    Rectangle panel = team_rect();
    draw_panel_header(panel, "TEAM", TextFormat("%d / %d", admin_team_count(), MAX_PARTY_SIZE));
    float slot_w = (panel.width - 16.0f) / (float)MAX_PARTY_SIZE;
    for (int i = 0; i < MAX_PARTY_SIZE; i++)
    {
        Rectangle slot = { panel.x + 8.0f + i * slot_w, panel.y + 18.0f, slot_w - 5.0f, 22.0f };
        ClassType ct = admin_team[i];
        Color c = ct == CLASS_NONE ? (Color){ 82, 86, 106, 220 } : theme_class_color(ct);
        draw_row(slot, ct != CLASS_NONE, c);
        if (ct == CLASS_NONE)
        {
            draw_text_box((Rectangle){ slot.x + 4.0f, slot.y + 5.0f, slot.width - 8.0f, 10.0f },
                "+", 10, 0, (Color){ 150, 158, 190, 220 }, TEXT_ALIGN_CENTER);
            continue;
        }
        draw_text_box((Rectangle){ slot.x + 4.0f, slot.y + 3.0f, slot.width - 52.0f, 10.0f },
            class_abbrev(ct), 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ slot.x + 4.0f, slot.y + 12.0f, slot.width - 52.0f, 9.0f },
            TextFormat("Lv %d", admin_levels[i]), 10, 0, (Color){ 220, 224, 245, 230 }, TEXT_ALIGN_LEFT);
        Rectangle dec = { slot.x + slot.width - 45.0f, slot.y + 3.0f, 14.0f, 14.0f };
        Rectangle inc = { slot.x + slot.width - 28.0f, slot.y + 3.0f, 14.0f, 14.0f };
        draw_btn_standard(dec, (Color){ 34, 38, 56, 255 }, (Color){ 68, 74, 108, 255 }, "-", -1);
        draw_btn_standard(inc, (Color){ 34, 38, 56, 255 }, (Color){ 68, 74, 108, 255 }, "+", -1);
    }
}

static void draw_deck_list(void)
{
    Rectangle panel = deck_rect();
    draw_panel_header(panel, "DECK", TextFormat("%d", admin_deck.card_count));
    int capacity = row_capacity(panel, 20);
    clamp_scroll(&deck_scroll, admin_deck.card_count, capacity);
    int last = deck_scroll + capacity;
    if (last > admin_deck.card_count) last = admin_deck.card_count;
    for (int i = deck_scroll; i < last; i++)
    {
        CardInstance *inst = &admin_deck.cards[i];
        if (!inst->def) continue;
        Rectangle row = { panel.x + 6.0f, panel.y + 20.0f + (float)(i - deck_scroll) * ADMIN_ROW_H, panel.width - 12.0f, 12.0f };
        Color c = theme_class_color(inst->def->class);
        draw_row(row, false, c);
        draw_text_box((Rectangle){ row.x + 4.0f, row.y + 1.0f, row.width - 52.0f, 10.0f },
            inst->def->name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ row.x + row.width - 46.0f, row.y + 1.0f, 30.0f, 10.0f },
            TextFormat("+%d", inst->upgrade_level), 10, 0, c, TEXT_ALIGN_RIGHT);
        draw_text_box((Rectangle){ row.x + row.width - 12.0f, row.y + 1.0f, 10.0f, 10.0f },
            "x", 10, 0, (Color){ 235, 130, 130, 230 }, TEXT_ALIGN_RIGHT);
    }
}

static void draw_card_library(void)
{
    Rectangle panel = cards_rect();
    draw_panel_header(panel, "CARD LIBRARY", TextFormat("+%d", add_upgrade_level));
    int total = card_defs_loaded_count();
    int capacity = row_capacity(panel, 20);
    clamp_scroll(&card_scroll, total, capacity);
    int last = card_scroll + capacity;
    if (last > total) last = total;
    for (int i = card_scroll; i < last; i++)
    {
        const CardDef *def = card_def_by_index(i);
        if (!def) continue;
        Rectangle row = { panel.x + 6.0f, panel.y + 20.0f + (float)(i - card_scroll) * ADMIN_ROW_H, panel.width - 12.0f, 12.0f };
        Color c = theme_class_color(def->class);
        draw_row(row, false, c);
        draw_text_box((Rectangle){ row.x + 4.0f, row.y + 1.0f, 14.0f, 10.0f },
            card_type_short(def->type), 10, 0, c, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ row.x + 20.0f, row.y + 1.0f, row.width - 54.0f, 10.0f },
            def->name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ row.x + row.width - 28.0f, row.y + 1.0f, 24.0f, 10.0f },
            TextFormat("%d", def->cost), 10, 0, (Color){ 240, 210, 90, 230 }, TEXT_ALIGN_RIGHT);
    }
}

static void draw_enemies(void)
{
    Rectangle panel = enemies_rect();
    draw_panel_header(panel, "ENCOUNTER", TextFormat("%d / %d", admin_encounter.count, MAX_ENEMIES));
    for (int i = 0; i < admin_encounter.count; i++)
    {
        const EnemyDef *def = admin_encounter.enemies[i];
        Rectangle row = { panel.x + 6.0f, panel.y + 20.0f + i * 8.0f, panel.width - 12.0f, 8.0f };
        DrawRectangleRec(row, (Color){ 40, 24, 30, 225 });
        draw_text_box((Rectangle){ row.x + 3.0f, row.y, row.width - 6.0f, row.height },
            def ? def->name : "Empty", 10, 0, (Color){ 245, 190, 190, 235 }, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ row.x + row.width - 10.0f, row.y, 8.0f, row.height },
            "x", 10, 0, (Color){ 245, 140, 140, 235 }, TEXT_ALIGN_RIGHT);
    }

    Rectangle library = { panel.x, panel.y + 68.0f, panel.width, panel.height - 68.0f };
    draw_text_box((Rectangle){ library.x + 6.0f, library.y + 2.0f, library.width - 12.0f, 10.0f },
        "ENEMY LIBRARY", 10, 0, (Color){ 145, 153, 190, 220 }, TEXT_ALIGN_LEFT);
    int total = enemy_defs_loaded_count();
    int capacity = row_capacity(library, 14);
    clamp_scroll(&enemy_scroll, total, capacity);
    int last = enemy_scroll + capacity;
    if (last > total) last = total;
    for (int i = enemy_scroll; i < last; i++)
    {
        const EnemyDef *def = enemy_def_by_index(i);
        if (!def) continue;
        Rectangle row = { library.x + 6.0f, library.y + 14.0f + (float)(i - enemy_scroll) * ADMIN_ROW_H, library.width - 12.0f, 12.0f };
        draw_row(row, false, (Color){ 220, 95, 95, 235 });
        draw_text_box((Rectangle){ row.x + 4.0f, row.y + 1.0f, row.width - 48.0f, 10.0f },
            def->name, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        draw_text_box((Rectangle){ row.x + row.width - 42.0f, row.y + 1.0f, 38.0f, 10.0f },
            TextFormat("%d", def->max_hp), 10, 0, (Color){ 245, 160, 150, 230 }, TEXT_ALIGN_RIGHT);
    }
}

static void draw_controls(void)
{
    Rectangle controls = controls_rect();
    draw_panel_header(controls, "ADMIN", admin_message);

    draw_text_box((Rectangle){ controls.x + 10.0f, controls.y + 14.0f, 42.0f, 10.0f },
        "ADD", 10, 0, (Color){ 150, 158, 190, 220 }, TEXT_ALIGN_LEFT);
    Rectangle base = { controls.x + 56.0f, controls.y + 10.0f, 44.0f, 18.0f };
    Rectangle upg = { controls.x + 104.0f, controls.y + 10.0f, 44.0f, 18.0f };
    Rectangle max = { controls.x + 152.0f, controls.y + 10.0f, 44.0f, 18.0f };
    draw_btn_standard(base, add_upgrade_level == 0 ? (Color){ 58, 80, 112, 255 } : (Color){ 34, 38, 56, 255 }, (Color){ 78, 104, 146, 255 }, "BASE", 20);
    draw_btn_standard(upg, add_upgrade_level == 1 ? (Color){ 58, 80, 112, 255 } : (Color){ 34, 38, 56, 255 }, (Color){ 78, 104, 146, 255 }, "+1", 21);
    draw_btn_standard(max, add_upgrade_level == 2 ? (Color){ 58, 80, 112, 255 } : (Color){ 34, 38, 56, 255 }, (Color){ 78, 104, 146, 255 }, "+2", 22);

    Rectangle starter = { controls.x + 210.0f, controls.y + 10.0f, 86.0f, 18.0f };
    Rectangle clear = { controls.x + 300.0f, controls.y + 10.0f, 74.0f, 18.0f };
    draw_btn_standard(starter, (Color){ 34, 50, 58, 255 }, (Color){ 58, 82, 92, 255 }, "STARTER", 23);
    draw_btn_standard(clear, (Color){ 58, 38, 44, 255 }, (Color){ 92, 58, 66, 255 }, "CLEAR", 24);

    draw_text_box((Rectangle){ controls.x + 10.0f, controls.y + 40.0f, 42.0f, 10.0f },
        "TYPE", 10, 0, (Color){ 150, 158, 190, 220 }, TEXT_ALIGN_LEFT);
    Rectangle normal = { controls.x + 56.0f, controls.y + 36.0f, 58.0f, 18.0f };
    Rectangle elite = { controls.x + 118.0f, controls.y + 36.0f, 54.0f, 18.0f };
    Rectangle boss = { controls.x + 176.0f, controls.y + 36.0f, 54.0f, 18.0f };
    draw_btn_standard(normal, encounter_mode == 0 ? (Color){ 58, 80, 112, 255 } : (Color){ 34, 38, 56, 255 }, (Color){ 78, 104, 146, 255 }, "NORMAL", 25);
    draw_btn_standard(elite, encounter_mode == 1 ? (Color){ 88, 70, 42, 255 } : (Color){ 34, 38, 56, 255 }, (Color){ 126, 98, 58, 255 }, "ELITE", 26);
    draw_btn_standard(boss, encounter_mode == 2 ? (Color){ 94, 48, 58, 255 } : (Color){ 34, 38, 56, 255 }, (Color){ 140, 70, 82, 255 }, "BOSS", 27);

    Rectangle area_minus = { controls.x + 284.0f, controls.y + 36.0f, 22.0f, 18.0f };
    Rectangle area_plus = { controls.x + 392.0f, controls.y + 36.0f, 22.0f, 18.0f };
    draw_text_box((Rectangle){ controls.x + 240.0f, controls.y + 40.0f, 38.0f, 10.0f },
        "AREA", 10, 0, (Color){ 150, 158, 190, 220 }, TEXT_ALIGN_LEFT);
    draw_btn_standard(area_minus, (Color){ 34, 38, 56, 255 }, (Color){ 68, 74, 108, 255 }, "-", 28);
    draw_text_box((Rectangle){ controls.x + 310.0f, controls.y + 40.0f, 78.0f, 10.0f },
        TextFormat("%d", admin_area + 1), 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    draw_btn_standard(area_plus, (Color){ 34, 38, 56, 255 }, (Color){ 68, 74, 108, 255 }, "+", 29);

    Rectangle launch = { controls.x + controls.width - 108.0f, controls.y + 10.0f, 96.0f, 44.0f };
    bool ready = admin_team_count() > 0 && admin_deck.card_count > 0 && admin_encounter.count > 0;
    draw_btn_large(launch,
        ready ? (Color){ 46, 117, 82, 255 } : (Color){ 50, 52, 66, 255 },
        ready ? (Color){ 74, 156, 112, 255 } : (Color){ 70, 72, 88, 255 },
        "FIGHT",
        ready ? "CUSTOM" : "INCOMPLETE",
        30);
}

void admin_screen_draw(void)
{
    if (!initialized)
        admin_init();

    theme_draw_background();
    draw_text_box((Rectangle){ 8.0f, 8.0f, 240.0f, 18.0f },
        "ADMIN LAB", 18, 0, RAYWHITE, TEXT_ALIGN_LEFT);
    draw_text_box((Rectangle){ 436.0f, 13.0f, 196.0f, 10.0f },
        admin_message, 10, 0, (Color){ 125, 132, 164, 200 }, TEXT_ALIGN_RIGHT);

    draw_team();
    draw_deck_list();
    draw_card_library();
    draw_enemies();
    draw_controls();

    Vector2 mouse = GetMousePosition();
    Rectangle cards = cards_rect();
    Rectangle deck = deck_rect();
    int card_capacity = row_capacity(cards, 20);
    int deck_capacity = row_capacity(deck, 20);
    if (CheckCollisionPointRec(mouse, cards))
    {
        int row = ((int)(mouse.y - cards.y - 20.0f)) / ADMIN_ROW_H;
        int idx = card_scroll + row;
        const CardDef *def = row >= 0 && row < card_capacity ? card_def_by_index(idx) : NULL;
        if (def)
            theme_draw_card_tooltip((Rectangle){ 438.0f, 276.0f, 96.0f, 74.0f }, def, add_upgrade_level);
    }
    else if (CheckCollisionPointRec(mouse, deck))
    {
        int row = ((int)(mouse.y - deck.y - 20.0f)) / ADMIN_ROW_H;
        int idx = deck_scroll + row;
        if (row >= 0 && row < deck_capacity && idx >= 0 && idx < admin_deck.card_count && admin_deck.cards[idx].def)
            theme_draw_card_tooltip((Rectangle){ 438.0f, 276.0f, 96.0f, 74.0f }, admin_deck.cards[idx].def, admin_deck.cards[idx].upgrade_level);
    }
}
