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

// Floor 1 — Basic Nature
extern const EnemyDef giant_spider;
extern const EnemyDef dire_wolf;
extern const EnemyDef forest_boar;
extern const EnemyDef elder_treant;
extern const EnemyDef alpha_wolf;

// Floor 2 — Poison
extern const EnemyDef manticore;
extern const EnemyDef basilisk;
extern const EnemyDef venom_stalker;
extern const EnemyDef venom_priest;
extern const EnemyDef greater_manticore;
extern const EnemyDef venom_hydra;

// Floor 3 — Fire / Dragon
extern const EnemyDef flame_imp;
extern const EnemyDef fire_drake;
extern const EnemyDef cinder_warden;
extern const EnemyDef living_armor;
extern const EnemyDef fire_giant;
extern const EnemyDef elder_dragon;

#endif
