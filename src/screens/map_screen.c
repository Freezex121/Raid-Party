#include "screens.h"
#include "game.h"
#include "systems/map.h"
#include "data/area_defs.h"
#include "data/encounter_defs.h"
#include "util/log.h"
#include "ui/relic_tray.h"
#include "ui/theme.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

static int hovered_node = -1;
static int last_floor = -1;
static float map_scroll_x = 0.0f;
static float map_scroll_y = 0.0f;

static float clampf_local(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static Rectangle map_content_bounds(MapState *map)
{
    if (!map || !map->nodes || map->node_count <= 0)
        return (Rectangle){ 0.0f, 52.0f, (float)VIRT_W, (float)(VIRT_H - 52) };

    int min_x = map->nodes[0].x;
    int min_y = map->nodes[0].y;
    int max_x = map->nodes[0].x;
    int max_y = map->nodes[0].y;
    for (int i = 1; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        if (n->x < min_x) min_x = n->x;
        if (n->y < min_y) min_y = n->y;
        if (n->x > max_x) max_x = n->x;
        if (n->y > max_y) max_y = n->y;
    }

    return (Rectangle){
        (float)(min_x - 54),
        (float)(min_y - 54),
        (float)(max_x - min_x + 108),
        (float)(max_y - min_y + 112)
    };
}

static void clamp_map_scroll(void)
{
    Rectangle bounds = map_content_bounds(&g_state.map);
    float max_x = bounds.width > VIRT_W ? bounds.width - VIRT_W : 0.0f;
    float max_y = bounds.height > (VIRT_H - 52) ? bounds.height - (VIRT_H - 52) : 0.0f;
    map_scroll_x = clampf_local(map_scroll_x, 0.0f, max_x);
    map_scroll_y = clampf_local(map_scroll_y, 0.0f, max_y);
}

static float map_center_offset_x(Rectangle bounds)
{
    return bounds.width < VIRT_W ? ((float)VIRT_W - bounds.width) * 0.5f : 0.0f;
}

static Vector2 node_screen_pos(MapNode *node)
{
    Rectangle bounds = map_content_bounds(&g_state.map);
    return (Vector2){
        (float)node->x - bounds.x - map_scroll_x + map_center_offset_x(bounds),
        (float)node->y - bounds.y - map_scroll_y + 52.0f
    };
}

void map_screen_update(void)
{
    if (g_state.map.floor != last_floor || g_state.map.node_count == 0)
    {
        const AreaDef *area = area_def(g_state.current_area);
        map_generate(&g_state.map, g_state.map.floor, area ? area->id : NULL);
        g_state.map.current_index = -1;
        last_floor = g_state.map.floor;
        map_scroll_x = 0.0f;
        map_scroll_y = 0.0f;
        LOG_I(CAT_SCREEN, "Map screen: floor %d, %d nodes", g_state.map.floor, g_state.map.node_count);
    }

    if (g_state.map.current_index >= 0)
    {
        int ci = g_state.map.current_index;
        NodeType t = g_state.map.nodes[ci].type;
        LOG_I(CAT_SCREEN, "Entering node %d type=%d", ci, t);

        g_state.encounter = NULL;
        g_state.encounter_is_elite = false;
        g_state.encounter_is_boss = false;

        if (t == NODE_COMBAT)
        {
            int idx = 0;
            for (int i = 0; i < g_state.map.node_count && i < ci; i++)
                if (g_state.map.nodes[i].type == NODE_COMBAT) idx++;
            g_state.encounter = encounter_for_floor(g_state.map.floor, idx);
        }
        else if (t == NODE_ELITE)
        {
            g_state.encounter = elite_for_floor(g_state.map.floor);
            g_state.encounter_is_elite = true;
        }
        else if (t == NODE_BOSS)
        {
            g_state.encounter = boss_for_floor(g_state.map.floor);
            g_state.encounter_is_boss = true;
        }
        else if (t == NODE_START)
        {
            g_state.map.nodes[ci].completed = true;
            g_state.map.current_index = -1;
            map_unlock_next(&g_state.map);
            return;
        }

        game_change_screen(t == NODE_REST ? SCREEN_REST :
                           t == NODE_SHOP ? SCREEN_SHOP :
                           t == NODE_EVENT ? SCREEN_EVENT :
                           SCREEN_RUN);
        return;
    }

    Vector2 mouse = GetMousePosition();
    hovered_node = -1;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            map_scroll_x -= wheel * 32.0f;
        else
            map_scroll_y -= wheel * 32.0f;
    }
    if (IsKeyDown(KEY_DOWN)) map_scroll_y += 3.0f;
    if (IsKeyDown(KEY_UP)) map_scroll_y -= 3.0f;
    if (IsKeyDown(KEY_RIGHT)) map_scroll_x += 3.0f;
    if (IsKeyDown(KEY_LEFT)) map_scroll_x -= 3.0f;
    clamp_map_scroll();

    for (int i = g_state.map.node_count - 1; i >= 0; i--)
    {
        MapNode *n = &g_state.map.nodes[i];
        if (!n->available || n->completed) continue;
        Vector2 pos = node_screen_pos(n);
        if (CheckCollisionPointRec(mouse, (Rectangle){ pos.x - 20.0f, pos.y - 20.0f, 40.0f, 40.0f }))
        {
            hovered_node = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                LOG_I(CAT_INPUT, "Clicked map node %d type=%d", i, n->type);
                n->completed = true;
                if (n->type == NODE_START)
                {
                    map_unlock_next(&g_state.map);
                }
                else
                {
                    g_state.map.current_index = i;
                }
            }
            break;
        }
    }
}

