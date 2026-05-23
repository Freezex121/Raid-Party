#include "encounter_defs.h"

static EncounterDef f1_storage[3];
static EncounterDef f2_storage[3];
static EncounterDef f3_storage[3];
static EncounterDef f1_elite_storage;
static EncounterDef f2_elite_storage;
static EncounterDef f3_elite_storage;
static EncounterDef boss_storage[3];
static int initialized = 0;

static void init_encounters(void)
{
    if (initialized) return;

    // Floor 1: Flame Imp, Cult Healer, Venom Stalker
    f1_storage[0] = (EncounterDef){ .enemies = { &flame_imp, 0, 0 }, .count = 1 };
    f1_storage[1] = (EncounterDef){ .enemies = { &cult_healer, &flame_imp, 0 }, .count = 2 };
    f1_storage[2] = (EncounterDef){ .enemies = { &venom_stalker, 0, 0 }, .count = 1 };
    f1_elite_storage = (EncounterDef){ .enemies = { &flame_imp, &cult_healer, 0 }, .count = 2 };

    // Floor 2: Rage Knight, Arcane Wisp, Berserker, mixed
    f2_storage[0] = (EncounterDef){ .enemies = { &rage_knight, 0, 0 }, .count = 1 };
    f2_storage[1] = (EncounterDef){ .enemies = { &arcane_wisp, &flame_imp, 0 }, .count = 2 };
    f2_storage[2] = (EncounterDef){ .enemies = { &berserker, &venom_stalker, 0 }, .count = 2 };
    f2_elite_storage = (EncounterDef){ .enemies = { &rage_knight, &cult_healer, 0 }, .count = 2 };

    // Floor 3: Living Armor, all mixed
    f3_storage[0] = (EncounterDef){ .enemies = { &living_armor, &flame_imp, 0 }, .count = 2 };
    f3_storage[1] = (EncounterDef){ .enemies = { &berserker, &arcane_wisp, 0 }, .count = 2 };
    f3_storage[2] = (EncounterDef){ .enemies = { &rage_knight, &venom_stalker, &cult_healer }, .count = 3 };
    f3_elite_storage = (EncounterDef){ .enemies = { &living_armor, &berserker, &arcane_wisp }, .count = 3 };

    boss_storage[0] = (EncounterDef){ .enemies = { &rage_knight, 0, 0 }, .count = 1 };
    boss_storage[1] = (EncounterDef){ .enemies = { &living_armor, &arcane_wisp, 0 }, .count = 2 };
    boss_storage[2] = (EncounterDef){ .enemies = { &rage_knight, &berserker, &cult_healer }, .count = 3 };

    initialized = 1;
}

const EncounterDef *encounter_for_floor(int floor, int index)
{
    init_encounters();
    switch (floor)
    {
        case 0: if (index < 3) return &f1_storage[index]; break;
        case 1: if (index < 3) return &f2_storage[index]; break;
        case 2: if (index < 3) return &f3_storage[index]; break;
    }
    return &f1_storage[0];
}

int encounter_count_for_floor(int floor)
{
    return 3;
}

const EncounterDef *elite_for_floor(int floor)
{
    init_encounters();
    switch (floor)
    {
        case 0: return &f1_elite_storage;
        case 1: return &f2_elite_storage;
        case 2: return &f3_elite_storage;
    }
    return &f1_elite_storage;
}

const EncounterDef *boss_for_floor(int floor)
{
    init_encounters();
    if (floor < 0) floor = 0;
    if (floor > 2) floor = 2;
    return &boss_storage[floor];
}
