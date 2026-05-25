#include "screens.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "game.h"
#include "data/area_defs.h"
#include "data/card_defs.h"
#include "systems/relic.h"
#include "util/tween.h"
#include "util/text.h"
#include "util/math_utils.h"
#include "util/log.h"
#include "constants.h"
#include "raylib.h"
#include <string.h>

#define CLASS_COLOR(r,g,b) (Color){ r, g, b, 255 }

static const struct {
    const char *name;
    const char *role;
    unsigned char cr, cg, cb;
    const char *tagline;
} class_info[CLASS_COUNT] = {
    { "Guardian", "Tank",           74, 144, 217, "Draws enemy fire. Protects the party." },
    { "Cleric",   "Healer",         225, 170, 50,  "Keeps everyone alive. Draws aggro." },
    { "Mage",     "Burst DPS",      155, 89, 182,  "Massive damage. Risk of overload." },
    { "Rogue",    "Interrupt DPS",  39, 174, 96,   "Clutch saves. Combo attacks." },
    { "Shaman",   "Support",        230, 126, 34,  "Totems. Buffs. Crowd control." },
    { "Ranger",   "Sustained DPS",  70, 190, 120,  "Marks. Traps. Consistent pressure." },
    { "Paladin",  "Tank / Healer",  240, 210, 95,  "Protective heals. Holy damage." },
    { "Warlock",  "DOT / Curses",   125, 70, 185,  "Bleeds power from risky curses." },
    { "Bard",     "Buffer",         235, 95, 155,  "Draw, tempo, and party buffs." },
};

static Color class_color(int i) { return CLASS_COLOR(class_info[i].cr, class_info[i].cg, class_info[i].cb); }

static const char *class_synergy_hint(int index)
{
    switch ((ClassType)index)
    {
        case CLASS_GUARDIAN: return "Feeds BLIGHT payoffs. Pairs: Mage Molten Armor, Shaman Bulwark.";
        case CLASS_CLERIC:   return "Consumes MARKED/BLIGHT for recovery. Pairs: Rogue Shadow Mend.";
        case CLASS_MAGE:     return "Detonates CONDUCTIVE and primes Rogue combos.";
        case CLASS_ROGUE:    return "Consumes MARKED, applies BLIGHT, chains with Mage and Cleric.";
        case CLASS_SHAMAN:   return "Applies CONDUCTIVE. Pairs with Mage, Ranger, Guardian.";
        case CLASS_RANGER:   return "Applies MARKED. Pairs with Rogue Ambush and MARKED payoffs.";
        case CLASS_PALADIN:  return "Consumes MARKED/BLIGHT for heals and control. Chains with Bard and Warlock.";
        case CLASS_WARLOCK:  return "Turns CONDUCTIVE into BLIGHT. Chains with Bard and Paladin.";
        case CLASS_BARD:     return "Applies MARKED/CONDUCTIVE, extends statuses, and amplifies Finale.";
        default:             return "";
    }
}

static Color class_feed_color(int index)
{
    switch ((ClassType)index)
    {
        case CLASS_MAGE:
        case CLASS_ROGUE:
        case CLASS_RANGER:
        case CLASS_WARLOCK:
            return (Color){ 220, 75, 80, 230 };
        case CLASS_GUARDIAN:
        case CLASS_PALADIN:
            return (Color){ 90, 165, 245, 230 };
        case CLASS_CLERIC:
        case CLASS_SHAMAN:
        case CLASS_BARD:
            return (Color){ 235, 205, 85, 230 };
        default:
            return (Color){ 180, 110, 230, 230 };
    }
}

static bool selected[CLASS_COUNT];
static int selected_count;
static int max_selected = 3;
static float card_alphas[CLASS_COUNT];
static float card_tweens[CLASS_COUNT];
static int card_tween_ids[CLASS_COUNT];
static Button begin_btn;
static float begin_btn_alpha = 0.0f;
static float title_y = -60.0f;
static bool initialized = false;

static Rectangle draft_card_rect_for(int index)
{
    const float card_w = 180.0f;
    const float card_h = 70.0f;
    const float gap_x = 14.0f;
    const float gap_y = 8.0f;
    int col = index % 3;
    int row = index / 3;
    float start_x = (VIRT_W - (3.0f * card_w + 2.0f * gap_x)) * 0.5f;
    return (Rectangle){ start_x + col * (card_w + gap_x), 72.0f + row * (card_h + gap_y), card_w, card_h };
}

static Rectangle draft_begin_rect(void)
{
    return (Rectangle){ (float)(VIRT_W / 2 - 90), 318.0f, 180.0f, (float)BTN_H };
}