void map_screen_draw(void)
{
    theme_draw_background();

    const AreaDef *area = area_def(g_state.current_area);
    int floor_count = area_floor_count(g_state.current_area);
    int loaded_floors = map_loaded_floor_count_for_area(area ? area->id : NULL);
    if (loaded_floors > 0 && floor_count > loaded_floors)
        floor_count = loaded_floors;

    char title[96];
    snprintf(title, sizeof(title), "%s", area ? area->name : "Area");
    DrawText(title, (VIRT_W / 2) - MeasureText(title, 18) / 2, 14, 18, RAYWHITE);
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "Area %d  Floor %d/%d  Choose your route",
        g_state.current_area + 1,
        g_state.map.floor + 1,
        floor_count);
    DrawText(subtitle, (VIRT_W / 2) - MeasureText(subtitle, 10) / 2, 36, 10, (Color){ 150, 155, 180, 210 });
    relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){ 482.0f, 10.0f, 146.0f, 42.0f });

    MapState *map = &g_state.map;
    clamp_map_scroll();

    BeginScissorMode(0, 52, VIRT_W, VIRT_H - 52);

    for (int i = 0; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        Vector2 from = node_screen_pos(n);
        Color conn_col = n->completed ? (Color){ 125, 140, 165, 170 } : (Color){ 60, 64, 86, 90 };
        for (int c = 0; c < n->conn_count; c++)
        {
            int ni = n->conns[c];
            if (ni < 0 || ni >= map->node_count) continue;
            MapNode *next = &map->nodes[ni];
            Vector2 to = node_screen_pos(next);
            DrawLineEx(from,
                       to,
                       n->completed ? 2.0f : 1.0f,
                       conn_col);
        }
    }

    for (int i = 0; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        Vector2 pos = node_screen_pos(n);
        int sx = (int)pos.x;
        int sy = (int)pos.y;
        int r = (n->type == NODE_BOSS) ? 18 : 14;
        Color c = theme_node_color(n->type);
        const char *icon = theme_node_icon(n->type);
        const char *name = theme_node_name(n->type);

        if (!n->available && !n->completed)
        {
            DrawCircle(sx, sy, (float)r, (Color){ 28, 29, 42, 255 });
            DrawCircleLines(sx, sy, (float)r, (Color){ 58, 58, 78, 120 });
            int locked_size = n->type == NODE_BOSS ? 18 : 10;
            Color dim = c;
            dim.a = 128;
            DrawText(icon, sx - MeasureText(icon, locked_size) / 2, sy - locked_size / 2, locked_size, dim);
            DrawText(name, sx - MeasureText(name, 10) / 2, sy + r + 5, 10, dim);
            continue;
        }

        if (n->completed)
        {
            DrawCircle(sx, sy, (float)r, (Color){ 48, 52, 68, 255 });
            DrawCircle(sx, sy, r - 3, (Color){ 70, 200, 115, 150 });
            DrawText("OK", sx - MeasureText("OK", 10) / 2, sy - 3, 10, (Color){ 190, 255, 205, 230 });
        }
        else if (i == hovered_node)
        {
            float pulse = 1.0f + 0.07f * sinf((float)GetTime() * 5.5f);
            DrawCircle(sx, sy, (float)(r + 4) * pulse, (Color){ c.r, c.g, c.b, 55 });
            DrawCircle(sx, sy, r + 2, RAYWHITE);
            DrawCircle(sx, sy, (float)r, c);
        }
        else
        {
            DrawCircle(sx, sy, r + 3, (Color){ c.r, c.g, c.b, 35 });
            DrawCircle(sx, sy, (float)r, c);
            DrawCircle(sx, sy, r - 3, (Color){ 0, 0, 0, 60 });
        }

        int icon_size = n->type == NODE_BOSS ? 18 : 10;
        DrawText(icon, sx - MeasureText(icon, icon_size) / 2, sy - icon_size / 2, icon_size, RAYWHITE);
        DrawText(name, sx - MeasureText(name, 10) / 2, sy + r + 5, 10, c);
    }

    EndScissorMode();

    Rectangle bounds = map_content_bounds(map);
    if (bounds.height > VIRT_H - 52 || bounds.width > VIRT_W)
    {
        const char *hint = "Wheel scrolls map  |  Shift+wheel pans";
        DrawText(hint, VIRT_W / 2 - MeasureText(hint, 10) / 2, VIRT_H - 12, 10, (Color){ 150, 155, 180, 185 });
    }

    if (hovered_node >= 0 && map->nodes[hovered_node].available && !map->nodes[hovered_node].completed)
    {
        MapNode *n = &map->nodes[hovered_node];
        Rectangle tip = { (float)((VIRT_W / 2) - 70), (float)(VIRT_H - 38), 140.0f, 26.0f };
        DrawRectangleRec(tip, (Color){ 20, 21, 32, 230 });
        DrawRectangleLinesEx(tip, 1.0f, (Color){ 95, 100, 130, 220 });
        DrawText(theme_node_name(n->type), (VIRT_W / 2) - MeasureText(theme_node_name(n->type), 10) / 2, (int)tip.y + 4, 10, theme_node_color(n->type));
        DrawText("Click to travel", (VIRT_W / 2) - MeasureText("Click to travel", 10) / 2, (int)tip.y + 16, 10, (Color){ 175, 178, 205, 220 });
    }
}




