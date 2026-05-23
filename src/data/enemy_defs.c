#include "enemy_defs.h"

const EnemyDef training_dummy = {
    .id = "dummy", .name = "Training Dummy", .hp = 60, .max_hp = 60,
    .ability_count = 1,
    .abilities = { { "Poke", INTENT_ATTACK, 2, 1, "A gentle poke.", false, 0, 0 } },
};

// ── Floor 1: Basic Nature ──────────────────────────────────────────────────

const EnemyDef giant_spider = {
    .id = "giant_spider", .name = "Giant Spider", .hp = 35, .max_hp = 35,
    .ability_count = 2,
    .abilities = {
        { "Web Shot",    INTENT_ATTACK,      7,  1, "Deals 7 damage.", false, 0, 0 },
        { "Venom Bite",  INTENT_TANK_BUSTER, 11, 2, "Deals 11 damage.", false, 0, 0 },
    },
};

const EnemyDef dire_wolf = {
    .id = "dire_wolf", .name = "Dire Wolf", .hp = 50, .max_hp = 50,
    .ability_count = 2,
    .abilities = {
        { "Savage Bite", INTENT_ATTACK, 9,  1, "Deals 9 damage.", false, 0, 0 },
        { "Howl",        INTENT_BUFF,   0,  2, "Gains 8 shield.", false, 0, 8 },
    },
};

const EnemyDef forest_boar = {
    .id = "forest_boar", .name = "Forest Boar", .hp = 45, .max_hp = 45,
    .ability_count = 2,
    .abilities = {
        { "Gore",      INTENT_TANK_BUSTER, 10, 2, "Deals 10 damage.", false, 0, 0 },
        { "Stampede",  INTENT_AOE,          6, 1, "Deals 6 damage to all.", false, 0, 0 },
    },
};

const EnemyDef elder_treant = {
    .id = "elder_treant", .name = "Elder Treant", .hp = 50, .max_hp = 50,
    .ability_count = 2,
    .abilities = {
        { "Bark Shield",  INTENT_SHIELD, 0, 2, "Gains 12 shield.", false, 0, 12 },
        { "Healing Sap",  INTENT_HEAL,   0, 1, "Heals an ally for 10.", false, 10, 0 },
    },
};

const EnemyDef alpha_wolf = {
    .id = "alpha_wolf", .name = "Alpha Wolf", .hp = 80, .max_hp = 80,
    .ability_count = 3,
    .abilities = {
        { "Feral Strike", INTENT_ATTACK,      12, 1, "Deals 12 damage.", false, 0, 0 },
        { "Howl",         INTENT_BUFF,         0, 2, "Gains 8 shield.", false, 0, 8 },
        { "Vicious Maul", INTENT_TANK_BUSTER, 16, 2, "Deals 16 damage.", false, 0, 0 },
    },
};

// ── Floor 2: Poison ────────────────────────────────────────────────────────

const EnemyDef manticore = {
    .id = "manticore", .name = "Manticore", .hp = 60, .max_hp = 60,
    .ability_count = 2,
    .abilities = {
        { "Tail Spikes", INTENT_AOE,         7,  2, "Deals 7 damage to all.", false, 0, 0 },
        { "Venom Sting", INTENT_ATTACK,      9,  1, "Deals 9 damage.", false, 0, 0 },
    },
};

