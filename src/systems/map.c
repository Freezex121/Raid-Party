#include "map.h"
#include "constants.h"
#include "util/log.h"
#include <string.h>

typedef struct {
    NodeType type;
    int row, col;
    int conns[3];
    int conn_count;
} NodeDef;

static NodeDef floor_layouts[MAX_FLOORS][MAX_NODES_PER_FLOOR];
static int floor_node_counts[MAX_FLOORS];

static void init_layouts(void)
{
    static bool initialized = false;
    if (initialized) return;

    // Floor 1 — 7 nodes, 5 rows
    NodeDef f1[] = {
        { NODE_START,  0, 0, {1, 2}, 2 },    // 0: Start → 1, 2
        { NODE_COMBAT, 1, 0, {3},    1 },    // 1: Combat A → 3
        { NODE_COMBAT, 1, 1, {4},    1 },    // 2: Combat B → 4
        { NODE_COMBAT, 2, 0, {5},    1 },    // 3: Combat → Rest
        { NODE_REST,   2, 1, {5},    1 },    // 4: Rest → Rest
        { NODE_ELITE,  3, 0, {6},    1 },    // 5: Elite → Boss
        { NODE_BOSS,   4, 0, {},     0 },    // 6: Boss
    };
    memcpy(floor_layouts[0], f1, sizeof(f1));
    floor_node_counts[0] = 7;

    // Floor 2 — 10 nodes, 6 rows
    NodeDef f2[] = {
        { NODE_START,  0, 0, {1, 2}, 2 },
        { NODE_COMBAT, 1, 0, {3, 4}, 2 },
        { NODE_COMBAT, 1, 1, {4},    1 },
        { NODE_ELITE,  2, 0, {5},    1 },
        { NODE_REST,   2, 1, {6},    1 },
        { NODE_COMBAT, 3, 0, {7},    1 },
        { NODE_SHOP,   3, 1, {7},    1 },
        { NODE_ELITE,  4, 0, {9},    1 },
        { NODE_COMBAT, 4, 1, {9},    1 },
        { NODE_BOSS,   5, 0, {},     0 },
    };
    memcpy(floor_layouts[1], f2, sizeof(f2));
    floor_node_counts[1] = 10;

    // Floor 3 — 12 nodes, 7 rows
    NodeDef f3[] = {
        { NODE_START,  0, 0, {1, 2}, 2 },
        { NODE_COMBAT, 1, 0, {3},    1 },
        { NODE_ELITE,  1, 1, {3, 4}, 2 },
        { NODE_REST,   2, 0, {5, 6}, 2 },
        { NODE_SHOP,   2, 1, {6},    1 },
        { NODE_COMBAT, 3, 0, {7},    1 },
        { NODE_ELITE,  3, 1, {8},    1 },
        { NODE_COMBAT, 4, 0, {9},    1 },
        { NODE_REST,   4, 1, {10},   1 },
        { NODE_ELITE,  5, 0, {11},   1 },
        { NODE_COMBAT, 5, 1, {11},   1 },
        { NODE_BOSS,   6, 0, {},     0 },
    };
    memcpy(floor_layouts[2], f3, sizeof(f3));
    floor_node_counts[2] = 12;

    initialized = true;
}

static void calc_positions(MapNode *nodes, int count, int floor)
{
    int rows = 0;
    for (int i = 0; i < count; i++)
        if (nodes[i].row > rows) rows = nodes[i].row;
    rows++;

    int top_y = 72;
    int bottom_y = 300;
    int row_height = (rows > 1) ? (bottom_y - top_y) / (rows - 1) : 0;

    for (int i = 0; i < count; i++)
    {
        MapNode *n = &nodes[i];

        // Count nodes in same row
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
        n->x = spacing * (col_index + 1);
        n->y = top_y + n->row * row_height;
    }
}

void map_generate(MapState *map, int floor)
{
    init_layouts();
    memset(map, 0, sizeof(MapState));

    map->floor = floor;
    int count = floor_node_counts[floor];
    map->node_count = count;

    NodeDef *defs = floor_layouts[floor];

    for (int i = 0; i < count; i++)
    {
        map->nodes[i].type = defs[i].type;
        map->nodes[i].row = defs[i].row;
        map->nodes[i].col = defs[i].col;
        map->nodes[i].conn_count = defs[i].conn_count;
        for (int c = 0; c < defs[i].conn_count; c++)
            map->nodes[i].conns[c] = defs[i].conns[c];
        map->nodes[i].completed = false;
        map->nodes[i].available = false;
        if (defs[i].type == NODE_BOSS)
            map->boss_index = i;
    }

    calc_positions(map->nodes, count, floor);

    int start = map_find_start(map);
    if (start >= 0) map->nodes[start].available = true;

    map->current_index = -1;

    LOG_I(CAT_SCREEN, "Map generated: floor=%d, nodes=%d, boss=%d", floor, count, map->boss_index);
}

void map_unlock_next(MapState *map)
{
    for (int i = 0; i < map->node_count; i++)
    {
        if (!map->nodes[i].completed) continue;

        for (int c = 0; c < map->nodes[i].conn_count; c++)
        {
            int next = map->nodes[i].conns[c];
            if (next >= 0 && next < map->node_count)
                map->nodes[next].available = true;
        }
    }
}

int map_find_start(MapState *map)
{
    for (int i = 0; i < map->node_count; i++)
        if (map->nodes[i].type == NODE_START) return i;
    return 0;
}


