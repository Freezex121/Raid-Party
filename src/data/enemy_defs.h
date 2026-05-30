#ifndef ENEMY_DEFS_H
#define ENEMY_DEFS_H

#include <stdbool.h>
#include "combat/status.h"

#define MAX_ENEMIES 3
#define MAX_ENEMY_CARDS 16
#define MAX_ENEMY_HAND 5
#define ENEMY_DECK_SIZE 16

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

typedef enum {
    ENEMY_TARGET_TANK,
    ENEMY_TARGET_RANDOM,
    ENEMY_TARGET_ALL,
    ENEMY_TARGET_SELF,
    ENEMY_TARGET_LOWEST_HP,
} EnemyTargetType;

typedef struct {
    const char *name;
    IntentType intent;
    EnemyTargetType target;
    int cost;
    int base_damage;
    int cast_time;
    const char *description;
    bool is_wipe;
    int heal_amount;
    int shield_amount;
    StatusType status;
    int status_amount;
    int status_turns;
    int count;  // copies in deck
} EnemyCardDef;

typedef struct {
    const char *id;
    const char *name;
    int hp, max_hp;
    int hand_size;
    int energy_per_turn;
    int card_count;
    EnemyCardDef cards[MAX_ENEMY_CARDS];
} EnemyDef;

bool enemy_defs_load_json(const char *path);
const EnemyDef *enemy_def_by_id(const char *id);
int enemy_defs_loaded_count(void);

#endif
