#include "card_defs.h"
#include <stddef.h>

static const CardEffect fx_util_prep[] = {
    { CARD_EFFECT_DRAW_CARDS, STATUS_NONE, 2, 0 },
};

static const CardEffect fx_util_energ[] = {
    { CARD_EFFECT_GAIN_ENERGY, STATUS_NONE, 2, 0 },
};

static const CardEffect fx_clr_renew[] = {
    { CARD_EFFECT_APPLY_STATUS_TARGET_ALLY, STATUS_RENEW, 5, 3 },
};

static const CardEffect fx_clr_revive[] = {
    { CARD_EFFECT_REVIVE_TARGET, STATUS_NONE, 0, 0 },
};

static const CardEffect fx_mag_combust[] = {
    { CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY, STATUS_BURNING, 3, 3 },
};

static const CardEffect fx_rog_stealth[] = {
    { CARD_EFFECT_RESET_CASTER_AGGRO, STATUS_NONE, 0, 0 },
};

static const CardEffect fx_rog_smoke[] = {
    { CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN, STATUS_NONE, 0, 0 },
};

static const CardEffect fx_shm_heal_totem[] = {
    { CARD_EFFECT_APPLY_STATUS_ALL_ALLIES, STATUS_TOTEM_HEAL, 4, 3 },
};

static const CardEffect fx_rng_trap[] = {
    { CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY, STATUS_TRAP, 4, 2 },
};

#define NOFX NULL, 0
#define FX(ARR) ARR, (int)(sizeof(ARR) / sizeof((ARR)[0]))
#define CARD(ID, NAME, TYPE, CLASS, COST, DAMAGE, HEAL, HEAL2, HEAL_SELF, SHIELD, BURN_STACKS, TAUNT, TAUNT_TURNS, INTERRUPT, AGGRO_SELF, EXHAUST, CHANNEL, CHANNEL_TURNS, TARGET, REPEAT_HITS, EFFECT_ARGS, DESC) \
    { ID, NAME, TYPE, CLASS, COST, DAMAGE, HEAL, HEAL2, HEAL_SELF, SHIELD, BURN_STACKS, TAUNT, TAUNT_TURNS, INTERRUPT, AGGRO_SELF, EXHAUST, CHANNEL, CHANNEL_TURNS, TARGET, REPEAT_HITS, EFFECT_ARGS, DESC }

