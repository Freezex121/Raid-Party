#ifndef PARTY_H
#define PARTY_H

#include <stdbool.h>
#include "combat/status.h"

#define MAX_PARTY_SIZE 5
#define MAX_PARTY_NAME 32

typedef enum {
    CLASS_NONE = -1,
    CLASS_GUARDIAN,
    CLASS_CLERIC,
    CLASS_MAGE,
    CLASS_ROGUE,
    CLASS_SHAMAN,
    CLASS_RANGER,
    CLASS_COUNT
} ClassType;

typedef struct {
    ClassType class;
    char name[MAX_PARTY_NAME];
    int hp, max_hp;
    int shield;
    int aggro;
    bool alive;
    StatusEffect statuses[MAX_STATUSES];
    int status_count;
} PartyMember;

typedef struct {
    PartyMember members[MAX_PARTY_SIZE];
    int count;
} Party;

void party_create(Party *party, int *class_indices, int count);
int party_lowest_hp(Party *party);
int party_highest_aggro(Party *party);
const char *class_name(ClassType ct);
const char *class_role(ClassType ct);

#endif
