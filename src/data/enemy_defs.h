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

extern const EnemyDef training_dummy;
extern const EnemyDef flame_imp;
extern const EnemyDef rage_knight;
extern const EnemyDef cult_healer;
extern const EnemyDef berserker;
extern const EnemyDef living_armor;
extern const EnemyDef venom_stalker;
extern const EnemyDef arcane_wisp;

#endif
