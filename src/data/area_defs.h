#ifndef AREA_DEFS_H
#define AREA_DEFS_H

#include <stdbool.h>

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    int floor_count;
    int difficulty_percent;
    char **map_bg_files;
    int map_bg_count;
} AreaDef;

bool area_defs_load_json(const char *path);
int area_defs_count(void);
const AreaDef *area_def(int index);
int area_clamp_index(int index);
int area_floor_count(int index);
float area_difficulty_scale(int index);

#endif