static void card_click_cb(int index)
{
    if (!meta_class_unlocked(&g_state.meta, index))
        return;

    if (selected[index])
    {
        selected[index] = false;
        selected_count--;
        tween_create(&card_tweens[index], 0.0f, 0.25f, EASE_OUT_QUAD);
        LOG_I(CAT_DRAFT, "Deselected class %d (%s) — %d/%d", index, class_info[index].name, selected_count, max_selected);
    }
    else if (selected_count < max_selected)
    {
        selected[index] = true;
        selected_count++;
        card_tweens[index] = 1.0f;
        tween_create(&card_tweens[index], -0.04f, 0.3f, EASE_OUT_BACK);
        tween_chain(tween_create(&card_alphas[index], 1.0f, 0.3f, EASE_OUT_QUAD), &card_alphas[index], 1.0f, 0.0f, EASE_LINEAR);
        LOG_I(CAT_DRAFT, "Selected class %d (%s) — %d/%d", index, class_info[index].name, selected_count, max_selected);
    }
    g_state.selected_count = selected_count;
}

void draft_screen_update(void)
{
    if (!initialized)
    {
        max_selected = g_state.max_party_size;
        if (max_selected < 1) max_selected = 3;
        if (max_selected > MAX_PARTY_SIZE) max_selected = MAX_PARTY_SIZE;

        for (int i = 0; i < CLASS_COUNT; i++)
        {
            selected[i] = false;
            card_alphas[i] = 0.0f;
            card_tweens[i] = 0.0f;
            card_tween_ids[i] = -1;
        }
        selected_count = 0;
        g_state.selected_count = 0;

        begin_btn = button_create(
            draft_begin_rect(),
            "ASSEMBLE YOUR PARTY",
            (Color){ 46, 117, 182, 255 },
            (Color){ 80, 160, 230, 255 },
            WHITE
        );

        title_y = -18.0f;
        tween_create(&title_y, 24.0f, 0.5f, EASE_OUT_BACK);

        for (int i = 0; i < CLASS_COUNT; i++)
        {
            float delay = 0.1f + i * 0.06f;
            int prev = tween_create(&card_alphas[i], 1.0f, 0.35f, EASE_OUT_QUAD);
            (void)prev;
            card_tweens[i] = 0.0f;
        }

        initialized = true;
    }

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        Rectangle card_rect = draft_card_rect_for(i);

        bool hovered = CheckCollisionPointRec(mouse, card_rect);
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && meta_class_unlocked(&g_state.meta, i))
            card_click_cb(i);
    }

    if (selected_count > 0)
    {
        begin_btn.text = selected_count == max_selected ? "BEGIN RUN" : "BEGIN UNDERMANNED";
        begin_btn_alpha = 1.0f;
        button_update(&begin_btn);

        if (begin_btn.pressed_this_frame)
        {
            int sel_idx = 0;
            for (int i = 0; i < CLASS_COUNT && sel_idx < selected_count; i++)
            {
                if (selected[i])
                    g_state.selected_classes[sel_idx++] = i;
            }
            g_state.selected_count = sel_idx;
            LOG_I(CAT_DRAFT, "BEGIN RUN pressed. selected_count=%d, max_selected=%d", g_state.selected_count, max_selected);
            for (int i = 0; i < g_state.selected_count; i++)
                LOG_I(CAT_DRAFT, "  selected_classes[%d] = %d", i, g_state.selected_classes[i]);
            map_clear(&g_state.map);
            g_state.map.floor = 0;
            g_state.current_area = area_clamp_index(g_state.current_area);
            g_state.gold = meta_starting_gold(&g_state.meta);
            g_state.relic_count = 0;
            g_state.relic_reward_pending = false;
            g_state.relic_reward_count = 0;
            g_state.encounter = NULL;
            g_state.encounter_is_elite = false;
            g_state.encounter_is_boss = false;
            g_state.result_area = g_state.current_area;
            g_state.result_floor = 1;
            g_state.result_bosses_defeated = 0;
            g_state.result_achievement_renown = 0;
            g_state.result_achievement_names[0] = '\0';
            g_state.result_recorded = false;
            g_state.result_unlocked_party_size = 0;
            g_state.result_unlocked_area = -1;
            g_state.result_renown_gained = 0;
            g_state.run_won = false;
            g_state.run_deaths = 0;
            g_state.run_interrupts = 0;
            g_state.run_best_combat_turns = 0;
            g_state.result_reason[0] = '\0';
            party_create(&g_state.run_party, g_state.selected_classes, g_state.selected_count);
            g_state.run_party_active = true;
            deck_init_from_classes(&g_state.run_deck, g_state.selected_classes, g_state.selected_count);
            const CardDef *doubt = card_def_by_id("curse_doubt");
            int asc = g_state.meta.ascension_level;
            if (doubt && asc >= 5)
                deck_add_card(&g_state.run_deck, doubt);
            if (doubt && asc >= 9)
                deck_add_card(&g_state.run_deck, doubt);

            if (g_state.meta.starting_relic_rank == 1 || g_state.meta.starting_relic_rank == 2)
            {
                RelicId start_relic = relic_random_unowned_by_rarity(g_state.relics, g_state.relic_count, g_state.meta.starting_relic_rank);
                if (start_relic != RELIC_NONE)
                    relic_add_unique(g_state.relics, &g_state.relic_count, start_relic);
            }
            else if (g_state.meta.starting_relic_rank >= 3)
            {
                g_state.relic_reward_count = relic_generate_choices_by_rarity(
                    g_state.relics,
                    g_state.relic_count,
                    g_state.relic_reward_choices,
                    2,
                    2);
                g_state.relic_reward_pending = g_state.relic_reward_count > 0;
            }
            LOG_I(CAT_SCREEN, "Run deck built: %d cards", g_state.run_deck.card_count);
            initialized = false;
            game_change_screen(g_state.relic_reward_pending ? SCREEN_RELIC_REWARD : SCREEN_MAP);
        }
    }
    else
    {
        begin_btn.text = selected_count > 0
            ? "BEGIN UNDERMANNED"
            : "ASSEMBLE YOUR PARTY";
        begin_btn_alpha = 0.4f;
    }
}

