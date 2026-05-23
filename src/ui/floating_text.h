#ifndef FLOATING_TEXT_H
#define FLOATING_TEXT_H

#include "raylib.h"
#include "game_text.h"
#include <stdbool.h>

#define MAX_FLOATING 32

typedef struct {
    float x, y;
    float start_y;
    char text[24];
    Color color;
    float life, max_life;
    int font_size;
    bool active;
    bool shake;
} FloatingText;

void ft_spawn(float x, float y, const char *text, int size, Color color);
void ft_spawn_shake(float x, float y, const char *text, int size, Color color);
void ft_spawn_gold(int amount);
void vfx_spawn_burst(float x, float y, Color color, int count);
void ft_update(float dt);
void ft_draw(void);
void ft_clear_all(void);

extern float g_shake_x, g_shake_y;
extern float g_shake_intensity;
extern float g_shake_timer;

#endif
