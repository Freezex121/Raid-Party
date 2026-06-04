#ifndef PARTY_H
#define PARTY_H

#include <stdbool.h>
#include "combat/status.h"

#define MAX_PARTY_SIZE 5
#define MAX_PARTY_NAME 32
#define MAX_MEMBER_PERKS 9
#define MAX_COMBAT_XP 20
#define MAX_LEVEL 10
#define CLASS_MAX_COUNT 24
#define MAX_PERK_DEFS 64
#define MAX_CLASS_PERKS 8

typedef enum {
    CLASS_NONE = -1,
    CLASS_GUARDIAN,
    CLASS_CLERIC,
    CLASS_MAGE,
    CLASS_ROGUE,
    CLASS_SHAMAN,
    CLASS_RANGER,
    CLASS_PALADIN,
    CLASS_WARLOCK,
    CLASS_BARD,
    CLASS_BUILTIN_COUNT,
    CLASS_COUNT = CLASS_MAX_COUNT
} ClassType;

typedef int PerkId;

#define PERK_COUNT MAX_PERK_DEFS
#define PERK_INVALID PERK_COUNT

typedef struct {
    ClassType class;
    char name[MAX_PARTY_NAME];
    int hp, max_hp;
    int shield;
    int aggro;
    int level;
    int xp;
    int combat_xp;
    int pending_levels;
    int perks[MAX_MEMBER_PERKS];
    int perk_count;
    bool alive;
    StatusEffect statuses[MAX_STATUSES];
    int status_count;
} PartyMember;

typedef struct {
    PartyMember members[MAX_PARTY_SIZE];
    int count;
} Party;

bool party_defs_load_json(const char *path);
bool perk_defs_load_json(const char *path);
int party_class_count(void);
ClassType party_class_at(int order_index);
bool class_is_loaded(ClassType ct);
ClassType class_from_id(const char *id);
void party_create(Party *party, int *class_indices, int count);
int party_lowest_hp(Party *party);
int party_highest_aggro(Party *party);
int party_random_alive(Party *party);
const char *class_id(ClassType ct);
const char *class_name(ClassType ct);
const char *class_role(ClassType ct);
const char *class_description(ClassType ct);
const char *class_hint(ClassType ct);
const char *class_abbrev(ClassType ct);
unsigned char class_color_r(ClassType ct);
unsigned char class_color_g(ClassType ct);
unsigned char class_color_b(ClassType ct);
const char *class_unlock_key(ClassType ct);
const char *class_unlock_event(ClassType ct);
int xp_for_level(int level);
int xp_total_for_level(int level);
int party_member_xp_into_level(const PartyMember *member);
int party_member_gain_xp(PartyMember *member, int amount, int *levels_gained);
bool party_member_has_perk(const PartyMember *member, PerkId perk);
int party_member_perk_count(const PartyMember *member, PerkId perk);
bool party_member_add_perk(PartyMember *member, PerkId perk);
bool perk_is_loaded(PerkId perk);
bool perk_is_class_specific(PerkId perk);
int perk_max_stacks(PerkId perk);
int perk_class_count(ClassType ct);
PerkId perk_class_at(ClassType ct, int index);
PerkId perk_for_class(ClassType ct);
const char *perk_id(PerkId perk);
const char *perk_name(PerkId perk);
const char *perk_description(PerkId perk);
const char *perk_effect(PerkId perk);
int perk_effect_value(PerkId perk);
int party_member_perk_effect_total(const PartyMember *member, const char *effect);
const char *party_member_perk_effect_name(const PartyMember *member, const char *effect);
int perk_generic_count(void);
PerkId perk_generic_at(int index);

#endif
