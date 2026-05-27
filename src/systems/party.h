#ifndef PARTY_H
#define PARTY_H

#include <stdbool.h>
#include "combat/status.h"

#define MAX_PARTY_SIZE 5
#define MAX_PARTY_NAME 32
#define MAX_MEMBER_PERKS 9
#define MAX_COMBAT_XP 20
#define MAX_LEVEL 10

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
    CLASS_COUNT
} ClassType;

typedef enum {
    PERK_HP_PLUS_4,
    PERK_STARTING_SHIELD_1,
    PERK_CARD_DMG_1,
    PERK_CARD_HEAL_1,
    PERK_CARD_SHIELD_1,
    PERK_GUARDIAN_TAUNT_SHIELD,
    PERK_CLERIC_OVERHEAL_SHIELD,
    PERK_MAGE_FIRST_SPELL_DMG,
    PERK_ROGUE_MARK_REFUND,
    PERK_SHAMAN_EXTEND_STATUS,
    PERK_RANGER_MARKED_DMG,
    PERK_PALADIN_HEAL_SHIELD,
    PERK_WARLOCK_BLIGHT_BOOST,
    PERK_BARD_FIRST_DRAW,
    PERK_COUNT
} PerkId;

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
void party_create(Party *party, int *class_indices, int count);
int party_lowest_hp(Party *party);
int party_highest_aggro(Party *party);
int party_random_alive(Party *party);
const char *class_name(ClassType ct);
const char *class_role(ClassType ct);
int xp_for_level(int level);
int xp_total_for_level(int level);
int party_member_xp_into_level(const PartyMember *member);
int party_member_gain_xp(PartyMember *member, int amount, int *levels_gained);
bool party_member_has_perk(const PartyMember *member, PerkId perk);
int party_member_perk_count(const PartyMember *member, PerkId perk);
bool party_member_add_perk(PartyMember *member, PerkId perk);
bool perk_is_class_specific(PerkId perk);
PerkId perk_for_class(ClassType ct);
const char *perk_name(PerkId perk);
const char *perk_description(PerkId perk);

#endif
