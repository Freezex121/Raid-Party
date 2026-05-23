#ifndef ENCOUNTER_DEFS_H
#define ENCOUNTER_DEFS_H

#include "enemy_defs.h"

typedef struct {
    const EnemyDef *enemies[3];
    int count;
} EncounterDef;

const EncounterDef *encounter_for_floor(int floor, int index);
int encounter_count_for_floor(int floor);
const EncounterDef *elite_for_floor(int floor);
const EncounterDef *boss_for_floor(int floor);

#endif
