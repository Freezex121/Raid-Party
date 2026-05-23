#include "screens.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "game.h"
#include "util/tween.h"
#include "constants.h"
#include "raylib.h"

static Button start_btn;
static float title_y = -24.0f;
static float subtitle_alpha = 0.0f;
static float button_y = 0.0f;
static int title_tween, subtitle_tween, btn_tween;

void title_screen_update(void)
{
    static bool initialized = false;
    if (!initialized)
    {
        start_btn = button_create(
            (Rectangle){ (float)(VIRT_W / 2 - 48), button_y, 96, 16 },
            "START RUN",
            (Color){ 46, 117, 182, 255 },
            (Color){ 80, 160, 230, 255 },
            WHITE
        );

        title_tween = tween_create(&title_y, 34.0f, 0.6f, EASE_OUT_BACK);
        subtitle_tween = tween_create(&subtitle_alpha, 1.0f, 0.5f, EASE_OUT_QUAD);
        btn_tween = tween_create(&button_y, 182.0f, 0.6f, EASE_OUT_ELASTIC);

        initialized = true;
    }

    start_btn.bounds.y = button_y;
    button_update(&start_btn);

    if (start_btn.pressed_this_frame)
    {
        initialized = false;
        g_state.selected_count = 0;
        game_change_screen(SCREEN_DRAFT);
    }
}

void title_screen_draw(void)
{
    theme_draw_background();

    for (int i = 0; i < 5; i++)
    {
        int x = VIRT_W / 2 - 92 + i * 46;
        int y = 118 + (i % 2) * 5;
        theme_draw_class_portrait((ClassType)(i % CLASS_COUNT), x, y, 10, true);
    }

    DrawText("RAID PARTY", VIRT_W/2 - MeasureText("RAID PARTY", 20) / 2, (int)title_y, 20, RAYWHITE);

    Color subtitle_color = { 180, 180, 200, (unsigned char)(subtitle_alpha * 180) };
    DrawText("A deck-building roguelite MMO", VIRT_W/2 - MeasureText("A deck-building roguelite MMO", 7) / 2, (int)(title_y + 25), 7, subtitle_color);

    button_draw(&start_btn);

    Color credit_color = { 100, 100, 120, 180 };
    DrawText("devlog v0.1", VIRT_W/2 - MeasureText("devlog v0.1", 5) / 2, VIRT_H - 14, 5, credit_color);
}



