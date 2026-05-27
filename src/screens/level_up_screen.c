#include "screens.h"
#include "game.h"
#include "ui/theme.h"
#include "util/text.h"
#include "constants.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>

static int current_member = -1;
static PerkId choice_a = PERK_COUNT;
static PerkId choice_b = PERK_COUNT;

static int find_next_pending_member(void)
{
    for (int i = 0; i < g_state.run_party.count; i++)
        if (g_state.run_party.members[i].pending_levels > 0)
            return i;
    return -1;
}

static PerkId generic_choice_for(const PartyMember *member, int salt)
{
    static const PerkId generic[] = {
        PERK_HP_PLUS_4,
        PERK_STARTING_SHIELD_1,
        PERK_CARD_DMG_1,
        PERK_CARD_HEAL_1,
        PERK_CARD_SHIELD_1
    };
    int count = (int)(sizeof(generic) / sizeof(generic[0]));
    int seed = member ? member->level + member->xp + member->perk_count * 3 : 0;
    return generic[(seed + salt + rand()) % count];
}

static void generate_choices(void)
{
    if (current_member < 0 || current_member >= g_state.run_party.count)
    {
        choice_a = PERK_COUNT;
        choice_b = PERK_COUNT;
        return;
    }

    PartyMember *member = &g_state.run_party.members[current_member];
    choice_a = generic_choice_for(member, 0);

    PerkId class_perk = perk_for_class(member->class);
    if (class_perk >= 0 && class_perk < PERK_COUNT &&
        !party_member_has_perk(member, class_perk))
    {
        choice_b = class_perk;
        return;
    }

    choice_b = generic_choice_for(member, 2);
    int attempts = 0;
    while (choice_b == choice_a && attempts++ < 8)
        choice_b = generic_choice_for(member, attempts + 3);
}

static void ensure_current_member(void)
{
    if (current_member >= 0 &&
        current_member < g_state.run_party.count &&
        g_state.run_party.members[current_member].pending_levels > 0 &&
        choice_a >= 0 && choice_a < PERK_COUNT &&
        choice_b >= 0 && choice_b < PERK_COUNT)
        return;

    current_member = find_next_pending_member();
    if (current_member < 0)
    {
        game_change_screen(g_state.post_combat_destination);
        return;
    }
    generate_choices();
}

static Rectangle choice_rect(int index)
{
    return (Rectangle){ index == 0 ? 110.0f : 334.0f, 178.0f, 196.0f, 92.0f };
}

static void choose_perk(PerkId perk)
{
    if (current_member < 0 || current_member >= g_state.run_party.count)
        return;
    PartyMember *member = &g_state.run_party.members[current_member];
    if (member->pending_levels <= 0)
        return;

    party_member_add_perk(member, perk);
    member->pending_levels--;

    current_member = find_next_pending_member();
    if (current_member < 0)
    {
        choice_a = PERK_COUNT;
        choice_b = PERK_COUNT;
        game_change_screen(g_state.post_combat_destination);
        return;
    }
    generate_choices();
}

void level_up_screen_update(void)
{
    ensure_current_member();
    if (current_member < 0)
        return;

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mouse, choice_rect(0)))
            choose_perk(choice_a);
        else if (CheckCollisionPointRec(mouse, choice_rect(1)))
            choose_perk(choice_b);
    }
}

static void draw_choice(Rectangle r, PerkId perk, bool hover)
{
    Color accent = perk_is_class_specific(perk) ? (Color){ 245, 210, 95, 235 } : (Color){ 120, 190, 245, 235 };
    Color bg = hover ? (Color){ 34, 35, 54, 245 } : (Color){ 20, 22, 34, 235 };
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, hover ? 2.0f : 1.0f, accent);
    draw_text_box((Rectangle){ r.x + 9.0f, r.y + 8.0f, r.width - 18.0f, 24.0f },
        perk_name(perk), 18, 0, accent, TEXT_ALIGN_CENTER);
    draw_text_box((Rectangle){ r.x + 10.0f, r.y + 39.0f, r.width - 20.0f, 38.0f },
        perk_description(perk), 10, 0, (Color){ 215, 220, 238, 235 }, TEXT_ALIGN_CENTER);
    draw_text_box((Rectangle){ r.x + 10.0f, r.y + r.height - 15.0f, r.width - 20.0f, 12.0f },
        perk_is_class_specific(perk) ? "CLASS PERK" : "STACKS", 10, 0, (Color){ 170, 175, 205, 210 }, TEXT_ALIGN_CENTER);
}

void level_up_screen_draw(void)
{
    theme_draw_background();
    ensure_current_member();
    if (current_member < 0)
        return;

    PartyMember *member = &g_state.run_party.members[current_member];
    Color accent = theme_class_color(member->class);
    Vector2 mouse = GetMousePosition();

    draw_text_box((Rectangle){ 80.0f, 24.0f, 480.0f, 22.0f },
        "LEVEL UP", 18, 0, (Color){ 245, 225, 130, 255 }, TEXT_ALIGN_CENTER);

    theme_draw_class_portrait(member->class, VIRT_W / 2, 88, 32, member->alive);

    char title[96];
    snprintf(title, sizeof(title), "%s - Level %d", member->name, member->level);
    draw_text_box((Rectangle){ 80.0f, 134.0f, 480.0f, 20.0f },
        title, 18, 0, accent, TEXT_ALIGN_CENTER);

    int next = xp_for_level(member->level);
    char progress[96];
    if (member->level >= MAX_LEVEL)
        snprintf(progress, sizeof(progress), "Max level reached. %d choice%s pending.",
            member->pending_levels, member->pending_levels == 1 ? "" : "s");
    else
        snprintf(progress, sizeof(progress), "XP %d/%d  |  %d choice%s pending",
            party_member_xp_into_level(member), next, member->pending_levels, member->pending_levels == 1 ? "" : "s");
    draw_text_box((Rectangle){ 80.0f, 154.0f, 480.0f, 14.0f },
        progress, 10, 0, (Color){ 190, 195, 220, 230 }, TEXT_ALIGN_CENTER);

    Rectangle a = choice_rect(0);
    Rectangle b = choice_rect(1);
    draw_choice(a, choice_a, CheckCollisionPointRec(mouse, a));
    draw_choice(b, choice_b, CheckCollisionPointRec(mouse, b));

    int remaining_members = 0;
    int remaining_choices = 0;
    for (int i = 0; i < g_state.run_party.count; i++)
    {
        if (g_state.run_party.members[i].pending_levels > 0)
            remaining_members++;
        remaining_choices += g_state.run_party.members[i].pending_levels;
    }
    char footer[96];
    snprintf(footer, sizeof(footer), "%d member%s, %d choice%s left",
        remaining_members, remaining_members == 1 ? "" : "s",
        remaining_choices, remaining_choices == 1 ? "" : "s");
    draw_text_box((Rectangle){ 80.0f, 302.0f, 480.0f, 14.0f },
        footer, 10, 0, (Color){ 170, 175, 205, 220 }, TEXT_ALIGN_CENTER);
}
