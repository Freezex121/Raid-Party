#ifndef SYNERGY_DEFS_H
#define SYNERGY_DEFS_H

#include <stdbool.h>
#include "systems/party.h"
#include "combat/status.h"

#define MAX_SYNERGY_PAIRS 16
#define MAX_SYNERGY_COMBOS 16
#define MAX_CARD_STATUS_INTERACTIONS 32

typedef struct {
    const char *id;
    const char *class_a;
    const char *class_b;
    const char *trigger;        // "shield_gained", "first_damage_combat", "status_applied", "aggro_self_removed", "blight_consumed"
    const char *on_class;       // optional class filter
    const char *on_status;      // optional status filter (for status_applied trigger)
    const char *effect;         // "damage_random_enemy", "heal_self", "heal_lowest_ally", "multiply_damage", "extend_status", "add_damage"
    float value;                // numeric parameter
    bool once_per_combat;
    const char *name;
    const char *desc;
} SynergyPairDef;

typedef struct {
    const char *id;
    ClassType prev_class;
    ClassType next_class;
    const char *title;
    const char *subtitle;
    const char *consume_card_type; // "heal", "attack", "aoe", "damage", "group_heal_or_shield", "mage_fire_spell", "warlock_damage", "paladin"
    bool free_cost;
    float multiply_damage;
    float multiply_heal;
    float multiply_shield;
    const char *apply_status;
    int status_turns;
    int status_value;
    const char *special_effect; // "consume_blight_heal_party", "reset_aggressive", null
    int combo_turns;            // how many turns the combo prime lasts (default 0 = current turn only, 1 = through next turn, etc.)
} SynergyComboDef;

typedef struct {
    const char *card_id;
    StatusType status;
    bool consume;
    const char *effect;         // "add_damage", "refund_energy", "draw_cards", "add_lowest_heal", "heal_caster", "add_shield", "apply_trap", "splash_adjacent_50", "aoe_conductive_50", "add_damage_per_stack", "apply_blight"
    int value;
    int turns;
} SynergyCardStatusDef;

int synergy_pair_count(void);
const SynergyPairDef *synergy_pair_by_index(int index);
int synergy_pair_count_for_trigger(const char *trigger);

int synergy_combo_count(void);
const SynergyComboDef *synergy_combo_by_index(int index);
const SynergyComboDef *synergy_combo_for_chain(ClassType prev, ClassType next);

int synergy_card_status_count(void);
const SynergyCardStatusDef *synergy_card_status_by_index(int index);
const SynergyCardStatusDef *synergy_card_status_for_card(const char *card_id);

bool synergy_defs_load_json(const char *path);

#endif
