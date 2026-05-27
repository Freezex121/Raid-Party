#ifndef MAP_H
#define MAP_H

#include <stdbool.h>

typedef enum {
    NODE_START,
    NODE_COMBAT,
    NODE_ELITE,
    NODE_REST,
    NODE_SHOP,
    NODE_EVENT,
    NODE_BOSS,
} NodeType;

typedef struct {
    NodeType type;
    int row, col;
    int x, y;
    int *conns;
    int conn_count;
    bool completed;
    bool available;
    char *event_id;
} MapNode;

typedef struct {
    MapNode *nodes;
    int node_count;
    int floor;
    int current_index;
    int boss_index;
} MapState;

void map_clear(MapState *map);
void map_generate(MapState *map, int floor, const char *area_id);
void map_unlock_next(MapState *map);
int  map_find_start(MapState *map);

#endif