const EnemyDef basilisk = {
    .id = "basilisk", .name = "Basilisk", .hp = 55, .max_hp = 55,
    .ability_count = 2,
    .abilities = {
        { "Petrifying Gaze", INTENT_TANK_BUSTER, 14, 2, "Deals 14 damage.", false, 0, 0 },
        { "Toxic Bite",      INTENT_ATTACK,       6, 1, "Deals 6 damage.", false, 0, 0 },
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

const EnemyDef venom_priest = {
    .id = "venom_priest", .name = "Venom Priest", .hp = 40, .max_hp = 40,
    .ability_count = 3,
    .abilities = {
        { "Dark Blessing", INTENT_HEAL,   0, 1, "Heals an ally for 14.", false, 14, 0 },
        { "Poison Ward",   INTENT_SHIELD, 0, 2, "Gains 10 shield.", false, 0, 10 },
        { "Shadow Bolt",   INTENT_ATTACK, 5, 1, "Deals 5 damage.", false, 0, 0 },
    },
};

const EnemyDef greater_manticore = {
    .id = "greater_manticore", .name = "Greater Manticore", .hp = 100, .max_hp = 100,
    .ability_count = 3,
    .abilities = {
        { "Volley",      INTENT_AOE,         10, 2, "Deals 10 damage to all.", false, 0, 0 },
        { "Poison Barb", INTENT_ATTACK,       9, 1, "Deals 9 damage.", false, 0, 0 },
        { "Swoop",       INTENT_TANK_BUSTER, 16, 2, "Deals 16 damage.", false, 0, 0 },
    },
};

const EnemyDef venom_hydra = {
    .id = "venom_hydra", .name = "Venom Hydra", .hp = 150, .max_hp = 150,
    .ability_count = 3,
    .abilities = {
        { "Toxic Spray",  INTENT_AOE,          8, 2, "Deals 8 damage to all.", false, 0, 0 },
        { "Venom Bite",   INTENT_TANK_BUSTER, 15, 2, "Deals 15 damage.", false, 0, 0 },
        { "Regrow Heads", INTENT_HEAL,         0, 2, "Heals self for 18.", false, 18, 0 },
    },
};

// ── Floor 3: Fire / Dragon ─────────────────────────────────────────────────

const EnemyDef flame_imp = {
    .id = "flame_imp", .name = "Flame Imp", .hp = 40, .max_hp = 40,
    .ability_count = 2,
    .abilities = {
        { "Firebolt",     INTENT_ATTACK,      8,  1, "Deals 8 damage.", false, 0, 0 },
        { "Flame Breath", INTENT_TANK_BUSTER, 14, 2, "Deals 14 damage.", false, 0, 0 },
    },
};

const EnemyDef fire_drake = {
    .id = "fire_drake", .name = "Fire Drake", .hp = 70, .max_hp = 70,
    .ability_count = 2,
    .abilities = {
        { "Flame Breath", INTENT_AOE,         9,  2, "Deals 9 damage to all.", false, 0, 0 },
        { "Tail Swipe",   INTENT_TANK_BUSTER, 13, 1, "Deals 13 damage.", false, 0, 0 },
    },
};

const EnemyDef cinder_warden = {
    .id = "cinder_warden", .name = "Cinder Warden", .hp = 55, .max_hp = 55,
    .ability_count = 3,
    .abilities = {
        { "Blazing Shield", INTENT_SHIELD, 0, 2, "Gains 14 shield.", false, 0, 14 },
        { "Ember Heal",     INTENT_HEAL,   0, 1, "Heals an ally for 12.", false, 12, 0 },
        { "Sear",           INTENT_ATTACK, 7, 1, "Deals 7 damage.", false, 0, 0 },
    },
};

const EnemyDef living_armor = {
    .id = "living_armor", .name = "Living Armor", .hp = 65, .max_hp = 65,
    .ability_count = 2,
    .abilities = {
        { "Shield Bash",   INTENT_ATTACK, 6,  1, "Deals 6 damage. Gains 8 shield.", false, 0, 8 },
        { "Rebuild Armor", INTENT_SHIELD, 0,  2, "Gains 15 shield.", false, 0, 15 },
    },
};

const EnemyDef fire_giant = {
    .id = "fire_giant", .name = "Fire Giant", .hp = 110, .max_hp = 110,
    .ability_count = 3,
    .abilities = {
        { "Magma Fist",    INTENT_TANK_BUSTER, 18, 2, "Deals 18 damage.", false, 0, 0 },
        { "Lava Wave",     INTENT_AOE,         10, 2, "Deals 10 damage to all.", false, 0, 0 },
        { "Molten Shield", INTENT_SHIELD,       0, 2, "Gains 12 shield.", false, 0, 12 },
    },
};

const EnemyDef elder_dragon = {
    .id = "elder_dragon", .name = "Elder Dragon", .hp = 220, .max_hp = 220,
    .ability_count = 4,
    .abilities = {
        { "Inferno Breath", INTENT_AOE,         12, 2, "Deals 12 damage to all.", false, 0, 0 },
        { "Tail Sweep",     INTENT_TANK_BUSTER, 20, 2, "Deals 20 damage.", false, 0, 0 },
        { "Wing Gust",      INTENT_AOE,          8, 1, "Deals 8 damage to all.", false, 0, 0 },
        { "Ancient Fury",   INTENT_BUFF,         0, 2, "Gains 20 shield.", false, 0, 20 },
    },
};
