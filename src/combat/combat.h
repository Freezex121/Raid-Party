#ifndef COMBAT_H
#define COMBAT_H

#include <stdbool.h>
#include "systems/party.h"
#include "systems/deck.h"
#include "systems/energy.h"
#include "data/encounter_defs.h"

typedef enum {
    COMBAT_TURN_START,
    COMBAT_PLAYER_TURN,
    COMBAT_ENEMY_TURN,
    COMBAT_VICTORY,
    COMBAT_DEFEAT
} CombatPhase;

typedef enum {
    TGT_NONE,
    TGT_SELECT_ENEMY,
    TGT_SELECT_ALLY,
} TargetMode;

typedef struct {
    int ability_idx;
    int remaining_turns;
    int index;
} EnemyIntent;

typedef struct {
    const EnemyDef *def;
    int hp;
    int max_hp;
    int shield;
    EnemyIntent intent;
    int current_ability;
    int phase;
    int pos_x, pos_y;
    StatusEffect statuses[MAX_STATUSES];
    int status_count;
} EnemyState;

typedef struct {
    CombatPhase phase;
    Party party;
    Deck deck;
    Energy energy;
    EnemyState enemies[MAX_ENEMIES];
    int enemy_count;
    int turn;
    int hovered_card;
    TargetMode target_mode;
    int target_hand_idx;
    int hovered_enemy;
    int hovered_ally;
    float target_offset;
    int target_offset_tween;
    ClassType combo_class;
    int combo_last_cost;
    int combo_count;
    float combo_scale;
    int combo_tween;
    float combo_shake;
    float state_timer;
    char result_message[128];
    bool gold_spawned;
    const CardDef *channel_card;
    int channel_remaining;
    ClassType channel_class;
    float turn_banner_timer;
    char turn_banner_text[32];
    float enemy_banner_timer;
    float end_turn_flash;
    float play_flash_timer;
    float play_flash_x;
    float play_flash_y;
    char play_flash_text[64];
    char action_feed[5][96];
    float action_feed_timer[5];
    float floor_scale;
} CombatState;

void combat_start(CombatState *cs, const Party *party, const EncounterDef *encounter);
void combat_update(CombatState *cs);
void combat_end_turn(CombatState *cs);

#endif
