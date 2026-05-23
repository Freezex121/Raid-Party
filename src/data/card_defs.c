#include "card_defs.h"

#define CARD(ID, NAME, TYPE, CLASS, COST, DAMAGE, HEAL, HEAL2, HEAL_SELF, SHIELD, BURN_STACKS, TAUNT, TAUNT_TURNS, INTERRUPT, AGGRO_SELF, EXHAUST, CHANNEL, CHANNEL_TURNS, TARGET, DESC) \
    { ID, NAME, TYPE, CLASS, COST, DAMAGE, HEAL, HEAL2, HEAL_SELF, SHIELD, BURN_STACKS, TAUNT, TAUNT_TURNS, INTERRUPT, AGGRO_SELF, EXHAUST, CHANNEL, CHANNEL_TURNS, TARGET, DESC }

const CardDef guardian_cards[CLASS_CARD_COUNT] = {
    CARD("grd_taunt",       "Taunt",            CARD_SKILL,  CLASS_GUARDIAN, 1, 0,  0,  0, false, 8,  0, true,  2, false, 0,  false, false, 0, TARGET_SELF,  "Taunt enemies. Gain Shield."),
    CARD("grd_shield_slam", "Shield Slam",      CARD_ATTACK, CLASS_GUARDIAN, 1, 8,  0,  0, false, 4,  0, false, 0, false, 8,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy. Gain Shield."),
    CARD("grd_stance",      "Defensive Stance", CARD_SKILL,  CLASS_GUARDIAN, 1, 0,  0,  0, false, 14, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  "Gain a large amount of Shield."),
    CARD("grd_provoke",     "Provoke",          CARD_SKILL,  CLASS_GUARDIAN, 0, 0,  0,  0, false, 0,  0, true,  1, false, 0,  false, false, 0, TARGET_SELF,  "Taunt enemies for 1 turn."),
    CARD("grd_bash",        "Bash",             CARD_ATTACK, CLASS_GUARDIAN, 2, 12, 0,  0, false, 0,  0, true,  1, false, 12, false, false, 0, TARGET_ENEMY, "Deal heavy damage to an enemy. Taunt."),
    CARD("grd_block",       "Block",            CARD_SKILL,  CLASS_GUARDIAN, 1, 0,  0,  0, false, 10, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  "Gain Shield."),
    CARD("grd_shield_wall", "Shield Wall",      CARD_SKILL,  CLASS_GUARDIAN, 3, 0,  0,  0, false, 28, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  "Gain a massive amount of Shield."),
};

const CardDef cleric_cards[CLASS_CARD_COUNT] = {
    CARD("clr_heal",      "Heal",              CARD_SKILL,  CLASS_CLERIC, 1, 0,  12, 0,  false, 0, 0, false, 0, false, 6,  false, false, 0, TARGET_ALLY,  "Heal an ally."),
    CARD("clr_flash",     "Flash Heal",        CARD_SKILL,  CLASS_CLERIC, 2, 0,  20, 0,  false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ALLY,  "Heal an ally for a large amount."),
    CARD("clr_renew",     "Renew",             CARD_SKILL,  CLASS_CLERIC, 1, 0,  0,  0,  false, 0, 0, false, 0, false, 4,  false, false, 0, TARGET_ALLY,  "Heal an ally over 3 turns."),
    CARD("clr_smite",     "Smite",             CARD_ATTACK, CLASS_CLERIC, 1, 7,  4,  0,  false, 0, 0, false, 0, false, 7,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy. Heal the lowest HP ally."),
    CARD("clr_revive",    "Revive",            CARD_SKILL,  CLASS_CLERIC, 2, 0,  0,  0,  false, 0, 0, false, 0, false, 0,  true,  false, 0, TARGET_ALLY,  "Revive a downed ally. Exhaust."),
    CARD("clr_holy_fire", "Holy Fire",         CARD_ATTACK, CLASS_CLERIC, 1, 10, 0,  0,  false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ENEMY, "Deal damage to an enemy."),
    CARD("clr_prayer",    "Prayer of Healing", CARD_SKILL,  CLASS_CLERIC, 0, 0,  30, 0,  false, 0, 0, false, 0, false, 0,  true,  true,  2, TARGET_SELF,  "Channel for 2 turns. Heal all allies."),
    CARD("clr_divine",    "Divine Light",      CARD_SKILL,  CLASS_CLERIC, 3, 0,  35, 0,  false, 0, 0, false, 0, false, 0,  true,  false, 0, TARGET_ALLY,  "Heal an ally for a massive amount."),
};

const CardDef mage_cards[CLASS_CARD_COUNT] = {
    CARD("mag_fireball",  "Fireball",        CARD_ATTACK, CLASS_MAGE, 2, 16, 0, 0, false, 0, 0, false, 0, false, 16, false, false, 0, TARGET_ENEMY, "Deal heavy damage to an enemy."),
    CARD("mag_missiles",  "Arcane Missiles", CARD_ATTACK, CLASS_MAGE, 1, 4,  0, 0, false, 0, 0, false, 0, false, 4,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy three times."),
    CARD("mag_frostbolt", "Frostbolt",       CARD_ATTACK, CLASS_MAGE, 1, 9,  0, 0, false, 0, 0, false, 0, false, 9,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy."),
    CARD("mag_scorch",    "Scorch",          CARD_ATTACK, CLASS_MAGE, 0, 5,  0, 0, false, 0, 0, false, 0, false, 5,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy."),
    CARD("mag_pyro",      "Pyroblast",       CARD_ATTACK, CLASS_MAGE, 3, 28, 0, 0, false, 0, 0, false, 0, false, 28, true,  false, 0, TARGET_ENEMY, "Deal massive damage to an enemy. Exhaust."),
    CARD("mag_combust",   "Combustion",      CARD_SKILL,  CLASS_MAGE, 1, 0,  0, 0, false, 0, 3, false, 0, false, 0,  false, false, 0, TARGET_ENEMY, "Apply Burning to an enemy."),
    CARD("mag_firestorm", "Fire Storm",      CARD_SKILL,  CLASS_MAGE, 0, 30, 0, 0, false, 0, 0, false, 0, false, 0,  true,  true,  2, TARGET_SELF,  "Channel for 2 turns. Deal damage to all enemies."),
    CARD("mag_inferno",   "Inferno",         CARD_ATTACK, CLASS_MAGE, 3, 28, 0, 0, false, 0, 0, false, 0, false, 28, false, false, 0, TARGET_ENEMY, "Deal massive damage to an enemy."),
};

const CardDef rogue_cards[CLASS_CARD_COUNT] = {
    CARD("rog_backstab", "Backstab",      CARD_ATTACK, CLASS_ROGUE, 1, 12, 0, 0, false, 0, 0, false, 0, false, 12, false, false, 0, TARGET_ENEMY, "Deal damage to an enemy."),
    CARD("rog_kick",     "Kick",          CARD_ATTACK, CLASS_ROGUE, 1, 6,  0, 0, false, 0, 0, false, 0, true,  6,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy. Interrupt."),
    CARD("rog_poison",   "Poison Blade",  CARD_ATTACK, CLASS_ROGUE, 1, 7,  0, 0, false, 0, 0, false, 0, false, 7,  false, false, 0, TARGET_ENEMY, "Deal damage to an enemy."),
    CARD("rog_stealth",  "Stealth",       CARD_SKILL,  CLASS_ROGUE, 1, 0,  0, 0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  "Reset your aggro to 0."),
    CARD("rog_smoke",    "Smoke Bomb",    CARD_SKILL,  CLASS_ROGUE, 1, 0,  0, 0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_ALLY,  "Transfer aggro from an ally to the tank."),
    CARD("rog_evis",     "Eviscerate",    CARD_ATTACK, CLASS_ROGUE, 2, 18, 0, 0, false, 0, 0, false, 0, false, 18, false, false, 0, TARGET_ENEMY, "Deal heavy damage to an enemy."),
    CARD("rog_shadow",   "Shadow Strike", CARD_ATTACK, CLASS_ROGUE, 3, 26, 0, 0, false, 0, 0, false, 0, false, 26, false, false, 0, TARGET_ENEMY, "Deal massive damage to an enemy."),
};

const CardDef shaman_cards[CLASS_CARD_COUNT] = {
    CARD("shm_lightning",  "Lightning Bolt", CARD_ATTACK, CLASS_SHAMAN, 1, 10, 0,  0, false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ENEMY,       "Deal damage to an enemy."),
    CARD("shm_heal_totem", "Healing Totem",  CARD_SKILL,  CLASS_SHAMAN, 1, 0,  0,  0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,        "Heal all allies for 3 turns."),
    CARD("shm_silence",    "Silence",        CARD_SKILL,  CLASS_SHAMAN, 1, 0,  0,  0, false, 0, 0, false, 0, true,  0,  false, false, 0, TARGET_ENEMY,       "Interrupt an enemy's cast."),
    CARD("shm_fury",       "Windfury",       CARD_ATTACK, CLASS_SHAMAN, 1, 6,  0,  0, false, 0, 0, false, 0, false, 6,  false, false, 0, TARGET_ENEMY,       "Deal damage to an enemy twice."),
    CARD("shm_chain",      "Chain Heal",     CARD_SKILL,  CLASS_SHAMAN, 2, 0,  10, 5, false, 0, 0, false, 0, false, 5,  false, false, 0, TARGET_ALLY,        "Heal an ally, then a second ally for less."),
    CARD("shm_earth",      "Earth Shield",   CARD_SKILL,  CLASS_SHAMAN, 1, 0,  0,  0, false, 8, 0, false, 0, false, 0,  false, false, 0, TARGET_ALLY,        "Grant Shield to an ally."),
    CARD("shm_storm",      "Thunder Storm",  CARD_ATTACK, CLASS_SHAMAN, 3, 22, 0,  0, false, 0, 0, false, 0, false, 22, false, false, 0, TARGET_ALL_ENEMIES, "Deal damage to all enemies."),
};

const CardDef ranger_cards[CLASS_CARD_COUNT] = {
    CARD("rng_shot",      "Shoot",         CARD_ATTACK, CLASS_RANGER, 1, 10, 0, 0, false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ENEMY,       "Deal damage to an enemy."),
    CARD("rng_multishot", "Multishot",     CARD_ATTACK, CLASS_RANGER, 1, 6,  0, 0, false, 0, 0, false, 0, false, 6,  false, false, 0, TARGET_ALL_ENEMIES, "Deal damage to all enemies."),
    CARD("rng_trap",      "Snare Trap",    CARD_SKILL,  CLASS_RANGER, 1, 0,  0, 0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_ENEMY,       "Reduce an enemy's next attack."),
    CARD("rng_aim",       "Aimed Shot",    CARD_ATTACK, CLASS_RANGER, 2, 20, 0, 0, false, 0, 0, false, 0, false, 20, false, false, 0, TARGET_ENEMY,       "Deal heavy damage to an enemy."),
    CARD("rng_heal",      "Tranquil Shot", CARD_ATTACK, CLASS_RANGER, 1, 6,  6, 0, true,  0, 0, false, 0, false, 6,  false, false, 0, TARGET_ENEMY,       "Deal damage to an enemy. Heal self."),
    CARD("rng_barrage",   "Barrage",       CARD_ATTACK, CLASS_RANGER, 2, 14, 0, 0, false, 0, 0, false, 0, false, 14, false, false, 0, TARGET_ENEMY,       "Deal damage to an enemy."),
    CARD("rng_snipe",     "Snipe",         CARD_ATTACK, CLASS_RANGER, 3, 25, 0, 0, false, 0, 0, false, 0, false, 25, false, false, 0, TARGET_ENEMY,       "Deal massive damage to an enemy."),
};

const CardDef utility_cards[UTILITY_CARD_COUNT] = {
    CARD("util_prep",  "Preparation", CARD_SKILL, CLASS_NONE, 0, 0, 0, 0, false, 0, 0, false, 0, false, 0, false, false, 0, TARGET_SELF,       "Draw 2 cards."),
    CARD("util_energ", "Energize",    CARD_SKILL, CLASS_NONE, 0, 0, 0, 0, false, 0, 0, false, 0, false, 0, false, false, 0, TARGET_SELF,       "Gain 2 energy."),
    CARD("util_for",   "Fortify",     CARD_SKILL, CLASS_NONE, 1, 0, 0, 0, false, 6, 0, false, 0, false, 0, false, false, 0, TARGET_ALL_ALLIES, "Grant 6 Shield to all allies."),
    CARD("util_rejuv", "Rejuvenate",  CARD_SKILL, CLASS_NONE, 1, 0, 8, 0, false, 0, 0, false, 0, false, 0, false, false, 0, TARGET_ALL_ALLIES, "Heal all allies for 8."),
};

const CardDef *class_card_sets[CLASS_COUNT] = {
    [CLASS_GUARDIAN] = guardian_cards,
    [CLASS_CLERIC]   = cleric_cards,
    [CLASS_MAGE]     = mage_cards,
    [CLASS_ROGUE]    = rogue_cards,
    [CLASS_SHAMAN]   = shaman_cards,
    [CLASS_RANGER]   = ranger_cards,
};

const int class_card_counts[CLASS_COUNT] = {
    [CLASS_GUARDIAN] = GUARDIAN_CARD_COUNT,
    [CLASS_CLERIC]   = CLERIC_CARD_COUNT,
    [CLASS_MAGE]     = MAGE_CARD_COUNT,
    [CLASS_ROGUE]    = ROGUE_CARD_COUNT,
    [CLASS_SHAMAN]   = SHAMAN_CARD_COUNT,
    [CLASS_RANGER]   = RANGER_CARD_COUNT,
};

#undef CARD
