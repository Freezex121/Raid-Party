#include "floating_text.h"
#include "constants.h"
#include "util/math_utils.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static FloatingText pool[MAX_FLOATING];

#define MAX_VFX 96
typedef struct {
    float x, y;
    float vx, vy;
    float life, max_life;
    float size;
    Color color;
    bool active;
} VfxParticle;

static VfxParticle vfx_pool[MAX_VFX];

float g_shake_x = 0, g_shake_y = 0;
float g_shake_intensity = 0;
float g_shake_timer = 0;

void shake_trigger(float intensity)
{
    g_shake_intensity = intensity;
    g_shake_timer = 0.15f;
}

static void shake_update(float dt)
{
    if (g_shake_timer > 0)
    {
        g_shake_timer -= dt;
        g_shake_x = ((float)(rand() % 200) / 100.0f - 1.0f) * g_shake_intensity;
        g_shake_y = ((float)(rand() % 200) / 100.0f - 1.0f) * g_shake_intensity;
        if (g_shake_timer <= 0)
        {
            g_shake_x = 0; g_shake_y = 0;
            g_shake_intensity = 0;
        }
    }
}

static int find_slot(void)
{
    int oldest = -1;
    float oldest_life = 999;
    for (int i = 0; i < MAX_FLOATING; i++)
    {
        if (!pool[i].active) return i;
        if (pool[i].life < oldest_life) { oldest_life = pool[i].life; oldest = i; }
    }
    return oldest;
}

void ft_spawn(float x, float y, const char *text, int size, Color color)
{
    int i = find_slot();
    if (i < 0) return;
    pool[i].x = x;
    pool[i].y = y;
    pool[i].start_y = y;
    pool[i].life = 1.2f;
    pool[i].max_life = 1.2f;
    pool[i].color = color;
    pool[i].font_size = size;
    pool[i].active = true;
    pool[i].shake = false;
    strncpy(pool[i].text, text, sizeof(pool[i].text) - 1);
    pool[i].text[sizeof(pool[i].text) - 1] = '\0';
}

void ft_spawn_shake(float x, float y, const char *text, int size, Color color)
{
    ft_spawn(x, y, text, size, color);
    shake_trigger(6.0f);
}

void ft_spawn_gold(int amount)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "+%dG", amount);
    ft_spawn((float)((VIRT_W / 2) - 12), 58.0f, buf, 10, (Color){ 255, 220, 60, 255 });
}

void vfx_spawn_burst(float x, float y, Color color, int count)
{
    if (count < 1) count = 1;
    if (count > 24) count = 24;

    for (int p = 0; p < count; p++)
    {
        int slot = -1;
        for (int i = 0; i < MAX_VFX; i++)
        {
            if (!vfx_pool[i].active)
            {
                slot = i;
                break;
            }
        }
        if (slot < 0) slot = p % MAX_VFX;

        float angle = ((float)(rand() % 628) / 100.0f);
        float speed = 45.0f + (float)(rand() % 90);
        vfx_pool[slot].x = x;
        vfx_pool[slot].y = y;
        vfx_pool[slot].vx = cosf(angle) * speed;
        vfx_pool[slot].vy = sinf(angle) * speed - 35.0f;
        vfx_pool[slot].life = 0.45f + (float)(rand() % 25) / 100.0f;
        vfx_pool[slot].max_life = vfx_pool[slot].life;
        vfx_pool[slot].size = 2.0f + (float)(rand() % 4);
        vfx_pool[slot].color = color;
        vfx_pool[slot].active = true;
    }
}

void ft_update(float dt)
{
    shake_update(dt);

    for (int i = 0; i < MAX_FLOATING; i++)
    {
        if (!pool[i].active) continue;
        pool[i].life -= dt;
        float t = 1.0f - pool[i].life / pool[i].max_life;
        pool[i].y = pool[i].start_y - t * 80.0f;
        if (pool[i].life <= 0)
            pool[i].active = false;
    }

    for (int i = 0; i < MAX_VFX; i++)
    {
        if (!vfx_pool[i].active) continue;
        vfx_pool[i].life -= dt;
        vfx_pool[i].x += vfx_pool[i].vx * dt;
        vfx_pool[i].y += vfx_pool[i].vy * dt;
        vfx_pool[i].vy += 210.0f * dt;
        if (vfx_pool[i].life <= 0.0f)
            vfx_pool[i].active = false;
    }
}

void ft_draw(void)
{
    for (int i = 0; i < MAX_FLOATING; i++)
    {
        if (!pool[i].active) continue;
        float t = 1.0f - pool[i].life / pool[i].max_life;
        float alpha = 1.0f - t * t;
        float scale = 1.0f + t * 0.1f;

        Color c = pool[i].color;
        c.a = (unsigned char)(alpha * 255);

        int fs = snap_i(pool[i].font_size * scale);
        DrawText(pool[i].text, snap_i(pool[i].x), snap_i(pool[i].y), fs, c);
    }

    for (int i = 0; i < MAX_VFX; i++)
    {
        if (!vfx_pool[i].active) continue;
        float t = 1.0f - vfx_pool[i].life / vfx_pool[i].max_life;
        Color c = vfx_pool[i].color;
        c.a = (unsigned char)((1.0f - t) * 220.0f);
        DrawCircle((int)vfx_pool[i].x, (int)vfx_pool[i].y, vfx_pool[i].size * (1.0f - t * 0.45f), c);
    }
}

void ft_clear_all(void)
{
    for (int i = 0; i < MAX_FLOATING; i++)
        pool[i].active = false;
    for (int i = 0; i < MAX_VFX; i++)
        vfx_pool[i].active = false;
}


