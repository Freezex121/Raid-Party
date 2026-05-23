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

void map_screen_update(void)
{
    if (g_state.map.floor != last_floor || g_state.map.node_count == 0)
    {
        map_generate(&g_state.map, g_state.map.floor);
        g_state.map.current_index = -1;
        last_floor = g_state.map.floor;
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

    for (int i = g_state.map.node_count - 1; i >= 0; i--)
    {
        MapNode *n = &g_state.map.nodes[i];
        if (!n->available || n->completed) continue;
        if (CheckCollisionPointRec(mouse, (Rectangle){ (float)(n->x - 20), (float)(n->y - 20), 40, 40 }))
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

    char title[96];
    snprintf(title, sizeof(title), "%s", area ? area->name : "Area");
    DrawText(title, (VIRT_W / 2) - MeasureText(title, 16) / 2, 14, 16, RAYWHITE);
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "Area %d  Floor %d/%d  Choose your route",
        g_state.current_area + 1,
        g_state.map.floor + 1,
        floor_count);
    DrawText(subtitle, (VIRT_W / 2) - MeasureText(subtitle, 8) / 2, 36, 8, (Color){ 150, 155, 180, 210 });
    relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){ 482.0f, 10.0f, 146.0f, 42.0f });

    MapState *map = &g_state.map;

    for (int i = 0; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        Color conn_col = n->completed ? (Color){ 125, 140, 165, 170 } : (Color){ 60, 64, 86, 90 };
        for (int c = 0; c < n->conn_count; c++)
        {
            int ni = n->conns[c];
            if (ni < 0 || ni >= map->node_count) continue;
            MapNode *next = &map->nodes[ni];
            DrawLineEx((Vector2){ (float)n->x, (float)n->y },
                       (Vector2){ (float)next->x, (float)next->y },
                       n->completed ? 2.0f : 1.0f,
                       conn_col);
        }
    }

    for (int i = 0; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        int r = (n->type == NODE_BOSS) ? 18 : 14;
        Color c = theme_node_color(n->type);
        const char *icon = theme_node_icon(n->type);
        const char *name = theme_node_name(n->type);

        if (!n->available && !n->completed)
        {
            DrawCircle(n->x, n->y, (float)r, (Color){ 28, 29, 42, 255 });
            DrawCircleLines(n->x, n->y, (float)r, (Color){ 58, 58, 78, 120 });
            int locked_size = n->type == NODE_BOSS ? 11 : 9;
            Color dim = c;
            dim.a = 128;
            DrawText(icon, n->x - MeasureText(icon, locked_size) / 2, n->y - locked_size / 2, locked_size, dim);
            DrawText(name, n->x - MeasureText(name, 6) / 2, n->y + r + 5, 6, dim);
            continue;
        }

        if (n->completed)
        {
            DrawCircle(n->x, n->y, (float)r, (Color){ 48, 52, 68, 255 });
            DrawCircle(n->x, n->y, r - 3, (Color){ 70, 200, 115, 150 });
            DrawText("OK", n->x - MeasureText("OK", 5) / 2, n->y - 3, 5, (Color){ 190, 255, 205, 230 });
        }
        else if (i == hovered_node)
        {
            float pulse = 1.0f + 0.07f * sinf((float)GetTime() * 5.5f);
            DrawCircle(n->x, n->y, (float)(r + 4) * pulse, (Color){ c.r, c.g, c.b, 55 });
            DrawCircle(n->x, n->y, r + 2, RAYWHITE);
            DrawCircle(n->x, n->y, (float)r, c);
        }
        else
        {
            DrawCircle(n->x, n->y, r + 3, (Color){ c.r, c.g, c.b, 35 });
            DrawCircle(n->x, n->y, (float)r, c);
            DrawCircle(n->x, n->y, r - 3, (Color){ 0, 0, 0, 60 });
        }

        int icon_size = n->type == NODE_BOSS ? 11 : 9;
        DrawText(icon, n->x - MeasureText(icon, icon_size) / 2, n->y - icon_size / 2, icon_size, RAYWHITE);
        DrawText(name, n->x - MeasureText(name, 6) / 2, n->y + r + 5, 6, c);
    }

    if (hovered_node >= 0 && map->nodes[hovered_node].available && !map->nodes[hovered_node].completed)
    {
        MapNode *n = &map->nodes[hovered_node];
        Rectangle tip = { (float)((VIRT_W / 2) - 70), (float)(VIRT_H - 38), 140.0f, 26.0f };
        DrawRectangleRec(tip, (Color){ 20, 21, 32, 230 });
        DrawRectangleLinesEx(tip, 1.0f, (Color){ 95, 100, 130, 220 });
        DrawText(theme_node_name(n->type), (VIRT_W / 2) - MeasureText(theme_node_name(n->type), 8) / 2, (int)tip.y + 4, 8, theme_node_color(n->type));
        DrawText("Click to travel", (VIRT_W / 2) - MeasureText("Click to travel", 6) / 2, (int)tip.y + 16, 6, (Color){ 175, 178, 205, 220 });
    }
}




