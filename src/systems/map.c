#include "map.h"
#include "constants.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    NodeType type;
    int row, col;
    int x, y;
    bool has_pos;
    int *conns;
    int conn_count;
} NodeDef;

typedef struct {
    char *area_id;
    int floor_index;
    NodeDef *nodes;
    int node_count;
} FloorLayout;

static FloorLayout *floor_layouts = NULL;
static int floor_layout_count = 0;

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

static void free_floor_layouts(void)
{
    for (int f = 0; f < floor_layout_count; f++)
    {
        free(floor_layouts[f].area_id);
        for (int n = 0; n < floor_layouts[f].node_count; n++)
            free(floor_layouts[f].nodes[n].conns);
        free(floor_layouts[f].nodes);
    }
    free(floor_layouts);
    floor_layouts = NULL;
    floor_layout_count = 0;
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

    free_floor_layouts();

    int json_floor_count = json_array_count(floors);
    if (json_floor_count <= 0)
    {
        LOG_E(CAT_SCREEN, "%s: no map floors loaded", path);
        json_free(root);
        return false;
    }

    floor_layout_count = json_floor_count;
    floor_layouts = (FloorLayout *)calloc((size_t)floor_layout_count, sizeof(FloorLayout));
    if (!floor_layouts)
    {
        json_free(root);
        floor_layout_count = 0;
        return false;
    }

    int loaded_floors = 0;
    for (int f = 0; f < json_floor_count; f++)
    {
        const JsonValue *floor_obj = json_array_get(floors, f);
        if (!floor_obj || floor_obj->type != JSON_OBJECT) continue;

        int floor_index = json_int(field(floor_obj, "floor"), f + 1) - 1;
        if (floor_index < 0) continue;

        const JsonValue *nodes = field(floor_obj, "nodes");
        int node_count = json_array_count(nodes);
        if (node_count <= 0) continue;

        FloorLayout *layout = &floor_layouts[f];
        const char *area_id = json_string(field(floor_obj, "area"), NULL);
        if (!area_id)
            area_id = json_string(field(floor_obj, "area_id"), NULL);
        if (area_id && area_id[0])
        {
            size_t len = strlen(area_id);
            layout->area_id = (char *)malloc(len + 1);
            if (layout->area_id)
                memcpy(layout->area_id, area_id, len + 1);
        }
        layout->floor_index = floor_index;
        layout->nodes = (NodeDef *)calloc((size_t)node_count, sizeof(NodeDef));
        if (!layout->nodes) continue;
        layout->node_count = node_count;

        for (int n = 0; n < node_count; n++)
        {
            const JsonValue *node = json_array_get(nodes, n);
            if (!node || node->type != JSON_OBJECT) continue;

            NodeDef *out = &layout->nodes[n];
            out->type = parse_node_type(json_string(field(node, "type"), "combat"));
            out->row = json_int(field(node, "row"), 0);
            out->col = json_int(field(node, "col"), 0);

            const JsonValue *x_value = field(node, "x");
            const JsonValue *y_value = field(node, "y");
            if (x_value && x_value->type == JSON_NUMBER && y_value && y_value->type == JSON_NUMBER)
            {
                out->x = json_int(x_value, 0);
                out->y = json_int(y_value, 0);
                out->has_pos = true;
            }

            const JsonValue *connections = field(node, "connections");
            int conn_count = json_array_count(connections);
            if (conn_count > 0)
            {
                out->conns = (int *)calloc((size_t)conn_count, sizeof(int));
                if (out->conns)
                {
                    out->conn_count = conn_count;
                    for (int c = 0; c < conn_count; c++)
                        out->conns[c] = json_int(json_array_get(connections, c), -1);
                }
            }
        }

        loaded_floors++;
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d map floors from %s", loaded_floors, path);
    return loaded_floors > 0;
}

static bool area_matches(const char *layout_area, const char *area_id)
{
    bool layout_empty = !layout_area || !layout_area[0];
    bool area_empty = !area_id || !area_id[0];
    if (layout_empty && area_empty) return true;
    if (layout_empty || area_empty) return false;
    return strcmp(layout_area, area_id) == 0;
}

static FloorLayout *find_layout_exact(const char *area_id, int floor)
{
    for (int i = 0; i < floor_layout_count; i++)
        if (floor_layouts[i].node_count > 0 &&
            floor_layouts[i].floor_index == floor &&
            area_matches(floor_layouts[i].area_id, area_id))
            return &floor_layouts[i];
    return NULL;
}

static FloorLayout *find_layout(const char *area_id, int floor)
{
    FloorLayout *layout = find_layout_exact(area_id, floor);
    if (layout) return layout;
    if (area_id && area_id[0])
        return find_layout_exact(NULL, floor);
    return NULL;
}

static int max_floor_for_area(const char *area_id)
{
    int max_floor = -1;
    for (int i = 0; i < floor_layout_count; i++)
    {
        if (floor_layouts[i].node_count <= 0) continue;
        bool exact = area_matches(floor_layouts[i].area_id, area_id);
        bool fallback = area_id && area_id[0] && area_matches(floor_layouts[i].area_id, NULL);
        if ((exact || fallback) && floor_layouts[i].floor_index > max_floor)
            max_floor = floor_layouts[i].floor_index;
    }

    return max_floor;
}

static void calc_fallback_positions(MapNode *nodes, int count)
{
    int max_row = 0;
    for (int i = 0; i < count; i++)
        if (nodes[i].row > max_row) max_row = nodes[i].row;

    const int top_y = 76;
    const int row_height = 42;

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

    (void)max_row;
}

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

void map_clear(MapState *map)
{
    if (!map) return;
    if (map->nodes)
    {
        for (int i = 0; i < map->node_count; i++)
            free(map->nodes[i].conns);
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
    if (floor_layout_count <= 0 || !floor_layouts)
    {
        map->floor = floor;
        return;
    }

    int max_floor = max_floor_for_area(area_id);
    if (max_floor >= 0 && floor > max_floor)
        floor = max_floor;

    map->floor = floor;
    map->current_index = -1;
    map->boss_index = -1;

    FloorLayout *layout = find_layout(area_id, floor);
    if (!layout)
        layout = find_layout(NULL, floor);
    if (!layout)
        return;

    int count = layout->node_count;
    if (count <= 0 || !layout->nodes)
        return;

    map->nodes = (MapNode *)calloc((size_t)count, sizeof(MapNode));
    if (!map->nodes)
    {
        map->node_count = 0;
        return;
    }
    map->node_count = count;

    bool needs_positioning = false;
    for (int i = 0; i < count; i++)
    {
        NodeDef *def = &layout->nodes[i];
        MapNode *out = &map->nodes[i];
        out->type = def->type;
        out->row = def->row;
        out->col = def->col;
        out->x = def->x;
        out->y = def->y;
        out->completed = false;
        out->available = false;

        if (!def->has_pos)
            needs_positioning = true;

        if (def->conn_count > 0)
        {
            out->conns = (int *)calloc((size_t)def->conn_count, sizeof(int));
            if (out->conns)
            {
                out->conn_count = def->conn_count;
                for (int c = 0; c < def->conn_count; c++)
                    out->conns[c] = def->conns[c];
            }
        }

        if (def->type == NODE_BOSS)
            map->boss_index = i;
    }

    if (needs_positioning)
        calc_fallback_positions(map->nodes, count);

    remove_backward_connections(map->nodes, map->node_count);

    int start = map_find_start(map);
    if (start >= 0 && start < map->node_count)
        map->nodes[start].available = true;

    LOG_I(CAT_SCREEN, "Map generated: floor=%d, nodes=%d, boss=%d", floor, count, map->boss_index);
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

int map_loaded_floor_count(void)
{
    return map_loaded_floor_count_for_area(NULL);
}

int map_loaded_floor_count_for_area(const char *area_id)
{
    int max_floor = max_floor_for_area(area_id);
    return max_floor >= 0 ? max_floor + 1 : 0;
}
