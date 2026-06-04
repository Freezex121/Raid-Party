#include "screens.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "game.h"
#include "assets.h"
#include "data/area_defs.h"
#include "data/card_defs.h"
#include "systems/relic.h"
#include "systems/telemetry.h"
#include "util/tween.h"
#include "util/text.h"
#include "util/math_utils.h"
#include "util/log.h"
#include "constants.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

static Color draft_class_color(ClassType ct)
{
    return (Color){ class_color_r(ct), class_color_g(ct), class_color_b(ct), 255 };
}

static bool selected[CLASS_COUNT];
static int selected_count;
static int max_selected = 3;
static float card_alphas[CLASS_COUNT];
static float card_tweens[CLASS_COUNT];
static int card_tween_ids[CLASS_COUNT];
static Button begin_btn;
static Button back_btn;
static float begin_btn_alpha = 0.0f;
static float title_y = -60.0f;
static float draft_scroll_y = 0.0f;
static bool initialized = false;

static Rectangle draft_list_viewport(void)
{
    return (Rectangle){ 0.0f, 68.0f, (float)VIRT_W, 228.0f };
}

static Rectangle draft_card_rect_for_slot(int slot)
{
    const float card_w = 180.0f;
    const float card_h = 70.0f;
    const float gap_x = 14.0f;
    const float gap_y = 8.0f;
    int col = slot % 3;
    int row = slot / 3;
    float start_x = (VIRT_W - (3.0f * card_w + 2.0f * gap_x)) * 0.5f;
    return (Rectangle){ start_x + col * (card_w + gap_x), 72.0f + row * (card_h + gap_y) - draft_scroll_y, card_w, card_h };
}

static float draft_max_scroll(void)
{
    int count = party_class_count();
    int rows = (count + 2) / 3;
    float content_h = rows > 0 ? rows * 70.0f + (rows - 1) * 8.0f : 0.0f;
    float viewport_h = draft_list_viewport().height - 4.0f;
    float max_scroll = content_h - viewport_h;
    return max_scroll > 0.0f ? max_scroll : 0.0f;
}

static bool rect_visible_in_draft_list(Rectangle r)
{
    Rectangle view = draft_list_viewport();
    return r.y + r.height >= view.y && r.y <= view.y + view.height;
}

static Rectangle draft_begin_rect(void)
{
    return (Rectangle){ (float)(VIRT_W / 2 - BTN_WIDE / 2), 318.0f, (float)BTN_WIDE, (float)BTN_H };
}

