#ifndef COMBAT_H
#define COMBAT_H

#include <stdbool.h>
#include "raylib.h"
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

typedef enum {
    COMBO_PRIME_NONE,
    COMBO_PRIME_SHIELD_OF_FAITH,
    COMBO_PRIME_ARCANE_ASSAULT,
    COMBO_PRIME_STORM_VOLLEY,
    COMBO_PRIME_SHADOW_DANCE,
    COMBO_PRIME_ELEMENTAL_FURY,
    COMBO_PRIME_BACKDRAFT,
    COMBO_PRIME_SACRED_CHORUS,
    COMBO_PRIME_DARK_REFRAIN,
    COMBO_PRIME_ABSOLUTION,
} ComboPrime;

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
    int last_interrupted_ability;
    int interrupt_cooldown;
    bool interrupted_recently;
    int phase;
    int pos_x, pos_y;
    StatusEffect statuses[MAX_STATUSES];
    int status_count;
} EnemyState;

#define MAX_CARD_THROW_ANIMS 8

typedef struct {
    bool active;
    const CardDef *card;
    bool upgraded;
    float t;
    float duration;
    Vector2 start;
    Vector2 control;
    Vector2 end;
    int width;
    int height;
} CardThrowAnim;

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
    ClassType last_played_class;
    ComboPrime combo_prime;
    float synergy_banner_timer;
    float synergy_flash_timer;
    char synergy_banner_title[32];
    char synergy_banner_subtitle[64];
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
    float enemy_damage_scale;
    int turn_draw_count;
    bool phoenix_used;
    bool echo_used;
    bool ambush_used;
    bool vengeful_active;
    int vengeful_ally;
    int mana_gem_bonus;
    CardThrowAnim card_throws[MAX_CARD_THROW_ANIMS];
} CombatState;

void combat_start(CombatState *cs, const Party *party, const EncounterDef *encounter);
void combat_update(CombatState *cs);
void combat_end_turn(CombatState *cs);
void combat_draw_card_throws(CombatState *cs);

#endif
