#ifndef ENEMY_DEFS_H
#define ENEMY_DEFS_H

#include <stdbool.h>
#include "combat/status.h"

#define MAX_ENEMIES 3
#define MAX_ENEMY_CARDS 16
#define MAX_ENEMY_HAND 5
#define ENEMY_DECK_SIZE 16
#define MAX_ENEMY_PHASES 6
#define MAX_ENEMY_PHASE_CARD_REFS 8
#define MAX_ENEMY_SEQUENCE 16
#define MAX_ENEMY_REACTIVE_CARDS 8
#define MAX_ENEMY_THRESHOLDS 8
#define MAX_ENEMY_MINION_BUFFS 4
#define MAX_ENEMY_CAST_STAGES 4
#define MAX_ENEMY_AURAS 4

typedef enum {
    INTENT_NONE,
    INTENT_ATTACK,
    INTENT_TANK_BUSTER,
    INTENT_AOE,
    INTENT_WIPE,
    INTENT_BUFF,
    INTENT_HEAL,
    INTENT_SHIELD,
    INTENT_BUFF_ATTACK,
    INTENT_AOE_SHIELD,
} IntentType;

typedef enum {
    ENEMY_TARGET_TANK,
    ENEMY_TARGET_RANDOM,
    ENEMY_TARGET_ALL,
    ENEMY_TARGET_SELF,
    ENEMY_TARGET_LOWEST_HP,
} EnemyTargetType;

typedef enum {
    AURA_NONE,
    AURA_DAMAGE_TICK,
    AURA_HEAL_TICK,
    AURA_SHIELD_TICK,
    AURA_COST_UP,
    AURA_COST_DOWN,
    AURA_DAMAGE_REDUCTION,
    AURA_HEAL_REDUCTION,
    AURA_DRAW_REDUCTION,
    AURA_ENERGY_DRAIN,
    AURA_ENEMY_DAMAGE_UP,
    AURA_SILENCE_CLASSES,
} ArenaAuraType;

typedef enum {
    AI_WEIGHTED,
    AI_PATTERN,
    AI_REACTIVE,
} EnemyAIOverride;

typedef enum {
    TETHER_RANDOM,
    TETHER_LOWEST_HP,
    TETHER_LOWEST_SHIELD,
    TETHER_LAST_ATTACKER,
} TetherTargetType;

typedef enum {
    INVULN_NONE,
    INVULN_ALL_ADDS_DEAD,
    INVULN_DAMAGE_THRESHOLD,
    INVULN_TURNS,
    INVULN_CARD_PLAYED,
} InvulnerableUntilType;

typedef enum {
    DETONATE_SINGLE,
    DETONATE_SELF,
    DETONATE_ALL,
} DetonateTargetType;

typedef enum {
    CURSE_PILE_DRAW,
    CURSE_PILE_DISCARD,
    CURSE_PILE_HAND,
} CursePileType;

typedef enum {
    REACTION_NONE,
    REACTION_ON_PLAYER_ATTACK,
    REACTION_ON_PLAYER_SKILL,
    REACTION_ON_PLAYER_SHIELD,
    REACTION_ON_PLAYER_HEAL,
    REACTION_ON_DAMAGE_TAKEN,
    REACTION_ON_CARD_PLAYED,
    REACTION_ON_TURN_END_ENERGY_UNSPENT,
} ReactionType;

typedef enum {
    REFLECT_DAMAGE,
    REFLECT_STATUS,
    REFLECT_SHIELD_ON_BOSS,
} ReflectType;

typedef enum {
    THRESHOLD_RESET_END_OF_TURN,
    THRESHOLD_RESET_PER_COMBAT,
    THRESHOLD_RESET_PER_PHASE,
} ThresholdResetType;

typedef enum {
    RAGE_FORMULA_LINEAR,
    RAGE_FORMULA_EXPONENTIAL,
    RAGE_FORMULA_STEP,
} RageFormulaType;

typedef enum {
    MINION_BUFF_DAMAGE_BOOST,
    MINION_BUFF_SHIELD_PER_TURN,
} MinionBuffType;

typedef struct {
    ArenaAuraType type;
    int value;
    int duration;
    int clear_on_damage_threshold;
} ArenaAuraDef;

