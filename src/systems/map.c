#include "map.h"
#include "map_gen.h"
#include "data/area_defs.h"
#include "constants.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

// ── Jitter ─────────────────────────────────────────────────────

#define JITTER_RANGE 0

static int jitter(void)
{
    return (rand() % (JITTER_RANGE * 2 + 1)) - JITTER_RANGE;
}

// ── Fallback positioning ───────────────────────────────────────

static void calc_fallback_positions(MapNode *nodes, int count)
{
    int max_row = 0;
    for (int i = 0; i < count; i++)
        if (nodes[i].row > max_row) max_row = nodes[i].row;

    const int top_y = 76;
    const int row_height = 62;

    for (int i = 0; i < count; i++)
    {
        MapNode *n = &nodes[i];
        int same_row = 0;
        for (int j = 0; j < count; j++)
            if (nodes[j].row == n->row) same_row++;

        int col_index = 0;
        for (int j = 0; j < count; j++)
        {
            if (nodes[j].row == n->row)
            {
                if (j == i) break;
                col_index++;
            }
        }

        int spacing = VIRT_W / (same_row + 1);
        n->x = spacing * (col_index + 1) + jitter();
        n->y = top_y + n->row * row_height + jitter();
    }

    (void)max_row;
}

// ── Connection helpers ─────────────────────────────────────────

static bool connection_allows_forward(const MapNode *nodes, int count, int from, int to)
{
    if (!nodes || from < 0 || from >= count || to < 0 || to >= count)
        return false;
    return nodes[to].y >= nodes[from].y;
}

static void remove_backward_connections(MapNode *nodes, int count)
{
    if (!nodes) return;
    for (int i = 0; i < count; i++)
    {
        MapNode *node = &nodes[i];
        int write = 0;
        for (int c = 0; c < node->conn_count; c++)
        {
            int next = node->conns[c];
            if (connection_allows_forward(nodes, count, i, next))
                node->conns[write++] = next;
        }
        node->conn_count = write;
    }
}

// ── Public API ─────────────────────────────────────────────────

void map_clear(MapState *map)
{
    if (!map) return;
    if (map->nodes)
    {
        for (int i = 0; i < map->node_count; i++)
        {
            free(map->nodes[i].conns);
            free(map->nodes[i].event_id);
        }
        free(map->nodes);
    }
    memset(map, 0, sizeof(*map));
    map->current_index = -1;
    map->boss_index = -1;
}

void map_generate(MapState *map, int floor, const char *area_id)
{
    if (!map) return;
    map_clear(map);

    if (floor < 0) floor = 0;

    map->floor = floor;
    map->current_index = -1;
    map->boss_index = -1;

    if (!map_generate_procedural(map, floor, area_id))
    {
        LOG_E(CAT_SCREEN, "Map generation failed for area=%s floor=%d",
              area_id ? area_id : "default", floor);
        return;
    }

    calc_fallback_positions(map->nodes, map->node_count);
    remove_backward_connections(map->nodes, map->node_count);

    int start = map_find_start(map);
    if (start >= 0 && start < map->node_count)
        map->nodes[start].available = true;
}

void map_unlock_next(MapState *map)
{
    if (!map || !map->nodes) return;

    int deepest_completed_y = -2147483647;
    for (int i = 0; i < map->node_count; i++)
    {
        if (map->nodes[i].completed && map->nodes[i].y > deepest_completed_y)
            deepest_completed_y = map->nodes[i].y;
    }

    if (deepest_completed_y <= -2147483647)
        return;

    for (int i = 0; i < map->node_count; i++)
        if (!map->nodes[i].completed)
            map->nodes[i].available = false;

    for (int i = 0; i < map->node_count; i++)
    {
        if (!map->nodes[i].completed || map->nodes[i].y != deepest_completed_y) continue;

        for (int c = 0; c < map->nodes[i].conn_count; c++)
        {
            int next = map->nodes[i].conns[c];
            if (connection_allows_forward(map->nodes, map->node_count, i, next) &&
                !map->nodes[next].completed)
                map->nodes[next].available = true;
        }
    }
}

int map_find_start(MapState *map)
{
    if (!map || !map->nodes) return -1;
    for (int i = 0; i < map->node_count; i++)
        if (map->nodes[i].type == NODE_START) return i;
    return map->node_count > 0 ? 0 : -1;
}
