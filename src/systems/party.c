#include "party.h"
#include <string.h>

static const char *class_names[] = {
    [CLASS_GUARDIAN] = "Guardian",
    [CLASS_CLERIC]   = "Cleric",
    [CLASS_MAGE]     = "Mage",
    [CLASS_ROGUE]    = "Rogue",
    [CLASS_SHAMAN]   = "Shaman",
    [CLASS_RANGER]   = "Ranger",
};

static const char *class_roles[] = {
    [CLASS_GUARDIAN] = "Tank",
    [CLASS_CLERIC]   = "Healer",
    [CLASS_MAGE]     = "Burst DPS",
    [CLASS_ROGUE]    = "Interrupt DPS",
    [CLASS_SHAMAN]   = "Support",
    [CLASS_RANGER]   = "Sustained DPS",
};

static const int class_hp[] = {
    [CLASS_GUARDIAN] = 80,
    [CLASS_CLERIC]   = 55,
    [CLASS_MAGE]     = 45,
    [CLASS_ROGUE]    = 60,
    [CLASS_SHAMAN]   = 55,
    [CLASS_RANGER]   = 55,
};

const char *class_name(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "Unknown";
    return class_names[ct];
}

const char *class_role(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_roles[ct];
}

void party_create(Party *party, int *class_indices, int count)
{
    if (count < 0) count = 0;
    if (count > MAX_PARTY_SIZE) count = MAX_PARTY_SIZE;

    memset(party, 0, sizeof(Party));
    party->count = count;
    for (int i = 0; i < count; i++)
    {
        ClassType ct = (ClassType)class_indices[i];
        if (ct < 0 || ct >= CLASS_COUNT) ct = CLASS_GUARDIAN;
        party->members[i].class = ct;
        strncpy(party->members[i].name, class_names[ct], MAX_PARTY_NAME - 1);
        party->members[i].name[MAX_PARTY_NAME - 1] = '\0';
        party->members[i].max_hp = class_hp[ct];
        party->members[i].hp = class_hp[ct];
        party->members[i].shield = 0;
        party->members[i].aggro = 0;
        party->members[i].alive = true;
        party->members[i].status_count = 0;
    }
}

int party_lowest_hp(Party *party)
{
    int idx = -1;
    int lowest = 99999;
    for (int i = 0; i < party->count; i++)
    {
        if (!party->members[i].alive) continue;
        int effective = party->members[i].hp;
        if (effective < lowest) { lowest = effective; idx = i; }
    }
    return idx;
}

int party_highest_aggro(Party *party)
{
    int idx = -1;
    int highest = -1;
    for (int i = 0; i < party->count; i++)
    {
        if (!party->members[i].alive) continue;
        if (party->members[i].aggro > highest) { highest = party->members[i].aggro; idx = i; }
    }
    return idx;
}