typedef struct {
    int turn;
    const char *effect;
    int value;
    const char *enemy_id;
    int count;
} CastStageEffectDef;

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
    int repeats;  // hits per attack (default 1)
    int lifesteal_pct;  // % of damage healed to self (default 0)
    int self_damage;  // damage dealt to self as cost (default 0)
    int buff_damage;  // % damage increase for future turns (default 0)
    int buff_turns;  // duration of damage buff (default 0)
    bool interrupts;  // if true, interrupts player channeling/combo
    bool enrage_allies;  // if true, applies shield to other living enemies at half
    int spend;
    bool spend_all;
    int damage_per_spent;
    int heal_per_spent;
    int shield_per_spent;
    int status_per_spent;
    StatusType detonate_status;
    int detonate_mult;
    DetonateTargetType detonate_target;
    int detonate_heal_mult;
    int detonate_shield_mult;
    bool detonate_clear;
    bool double_status_on_party;
    StatusType double_status;
    bool tether;
    int tether_duration;
    TetherTargetType tether_target;
    bool tether_transferrable;
    bool set_invulnerable;
    InvulnerableUntilType invulnerable_until;
    int invulnerable_turns;
    int invulnerable_clear_on_damage;
    const char *summon_id;
    int summon_count;
    int phase_gate_hp_pct;
    const char *gate_summon_id;
    int gate_summon_count;
    ArenaAuraDef gate_aura;
    const char *gate_deck_override;
    int curse_count;
    const char *curse_card_id;
    CursePileType curse_pile;
    bool steal_card;
    int steal_return_turns;
    bool steal_use_against;
    int exhaust_from_deck;
    int force_discard;
    int increase_costs;
    int decrease_hand_size;
    bool set_player_energy_zero;
    int damage_per_player_energy_drained;
    ArenaAuraDef apply_arena_aura;
    ReactionType reaction_type;
    int reaction_trigger;
    bool reaction_once;
    int reaction_priority;
    int reflect_pct;
    ReflectType reflect_type;
    int reflect_turns;
    int reflect_cap;
    int cast_stages;
    const char *stage_labels[MAX_ENEMY_CAST_STAGES];
    CastStageEffectDef stage_effects[MAX_ENEMY_CAST_STAGES];
    int stage_effect_count;
    unsigned int interruptible_stage_mask;
    bool stage_warning;
} EnemyCardDef;

typedef struct {
    int trigger_hp_pct;
    int gain_shield;
    int gain_energy;
    int heal_pct;
    StatusType party_status;
    int party_status_turns;
    int party_status_value;
    bool set_invulnerable;
    int invulnerable_turns;
    const char *swap_deck;
    const char *spawn_id;
    int spawn_count;
    ArenaAuraDef aura;
    ArenaAuraDef auras[MAX_ENEMY_AURAS];
    int aura_count;
    int energy_per_turn_delta;
    int hand_size_delta;
    float damage_scale_delta;
    int cast_time_reduction;
    int new_cards[MAX_ENEMY_PHASE_CARD_REFS];
    int new_card_count;
    int remove_cards[MAX_ENEMY_PHASE_CARD_REFS];
    int remove_card_count;
} EnemyPhaseDef;

typedef struct {
    int card_index;
    const char *condition;
} ReactiveCardDef;

typedef struct {
    MinionBuffType effect;
    int value;
} MinionBuffDef;

typedef struct {
    int threshold;
    int gain_shield;
    int heal;
    int retaliate;
    StatusType apply_status;
    int status_turns;
    int status_value;
    int gain_energy;
    bool clear_debuffs;
} DamageThresholdDef;

typedef struct {
    const char *id;
    const char *name;
    int hp, max_hp;
    int hand_size;
    int energy_per_turn;
    int card_count;
    EnemyCardDef cards[MAX_ENEMY_CARDS];
    bool accumulate;
    int accumulator_cap;
    int gain_on_player_card_play;
    int gain_on_player_attack;
    int gain_on_player_skill;
    int gain_on_player_shield;
    int gain_on_player_heal;
    int gain_on_boss_attacked;
    int gain_on_turn_start;
    int gain_on_ally_damaged;
    int decay_per_turn;
    bool decay_on_defend;
    const char *linked_group_id;
    bool linked_revive;
    int linked_revive_hp;
    int linked_revive_turns;
    int linked_revive_stagger;
    EnemyPhaseDef phases[MAX_ENEMY_PHASES];
    int phase_count;
    int opening_sequence[MAX_ENEMY_SEQUENCE];
    int opening_count;
    int pattern_cycle[MAX_ENEMY_SEQUENCE];
    int pattern_count;
    bool pattern_loop;
    EnemyAIOverride ai_override;
    ReactiveCardDef reactive_cards[MAX_ENEMY_REACTIVE_CARDS];
    int reactive_count;
    MinionBuffDef minion_buffs[MAX_ENEMY_MINION_BUFFS];
    int minion_buff_count;
    const char *minion_buff_tooltip;
    DamageThresholdDef damage_thresholds[MAX_ENEMY_THRESHOLDS];
    int damage_threshold_count;
    ThresholdResetType threshold_reset;
    float rage_scale;
    int rage_hp_threshold;
    float rage_max_mult;
    RageFormulaType rage_formula;
    ArenaAuraDef start_aura;
    ArenaAuraDef start_auras[MAX_ENEMY_AURAS];
    int start_aura_count;
} EnemyDef;

ArenaAuraType enemy_parse_arena_aura_type(const char *text);
const char *enemy_arena_aura_name(ArenaAuraType type);
const char *enemy_arena_aura_description(ArenaAuraType type);
const char *enemy_reaction_type_name(ReactionType type);

bool enemy_defs_load_json(const char *path);
const EnemyDef *enemy_def_by_id(const char *id);
const EnemyDef *enemy_def_by_index(int index);
int enemy_defs_loaded_count(void);

#endif
