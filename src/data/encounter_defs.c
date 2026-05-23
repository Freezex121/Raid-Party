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

    // Floor 1 — Basic Nature (spider, wolf, boar, treant guardian)
    f1_storage[0] = (EncounterDef){ .enemies = { &giant_spider, 0, 0 }, .count = 1 };
    f1_storage[1] = (EncounterDef){ .enemies = { &dire_wolf, 0, 0 }, .count = 1 };
    f1_storage[2] = (EncounterDef){ .enemies = { &elder_treant, &forest_boar, 0 }, .count = 2 };
    f1_elite_storage = (EncounterDef){ .enemies = { &alpha_wolf, &elder_treant, 0 }, .count = 2 };

    // Floor 2 — Poison (manticore, basilisk, stalker, venom priest guardian)
    f2_storage[0] = (EncounterDef){ .enemies = { &venom_priest, &manticore, 0 }, .count = 2 };
    f2_storage[1] = (EncounterDef){ .enemies = { &basilisk, &venom_stalker, 0 }, .count = 2 };
    f2_storage[2] = (EncounterDef){ .enemies = { &manticore, &venom_priest, 0 }, .count = 2 };
    f2_elite_storage = (EncounterDef){ .enemies = { &greater_manticore, &venom_priest, 0 }, .count = 2 };

    // Floor 3 — Fire / Dragon (flame_imp, fire_drake, cinder warden guardian, living_armor)
    f3_storage[0] = (EncounterDef){ .enemies = { &cinder_warden, &flame_imp, 0 }, .count = 2 };
    f3_storage[1] = (EncounterDef){ .enemies = { &fire_drake, &living_armor, 0 }, .count = 2 };
    f3_storage[2] = (EncounterDef){ .enemies = { &cinder_warden, &fire_drake, &flame_imp }, .count = 3 };
    f3_elite_storage = (EncounterDef){ .enemies = { &fire_giant, &cinder_warden, 0 }, .count = 2 };

    boss_storage[0] = (EncounterDef){ .enemies = { &alpha_wolf, &giant_spider, &dire_wolf }, .count = 3 };
    boss_storage[1] = (EncounterDef){ .enemies = { &venom_hydra, &greater_manticore, 0 }, .count = 2 };
    boss_storage[2] = (EncounterDef){ .enemies = { &elder_dragon, &fire_giant, &fire_drake }, .count = 3 };

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
