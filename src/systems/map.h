#ifndef MAP_H
#define MAP_H

#include <stdbool.h>

#define MAX_NODES_PER_FLOOR 14
#define MAX_FLOORS 3

typedef enum {
    NODE_START,
    NODE_COMBAT,
    NODE_ELITE,
    NODE_REST,
    NODE_SHOP,
    NODE_BOSS,
} NodeType;

typedef struct {
    NodeType type;
    int row, col;
    int x, y;
    int conns[3];
    int conn_count;
    bool completed;
    bool available;
} MapNode;

typedef struct {
    MapNode nodes[MAX_NODES_PER_FLOOR];
    int node_count;
    int floor;
    int current_index;
    int boss_index;
} MapState;

void map_generate(MapState *map, int floor);
void map_unlock_next(MapState *map);
int  map_find_start(MapState *map);

#endif
