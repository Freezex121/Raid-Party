#ifndef ENEMY_DEFS_H
#define ENEMY_DEFS_H

#include <stdbool.h>

#define MAX_ENEMIES 3

typedef enum {
    INTENT_NONE,
    INTENT_ATTACK,
    INTENT_TANK_BUSTER,
    INTENT_AOE,
    INTENT_WIPE,
    INTENT_BUFF,
    INTENT_HEAL,
    INTENT_SHIELD,
} IntentType;

typedef struct {
    const char *name;
    IntentType intent;
    int base_damage;
    int cast_time;
    const char *description;
    bool is_wipe;
    int heal_amount;
    int shield_amount;
} EnemyAbility;

typedef struct {
    const char *id;
    const char *name;
    int hp, max_hp;
    int ability_count;
    EnemyAbility abilities[4];
} EnemyDef;

bool enemy_defs_load_json(const char *path);
const EnemyDef *enemy_def_by_id(const char *id);
int enemy_defs_loaded_count(void);

#endif