static Rectangle draft_back_rect(void)
{
    return (Rectangle){ 12.0f, 318.0f, (float)BTN_NARROW, (float)BTN_H };
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
        LOG_I(CAT_DRAFT, "Deselected class %d (%s) - %d/%d", index, class_name((ClassType)index), selected_count, max_selected);
    }
    else if (selected_count < max_selected)
    {
        selected[index] = true;
        selected_count++;
        card_tweens[index] = 1.0f;
        tween_create(&card_tweens[index], -0.04f, 0.3f, EASE_OUT_BACK);
        tween_chain(tween_create(&card_alphas[index], 1.0f, 0.3f, EASE_OUT_QUAD), &card_alphas[index], 1.0f, 0.0f, EASE_LINEAR);
        LOG_I(CAT_DRAFT, "Selected class %d (%s) - %d/%d", index, class_name((ClassType)index), selected_count, max_selected);
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
        draft_scroll_y = 0.0f;

        begin_btn = button_create(
            draft_begin_rect(),
            "ASSEMBLE YOUR PARTY",
            (Color){ 46, 117, 182, 255 },
            (Color){ 80, 160, 230, 255 },
            WHITE
        );
        back_btn = button_create(
            draft_back_rect(),
            "BACK",
            (Color){ 42, 48, 70, 255 },
            (Color){ 70, 78, 110, 255 },
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
    button_update(&back_btn);
    if (back_btn.pressed_this_frame)
    {
        initialized = false;
        g_state.selected_count = 0;
        game_change_screen(SCREEN_TITLE);
        return;
    }

    Rectangle viewport = draft_list_viewport();
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && CheckCollisionPointRec(mouse, viewport))
    {
        draft_scroll_y -= wheel * 24.0f;
        float max_scroll = draft_max_scroll();
        if (draft_scroll_y < 0.0f) draft_scroll_y = 0.0f;
        if (draft_scroll_y > max_scroll) draft_scroll_y = max_scroll;
    }

    int class_count = party_class_count();
    for (int slot = 0; slot < class_count; slot++)
    {
        ClassType ct = party_class_at(slot);
        Rectangle card_rect = draft_card_rect_for_slot(slot);
        if (!rect_visible_in_draft_list(card_rect))
            continue;

        bool hovered = CheckCollisionPointRec(mouse, card_rect) && CheckCollisionPointRec(mouse, viewport);
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && meta_class_unlocked(&g_state.meta, ct))
            card_click_cb((int)ct);
    }

    if (selected_count > 0)
    {
        begin_btn.text = selected_count == max_selected ? "BEGIN RUN" : "BEGIN UNDERMANNED";
        begin_btn_alpha = 1.0f;
        button_update(&begin_btn);

        if (begin_btn.pressed_this_frame)
        {
            int sel_idx = 0;
            for (int i = 0; i < class_count && sel_idx < selected_count; i++)
            {
                ClassType ct = party_class_at(i);
                if (ct >= 0 && ct < CLASS_COUNT && selected[ct])
                    g_state.selected_classes[sel_idx++] = ct;
            }
            g_state.selected_count = sel_idx;
            LOG_I(CAT_DRAFT, "BEGIN RUN pressed. selected_count=%d, max_selected=%d", g_state.selected_count, max_selected);
            for (int i = 0; i < g_state.selected_count; i++)
                LOG_I(CAT_DRAFT, "  selected_classes[%d] = %d", i, g_state.selected_classes[i]);
            map_clear(&g_state.map);
            g_state.map.floor = 0;
            g_state.current_area = area_clamp_index(g_state.current_area);
            g_state.gold = 0;
            game_gain_gold(meta_starting_gold(&g_state.meta), "run_start");
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
            g_state.result_gold_renown = 0;
            g_state.run_won = false;
            g_state.run_deaths = 0;
            g_state.run_interrupts = 0;
            g_state.run_best_combat_turns = 0;
            g_state.next_combat_energy_bonus = 0;
            g_state.next_combat_draw_bonus = 0;
            g_state.next_combat_boon_turns = 0;
            g_state.result_reason[0] = '\0';
            g_state.tutorial_active = (g_state.meta.runs_completed == 0);
            g_state.tutorial_step = TUTORIAL_STEP_MAP;
            g_state.tutorial_reward_pending = false;
            g_state.telemetry_run_id = g_state.meta.runs_completed + 1;
            telemetry_begin_run(g_state.telemetry_run_id);
            if (g_state.tutorial_active)
                telemetry_log_tutorial("map_route", "shown", "map");
            party_create(&g_state.run_party, g_state.selected_classes, g_state.selected_count);
            int max_hp_bonus = meta_max_hp_bonus(&g_state.meta);
            if (max_hp_bonus > 0)
            {
                for (int i = 0; i < g_state.run_party.count; i++)
                {
                    g_state.run_party.members[i].max_hp += max_hp_bonus;
                    g_state.run_party.members[i].hp += max_hp_bonus;
                }
            }
            g_state.run_party_active = true;
            deck_init_from_classes(&g_state.run_deck, g_state.selected_classes, g_state.selected_count);
            const CardDef *doubt = card_def_by_id("curse_doubt");
            int asc = g_state.meta.ascension_level;
            if (doubt && asc >= 5)
                deck_add_card(&g_state.run_deck, doubt);
            if (doubt && asc >= 9)
                deck_add_card(&g_state.run_deck, doubt);

            // Starting deck cards from meta upgrades
            const CardDef *prep = card_def_by_id("util_prep");
            const CardDef *energize = card_def_by_id("util_energ");
            const CardDef *fortify = card_def_by_id("util_for");
            const CardDef *rejuv = card_def_by_id("util_rejuv");
            if (prep && g_state.meta.start_prep) deck_add_card_with_level(&g_state.run_deck, prep, g_state.meta.prep_upgraded ? 1 : 0);
            if (energize && g_state.meta.start_energize) deck_add_card(&g_state.run_deck, energize);
            if (fortify && g_state.meta.start_fortify) deck_add_card_with_level(&g_state.run_deck, fortify, g_state.meta.fortify_upgraded ? 1 : 0);
            if (rejuv && g_state.meta.start_rejuv) deck_add_card_with_level(&g_state.run_deck, rejuv, g_state.meta.rejuv_upgraded ? 1 : 0);

            if (g_state.meta.starting_relic_rank == 1 || g_state.meta.starting_relic_rank == 2)
            {
                RelicId start_relic = relic_random_unowned_by_rarity(g_state.relics, g_state.relic_count, g_state.meta.starting_relic_rank);
                if (start_relic != RELIC_NONE)
                    relic_add_unique(g_state.relics, &g_state.relic_count, start_relic);
            }
            else if (g_state.meta.starting_relic_rank >= 3)
            {
                int relic_choices = 2 + meta_relic_choice_bonus(&g_state.meta);
                if (relic_choices > RELIC_REWARD_CHOICES)
                    relic_choices = RELIC_REWARD_CHOICES;
                g_state.relic_reward_count = relic_generate_choices_by_rarity(
                    g_state.relics,
                    g_state.relic_count,
                    g_state.relic_reward_choices,
                    relic_choices,
                    2);
                g_state.relic_reward_pending = g_state.relic_reward_count > 0;
            }
            LOG_I(CAT_SCREEN, "Run deck built: %d cards", g_state.run_deck.card_count);

            char run_id[16], area[16], ascension[16], party_size[16], max_party[16], not_full[8], classes_csv[160] = "";
            snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
            snprintf(area, sizeof(area), "%d", g_state.current_area);
            snprintf(ascension, sizeof(ascension), "%d", g_state.meta.ascension_level);
            snprintf(party_size, sizeof(party_size), "%d", g_state.selected_count);
            snprintf(max_party, sizeof(max_party), "%d", g_state.max_party_size);
            snprintf(not_full, sizeof(not_full), "%d", g_state.selected_count < g_state.max_party_size ? 1 : 0);
            for (int i = 0; i < g_state.selected_count; i++)
            {
                if (i > 0) strncat(classes_csv, "|", sizeof(classes_csv) - strlen(classes_csv) - 1);
                strncat(classes_csv, class_name((ClassType)g_state.selected_classes[i]), sizeof(classes_csv) - strlen(classes_csv) - 1);
            }
            const char *draft_fields[] = { run_id, area, ascension, party_size, max_party, not_full, classes_csv };
            telemetry_csv_append("draft_metrics.csv",
                "timestamp,run_id,area,ascension,party_size,max_party_size,party_not_full,classes",
                draft_fields,
                7);
            char classes_json[192] = "";
            for (int i = 0; i < g_state.selected_count; i++)
            {
                char item[32];
                snprintf(item, sizeof(item), "%s\"%s\"", i > 0 ? "," : "", class_name((ClassType)g_state.selected_classes[i]));
                strncat(classes_json, item, sizeof(classes_json) - strlen(classes_json) - 1);
            }
            char draft_json[384];
            snprintf(draft_json, sizeof(draft_json),
                "\"area\":%d,\"ascension\":%d,\"party_size\":%d,\"max_party_size\":%d,\"party_not_full\":%s,\"classes\":[%s]",
                g_state.current_area,
                g_state.meta.ascension_level,
                g_state.selected_count,
                g_state.max_party_size,
                g_state.selected_count < g_state.max_party_size ? "true" : "false",
                classes_json);
            telemetry_push_json("draft", draft_json);
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

    Rectangle viewport = draft_list_viewport();
    Vector2 mouse = GetMousePosition();
    int hover_idx = -1;
    int class_count = party_class_count();

    BeginScissorMode(snap_i(viewport.x), snap_i(viewport.y), snap_i(viewport.width), snap_i(viewport.height));
    for (int slot = 0; slot < class_count; slot++)
    {
        ClassType ct = party_class_at(slot);
        if (ct < 0 || ct >= CLASS_COUNT)
            continue;

        Color c = draft_class_color(ct);
        c.a = (unsigned char)(card_alphas[ct] * 255);

        Rectangle card_rect = draft_card_rect_for_slot(slot);
        if (!rect_visible_in_draft_list(card_rect))
            continue;

        bool hovered = CheckCollisionPointRec(mouse, card_rect) && CheckCollisionPointRec(mouse, viewport);
        if (hovered)
            hover_idx = ct;
        bool unlocked = meta_class_unlocked(&g_state.meta, ct);

        Color card_bg = c;
        card_bg.r = (unsigned char)(card_bg.r * 0.2f);
        card_bg.g = (unsigned char)(card_bg.g * 0.2f);
        card_bg.b = (unsigned char)(card_bg.b * 0.2f);
        card_bg.a = (unsigned char)(card_alphas[ct] * 255);

        if (hovered && unlocked)
        {
            card_bg.r = (unsigned char)(card_bg.r * 1.3f);
            card_bg.g = (unsigned char)(card_bg.g * 1.3f);
            card_bg.b = (unsigned char)(card_bg.b * 1.3f);
        }

        float offset_y = card_tweens[ct];
        Rectangle draw_rect = card_rect;
        draw_rect.y += offset_y;

        DrawRectangleRec(draw_rect, card_bg);

        if (!unlocked)
        {
            DrawRectangleRec(draw_rect, (Color){ 18, 19, 28, (unsigned char)(card_alphas[ct] * 230) });
            c = (Color){ 95, 95, 115, 255 };
        }

        Color border = selected[ct] ? c : (Color){ 60, 60, 80, (unsigned char)(card_alphas[ct] * 100) };
        DrawRectangleLinesEx(draw_rect, selected[ct] ? 2.0f : 1.0f, border);

        int text_x = snap_i(draw_rect.x + 8);
        Color name_c = unlocked ? RAYWHITE : (Color){ 125, 128, 145, 230 };
        name_c.a = (unsigned char)(card_alphas[ct] * 255);
        draw_text_box((Rectangle){ (float)text_x, draw_rect.y + 7.0f, draw_rect.width - 78.0f, 14.0f },
            class_name(ct), 10, 0, name_c, TEXT_ALIGN_LEFT);

        Color role_c = c;
        role_c.a = (unsigned char)(card_alphas[ct] * 200);
        draw_text_box((Rectangle){ (float)text_x, draw_rect.y + 23.0f, draw_rect.width - 78.0f, 14.0f },
            class_role(ct), 10, 0, role_c, TEXT_ALIGN_LEFT);

        DrawRectangle(text_x, snap_i(draw_rect.y + 38), snap_i(draw_rect.width - 18), 1, (Color){ 60, 60, 80, (unsigned char)(card_alphas[ct] * 200) });

        Color tag_c = unlocked ? (Color){ 160, 160, 180, (unsigned char)(card_alphas[ct] * 200) } : (Color){ 105, 108, 125, (unsigned char)(card_alphas[ct] * 200) };
        draw_text_box((Rectangle){ (float)text_x, draw_rect.y + 43.0f, draw_rect.width - 58.0f, 24.0f },
            unlocked ? class_description(ct) : "Locked in the Skill Tree.", 10, 0, tag_c, TEXT_ALIGN_LEFT);

        theme_draw_class_portrait(ct,
            snap_i(draw_rect.x + draw_rect.width - 28),
            snap_i(draw_rect.y + draw_rect.height - 28),
            13,
            unlocked);

        if (selected[ct])
            draw_text_box((Rectangle){ draw_rect.x + draw_rect.width - 75.0f, draw_rect.y + 9.0f, 66.0f, 14.0f },
                "SELECTED", 10, 0, (Color){ 220, 245, 230, 230 }, TEXT_ALIGN_RIGHT);
        else if (!unlocked)
            draw_text_box((Rectangle){ draw_rect.x + draw_rect.width - 68.0f, draw_rect.y + 9.0f, 58.0f, 14.0f },
                "LOCKED", 10, 0, (Color){ 145, 145, 165, 220 }, TEXT_ALIGN_RIGHT);

        Color feed = draft_class_color(ct);
        feed.a = 230;
        DrawRectangle(snap_i(draw_rect.x), snap_i(draw_rect.y + draw_rect.height - 3), snap_i(draw_rect.width), 3, feed);
    }
    EndScissorMode();

    float max_scroll = draft_max_scroll();
    if (max_scroll > 0.0f)
    {
        Rectangle track = { 616.0f, viewport.y + 4.0f, 4.0f, viewport.height - 8.0f };
        float thumb_h = track.height * (track.height / (track.height + max_scroll));
        if (thumb_h < 24.0f) thumb_h = 24.0f;
        float thumb_y = track.y + (track.height - thumb_h) * (draft_scroll_y / max_scroll);
        DrawRectangleRec(track, (Color){ 35, 38, 55, 170 });
        DrawRectangleRec((Rectangle){ track.x, thumb_y, track.width, thumb_h }, (Color){ 120, 128, 165, 220 });
    }

    if (hover_idx >= 0 && class_hint((ClassType)hover_idx)[0])
    {
        Rectangle hint = { 118.0f, 300.0f, 404.0f, 18.0f };
        Color c = draft_class_color((ClassType)hover_idx);
        DrawRectangleRec(hint, (Color){ 10, 11, 18, 235 });
        DrawRectangleLinesEx(hint, 1.0f, (Color){ c.r, c.g, c.b, 190 });
        draw_text_box((Rectangle){ hint.x + 6.0f, hint.y + 3.0f, hint.width - 12.0f, hint.height - 5.0f },
            class_hint((ClassType)hover_idx), 10, 0, (Color){ 215, 220, 240, 235 }, TEXT_ALIGN_LEFT);
    }

    if (selected_count > 0)
    {
        button_draw_9slice(&begin_btn);
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
        Rectangle btn_rect = draft_begin_rect();
        draw_9slice(g_assets.btn_standard, 6, 6, btn_rect, bg);
        Color txt = { 160, 160, 180, (unsigned char)(begin_btn_alpha * 100) };
        draw_text_box((Rectangle){ btn_rect.x + 6.0f, btn_rect.y + 5.0f, btn_rect.width - 12.0f, 12.0f },
            begin_btn.text, 10, 0, txt, TEXT_ALIGN_CENTER);
    }
    button_draw_9slice(&back_btn);

    Color sep = { 60, 60, 80, 120 };
    DrawRectangle(0, 300, VIRT_W, 1, sep);
    Color hint = { 100, 100, 130, 120 };
}
