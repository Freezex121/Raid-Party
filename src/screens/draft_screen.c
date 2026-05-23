#include "screens.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "game.h"
#include "util/tween.h"
#include "util/text.h"
#include "util/log.h"
#include "constants.h"
#include "raylib.h"
#include <string.h>

#define CLASS_COUNT 6

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
};

static Color class_color(int i) { return CLASS_COLOR(class_info[i].cr, class_info[i].cg, class_info[i].cb); }

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
    const float card_w = 120.0f;
    const float card_h = 70.0f;
    const float gap_x = 10.0f;
    const float gap_y = 10.0f;
    int col = index % 3;
    int row = index / 3;
    float start_x = (VIRT_W - (3.0f * card_w + 2.0f * gap_x)) * 0.5f;
    return (Rectangle){ start_x + col * (card_w + gap_x), 45.0f + row * (card_h + gap_y), card_w, card_h };
}

static Rectangle draft_begin_rect(void)
{
    return (Rectangle){ (float)(VIRT_W / 2 - 72), 216.0f, 144.0f, 16.0f };
}

static void card_click_cb(int index)
{
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
        if (max_selected > CLASS_COUNT) max_selected = CLASS_COUNT;

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
        tween_create(&title_y, 8.0f, 0.5f, EASE_OUT_BACK);

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
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            card_click_cb(i);
    }

    if (selected_count == max_selected)
    {
        begin_btn.text = "BEGIN RUN";
        begin_btn_alpha = 1.0f;
        button_update(&begin_btn);

        if (begin_btn.pressed_this_frame)
        {
            int sel_idx = 0;
            for (int i = 0; i < CLASS_COUNT && sel_idx < max_selected; i++)
            {
                if (selected[i])
                    g_state.selected_classes[sel_idx++] = i;
            }
            LOG_I(CAT_DRAFT, "BEGIN RUN pressed. selected_count=%d, max_selected=%d", g_state.selected_count, max_selected);
            for (int i = 0; i < g_state.selected_count; i++)
                LOG_I(CAT_DRAFT, "  selected_classes[%d] = %d", i, g_state.selected_classes[i]);
            memset(&g_state.map, 0, sizeof(g_state.map));
            g_state.map.floor = 0;
            g_state.gold = 0;
            g_state.encounter = NULL;
            g_state.encounter_is_elite = false;
            g_state.encounter_is_boss = false;
            g_state.result_floor = 1;
            g_state.result_bosses_defeated = 0;
            g_state.run_won = false;
            g_state.result_reason[0] = '\0';
            party_create(&g_state.run_party, g_state.selected_classes, g_state.selected_count);
            g_state.run_party_active = true;
            deck_init_from_classes(&g_state.run_deck, g_state.selected_classes, g_state.selected_count);
            LOG_I(CAT_SCREEN, "Run deck built: %d cards", g_state.run_deck.card_count);
            initialized = false;
            game_change_screen(SCREEN_MAP);
        }
    }
    else
    {
        begin_btn.text = selected_count > 0
            ? TextFormat("SELECT %d TO BEGIN", max_selected)
            : "ASSEMBLE YOUR PARTY";
        begin_btn_alpha = 0.4f;
    }
}

void draft_screen_draw(void)
{
    theme_draw_background();

    DrawText("ASSEMBLE YOUR PARTY", VIRT_W/2 - MeasureText("ASSEMBLE YOUR PARTY", 12) / 2, (int)title_y, 12, RAYWHITE);

    char counter[64];
    snprintf(counter, sizeof(counter), "%d / %d selected", selected_count, max_selected);
    Color counter_color = selected_count == max_selected ? (Color){ 70, 220, 120, 255 } : (Color){ 160, 160, 180, 255 };
    DrawText(counter, VIRT_W/2 - MeasureText(counter, 6) / 2, (int)title_y + 16, 6, counter_color);

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        Color c = class_color(i);
        c.a = (unsigned char)(card_alphas[i] * 255);

        Rectangle card_rect = draft_card_rect_for(i);

        Vector2 mouse = GetMousePosition();
        bool hovered = CheckCollisionPointRec(mouse, card_rect);

        Color card_bg = c;
        card_bg.r = (unsigned char)(card_bg.r * 0.2f);
        card_bg.g = (unsigned char)(card_bg.g * 0.2f);
        card_bg.b = (unsigned char)(card_bg.b * 0.2f);
        card_bg.a = (unsigned char)(card_alphas[i] * 255);

        if (hovered)
        {
            card_bg.r = (unsigned char)(card_bg.r * 1.3f);
            card_bg.g = (unsigned char)(card_bg.g * 1.3f);
            card_bg.b = (unsigned char)(card_bg.b * 1.3f);
        }

        float offset_y = card_tweens[i];
        Rectangle draw_rect = card_rect;
        draw_rect.y += offset_y;

        DrawRectangleRec(draw_rect, card_bg);

        Color border = selected[i] ? c : (Color){ 60, 60, 80, (unsigned char)(card_alphas[i] * 100) };
        DrawRectangleLinesEx(draw_rect, selected[i] ? 2.0f : 1.0f, border);

        int text_x = (int)(draw_rect.x + 8);
        Color name_c = RAYWHITE;
        name_c.a = (unsigned char)(card_alphas[i] * 255);
        DrawText(class_info[i].name, text_x, (int)(draw_rect.y + 6), 10, name_c);

        Color role_c = c;
        role_c.a = (unsigned char)(card_alphas[i] * 200);
        DrawText(class_info[i].role, text_x, (int)(draw_rect.y + 18), 6, role_c);

        DrawRectangle(text_x, (int)(draw_rect.y + 28), (int)draw_rect.width - 16, 1, (Color){ 60, 60, 80, (unsigned char)(card_alphas[i] * 200) });

        Color tag_c = { 160, 160, 180, (unsigned char)(card_alphas[i] * 200) };
        draw_text_wrapped(class_info[i].tagline, text_x, (int)(draw_rect.y + 34), (int)(draw_rect.width - 42), 8, 1, tag_c);

        theme_draw_class_portrait((ClassType)i,
            (int)(draw_rect.x + draw_rect.width - 18),
            (int)(draw_rect.y + draw_rect.height - 18),
            8,
            true);

        if (selected[i])
            DrawText("SELECTED", (int)(draw_rect.x + draw_rect.width - 60), (int)(draw_rect.y + 7), 5, (Color){ 220, 245, 230, 230 });
    }

    if (selected_count == max_selected)
    {
        button_draw(&begin_btn);
    }
    else
    {
        Color bg = { 46, 117, 182, (unsigned char)(begin_btn_alpha * 100) };
        Color border = { 100, 100, 130, (unsigned char)(begin_btn_alpha * 80) };
        Rectangle btn_rect = draft_begin_rect();
        DrawRectangleRec(btn_rect, bg);
        DrawRectangleLinesEx(btn_rect, 1.0f, border);
        Color txt = { 160, 160, 180, (unsigned char)(begin_btn_alpha * 100) };
        int tw = MeasureText(begin_btn.text, 7);
        DrawText(begin_btn.text, VIRT_W/2 - tw / 2, (int)btn_rect.y + 5, 7, txt);
    }

    Color sep = { 60, 60, 80, 120 };
    DrawRectangle(0, 207, VIRT_W, 1, sep);
    Color hint = { 100, 100, 130, 120 };
}