const CardDef guardian_cards[CLASS_CARD_COUNT] = {
    CARD("grd_taunt",       "Taunt",            CARD_SKILL,  CLASS_GUARDIAN, 1, 0,  0,  0, false, 8,  0, true,  2, false, 0,  false, false, 0, TARGET_SELF,  1, NOFX, "Taunt enemies. Gain Shield."),
    CARD("grd_shield_slam", "Shield Slam",      CARD_ATTACK, CLASS_GUARDIAN, 1, 8,  0,  0, false, 4,  0, false, 0, false, 8,  false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy. Gain Shield."),
    CARD("grd_stance",      "Defensive Stance", CARD_SKILL,  CLASS_GUARDIAN, 1, 0,  0,  0, false, 14, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  1, NOFX, "Gain a large amount of Shield."),
    CARD("grd_provoke",     "Provoke",          CARD_SKILL,  CLASS_GUARDIAN, 0, 0,  0,  0, false, 0,  0, true,  1, false, 0,  false, false, 0, TARGET_SELF,  1, NOFX, "Taunt enemies for 1 turn."),
    CARD("grd_bash",        "Bash",             CARD_ATTACK, CLASS_GUARDIAN, 2, 12, 0,  0, false, 0,  0, true,  1, false, 12, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal heavy damage to an enemy. Taunt."),
    CARD("grd_block",       "Block",            CARD_SKILL,  CLASS_GUARDIAN, 1, 0,  0,  0, false, 10, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  1, NOFX, "Gain Shield."),
    CARD("grd_shield_wall", "Shield Wall",      CARD_SKILL,  CLASS_GUARDIAN, 3, 0,  0,  0, false, 28, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  1, NOFX, "Gain a massive amount of Shield."),
    CARD("grd_fortress",    "Fortress",         CARD_SKILL,  CLASS_GUARDIAN, 4, 0,  0,  0, false, 40, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  1, NOFX, "Gain an immense amount of Shield."),
};

const CardDef cleric_cards[CLASS_CARD_COUNT] = {
    CARD("clr_heal",      "Heal",              CARD_SKILL,  CLASS_CLERIC, 1, 0,  12, 0,  false, 0, 0, false, 0, false, 6,  false, false, 0, TARGET_ALLY, 1, NOFX, "Heal an ally."),
    CARD("clr_flash",     "Flash Heal",        CARD_SKILL,  CLASS_CLERIC, 2, 0,  20, 0,  false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ALLY, 1, NOFX, "Heal an ally for a large amount."),
    CARD("clr_renew",     "Renew",             CARD_SKILL,  CLASS_CLERIC, 1, 0,  0,  0,  false, 0, 0, false, 0, false, 4,  false, false, 0, TARGET_ALLY, 1, FX(fx_clr_renew), "Heal an ally over 3 turns."),
    CARD("clr_smite",     "Smite",             CARD_ATTACK, CLASS_CLERIC, 1, 7,  4,  0,  false, 0, 0, false, 0, false, 7,  false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy. Heal the lowest HP ally."),
    CARD("clr_revive",    "Revive",            CARD_SKILL,  CLASS_CLERIC, 2, 0,  0,  0,  false, 0, 0, false, 0, false, 0,  true,  false, 0, TARGET_ALLY, 1, FX(fx_clr_revive), "Revive a downed ally. Exhaust."),
    CARD("clr_holy_fire", "Holy Fire",         CARD_ATTACK, CLASS_CLERIC, 1, 10, 0,  0,  false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy."),
    CARD("clr_prayer",    "Prayer of Healing", CARD_SKILL,  CLASS_CLERIC, 0, 0,  30, 0,  false, 0, 0, false, 0, false, 0,  true,  true,  2, TARGET_SELF,  1, NOFX, "Channel for 2 turns. Heal all allies."),
    CARD("clr_divine",    "Divine Light",      CARD_SKILL,  CLASS_CLERIC, 3, 0,  35, 0,  false, 0, 0, false, 0, false, 0,  true,  false, 0, TARGET_ALLY, 1, NOFX, "Heal an ally for a massive amount."),
};

const CardDef mage_cards[CLASS_CARD_COUNT] = {
    CARD("mag_fireball",  "Fireball",        CARD_ATTACK, CLASS_MAGE, 2, 16, 0, 0, false, 0, 0, false, 0, false, 16, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal heavy damage to an enemy."),
    CARD("mag_missiles",  "Arcane Missiles", CARD_ATTACK, CLASS_MAGE, 1, 4,  0, 0, false, 0, 0, false, 0, false, 4,  false, false, 0, TARGET_ENEMY, 3, NOFX, "Deal damage to an enemy three times."),
    CARD("mag_frostbolt", "Frostbolt",       CARD_ATTACK, CLASS_MAGE, 1, 9,  0, 0, false, 0, 0, false, 0, false, 9,  false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy."),
    CARD("mag_scorch",    "Scorch",          CARD_ATTACK, CLASS_MAGE, 0, 5,  0, 0, false, 0, 0, false, 0, false, 5,  false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy."),
    CARD("mag_pyro",      "Pyroblast",       CARD_ATTACK, CLASS_MAGE, 3, 28, 0, 0, false, 0, 0, false, 0, false, 28, true,  false, 0, TARGET_ENEMY, 1, NOFX, "Deal massive damage to an enemy. Exhaust."),
    CARD("mag_combust",   "Combustion",      CARD_SKILL,  CLASS_MAGE, 1, 0,  0, 0, false, 0, 3, false, 0, false, 0,  false, false, 0, TARGET_ENEMY, 1, FX(fx_mag_combust), "Apply Burning to an enemy."),
    CARD("mag_firestorm", "Fire Storm",      CARD_SKILL,  CLASS_MAGE, 0, 30, 0, 0, false, 0, 0, false, 0, false, 0,  true,  true,  2, TARGET_SELF,  1, NOFX, "Channel for 2 turns. Deal damage to all enemies."),
    CARD("mag_inferno",   "Inferno",         CARD_ATTACK, CLASS_MAGE, 3, 28, 0, 0, false, 0, 0, false, 0, false, 28, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal massive damage to an enemy."),
    CARD("mag_meteor",    "Meteor",          CARD_ATTACK, CLASS_MAGE, 4, 35, 0, 0, false, 0, 0, false, 0, false, 35, false, false, 0, TARGET_ALL_ENEMIES, 1, NOFX, "Deal immense damage to all enemies."),
};

const CardDef rogue_cards[CLASS_CARD_COUNT] = {
    CARD("rog_backstab", "Backstab",      CARD_ATTACK, CLASS_ROGUE, 1, 12, 0, 0, false, 0, 0, false, 0, false, 12, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy."),
    CARD("rog_kick",     "Kick",          CARD_ATTACK, CLASS_ROGUE, 1, 6,  0, 0, false, 0, 0, false, 0, true,  6,  false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy. Interrupt."),
    CARD("rog_poison",   "Poison Blade",  CARD_ATTACK, CLASS_ROGUE, 1, 7,  0, 0, false, 0, 0, false, 0, false, 7,  false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal damage to an enemy."),
    CARD("rog_stealth",  "Stealth",       CARD_SKILL,  CLASS_ROGUE, 1, 0,  0, 0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,  1, FX(fx_rog_stealth), "Reset your aggro to 0."),
    CARD("rog_smoke",    "Smoke Bomb",    CARD_SKILL,  CLASS_ROGUE, 1, 0,  0, 0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_ALLY, 1, FX(fx_rog_smoke), "Transfer aggro from an ally to the tank."),
    CARD("rog_evis",     "Eviscerate",    CARD_ATTACK, CLASS_ROGUE, 2, 18, 0, 0, false, 0, 0, false, 0, false, 18, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal heavy damage to an enemy."),
    CARD("rog_shadow",   "Shadow Strike", CARD_ATTACK, CLASS_ROGUE, 3, 26, 0, 0, false, 0, 0, false, 0, false, 26, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal massive damage to an enemy."),
    CARD("rog_assass",   "Assassinate",   CARD_ATTACK, CLASS_ROGUE, 4, 42, 0, 0, false, 0, 0, false, 0, false, 42, false, false, 0, TARGET_ENEMY, 1, NOFX, "Deal devastating damage to an enemy."),
};

const CardDef shaman_cards[CLASS_CARD_COUNT] = {
    CARD("shm_lightning",  "Lightning Bolt", CARD_ATTACK, CLASS_SHAMAN, 1, 10, 0,  0, false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ENEMY,       1, NOFX, "Deal damage to an enemy."),
    CARD("shm_heal_totem", "Healing Totem",  CARD_SKILL,  CLASS_SHAMAN, 1, 0,  0,  0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_SELF,        1, FX(fx_shm_heal_totem), "Heal all allies for 3 turns."),
    CARD("shm_silence",    "Silence",        CARD_SKILL,  CLASS_SHAMAN, 1, 0,  0,  0, false, 0, 0, false, 0, true,  0,  false, false, 0, TARGET_ENEMY,       1, NOFX, "Interrupt an enemy's cast."),
    CARD("shm_fury",       "Windfury",       CARD_ATTACK, CLASS_SHAMAN, 1, 6,  0,  0, false, 0, 0, false, 0, false, 6,  false, false, 0, TARGET_ENEMY,       2, NOFX, "Deal damage to an enemy twice."),
    CARD("shm_chain",      "Chain Heal",     CARD_SKILL,  CLASS_SHAMAN, 2, 0,  10, 5, false, 0, 0, false, 0, false, 5,  false, false, 0, TARGET_ALLY,        1, NOFX, "Heal an ally, then a second ally for less."),
    CARD("shm_earth",      "Earth Shield",   CARD_SKILL,  CLASS_SHAMAN, 1, 0,  0,  0, false, 8, 0, false, 0, false, 0,  false, false, 0, TARGET_ALLY,        1, NOFX, "Grant Shield to an ally."),
    CARD("shm_storm",      "Thunder Storm",  CARD_ATTACK, CLASS_SHAMAN, 3, 22, 0,  0, false, 0, 0, false, 0, false, 22, false, false, 0, TARGET_ALL_ENEMIES, 1, NOFX, "Deal damage to all enemies."),
    CARD("shm_cataclysm",  "Cataclysm",      CARD_ATTACK, CLASS_SHAMAN, 4, 30, 0,  0, false, 0, 0, false, 0, false, 30, false, false, 0, TARGET_ALL_ENEMIES, 1, NOFX, "Deal immense damage to all enemies."),
};

const CardDef ranger_cards[CLASS_CARD_COUNT] = {
    CARD("rng_shot",      "Shoot",         CARD_ATTACK, CLASS_RANGER, 1, 10, 0, 0, false, 0, 0, false, 0, false, 10, false, false, 0, TARGET_ENEMY,       1, NOFX, "Deal damage to an enemy."),
    CARD("rng_multishot", "Multishot",     CARD_ATTACK, CLASS_RANGER, 1, 6,  0, 0, false, 0, 0, false, 0, false, 6,  false, false, 0, TARGET_ALL_ENEMIES, 1, NOFX, "Deal damage to all enemies."),
    CARD("rng_trap",      "Snare Trap",    CARD_SKILL,  CLASS_RANGER, 1, 0,  0, 0, false, 0, 0, false, 0, false, 0,  false, false, 0, TARGET_ENEMY,       1, FX(fx_rng_trap), "Reduce an enemy's next attack."),
    CARD("rng_aim",       "Aimed Shot",    CARD_ATTACK, CLASS_RANGER, 2, 20, 0, 0, false, 0, 0, false, 0, false, 20, false, false, 0, TARGET_ENEMY,       1, NOFX, "Deal heavy damage to an enemy."),
    CARD("rng_heal",      "Tranquil Shot", CARD_ATTACK, CLASS_RANGER, 1, 6,  6, 0, true,  0, 0, false, 0, false, 6,  false, false, 0, TARGET_ENEMY,       1, NOFX, "Deal damage to an enemy. Heal self."),
    CARD("rng_barrage",   "Barrage",       CARD_ATTACK, CLASS_RANGER, 2, 14, 0, 0, false, 0, 0, false, 0, false, 14, false, false, 0, TARGET_ENEMY,       1, NOFX, "Deal damage to an enemy."),
    CARD("rng_snipe",     "Snipe",         CARD_ATTACK, CLASS_RANGER, 3, 25, 0, 0, false, 0, 0, false, 0, false, 25, false, false, 0, TARGET_ENEMY,       1, NOFX, "Deal massive damage to an enemy."),
    CARD("rng_rain",      "Rain of Arrows",CARD_ATTACK, CLASS_RANGER, 4, 18, 0, 0, false, 0, 0, false, 0, false, 18, false, false, 0, TARGET_ALL_ENEMIES, 1, NOFX, "Deal heavy damage to all enemies."),
};

const CardDef utility_cards[UTILITY_CARD_COUNT] = {
    CARD("util_prep",  "Preparation", CARD_SKILL, CLASS_NONE, 0, 0, 0, 0, false, 0, 0, false, 0, false, 0, false, false, 0, TARGET_SELF,       1, FX(fx_util_prep),  "Draw 2 cards."),
    CARD("util_energ", "Energize",    CARD_SKILL, CLASS_NONE, 0, 0, 0, 0, false, 0, 0, false, 0, false, 0, false, false, 0, TARGET_SELF,       1, FX(fx_util_energ), "Gain 2 energy."),
    CARD("util_for",   "Fortify",     CARD_SKILL, CLASS_NONE, 1, 0, 0, 0, false, 6, 0, false, 0, false, 0, false, false, 0, TARGET_ALL_ALLIES, 1, NOFX,           "Grant 6 Shield to all allies."),
    CARD("util_rejuv", "Rejuvenate",  CARD_SKILL, CLASS_NONE, 1, 0, 8, 0, false, 0, 0, false, 0, false, 0, false, false, 0, TARGET_ALL_ALLIES, 1, NOFX,           "Heal all allies for 8."),
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
#undef FX
#undef NOFX
