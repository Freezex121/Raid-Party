#include "enemy_defs.h"

const EnemyDef training_dummy = {
    .id = "dummy", .name = "Training Dummy", .hp = 60, .max_hp = 60,
    .ability_count = 1,
    .abilities = { { "Poke", INTENT_ATTACK, 2, 1, "A gentle poke.", false, 0, 0 } },
};

const EnemyDef flame_imp = {
    .id = "flame_imp", .name = "Flame Imp", .hp = 40, .max_hp = 40,
    .ability_count = 2,
    .abilities = {
        { "Firebolt",     INTENT_ATTACK,      8,  1, "Deals 8 damage.", false, 0, 0 },
        { "Flame Breath", INTENT_TANK_BUSTER, 14, 2, "Deals 14 damage.", false, 0, 0 },
    },
};

const EnemyDef rage_knight = {
    .id = "rage_knight", .name = "Rage Knight", .hp = 80, .max_hp = 80,
    .ability_count = 3,
    .abilities = {
        { "Slash",     INTENT_ATTACK,      10, 1, "Deals 10 damage.", false, 0, 0 },
        { "Cleave",    INTENT_AOE,         7,  2, "Deals 7 damage to all.", false, 0, 0 },
        { "Execution", INTENT_TANK_BUSTER, 22, 2, "Deals 22 damage.", false, 0, 0 },
    },
};

const EnemyDef cult_healer = {
    .id = "cult_healer", .name = "Cult Healer", .hp = 30, .max_hp = 30,
    .ability_count = 2,
    .abilities = {
        { "Heal",  INTENT_HEAL, 0,  1, "Heals an ally for 12.", false, 12, 0 },
        { "Smite", INTENT_ATTACK, 5, 1, "Deals 5 damage.", false, 0, 0 },
    },
};

const EnemyDef berserker = {
    .id = "berserker", .name = "Berserker", .hp = 55, .max_hp = 55,
    .ability_count = 2,
    .abilities = {
        { "Dual Strike", INTENT_TANK_BUSTER, 7,  2, "Strikes twice for 7 each.", false, 0, 0 },
        { "Frenzy",      INTENT_ATTACK,      12, 1, "Deals 12 damage.", false, 0, 0 },
    },
};

const EnemyDef living_armor = {
    .id = "living_armor", .name = "Living Armor", .hp = 65, .max_hp = 65,
    .ability_count = 2,
    .abilities = {
        { "Shield Bash",    INTENT_ATTACK, 6,  1, "Deals 6 damage. Gains 8 shield.", false, 0, 8 },
        { "Rebuild Armor",  INTENT_SHIELD, 0,  2, "Gains 15 shield.", false, 0, 15 },
    },
};

const EnemyDef venom_stalker = {
    .id = "venom_stalker", .name = "Venom Stalker", .hp = 30, .max_hp = 30,
    .ability_count = 2,
    .abilities = {
        { "Venom Strike", INTENT_ATTACK, 6,  1, "Deals 6 damage.", false, 0, 0 },
        { "Rapid Strikes",INTENT_ATTACK, 4,  1, "Deals 4 damage twice.", false, 0, 0 },
    },
};

const EnemyDef arcane_wisp = {
    .id = "arcane_wisp", .name = "Arcane Wisp", .hp = 20, .max_hp = 20,
    .ability_count = 2,
    .abilities = {
        { "Arcane Bolt", INTENT_TANK_BUSTER, 14, 2, "Deals 14 damage.", false, 0, 0 },
        { "Flicker",     INTENT_ATTACK,      7,  1, "Deals 7 damage.", false, 0, 0 },
    },
};
