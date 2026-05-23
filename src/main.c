#include "raylib.h"
#include "game.h"
#include "util/tween.h"
#include "util/log.h"
#include "ui/floating_text.h"
#include "assets.h"
#include "screens/screens.h"
#include "constants.h"
#include <stdlib.h>
#include <time.h>

int main(void)
{
    srand((unsigned int)time(NULL));
    log_init();

    InitWindow(SCREEN_W, SCREEN_H, "Raid Party - devlog");
    InitAudioDevice();
    SetTargetFPS(60);
    SetExitKey(0); // Disable default exit key (ESC)
    SetMouseScale(1.0f / (float)SCALE, 1.0f / (float)SCALE);

    RenderTexture2D target = LoadRenderTexture(VIRT_W, VIRT_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    assets_load();
    game_init();
    LOG_I(CAT_SCREEN, "Game initialized. Starting title screen.");

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        GameScreen prev = g_state.screen;

        game_update_transition(dt);

        if (game_transition_allows_update())
        {
            switch (g_state.screen)
            {
                case SCREEN_TITLE: title_screen_update(); break;
                case SCREEN_DRAFT: draft_screen_update(); break;
                case SCREEN_MAP:   map_screen_update();   break;
                case SCREEN_RUN:   run_screen_update();   break;
                case SCREEN_REST:  rest_screen_update();  break;
                case SCREEN_SHOP:  shop_screen_update();  break;
                case SCREEN_EVENT: event_screen_update(); break;
                case SCREEN_REWARD: reward_screen_update(); break;
                case SCREEN_RELIC_REWARD: relic_reward_screen_update(); break;
                case SCREEN_DISCARD: discard_screen_update(); break;
                case SCREEN_GAME_OVER: game_over_screen_update(); break;
                default: break;
            }
        }

        if (prev != g_state.screen)
            LOG_I(CAT_SCREEN, "Screen transition: %d -> %d", prev, g_state.screen);

        tween_update(dt);
        assets_update_audio();

        BeginTextureMode(target);
        ClearBackground((Color){ 14, 14, 26, 255 });

        switch (g_state.screen)
        {
            case SCREEN_TITLE: title_screen_draw(); break;
            case SCREEN_DRAFT: draft_screen_draw(); break;
            case SCREEN_MAP:   map_screen_draw();   break;
            case SCREEN_RUN:   run_screen_draw();   break;
            case SCREEN_REST:  rest_screen_draw();  break;
            case SCREEN_SHOP:  shop_screen_draw();  break;
            case SCREEN_EVENT: event_screen_draw(); break;
            case SCREEN_REWARD: reward_screen_draw(); break;
            case SCREEN_RELIC_REWARD: relic_reward_screen_draw(); break;
            case SCREEN_DISCARD: discard_screen_draw(); break;
            case SCREEN_GAME_OVER: game_over_screen_draw(); break;
            default: break;
        }

        ft_draw();
        game_draw_transition();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(
            target.texture,
            (Rectangle){ 0.0f, 0.0f, (float)VIRT_W, (float)-VIRT_H },
            (Rectangle){ 0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H },
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    assets_unload();
    if (IsAudioDeviceReady())
        CloseAudioDevice();
    CloseWindow();

    LOG_I(CAT_SCREEN, "Shutting down.");
    log_close();

    return 0;
}
