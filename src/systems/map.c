#include "map.h"
#include "constants.h"
#include "util/json.h"
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

static const JsonValue *field(const JsonValue *object, const char *key)
{
    return json_object_get(object, key);
}

static NodeType parse_node_type(const char *text)
{
    if (text && strcmp(text, "start") == 0) return NODE_START;
    if (text && strcmp(text, "combat") == 0) return NODE_COMBAT;
    if (text && strcmp(text, "elite") == 0) return NODE_ELITE;
    if (text && strcmp(text, "rest") == 0) return NODE_REST;
    if (text && strcmp(text, "shop") == 0) return NODE_SHOP;
    if (text && strcmp(text, "event") == 0) return NODE_EVENT;
    if (text && strcmp(text, "boss") == 0) return NODE_BOSS;
    return NODE_COMBAT;
}

bool map_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *floors = field(root, "floors");
    if (!floors || floors->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: floors must be an array", path);
        json_free(root);
        return false;
    }

    memset(floor_layouts, 0, sizeof(floor_layouts));
    memset(floor_node_counts, 0, sizeof(floor_node_counts));

    int loaded_floors = 0;
    int floor_count = json_array_count(floors);
    for (int f = 0; f < floor_count; f++)
    {
        const JsonValue *floor_obj = json_array_get(floors, f);
        if (!floor_obj || floor_obj->type != JSON_OBJECT) continue;

        int floor_index = json_int(field(floor_obj, "floor"), f + 1) - 1;
        if (floor_index < 0 || floor_index >= MAX_FLOORS) continue;

        const JsonValue *nodes = field(floor_obj, "nodes");
        int node_count = json_array_count(nodes);
        if (node_count > MAX_NODES_PER_FLOOR)
            node_count = MAX_NODES_PER_FLOOR;

        for (int n = 0; n < node_count; n++)
        {
            const JsonValue *node = json_array_get(nodes, n);
            if (!node || node->type != JSON_OBJECT) continue;

            NodeDef *out = &floor_layouts[floor_index][floor_node_counts[floor_index]++];
            out->type = parse_node_type(json_string(field(node, "type"), "combat"));
            out->row = json_int(field(node, "row"), 0);
            out->col = json_int(field(node, "col"), 0);

            const JsonValue *connections = field(node, "connections");
            int conn_count = json_array_count(connections);
            if (conn_count > 3) conn_count = 3;
            for (int c = 0; c < conn_count; c++)
                out->conns[out->conn_count++] = json_int(json_array_get(connections, c), -1);
        }

        if (floor_node_counts[floor_index] > 0)
            loaded_floors++;
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d map floors from %s", loaded_floors, path);
    return loaded_floors == MAX_FLOORS;
}

static void calc_positions(MapNode *nodes, int count, int floor)
{
    (void)floor;
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
    memset(map, 0, sizeof(MapState));

    if (floor < 0) floor = 0;
    if (floor >= MAX_FLOORS) floor = MAX_FLOORS - 1;
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
    if (start >= 0 && start < map->node_count)
        map->nodes[start].available = true;

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