void draft_screen_draw(void)
{
    theme_draw_background();

    draw_text_box((Rectangle){ 80.0f, (float)title_y, 480.0f, 22.0f },
        "ASSEMBLE YOUR PARTY", 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);

    char counter[64];
    snprintf(counter, sizeof(counter), "%d / %d selected", selected_count, max_selected);
    Color counter_color = selected_count > 0 ? (Color){ 70, 220, 120, 255 } : (Color){ 160, 160, 180, 255 };
    draw_text_box((Rectangle){ 80.0f, title_y + 21.0f, 480.0f, 14.0f },
        counter, 10, 0, counter_color, TEXT_ALIGN_CENTER);

    const AreaDef *area = area_def(g_state.current_area);
    if (area)
    {
        char area_line[96];
        snprintf(area_line, sizeof(area_line), "%s  Floor set: %d", area->name, area->floor_count);
        draw_text_box((Rectangle){ 80.0f, title_y + 34.0f, 480.0f, 14.0f },
            area_line, 10, 0, (Color){ 150, 155, 180, 200 }, TEXT_ALIGN_CENTER);
    }

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        Color c = class_color(i);
        c.a = (unsigned char)(card_alphas[i] * 255);

        Rectangle card_rect = draft_card_rect_for(i);

        Vector2 mouse = GetMousePosition();
        bool hovered = CheckCollisionPointRec(mouse, card_rect);
        bool unlocked = meta_class_unlocked(&g_state.meta, i);

        Color card_bg = c;
        card_bg.r = (unsigned char)(card_bg.r * 0.2f);
        card_bg.g = (unsigned char)(card_bg.g * 0.2f);
        card_bg.b = (unsigned char)(card_bg.b * 0.2f);
        card_bg.a = (unsigned char)(card_alphas[i] * 255);

        if (hovered && unlocked)
        {
            card_bg.r = (unsigned char)(card_bg.r * 1.3f);
            card_bg.g = (unsigned char)(card_bg.g * 1.3f);
            card_bg.b = (unsigned char)(card_bg.b * 1.3f);
        }

        float offset_y = card_tweens[i];
        Rectangle draw_rect = card_rect;
        draw_rect.y += offset_y;

        DrawRectangleRec(draw_rect, card_bg);

        if (!unlocked)
        {
            DrawRectangleRec(draw_rect, (Color){ 18, 19, 28, (unsigned char)(card_alphas[i] * 230) });
            c = (Color){ 95, 95, 115, 255 };
        }

        Color border = selected[i] ? c : (Color){ 60, 60, 80, (unsigned char)(card_alphas[i] * 100) };
        DrawRectangleLinesEx(draw_rect, selected[i] ? 2.0f : 1.0f, border);

        int text_x = snap_i(draw_rect.x + 8);
        Color name_c = unlocked ? RAYWHITE : (Color){ 125, 128, 145, 230 };
        name_c.a = (unsigned char)(card_alphas[i] * 255);
        draw_text_box((Rectangle){ (float)text_x, draw_rect.y + 7.0f, draw_rect.width - 78.0f, 14.0f },
            class_info[i].name, 10, 0, name_c, TEXT_ALIGN_LEFT);

        Color role_c = c;
        role_c.a = (unsigned char)(card_alphas[i] * 200);
        draw_text_box((Rectangle){ (float)text_x, draw_rect.y + 23.0f, draw_rect.width - 78.0f, 14.0f },
            class_info[i].role, 10, 0, role_c, TEXT_ALIGN_LEFT);

        DrawRectangle(text_x, snap_i(draw_rect.y + 38), snap_i(draw_rect.width - 18), 1, (Color){ 60, 60, 80, (unsigned char)(card_alphas[i] * 200) });

        Color tag_c = unlocked ? (Color){ 160, 160, 180, (unsigned char)(card_alphas[i] * 200) } : (Color){ 105, 108, 125, (unsigned char)(card_alphas[i] * 200) };
        draw_text_box((Rectangle){ (float)text_x, draw_rect.y + 43.0f, draw_rect.width - 58.0f, 24.0f },
            unlocked ? class_info[i].tagline : "Locked in the Meta Shop.", 10, 0, tag_c, TEXT_ALIGN_LEFT);

        theme_draw_class_portrait((ClassType)i,
            snap_i(draw_rect.x + draw_rect.width - 28),
            snap_i(draw_rect.y + draw_rect.height - 28),
            13,
            unlocked);

        if (selected[i])
            draw_text_box((Rectangle){ draw_rect.x + draw_rect.width - 75.0f, draw_rect.y + 9.0f, 66.0f, 14.0f },
                "SELECTED", 10, 0, (Color){ 220, 245, 230, 230 }, TEXT_ALIGN_RIGHT);
        else if (!unlocked)
            draw_text_box((Rectangle){ draw_rect.x + draw_rect.width - 68.0f, draw_rect.y + 9.0f, 58.0f, 14.0f },
                "LOCKED", 10, 0, (Color){ 145, 145, 165, 220 }, TEXT_ALIGN_RIGHT);

        Color feed = class_feed_color(i);
        DrawRectangle(snap_i(draw_rect.x), snap_i(draw_rect.y + draw_rect.height - 3), snap_i(draw_rect.width), 3, feed);
    }

    int hover_idx = -1;
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < CLASS_COUNT; i++)
        if (CheckCollisionPointRec(mouse, draft_card_rect_for(i)))
            hover_idx = i;
    if (hover_idx >= 0)
    {
        Rectangle hint = { 118.0f, 300.0f, 404.0f, 18.0f };
        Color c = class_color(hover_idx);
        DrawRectangleRec(hint, (Color){ 10, 11, 18, 235 });
        DrawRectangleLinesEx(hint, 1.0f, (Color){ c.r, c.g, c.b, 190 });
        draw_text_box((Rectangle){ hint.x + 6.0f, hint.y + 3.0f, hint.width - 12.0f, hint.height - 5.0f },
            class_synergy_hint(hover_idx), 10, 0, (Color){ 215, 220, 240, 235 }, TEXT_ALIGN_LEFT);
    }

    if (selected_count > 0)
    {
        button_draw(&begin_btn);
        if (selected_count < max_selected)
        {
            char warn[96];
            snprintf(warn, sizeof(warn), "Party not full: %d/%d selected", selected_count, max_selected);
            draw_text_box((Rectangle){ 80.0f, 342.0f, 480.0f, 16.0f },
                warn, 10, 0, (Color){ 230, 205, 95, 220 }, TEXT_ALIGN_CENTER);
        }
    }
    else
    {
        Color bg = { 46, 117, 182, (unsigned char)(begin_btn_alpha * 100) };
        Color border = { 100, 100, 130, (unsigned char)(begin_btn_alpha * 80) };
        Rectangle btn_rect = draft_begin_rect();
        DrawRectangleRec(btn_rect, bg);
        DrawRectangleLinesEx(btn_rect, 1.0f, border);
        Color txt = { 160, 160, 180, (unsigned char)(begin_btn_alpha * 100) };
        draw_text_box((Rectangle){ btn_rect.x + 6.0f, btn_rect.y + 5.0f, btn_rect.width - 12.0f, btn_rect.height - 8.0f },
            begin_btn.text, 10, 0, txt, TEXT_ALIGN_CENTER);
    }

    Color sep = { 60, 60, 80, 120 };
    DrawRectangle(0, 300, VIRT_W, 1, sep);
    Color hint = { 100, 100, 130, 120 };
}



