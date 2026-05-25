#ifndef ENCOUNTER_DEFS_H
#define ENCOUNTER_DEFS_H

#include <stdbool.h>
#include "enemy_defs.h"

typedef struct {
    const EnemyDef *enemies[3];
    int count;
} EncounterDef;

bool encounter_defs_load_json(const char *path);
const EncounterDef *encounter_for_floor(int floor, int index);
int encounter_count_for_floor(int floor);
const EncounterDef *elite_for_floor(int floor);
const EncounterDef *boss_for_floor(int floor);
const EncounterDef *encounter_for_area_floor(const char *area_id, int floor, int index);
int encounter_count_for_area_floor(const char *area_id, int floor);
const EncounterDef *elite_for_area_floor(const char *area_id, int floor);
const EncounterDef *boss_for_area_floor(const char *area_id, int floor);

#endif
