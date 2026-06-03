#include "screens.h"

#include <math.h>
#include <stdio.h>

#include "raylib.h"

#include "assets.h"
#include "game.h"
#include "systems/meta.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "util/text.h"

typedef enum {
    META_BRANCH_ROOT,
    META_BRANCH_ATTACK,
    META_BRANCH_DEFENSE,
    META_BRANCH_SUPPORT,
    META_BRANCH_UTILITY
} MetaBranch;

typedef enum {
    META_EFFECT_UNLOCK_PROGRESS,
    META_EFFECT_DAMAGE_RANK,
    META_EFFECT_OPENING_DAMAGE,
    META_EFFECT_EXECUTE_DRAW,
    META_EFFECT_WEAK_DAMAGE,
    META_EFFECT_ELITE_DAMAGE,
    META_EFFECT_BOSS_DAMAGE,
    META_EFFECT_WARLOCK_DAMAGE,
    META_EFFECT_CLASS_WARLOCK,
    META_EFFECT_SHIELD_RANK,
    META_EFFECT_COMBAT_START_SHIELD,
    META_EFFECT_MAX_HP_RANK,
    META_EFFECT_SHIELD_CAP_RANK,
    META_EFFECT_START_CARD_FORTIFY,
    META_EFFECT_FORTIFY_UPGRADED,
    META_EFFECT_PALADIN_SHIELD,
    META_EFFECT_CLASS_PALADIN,
    META_EFFECT_EMERGENCY_BARRIER,
    META_EFFECT_LAST_STAND,
    META_EFFECT_START_CARD_ENERGIZE,
    META_EFFECT_STARTING_ENERGY,
    META_EFFECT_START_CARD_PREP,
    META_EFFECT_PREP_UPGRADED,
    META_EFFECT_CAMP_BONUS,
    META_EFFECT_FIRST_DRAW_RANK,
    META_EFFECT_START_CARD_REJUV,
    META_EFFECT_REJUV_UPGRADED,
    META_EFFECT_HEAL_BONUS,
    META_EFFECT_BARD_DRAW,
    META_EFFECT_CLASS_BARD,
    META_EFFECT_STARTING_GOLD_RANK,
    META_EFFECT_GOLD_CONVERSION,
    META_EFFECT_MASTER_RAIDER,
    META_EFFECT_SHOP_DISCOUNT,
    META_EFFECT_REWARD_REROLL_DISCOUNT,
    META_EFFECT_REWARD_CHOICE,
    META_EFFECT_REWARD_UPGRADE_CHANCE,
    META_EFFECT_STARTING_RELIC_RANK,
    META_EFFECT_RELIC_CHOICE,
    META_EFFECT_RELIC_UNLOCK,
    META_EFFECT_SLOT4,
    META_EFFECT_FORMATION_DRILLS,
    META_EFFECT_SLOT5,
    META_EFFECT_SEASONED_ADVENTURER
} MetaNodeEffect;

typedef enum {
    META_NODE_UNLOCK_PROGRESS,
    META_NODE_A_BLADES_I,
    META_NODE_A_BLADES_II,
    META_NODE_A_BLADES_III,
    META_NODE_A_OPENING_STRIKE_I,
    META_NODE_A_OPENING_STRIKE_II,
    META_NODE_A_VICTORY_MOMENTUM_I,
    META_NODE_A_VICTORY_MOMENTUM_II,
    META_NODE_A_EXPLOIT_WEAKNESS_I,
    META_NODE_A_EXPLOIT_WEAKNESS_II,
    META_NODE_A_ELITE_HUNTER,
    META_NODE_A_BOSS_SLAYER,
    META_NODE_A_WARLOCK_RUMORS,
    META_NODE_A_UNLOCK_WARLOCK,
    META_NODE_A_WARLOCK_ADEPT,
    META_NODE_A_TROPHY_HUNTER_I,
    META_NODE_A_TROPHY_HUNTER_II,
    META_NODE_D_ARMOR_I,
    META_NODE_D_ARMOR_II,
    META_NODE_D_ARMOR_III,
    META_NODE_D_WARDENS_OATH,
    META_NODE_D_LAST_STAND,
    META_NODE_D_COMBAT_SHIELD_I,
    META_NODE_D_COMBAT_SHIELD_II,
    META_NODE_D_COMBAT_SHIELD_III,
    META_NODE_D_EMERGENCY_BARRIER,
    META_NODE_D_THICK_SKIN_I,
    META_NODE_D_THICK_SKIN_II,
    META_NODE_D_THICK_SKIN_III,
    META_NODE_D_SHIELD_CAP_I,
    META_NODE_D_SHIELD_CAP_II,
    META_NODE_D_SHIELD_CAP_III,
    META_NODE_D_SHIELD_CAP_IV,
    META_NODE_D_SHIELD_CAP_V,
    META_NODE_D_SHIELD_CAP_VI,
    META_NODE_D_TRAVELERS_PACK,
    META_NODE_D_UPGRADED_FORTIFY,
    META_NODE_D_PALADIN_OATH,
    META_NODE_D_UNLOCK_PALADIN,
    META_NODE_D_PALADIN_BULWARK,
    META_NODE_S_MANA_CRYSTAL,
    META_NODE_S_CHARGED_CRYSTAL,
    META_NODE_S_STARTING_ENERGY,
    META_NODE_S_PREPARATION,
    META_NODE_S_UPGRADED_PREPARATION,
    META_NODE_S_CAMP_SUPPLIES,
    META_NODE_S_CAMP_MASTER,
    META_NODE_S_SCOUTS_KIT_I,
    META_NODE_S_SCOUTS_KIT_II,
    META_NODE_S_SCOUTS_KIT_III,
    META_NODE_S_FIRST_AID,
    META_NODE_S_UPGRADED_REJUVENATION,
    META_NODE_S_HEALING_TOUCH_I,
    META_NODE_S_HEALING_TOUCH_II,
    META_NODE_S_BARD_SCHOOL,
    META_NODE_S_UNLOCK_BARD,
    META_NODE_S_BARD_ENCORE,
    META_NODE_S_HARMONY,
    META_NODE_U_TRAVEL_FUND_I,
    META_NODE_U_TRAVEL_FUND_II,
    META_NODE_U_TRAVEL_FUND_III,
    META_NODE_U_GOLD_CONVERSION,
    META_NODE_U_MASTER_RAIDER,
    META_NODE_U_COMPLETIONIST_BANNER,
    META_NODE_U_MERCHANT_CONTACTS_I,
    META_NODE_U_MERCHANT_CONTACTS_II,
    META_NODE_U_BLACK_MARKET,
    META_NODE_U_REWARD_REROLL_I,
    META_NODE_U_REWARD_CHOICE_I,
    META_NODE_U_REWARD_CHOICE_II,
    META_NODE_U_VETERAN_REWARDS_I,
    META_NODE_U_VETERAN_REWARDS_II,
    META_NODE_U_LEGACY_I,
    META_NODE_U_LEGACY_II,
    META_NODE_U_LEGACY_III,
    META_NODE_U_RELIC_CHOICE,
    META_NODE_U_RELIC_ECHO_BELL,
    META_NODE_U_RELIC_SPLIT_PRISM,
    META_NODE_U_RELIC_BLOOD_AMBER,
    META_NODE_U_RELIC_TITAN_HEART,
    META_NODE_U_RELIC_FRUGAL_TOME,
    META_NODE_U_PARTY_SLOT_IV,
    META_NODE_U_FORMATION_DRILLS,
    META_NODE_U_PARTY_SLOT_V,
    META_NODE_COUNT
} MetaNodeId;

typedef struct {
    const char *title;
    const char *body;
    const char *label;
    const char *fallback;
    MetaUpgradeIcon icon;
    MetaBranch branch;
    float x;
    float y;
    int prerequisite;
    int cost;
    MetaNodeEffect effect;
    int effect_value;
} MetaTreeNode;

static const MetaTreeNode TREE_NODES[META_NODE_COUNT] = {
    [META_NODE_UNLOCK_PROGRESS] = {
        "UNLOCK META PROGRESS", "Begin permanent progression.", "META", "M",
        META_ICON_UNLOCK_PROGRESS, META_BRANCH_ROOT, 0, 0, -1, 1, META_EFFECT_UNLOCK_PROGRESS, 1
    },

    [META_NODE_A_BLADES_I] = {
        "BLADES I", "+1 damage from attack cards.", "BLADES I", "B1",
        META_ICON_BLADES_I, META_BRANCH_ATTACK, 0, -150, META_NODE_UNLOCK_PROGRESS, 5, META_EFFECT_DAMAGE_RANK, 1
    },
    [META_NODE_A_BLADES_II] = {
        "BLADES II", "+2 damage from attack cards.", "BLADES II", "B2",
        META_ICON_BLADES_II, META_BRANCH_ATTACK, 0, -300, META_NODE_A_BLADES_I, 10, META_EFFECT_DAMAGE_RANK, 2
    },
    [META_NODE_A_BLADES_III] = {
        "BLADES III", "+3 damage from attack cards.", "BLADES III", "B3",
        META_ICON_BLADES_III, META_BRANCH_ATTACK, 0, -450, META_NODE_A_BLADES_II, 15, META_EFFECT_DAMAGE_RANK, 3
    },
    [META_NODE_A_BOSS_SLAYER] = {
        "BOSS SLAYER", "+1 damage against bosses.", "BOSS", "BS",
        META_ICON_BOSS_SLAYER, META_BRANCH_ATTACK, 0, -600, META_NODE_A_BLADES_III, 28, META_EFFECT_BOSS_DAMAGE, 1
    },
    [META_NODE_A_TROPHY_HUNTER_II] = {
        "TROPHY HUNTER II", "+2 damage against bosses.", "TROPHY II", "T2",
        META_ICON_TROPHY_HUNTER_II, META_BRANCH_ATTACK, 0, -750, META_NODE_A_BOSS_SLAYER, 45, META_EFFECT_BOSS_DAMAGE, 2
    },
    [META_NODE_A_OPENING_STRIKE_I] = {
        "OPENING STRIKE I", "First damaging card each combat gets +1 damage.", "OPEN I", "O1",
        META_ICON_OPENING_STRIKE_I, META_BRANCH_ATTACK, -150, -300, META_NODE_A_BLADES_I, 10, META_EFFECT_OPENING_DAMAGE, 1
    },
    [META_NODE_A_OPENING_STRIKE_II] = {
        "OPENING STRIKE II", "First damaging card each combat gets +2 damage.", "OPEN II", "O2",
        META_ICON_OPENING_STRIKE_II, META_BRANCH_ATTACK, -150, -450, META_NODE_A_OPENING_STRIKE_I, 16, META_EFFECT_OPENING_DAMAGE, 2
    },
    [META_NODE_A_VICTORY_MOMENTUM_I] = {
        "VICTORY MOMENTUM I", "First enemy kill each combat draws 1 card.", "MOM I", "M1",
        META_ICON_VICTORY_MOMENTUM_I, META_BRANCH_ATTACK, -150, -600, META_NODE_A_OPENING_STRIKE_II, 18, META_EFFECT_EXECUTE_DRAW, 1
    },
    [META_NODE_A_VICTORY_MOMENTUM_II] = {
        "VICTORY MOMENTUM II", "First enemy kill each combat draws 2 cards.", "MOM II", "M2",
        META_ICON_VICTORY_MOMENTUM_II, META_BRANCH_ATTACK, -150, -750, META_NODE_A_VICTORY_MOMENTUM_I, 24, META_EFFECT_EXECUTE_DRAW, 2
    },
    [META_NODE_A_EXPLOIT_WEAKNESS_I] = {
        "EXPLOIT WEAKNESS I", "+1 damage to enemies at half HP or lower.", "WEAK I", "W1",
        META_ICON_EXPLOIT_WEAKNESS_I, META_BRANCH_ATTACK, 150, -300, META_NODE_A_BLADES_I, 10, META_EFFECT_WEAK_DAMAGE, 1
    },
    [META_NODE_A_EXPLOIT_WEAKNESS_II] = {
        "EXPLOIT WEAKNESS II", "+2 damage to enemies at half HP or lower.", "WEAK II", "W2",
        META_ICON_EXPLOIT_WEAKNESS_II, META_BRANCH_ATTACK, 150, -450, META_NODE_A_EXPLOIT_WEAKNESS_I, 16, META_EFFECT_WEAK_DAMAGE, 2
    },
    [META_NODE_A_ELITE_HUNTER] = {
        "ELITE HUNTER", "+1 damage in elite and boss fights.", "ELITE", "EH",
        META_ICON_ELITE_HUNTER, META_BRANCH_ATTACK, 150, -600, META_NODE_A_EXPLOIT_WEAKNESS_II, 22, META_EFFECT_ELITE_DAMAGE, 1
    },
    [META_NODE_A_TROPHY_HUNTER_I] = {
        "TROPHY HUNTER I", "+1 Renown for each boss defeated.", "TROPHY I", "T1",
        META_ICON_TROPHY_HUNTER_I, META_BRANCH_ATTACK, 150, -750, META_NODE_A_ELITE_HUNTER, 28, META_EFFECT_SEASONED_ADVENTURER, 1
    },
    [META_NODE_A_WARLOCK_RUMORS] = {
        "WARLOCK RUMORS", "Warlock damage cards get +1 damage.", "RUMORS", "WR",
        META_ICON_WARLOCK_RUMORS, META_BRANCH_ATTACK, 300, -450, META_NODE_A_UNLOCK_WARLOCK, 12, META_EFFECT_WARLOCK_DAMAGE, 1
    },
    [META_NODE_A_UNLOCK_WARLOCK] = {
        "UNLOCK WARLOCK", "Add Warlock to party selection.", "WARLOCK", "WL",
        META_ICON_UNLOCK_WARLOCK, META_BRANCH_ATTACK, 300, -300, META_NODE_A_BLADES_I, META_CLASS_UNLOCK_COST, META_EFFECT_CLASS_WARLOCK, 1
    },
    [META_NODE_A_WARLOCK_ADEPT] = {
        "WARLOCK ADEPT", "Warlock damage cards get +2 damage.", "ADEPT", "WA",
        META_ICON_WARLOCK_ADEPT, META_BRANCH_ATTACK, 300, -600, META_NODE_A_WARLOCK_RUMORS, 18, META_EFFECT_WARLOCK_DAMAGE, 2
    },

    [META_NODE_D_ARMOR_I] = {
        "ARMOR I", "+1 shield from shield cards.", "ARMOR I", "A1",
        META_ICON_ARMOR_I, META_BRANCH_DEFENSE, 180, 0, META_NODE_UNLOCK_PROGRESS, 5, META_EFFECT_SHIELD_RANK, 1
    },
    [META_NODE_D_ARMOR_II] = {
        "ARMOR II", "+2 shield from shield cards.", "ARMOR II", "A2",
        META_ICON_ARMOR_II, META_BRANCH_DEFENSE, 330, 0, META_NODE_D_ARMOR_I, 10, META_EFFECT_SHIELD_RANK, 2
    },
    [META_NODE_D_ARMOR_III] = {
        "ARMOR III", "+3 shield from shield cards.", "ARMOR III", "A3",
        META_ICON_ARMOR_III, META_BRANCH_DEFENSE, 480, 0, META_NODE_D_ARMOR_II, 15, META_EFFECT_SHIELD_RANK, 3
    },
    [META_NODE_D_WARDENS_OATH] = {
        "WARDEN'S OATH", "All allies start combat with 4 shield.", "OATH", "WO",
        META_ICON_WARDENS_OATH, META_BRANCH_DEFENSE, 630, 0, META_NODE_D_ARMOR_III, 24, META_EFFECT_COMBAT_START_SHIELD, 4
    },
    [META_NODE_D_LAST_STAND] = {
        "LAST STAND", "Once per combat, a fatal hit leaves an ally at 1 HP.", "LAST", "LS",
        META_ICON_LAST_STAND, META_BRANCH_DEFENSE, 780, 0, META_NODE_D_WARDENS_OATH, 45, META_EFFECT_LAST_STAND, 1
    },
    [META_NODE_D_COMBAT_SHIELD_I] = {
        "COMBAT SHIELD I", "All allies start combat with 1 shield.", "SHIELD I", "S1",
        META_ICON_COMBAT_SHIELD_I, META_BRANCH_DEFENSE, 330, -150, META_NODE_D_ARMOR_I, 10, META_EFFECT_COMBAT_START_SHIELD, 1
    },
    [META_NODE_D_COMBAT_SHIELD_II] = {
        "COMBAT SHIELD II", "All allies start combat with 2 shield.", "SHIELD II", "S2",
        META_ICON_COMBAT_SHIELD_II, META_BRANCH_DEFENSE, 480, -150, META_NODE_D_COMBAT_SHIELD_I, 16, META_EFFECT_COMBAT_START_SHIELD, 2
    },
    [META_NODE_D_COMBAT_SHIELD_III] = {
        "COMBAT SHIELD III", "All allies start combat with 3 shield.", "SHIELD III", "S3",
        META_ICON_COMBAT_SHIELD_III, META_BRANCH_DEFENSE, 630, -150, META_NODE_D_COMBAT_SHIELD_II, 22, META_EFFECT_COMBAT_START_SHIELD, 3
    },
    [META_NODE_D_EMERGENCY_BARRIER] = {
        "EMERGENCY BARRIER", "Once per combat, first damaged ally gains 4 shield.", "BARRIER", "EB",
        META_ICON_EMERGENCY_BARRIER, META_BRANCH_DEFENSE, 780, -150, META_NODE_D_COMBAT_SHIELD_III, 30, META_EFFECT_EMERGENCY_BARRIER, 1
    },
    [META_NODE_D_THICK_SKIN_I] = {
        "THICK SKIN I", "All party members start runs with +2 max HP.", "HP I", "H1",
        META_ICON_THICK_SKIN_I, META_BRANCH_DEFENSE, 330, 150, META_NODE_D_ARMOR_I, 10, META_EFFECT_MAX_HP_RANK, 1
    },
    [META_NODE_D_THICK_SKIN_II] = {
        "THICK SKIN II", "All party members start runs with +4 max HP.", "HP II", "H2",
        META_ICON_THICK_SKIN_II, META_BRANCH_DEFENSE, 480, 150, META_NODE_D_THICK_SKIN_I, 16, META_EFFECT_MAX_HP_RANK, 2
    },
    [META_NODE_D_THICK_SKIN_III] = {
        "THICK SKIN III", "All party members start runs with +6 max HP.", "HP III", "H3",
        META_ICON_THICK_SKIN_III, META_BRANCH_DEFENSE, 630, 150, META_NODE_D_THICK_SKIN_II, 24, META_EFFECT_MAX_HP_RANK, 3
    },
    [META_NODE_D_SHIELD_CAP_I] = {
        "AEGIS CAPACITY I", "Max Shield rises to 75% of max HP.", "CAP I", "C1",
        META_ICON_SHIELD_CAP_I, META_BRANCH_DEFENSE, 780, 450, META_NODE_D_THICK_SKIN_III, 12, META_EFFECT_SHIELD_CAP_RANK, 1
    },
    [META_NODE_D_SHIELD_CAP_II] = {
        "AEGIS CAPACITY II", "Max Shield rises to 100% of max HP.", "CAP II", "C2",
        META_ICON_SHIELD_CAP_II, META_BRANCH_DEFENSE, 930, 450, META_NODE_D_SHIELD_CAP_I, 16, META_EFFECT_SHIELD_CAP_RANK, 2
    },
    [META_NODE_D_SHIELD_CAP_III] = {
        "AEGIS CAPACITY III", "Max Shield rises to 125% of max HP.", "CAP III", "C3",
        META_ICON_SHIELD_CAP_III, META_BRANCH_DEFENSE, 1080, 450, META_NODE_D_SHIELD_CAP_II, 20, META_EFFECT_SHIELD_CAP_RANK, 3
    },
    [META_NODE_D_SHIELD_CAP_IV] = {
        "AEGIS CAPACITY IV", "Max Shield rises to 150% of max HP.", "CAP IV", "C4",
        META_ICON_SHIELD_CAP_IV, META_BRANCH_DEFENSE, 1230, 450, META_NODE_D_SHIELD_CAP_III, 24, META_EFFECT_SHIELD_CAP_RANK, 4
    },
    [META_NODE_D_SHIELD_CAP_V] = {
        "AEGIS CAPACITY V", "Max Shield rises to 175% of max HP.", "CAP V", "C5",
        META_ICON_SHIELD_CAP_V, META_BRANCH_DEFENSE, 1380, 450, META_NODE_D_SHIELD_CAP_IV, 30, META_EFFECT_SHIELD_CAP_RANK, 5
    },
    [META_NODE_D_SHIELD_CAP_VI] = {
        "AEGIS CAPACITY VI", "Max Shield rises to 200% of max HP.", "CAP VI", "C6",
        META_ICON_SHIELD_CAP_VI, META_BRANCH_DEFENSE, 1530, 450, META_NODE_D_SHIELD_CAP_V, 40, META_EFFECT_SHIELD_CAP_RANK, 6
    },
    [META_NODE_D_TRAVELERS_PACK] = {
        "TRAVELERS PACK", "Begin each run with Fortify.", "FORTIFY", "F",
        META_ICON_TRAVELERS_PACK, META_BRANCH_DEFENSE, 480, -300, META_NODE_D_ARMOR_II, 12, META_EFFECT_START_CARD_FORTIFY, 1
    },
    [META_NODE_D_UPGRADED_FORTIFY] = {
        "UPGRADED FORTIFY", "Starting Fortify is upgraded.", "FORTIFY+", "F+",
        META_ICON_UPGRADED_FORTIFY, META_BRANCH_DEFENSE, 630, -300, META_NODE_D_TRAVELERS_PACK, 18, META_EFFECT_FORTIFY_UPGRADED, 1
    },
    [META_NODE_D_PALADIN_OATH] = {
        "PALADIN OATH", "Paladin shield cards get +1 shield.", "P OATH", "PO",
        META_ICON_PALADIN_OATH, META_BRANCH_DEFENSE, 630, 300, META_NODE_D_UNLOCK_PALADIN, 12, META_EFFECT_PALADIN_SHIELD, 1
    },
    [META_NODE_D_UNLOCK_PALADIN] = {
        "UNLOCK PALADIN", "Add Paladin to party selection.", "PALADIN", "P",
        META_ICON_UNLOCK_PALADIN, META_BRANCH_DEFENSE, 480, 300, META_NODE_D_ARMOR_II, META_CLASS_UNLOCK_COST, META_EFFECT_CLASS_PALADIN, 1
    },
    [META_NODE_D_PALADIN_BULWARK] = {
        "PALADIN BULWARK", "Paladin shield cards get +2 shield.", "BULWARK", "PB",
        META_ICON_PALADIN_BULWARK, META_BRANCH_DEFENSE, 780, 300, META_NODE_D_PALADIN_OATH, 18, META_EFFECT_PALADIN_SHIELD, 2
    },

    [META_NODE_S_MANA_CRYSTAL] = {
        "MANA CRYSTAL", "Begin each run with Energize.", "MANA", "EN",
        META_ICON_MANA_CRYSTAL, META_BRANCH_SUPPORT, 0, 150, META_NODE_UNLOCK_PROGRESS, 12, META_EFFECT_START_CARD_ENERGIZE, 1
    },
    [META_NODE_S_CHARGED_CRYSTAL] = {
        "CHARGED CRYSTAL", "+1 starting energy each combat.", "CHARGE", "C1",
        META_ICON_CHARGED_CRYSTAL, META_BRANCH_SUPPORT, 0, 300, META_NODE_S_MANA_CRYSTAL, 18, META_EFFECT_STARTING_ENERGY, 1
    },
    [META_NODE_S_STARTING_ENERGY] = {
        "STARTING ENERGY", "+2 starting energy each combat.", "ENERGY", "C2",
        META_ICON_STARTING_ENERGY, META_BRANCH_SUPPORT, 0, 450, META_NODE_S_CHARGED_CRYSTAL, 28, META_EFFECT_STARTING_ENERGY, 2
    },
    [META_NODE_S_PREPARATION] = {
        "PREPARATION", "Begin each run with Preparation.", "PREP", "PR",
        META_ICON_PREPARATION, META_BRANCH_SUPPORT, -150, 300, META_NODE_S_MANA_CRYSTAL, 10, META_EFFECT_START_CARD_PREP, 1
    },
    [META_NODE_S_UPGRADED_PREPARATION] = {
        "UPGRADED PREPARATION", "Starting Preparation is upgraded.", "PREP+", "P+",
        META_ICON_UPGRADED_PREPARATION, META_BRANCH_SUPPORT, -150, 450, META_NODE_S_PREPARATION, 16, META_EFFECT_PREP_UPGRADED, 1
    },
    [META_NODE_S_CAMP_SUPPLIES] = {
        "CAMP SUPPLIES", "Healing at rest sites grants 5 gold.", "CAMP I", "C1",
        META_ICON_CAMP_SUPPLIES, META_BRANCH_SUPPORT, -150, 600, META_NODE_S_UPGRADED_PREPARATION, 14, META_EFFECT_CAMP_BONUS, 1
    },
    [META_NODE_S_CAMP_MASTER] = {
        "CAMP MASTER", "Healing at rest sites grants 10 gold.", "CAMP II", "C2",
        META_ICON_CAMP_MASTER, META_BRANCH_SUPPORT, -150, 750, META_NODE_S_CAMP_SUPPLIES, 24, META_EFFECT_CAMP_BONUS, 2
    },
    [META_NODE_S_SCOUTS_KIT_I] = {
        "SCOUT'S KIT I", "+1 card in your opening hand.", "SCOUT I", "D1",
        META_ICON_SCOUTS_KIT_I, META_BRANCH_SUPPORT, -300, 300, META_NODE_S_MANA_CRYSTAL, 10, META_EFFECT_FIRST_DRAW_RANK, 1
    },
    [META_NODE_S_SCOUTS_KIT_II] = {
        "SCOUT'S KIT II", "+2 cards in your opening hand.", "SCOUT II", "D2",
        META_ICON_SCOUTS_KIT_II, META_BRANCH_SUPPORT, -300, 450, META_NODE_S_SCOUTS_KIT_I, 18, META_EFFECT_FIRST_DRAW_RANK, 2
    },
    [META_NODE_S_SCOUTS_KIT_III] = {
        "SCOUT'S KIT III", "+3 cards in your opening hand.", "SCOUT III", "D3",
        META_ICON_SCOUTS_KIT_III, META_BRANCH_SUPPORT, -300, 600, META_NODE_S_SCOUTS_KIT_II, 28, META_EFFECT_FIRST_DRAW_RANK, 3
    },
    [META_NODE_S_FIRST_AID] = {
        "FIRST AID", "Begin each run with Rejuvenation.", "FIRST AID", "HP",
        META_ICON_FIRST_AID, META_BRANCH_SUPPORT, 150, 300, META_NODE_S_MANA_CRYSTAL, 12, META_EFFECT_START_CARD_REJUV, 1
    },
    [META_NODE_S_UPGRADED_REJUVENATION] = {
        "UPGRADED REJUVENATION", "Starting Rejuvenation is upgraded.", "REJUV+", "R+",
        META_ICON_UPGRADED_REJUVENATION, META_BRANCH_SUPPORT, 150, 450, META_NODE_S_FIRST_AID, 16, META_EFFECT_REJUV_UPGRADED, 1
    },
    [META_NODE_S_HEALING_TOUCH_I] = {
        "HEALING TOUCH I", "+1 healing from healing cards.", "HEAL I", "H1",
        META_ICON_HEALING_TOUCH_I, META_BRANCH_SUPPORT, 150, 600, META_NODE_S_UPGRADED_REJUVENATION, 14, META_EFFECT_HEAL_BONUS, 1
    },
    [META_NODE_S_HEALING_TOUCH_II] = {
        "HEALING TOUCH II", "+2 healing from healing cards.", "HEAL II", "H2",
        META_ICON_HEALING_TOUCH_II, META_BRANCH_SUPPORT, 150, 750, META_NODE_S_HEALING_TOUCH_I, 22, META_EFFECT_HEAL_BONUS, 2
    },
    [META_NODE_S_BARD_SCHOOL] = {
        "BARD SCHOOL", "First Bard card each combat draws 1 card.", "SCHOOL", "BS",
        META_ICON_BARD_SCHOOL, META_BRANCH_SUPPORT, 300, 450, META_NODE_S_UNLOCK_BARD, 12, META_EFFECT_BARD_DRAW, 1
    },
    [META_NODE_S_UNLOCK_BARD] = {
        "UNLOCK BARD", "Add Bard to party selection.", "BARD", "B",
        META_ICON_UNLOCK_BARD, META_BRANCH_SUPPORT, 300, 300, META_NODE_S_MANA_CRYSTAL, META_CLASS_UNLOCK_COST, META_EFFECT_CLASS_BARD, 1
    },
    [META_NODE_S_BARD_ENCORE] = {
        "BARD ENCORE", "First Bard card each combat draws 2 cards.", "ENCORE", "BE",
        META_ICON_BARD_ENCORE, META_BRANCH_SUPPORT, 300, 600, META_NODE_S_BARD_SCHOOL, 18, META_EFFECT_BARD_DRAW, 2
    },
    [META_NODE_S_HARMONY] = {
        "HARMONY", "First Bard card each combat draws 3 cards.", "HARMONY", "HY",
        META_ICON_HARMONY, META_BRANCH_SUPPORT, 300, 750, META_NODE_S_BARD_ENCORE, 35, META_EFFECT_BARD_DRAW, 3
    },

    [META_NODE_U_TRAVEL_FUND_I] = {
        "TRAVEL FUND I", "+10 starting gold.", "FUND I", "G1",
        META_ICON_TRAVEL_FUND_I, META_BRANCH_UTILITY, -180, 0, META_NODE_UNLOCK_PROGRESS, 6, META_EFFECT_STARTING_GOLD_RANK, 1
    },
    [META_NODE_U_TRAVEL_FUND_II] = {
        "TRAVEL FUND II", "+20 starting gold.", "FUND II", "G2",
        META_ICON_TRAVEL_FUND_II, META_BRANCH_UTILITY, -330, 0, META_NODE_U_TRAVEL_FUND_I, 12, META_EFFECT_STARTING_GOLD_RANK, 2
    },
    [META_NODE_U_TRAVEL_FUND_III] = {
        "TRAVEL FUND III", "+30 starting gold.", "FUND III", "G3",
        META_ICON_TRAVEL_FUND_III, META_BRANCH_UTILITY, -480, 0, META_NODE_U_TRAVEL_FUND_II, 18, META_EFFECT_STARTING_GOLD_RANK, 3
    },
    [META_NODE_U_GOLD_CONVERSION] = {
        "GOLD CONVERSION", "Leftover gold converts to Renown at 1 per 45 gold.", "CONVERT", "GC",
        META_ICON_GOLD_CONVERSION, META_BRANCH_UTILITY, -630, 0, META_NODE_U_TRAVEL_FUND_III, 22, META_EFFECT_GOLD_CONVERSION, 1
    },
    [META_NODE_U_MASTER_RAIDER] = {
        "MASTER RAIDER", "+2 Renown for winning a run.", "MASTER", "MR",
        META_ICON_MASTER_RAIDER, META_BRANCH_UTILITY, -780, 0, META_NODE_U_GOLD_CONVERSION, 35, META_EFFECT_MASTER_RAIDER, 1
    },
    [META_NODE_U_COMPLETIONIST_BANNER] = {
        "COMPLETIONIST BANNER", "Leftover gold converts to Renown at 1 per 40 gold.", "BANNER", "CB",
        META_ICON_COMPLETIONIST_BANNER, META_BRANCH_UTILITY, -930, 0, META_NODE_U_MASTER_RAIDER, 55, META_EFFECT_GOLD_CONVERSION, 2
    },
    [META_NODE_U_MERCHANT_CONTACTS_I] = {
        "MERCHANT CONTACTS I", "Shop prices are reduced by 2 gold.", "SHOP I", "M1",
        META_ICON_MERCHANT_CONTACTS_I, META_BRANCH_UTILITY, -480, -150, META_NODE_U_TRAVEL_FUND_III, 12, META_EFFECT_SHOP_DISCOUNT, 1
    },
    [META_NODE_U_MERCHANT_CONTACTS_II] = {
        "MERCHANT CONTACTS II", "Shop prices are reduced by 4 gold.", "SHOP II", "M2",
        META_ICON_MERCHANT_CONTACTS_II, META_BRANCH_UTILITY, -630, -150, META_NODE_U_MERCHANT_CONTACTS_I, 18, META_EFFECT_SHOP_DISCOUNT, 2
    },
    [META_NODE_U_BLACK_MARKET] = {
        "BLACK MARKET", "Shop prices are reduced by 6 gold.", "MARKET", "BM",
        META_ICON_BLACK_MARKET, META_BRANCH_UTILITY, -780, -150, META_NODE_U_MERCHANT_CONTACTS_II, 28, META_EFFECT_SHOP_DISCOUNT, 3
    },
    [META_NODE_U_REWARD_REROLL_I] = {
        "REWARD REROLL I", "Card reward rerolls cost 2 less gold.", "REROLL", "RR",
        META_ICON_REWARD_REROLL_I, META_BRANCH_UTILITY, -480, 150, META_NODE_U_TRAVEL_FUND_III, 10, META_EFFECT_REWARD_REROLL_DISCOUNT, 1
    },
    [META_NODE_U_REWARD_CHOICE_I] = {
        "REWARD CHOICE I", "+1 card reward option.", "CHOICE I", "C1",
        META_ICON_REWARD_CHOICE_I, META_BRANCH_UTILITY, -630, 150, META_NODE_U_REWARD_REROLL_I, 18, META_EFFECT_REWARD_CHOICE, 1
    },
    [META_NODE_U_REWARD_CHOICE_II] = {
        "REWARD CHOICE II", "+2 card reward options.", "CHOICE II", "C2",
        META_ICON_REWARD_CHOICE_II, META_BRANCH_UTILITY, -780, 150, META_NODE_U_REWARD_CHOICE_I, 28, META_EFFECT_REWARD_CHOICE, 2
    },
    [META_NODE_U_VETERAN_REWARDS_I] = {
        "VETERAN REWARDS I", "Card rewards have +10% upgraded chance.", "VET I", "V1",
        META_ICON_VETERAN_REWARDS, META_BRANCH_UTILITY, -930, 150, META_NODE_U_REWARD_CHOICE_II, 22, META_EFFECT_REWARD_UPGRADE_CHANCE, 1
    },
    [META_NODE_U_VETERAN_REWARDS_II] = {
        "VETERAN REWARDS II", "Card rewards have +20% upgraded chance.", "VET II", "V2",
        META_ICON_VETERAN_REWARDS, META_BRANCH_UTILITY, -1080, 150, META_NODE_U_VETERAN_REWARDS_I, 35, META_EFFECT_REWARD_UPGRADE_CHANCE, 2
    },
    [META_NODE_U_LEGACY_I] = {
        "LEGACY I", "Start runs with a random common relic.", "LEGACY I", "L1",
        META_ICON_LEGACY_I, META_BRANCH_UTILITY, -480, 360, META_NODE_U_TRAVEL_FUND_I, 20, META_EFFECT_STARTING_RELIC_RANK, 1
    },
    [META_NODE_U_LEGACY_II] = {
        "LEGACY II", "Start runs with a random uncommon relic.", "LEGACY II", "L2",
        META_ICON_LEGACY_II, META_BRANCH_UTILITY, -630, 360, META_NODE_U_LEGACY_I, 30, META_EFFECT_STARTING_RELIC_RANK, 2
    },
    [META_NODE_U_LEGACY_III] = {
        "LEGACY III", "Choose from 2 starting relics.", "LEGACY III", "L3",
        META_ICON_LEGACY_III, META_BRANCH_UTILITY, -780, 360, META_NODE_U_LEGACY_II, 45, META_EFFECT_STARTING_RELIC_RANK, 3
    },
    [META_NODE_U_RELIC_CHOICE] = {
        "RELIC CHOICE", "+1 choice on relic reward screens.", "RELIC+", "RC",
        META_ICON_RELIC_CHOICE, META_BRANCH_UTILITY, -930, 360, META_NODE_U_LEGACY_III, 35, META_EFFECT_RELIC_CHOICE, 1
    },
    [META_NODE_U_RELIC_ECHO_BELL] = {
        "ECHO BELL", "Echo Bell can appear in relic rewards.", "ECHO BELL", "EB",
        META_ICON_RELIC_ECHO_BELL, META_BRANCH_UTILITY, -1080, 360, META_NODE_U_RELIC_CHOICE, 28, META_EFFECT_RELIC_UNLOCK, META_RELIC_UNLOCK_ECHO_BELL
    },
    [META_NODE_U_RELIC_SPLIT_PRISM] = {
        "SPLIT PRISM", "Split Prism can appear in relic rewards.", "PRISM", "SP",
        META_ICON_RELIC_SPLIT_PRISM, META_BRANCH_UTILITY, -1230, 360, META_NODE_U_RELIC_ECHO_BELL, 34, META_EFFECT_RELIC_UNLOCK, META_RELIC_UNLOCK_SPLIT_PRISM
    },
    [META_NODE_U_RELIC_BLOOD_AMBER] = {
        "BLOOD AMBER", "Blood Amber can appear in relic rewards.", "AMBER", "BA",
        META_ICON_RELIC_BLOOD_AMBER, META_BRANCH_UTILITY, -1080, 240, META_NODE_U_RELIC_CHOICE, 32, META_EFFECT_RELIC_UNLOCK, META_RELIC_UNLOCK_BLOOD_AMBER
    },
    [META_NODE_U_RELIC_TITAN_HEART] = {
        "TITAN HEART", "Titan Heart can appear in relic rewards.", "TITAN", "TH",
        META_ICON_RELIC_TITAN_HEART, META_BRANCH_UTILITY, -1080, 510, META_NODE_U_RELIC_CHOICE, 32, META_EFFECT_RELIC_UNLOCK, META_RELIC_UNLOCK_TITAN_HEART
    },
    [META_NODE_U_RELIC_FRUGAL_TOME] = {
        "FRUGAL TOME", "Frugal Tome can appear in relic rewards.", "FRUGAL", "FT",
        META_ICON_RELIC_FRUGAL_TOME, META_BRANCH_UTILITY, -1230, 510, META_NODE_U_RELIC_TITAN_HEART, 34, META_EFFECT_RELIC_UNLOCK, META_RELIC_UNLOCK_FRUGAL_TOME
    },
    [META_NODE_U_PARTY_SLOT_IV] = {
        "PARTY SLOT IV", "Unlock a fourth party member.", "SLOT IV", "4",
        META_ICON_PARTY_SLOT_IV, META_BRANCH_UTILITY, -630, -300, META_NODE_U_MERCHANT_CONTACTS_II, META_SLOT4_COST, META_EFFECT_SLOT4, 1
    },
    [META_NODE_U_FORMATION_DRILLS] = {
        "FORMATION DRILLS", "Unlock training toward a fifth party slot.", "DRILLS", "FD",
        META_ICON_FORMATION_DRILLS, META_BRANCH_UTILITY, -780, -300, META_NODE_U_PARTY_SLOT_IV, 24, META_EFFECT_FORMATION_DRILLS, 1
    },
    [META_NODE_U_PARTY_SLOT_V] = {
        "PARTY SLOT V", "Unlock a fifth party member.", "SLOT V", "5",
        META_ICON_PARTY_SLOT_V, META_BRANCH_UTILITY, -930, -300, META_NODE_U_FORMATION_DRILLS, META_SLOT5_COST, META_EFFECT_SLOT5, 1
    },
};

static Rectangle back_btn = {12, 330, 86, 22};
static Button back_button;
static bool back_button_ready = false;
static char shop_msg[160];
static Vector2 tree_camera_center = {0.0f, 0.0f};
static Vector2 previous_mouse = {0.0f, 0.0f};
static bool previous_mouse_valid = false;
static bool dragging_tree_camera = false;
static float tree_zoom = 0.62f;

#define TREE_ZOOM_MIN 0.24f
#define TREE_ZOOM_MAX 1.85f

static void draw_tree_text(const char *text, Rectangle bounds, int font_size, Color color, TextAlign align) {
    draw_text_box(bounds, text, font_size, 0, color, align);
}

static Rectangle tree_viewport(void) {
    return (Rectangle){12, 65, 616, 246};
}

static void ensure_back_button(void) {
    if (back_button_ready)
        return;
    back_button = button_create(
        back_btn,
        "BACK",
        (Color){50, 58, 73, 255},
        (Color){74, 84, 104, 255},
        WHITE);
    back_button_ready = true;
}

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void reset_tree_camera(void) {
    tree_camera_center = (Vector2){0.0f, 0.0f};
    tree_zoom = 0.62f;
}

static Vector2 tree_viewport_center(void) {
    Rectangle viewport = tree_viewport();
    return (Vector2){viewport.x + viewport.width * 0.5f, viewport.y + viewport.height * 0.5f};
}

static Vector2 tree_world_to_screen(Vector2 world) {
    Vector2 center = tree_viewport_center();
    return (Vector2){
        center.x + (world.x - tree_camera_center.x) * tree_zoom,
        center.y + (world.y - tree_camera_center.y) * tree_zoom
    };
}

static Vector2 tree_screen_to_world(Vector2 screen) {
    Vector2 center = tree_viewport_center();
    return (Vector2){
        tree_camera_center.x + (screen.x - center.x) / tree_zoom,
        tree_camera_center.y + (screen.y - center.y) / tree_zoom
    };
}

static Rectangle tree_icon_rect_screen(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    Vector2 pos = tree_world_to_screen((Vector2){node->x, node->y});
    float size = META_UPGRADE_ICON_SIZE * tree_zoom;
    return (Rectangle){pos.x - size * 0.5f, pos.y - size * 0.5f, size, size};
}

static Rectangle tree_hit_rect_screen(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    Vector2 pos = tree_world_to_screen((Vector2){node->x, node->y});
    float size = 50.0f * tree_zoom;
    if (size < 24.0f) size = 24.0f;
    return (Rectangle){pos.x - size * 0.5f, pos.y - size * 0.5f, size, size};
}

static int tree_hovered_node(Vector2 mouse, Rectangle viewport) {
    if (!CheckCollisionPointRec(mouse, viewport))
        return -1;
    for (int i = META_NODE_COUNT - 1; i >= 0; i--)
        if (CheckCollisionPointRec(mouse, tree_hit_rect_screen(i)))
            return i;
    return -1;
}

static int tree_scaled_font(int base_size, int min_size) {
    int size = (int)((float)base_size * tree_zoom + 0.5f);
    return size < min_size ? min_size : size;
}

static const char *branch_name(MetaBranch branch) {
    switch (branch) {
        case META_BRANCH_ATTACK: return "Attack";
        case META_BRANCH_DEFENSE: return "Defense";
        case META_BRANCH_SUPPORT: return "Support";
        case META_BRANCH_UTILITY: return "Utility";
        default: return "Meta";
    }
}

static Color branch_color(MetaBranch branch) {
    switch (branch) {
        case META_BRANCH_ATTACK: return (Color){203, 82, 72, 255};
        case META_BRANCH_DEFENSE: return (Color){79, 143, 202, 255};
        case META_BRANCH_SUPPORT: return (Color){102, 174, 111, 255};
        case META_BRANCH_UTILITY: return (Color){202, 161, 78, 255};
        default: return (Color){184, 126, 218, 255};
    }
}

static int node_effect_current(const MetaTreeNode *node) {
    const MetaProgress *meta = &g_state.meta;
    switch (node->effect) {
        case META_EFFECT_UNLOCK_PROGRESS: return meta->meta_progress_unlocked ? 1 : 0;
        case META_EFFECT_DAMAGE_RANK: return meta->dmg_bonus;
        case META_EFFECT_OPENING_DAMAGE: return meta->opening_damage_bonus;
        case META_EFFECT_EXECUTE_DRAW: return meta->execute_draw_rank;
        case META_EFFECT_WEAK_DAMAGE: return meta->weak_enemy_damage_bonus;
        case META_EFFECT_ELITE_DAMAGE: return meta->elite_damage_bonus;
        case META_EFFECT_BOSS_DAMAGE: return meta->boss_damage_bonus;
        case META_EFFECT_WARLOCK_DAMAGE: return meta->warlock_damage_bonus;
        case META_EFFECT_CLASS_WARLOCK: return meta->warlock_unlocked ? 1 : 0;
        case META_EFFECT_SHIELD_RANK: return meta->shield_bonus;
        case META_EFFECT_COMBAT_START_SHIELD: return meta->combat_start_shield;
        case META_EFFECT_MAX_HP_RANK: return meta->max_hp_bonus_rank;
        case META_EFFECT_SHIELD_CAP_RANK: return meta->shield_cap_rank;
        case META_EFFECT_START_CARD_FORTIFY: return meta->start_fortify ? 1 : 0;
        case META_EFFECT_FORTIFY_UPGRADED: return meta->fortify_upgraded ? 1 : 0;
        case META_EFFECT_PALADIN_SHIELD: return meta->paladin_shield_bonus;
        case META_EFFECT_CLASS_PALADIN: return meta->paladin_unlocked ? 1 : 0;
        case META_EFFECT_EMERGENCY_BARRIER: return meta->emergency_barrier_unlocked ? 1 : 0;
        case META_EFFECT_LAST_STAND: return meta->last_stand_unlocked ? 1 : 0;
        case META_EFFECT_START_CARD_ENERGIZE: return meta->start_energize ? 1 : 0;
        case META_EFFECT_STARTING_ENERGY: return meta->starting_energy_bonus;
        case META_EFFECT_START_CARD_PREP: return meta->start_prep ? 1 : 0;
        case META_EFFECT_PREP_UPGRADED: return meta->prep_upgraded ? 1 : 0;
        case META_EFFECT_CAMP_BONUS: return meta->camp_bonus_rank;
        case META_EFFECT_FIRST_DRAW_RANK: return meta->first_draw_bonus;
        case META_EFFECT_START_CARD_REJUV: return meta->start_rejuv ? 1 : 0;
        case META_EFFECT_REJUV_UPGRADED: return meta->rejuv_upgraded ? 1 : 0;
        case META_EFFECT_HEAL_BONUS: return meta->heal_bonus;
        case META_EFFECT_BARD_DRAW: return meta->bard_draw_bonus;
        case META_EFFECT_CLASS_BARD: return meta->bard_unlocked ? 1 : 0;
        case META_EFFECT_STARTING_GOLD_RANK: return meta->starting_gold_rank;
        case META_EFFECT_GOLD_CONVERSION: return meta->gold_conversion_rank;
        case META_EFFECT_MASTER_RAIDER: return meta->master_raider ? 1 : 0;
        case META_EFFECT_SHOP_DISCOUNT: return meta->shop_discount_rank;
        case META_EFFECT_REWARD_REROLL_DISCOUNT: return meta->reward_reroll_discount_rank;
        case META_EFFECT_REWARD_CHOICE: return meta->reward_choice_bonus;
        case META_EFFECT_REWARD_UPGRADE_CHANCE: return meta->reward_upgrade_chance_rank;
        case META_EFFECT_STARTING_RELIC_RANK: return meta->starting_relic_rank;
        case META_EFFECT_RELIC_CHOICE: return meta->relic_choice_bonus;
        case META_EFFECT_RELIC_UNLOCK: return meta->relic_unlock_flags & node->effect_value;
        case META_EFFECT_SLOT4: return meta->slot4_unlocked ? 1 : 0;
        case META_EFFECT_FORMATION_DRILLS: return meta->formation_drills ? 1 : 0;
        case META_EFFECT_SLOT5: return meta->slot5_unlocked ? 1 : 0;
        case META_EFFECT_SEASONED_ADVENTURER: return meta->seasoned_adventurer ? 1 : 0;
        default: return 0;
    }
}

static int node_effect_max(MetaNodeEffect effect) {
    switch (effect) {
        case META_EFFECT_DAMAGE_RANK:
        case META_EFFECT_SHIELD_RANK:
        case META_EFFECT_FIRST_DRAW_RANK:
        case META_EFFECT_STARTING_GOLD_RANK:
        case META_EFFECT_STARTING_RELIC_RANK:
        case META_EFFECT_MAX_HP_RANK:
        case META_EFFECT_SHOP_DISCOUNT:
        case META_EFFECT_BARD_DRAW:
            return 3;
        case META_EFFECT_OPENING_DAMAGE:
        case META_EFFECT_EXECUTE_DRAW:
        case META_EFFECT_WEAK_DAMAGE:
        case META_EFFECT_BOSS_DAMAGE:
        case META_EFFECT_WARLOCK_DAMAGE:
        case META_EFFECT_PALADIN_SHIELD:
        case META_EFFECT_STARTING_ENERGY:
        case META_EFFECT_CAMP_BONUS:
        case META_EFFECT_HEAL_BONUS:
        case META_EFFECT_REWARD_CHOICE:
        case META_EFFECT_REWARD_UPGRADE_CHANCE:
        case META_EFFECT_GOLD_CONVERSION:
            return 2;
        case META_EFFECT_COMBAT_START_SHIELD:
            return 4;
        case META_EFFECT_SHIELD_CAP_RANK:
            return META_SHIELD_CAP_MAX_RANK;
        default:
            return 1;
    }
}

static bool node_activated(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    return node_effect_current(node) >= node->effect_value;
}

static bool node_complete(int node_id) {
    return node_activated(node_id);
}

static int branch_purchase_count(MetaBranch branch) {
    int count = 0;
    for (int i = 0; i < META_NODE_COUNT; i++)
        if (TREE_NODES[i].branch == branch && node_activated(i))
            count++;
    return count;
}

static bool class_gate_met(int node_id) {
    if (node_id != META_NODE_A_UNLOCK_WARLOCK &&
        node_id != META_NODE_D_UNLOCK_PALADIN &&
        node_id != META_NODE_S_UNLOCK_BARD)
        return true;
    const MetaTreeNode *node = &TREE_NODES[node_id];
    return branch_purchase_count(node->branch) >= 2;
}

static bool prerequisite_met(int node_id) {
    int prerequisite = TREE_NODES[node_id].prerequisite;
    if (prerequisite >= 0 && !node_activated(prerequisite))
        return false;
    return class_gate_met(node_id);
}

static bool node_unlocked(int node_id) {
    return prerequisite_met(node_id) && !node_complete(node_id);
}

static bool node_can_buy_now(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    return node_unlocked(node_id) && g_state.meta.renown >= node->cost;
}

static bool node_needs_renown(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    return node_unlocked(node_id) && g_state.meta.renown < node->cost;
}

static void node_detail(int node_id, char *detail, int detail_size) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    int max = node_effect_max(node->effect);
    int current = node_effect_current(node);
    if (max > 1)
        snprintf(detail, detail_size, "Rank %d/%d", current, max);
    else
        snprintf(detail, detail_size, "%s", node_activated(node_id) ? "Owned" : "Not owned");
}

static void apply_node_purchase(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    MetaProgress *meta = &g_state.meta;
    int value = node->effect_value;

    switch (node->effect) {
        case META_EFFECT_UNLOCK_PROGRESS: meta->meta_progress_unlocked = true; break;
        case META_EFFECT_DAMAGE_RANK: if (meta->dmg_bonus < value) meta->dmg_bonus = value; break;
        case META_EFFECT_OPENING_DAMAGE: if (meta->opening_damage_bonus < value) meta->opening_damage_bonus = value; break;
        case META_EFFECT_EXECUTE_DRAW: if (meta->execute_draw_rank < value) meta->execute_draw_rank = value; break;
        case META_EFFECT_WEAK_DAMAGE: if (meta->weak_enemy_damage_bonus < value) meta->weak_enemy_damage_bonus = value; break;
        case META_EFFECT_ELITE_DAMAGE: if (meta->elite_damage_bonus < value) meta->elite_damage_bonus = value; break;
        case META_EFFECT_BOSS_DAMAGE: if (meta->boss_damage_bonus < value) meta->boss_damage_bonus = value; break;
        case META_EFFECT_WARLOCK_DAMAGE: if (meta->warlock_damage_bonus < value) meta->warlock_damage_bonus = value; break;
        case META_EFFECT_CLASS_WARLOCK: meta->warlock_unlocked = true; break;
        case META_EFFECT_SHIELD_RANK: if (meta->shield_bonus < value) meta->shield_bonus = value; break;
        case META_EFFECT_COMBAT_START_SHIELD: if (meta->combat_start_shield < value) meta->combat_start_shield = value; break;
        case META_EFFECT_MAX_HP_RANK: if (meta->max_hp_bonus_rank < value) meta->max_hp_bonus_rank = value; break;
        case META_EFFECT_SHIELD_CAP_RANK: if (meta->shield_cap_rank < value) meta->shield_cap_rank = value; break;
        case META_EFFECT_START_CARD_FORTIFY: meta->start_fortify = true; break;
        case META_EFFECT_FORTIFY_UPGRADED: meta->fortify_upgraded = true; break;
        case META_EFFECT_PALADIN_SHIELD: if (meta->paladin_shield_bonus < value) meta->paladin_shield_bonus = value; break;
        case META_EFFECT_CLASS_PALADIN: meta->paladin_unlocked = true; break;
        case META_EFFECT_EMERGENCY_BARRIER: meta->emergency_barrier_unlocked = true; break;
        case META_EFFECT_LAST_STAND: meta->last_stand_unlocked = true; break;
        case META_EFFECT_START_CARD_ENERGIZE: meta->start_energize = true; break;
        case META_EFFECT_STARTING_ENERGY: if (meta->starting_energy_bonus < value) meta->starting_energy_bonus = value; break;
        case META_EFFECT_START_CARD_PREP: meta->start_prep = true; break;
        case META_EFFECT_PREP_UPGRADED: meta->prep_upgraded = true; break;
        case META_EFFECT_CAMP_BONUS: if (meta->camp_bonus_rank < value) meta->camp_bonus_rank = value; break;
        case META_EFFECT_FIRST_DRAW_RANK: if (meta->first_draw_bonus < value) meta->first_draw_bonus = value; break;
        case META_EFFECT_START_CARD_REJUV: meta->start_rejuv = true; break;
        case META_EFFECT_REJUV_UPGRADED: meta->rejuv_upgraded = true; break;
        case META_EFFECT_HEAL_BONUS: if (meta->heal_bonus < value) meta->heal_bonus = value; break;
        case META_EFFECT_BARD_DRAW: if (meta->bard_draw_bonus < value) meta->bard_draw_bonus = value; break;
        case META_EFFECT_CLASS_BARD: meta->bard_unlocked = true; break;
        case META_EFFECT_STARTING_GOLD_RANK: if (meta->starting_gold_rank < value) meta->starting_gold_rank = value; break;
        case META_EFFECT_GOLD_CONVERSION: if (meta->gold_conversion_rank < value) meta->gold_conversion_rank = value; break;
        case META_EFFECT_MASTER_RAIDER: meta->master_raider = true; break;
        case META_EFFECT_SHOP_DISCOUNT: if (meta->shop_discount_rank < value) meta->shop_discount_rank = value; break;
        case META_EFFECT_REWARD_REROLL_DISCOUNT: if (meta->reward_reroll_discount_rank < value) meta->reward_reroll_discount_rank = value; break;
        case META_EFFECT_REWARD_CHOICE: if (meta->reward_choice_bonus < value) meta->reward_choice_bonus = value; break;
        case META_EFFECT_REWARD_UPGRADE_CHANCE: if (meta->reward_upgrade_chance_rank < value) meta->reward_upgrade_chance_rank = value; break;
        case META_EFFECT_STARTING_RELIC_RANK: if (meta->starting_relic_rank < value) meta->starting_relic_rank = value; break;
        case META_EFFECT_RELIC_CHOICE: if (meta->relic_choice_bonus < value) meta->relic_choice_bonus = value; break;
        case META_EFFECT_RELIC_UNLOCK: meta->relic_unlock_flags |= value; break;
        case META_EFFECT_SLOT4: meta->slot4_unlocked = true; break;
        case META_EFFECT_FORMATION_DRILLS: meta->formation_drills = true; break;
        case META_EFFECT_SLOT5: meta->slot4_unlocked = true; meta->slot5_unlocked = true; break;
        case META_EFFECT_SEASONED_ADVENTURER: meta->seasoned_adventurer = true; break;
        default: break;
    }
}

static void try_buy_node(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    int cost = node->cost;

    if (node_complete(node_id)) {
        snprintf(shop_msg, sizeof(shop_msg), "%s is already complete.", node->title);
        return;
    }
    if (node->prerequisite >= 0 && !node_activated(node->prerequisite)) {
        snprintf(shop_msg, sizeof(shop_msg), "Requires %s.", TREE_NODES[node->prerequisite].title);
        return;
    }
    if (!class_gate_met(node_id)) {
        snprintf(shop_msg, sizeof(shop_msg), "Requires 2 %s purchases first.", branch_name(node->branch));
        return;
    }
    if (g_state.meta.renown < cost) {
        snprintf(shop_msg, sizeof(shop_msg), "Need %d Renown for %s.", cost, node->title);
        return;
    }

    g_state.meta.renown -= cost;
    apply_node_purchase(node_id);
    g_state.max_party_size = meta_party_slots(&g_state.meta);
    meta_save(&g_state.meta);
    assets_play_sfx(SFX_SHOP_PURCHASE);
    snprintf(shop_msg, sizeof(shop_msg), "Unlocked %s.", node->title);
}

static void draw_tree_grid(Rectangle viewport) {
    Vector2 top_left = tree_screen_to_world((Vector2){viewport.x, viewport.y});
    Vector2 bottom_right = tree_screen_to_world((Vector2){viewport.x + viewport.width, viewport.y + viewport.height});
    float min_x = fminf(top_left.x, bottom_right.x);
    float max_x = fmaxf(top_left.x, bottom_right.x);
    float min_y = fminf(top_left.y, bottom_right.y);
    float max_y = fmaxf(top_left.y, bottom_right.y);
    const float grid_step = 120.0f;

    int first_x = (int)floorf(min_x / grid_step) - 1;
    int last_x = (int)ceilf(max_x / grid_step) + 1;
    int first_y = (int)floorf(min_y / grid_step) - 1;
    int last_y = (int)ceilf(max_y / grid_step) + 1;

    for (int gx = first_x; gx <= last_x; gx++) {
        float world_x = (float)gx * grid_step;
        Vector2 a = tree_world_to_screen((Vector2){world_x, min_y - grid_step});
        Vector2 b = tree_world_to_screen((Vector2){world_x, max_y + grid_step});
        DrawLineEx(a, b, 1.0f, (Color){31, 37, 50, 110});
    }
    for (int gy = first_y; gy <= last_y; gy++) {
        float world_y = (float)gy * grid_step;
        Vector2 a = tree_world_to_screen((Vector2){min_x - grid_step, world_y});
        Vector2 b = tree_world_to_screen((Vector2){max_x + grid_step, world_y});
        DrawLineEx(a, b, 1.0f, (Color){31, 37, 50, 110});
    }

    DrawLineEx(tree_world_to_screen((Vector2){min_x - grid_step, 0.0f}),
               tree_world_to_screen((Vector2){max_x + grid_step, 0.0f}),
               1.5f, (Color){68, 71, 92, 130});
    DrawLineEx(tree_world_to_screen((Vector2){0.0f, min_y - grid_step}),
               tree_world_to_screen((Vector2){0.0f, max_y + grid_step}),
               1.5f, (Color){68, 71, 92, 130});
}

static void draw_branch_label(const char *text, float x, float y, MetaBranch branch) {
    Color color = branch_color(branch);
    Vector2 pos = tree_world_to_screen((Vector2){x, y});
    draw_tree_text(text,
                   (Rectangle){pos.x - 58.0f * tree_zoom, pos.y - 7.0f * tree_zoom, 116.0f * tree_zoom, 14.0f * tree_zoom},
                   tree_scaled_font(8, 5),
                   color,
                   TEXT_ALIGN_CENTER);
}

static void draw_connection(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    int parent_id = node->prerequisite;
    if (parent_id < 0) return;

    const MetaTreeNode *parent = &TREE_NODES[parent_id];
    bool owned = node_activated(node_id);
    bool can_buy = node_can_buy_now(node_id);
    bool needs_renown = node_needs_renown(node_id);
    bool parent_owned = node_activated(parent_id);
    Color color = (Color){59, 64, 76, 255};
    if (owned)
        color = branch_color(node->branch);
    else if (can_buy)
        color = (Color){255, 222, 92, 255};
    else if (needs_renown)
        color = (Color){218, 144, 70, 220};
    else if (parent_owned)
        color = (Color){83, 89, 108, 230};
    float pulse = (float)(0.5 + 0.5 * sin(GetTime() * 5.0));
    float thickness = (owned ? 4.0f : (can_buy ? 3.8f + pulse * 1.2f : 2.2f)) * tree_zoom;
    if (thickness < 1.0f) thickness = 1.0f;
    DrawLineEx(tree_world_to_screen((Vector2){parent->x, parent->y}),
               tree_world_to_screen((Vector2){node->x, node->y}),
               thickness,
               color);
}

static void draw_node_badge(Rectangle frame_rect, const char *text, Color bg, Color border, Color text_color) {
    if (tree_zoom < 0.30f)
        return;
    float badge_w = 34.0f * tree_zoom;
    float badge_h = 11.0f * tree_zoom;
    if (badge_w < 28.0f) badge_w = 28.0f;
    if (badge_h < 10.0f) badge_h = 10.0f;
    Rectangle badge = {
        frame_rect.x + frame_rect.width * 0.5f - badge_w * 0.5f,
        frame_rect.y - badge_h - 3.0f,
        badge_w,
        badge_h
    };
    DrawRectangleRec(badge, bg);
    DrawRectangleLinesEx(badge, 1.0f, border);
    draw_tree_text(text, badge, tree_scaled_font(7, 5), text_color, TEXT_ALIGN_CENTER);
}

static void draw_state_legend(void) {
    const struct {
        const char *label;
        Color color;
    } items[] = {
        { "BUY now", (Color){255, 222, 92, 255} },
        { "need Renown", (Color){218, 144, 70, 255} },
        { "owned", (Color){115, 222, 134, 255} },
        { "locked", (Color){92, 96, 112, 255} },
    };
    float x = 16.0f;
    float y = 50.0f;
    for (int i = 0; i < 4; i++)
    {
        DrawRectangleRec((Rectangle){x, y + 3.0f, 8.0f, 8.0f}, items[i].color);
        DrawRectangleLinesEx((Rectangle){x, y + 3.0f, 8.0f, 8.0f}, 1.0f, Fade(WHITE, 0.55f));
        draw_tree_text(items[i].label, (Rectangle){x + 12.0f, y, 86.0f, 14.0f}, 8, (Color){190, 196, 210, 245}, TEXT_ALIGN_LEFT);
        x += 106.0f;
    }
}

static void draw_node(int node_id, bool hovered) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    Rectangle icon_rect = tree_icon_rect_screen(node_id);
    float pad = 3.0f * tree_zoom;
    if (pad < 1.0f) pad = 1.0f;
    Rectangle frame_rect = {icon_rect.x - pad, icon_rect.y - pad, icon_rect.width + pad * 2.0f, icon_rect.height + pad * 2.0f};
    Color color = branch_color(node->branch);
    bool owned = node_activated(node_id);
    bool can_buy = node_can_buy_now(node_id);
    bool needs_renown = node_needs_renown(node_id);
    bool unlocked = can_buy || needs_renown;
    Texture2D icon = g_assets.meta_upgrade_icons[node->icon];
    float pulse = (float)(0.5 + 0.5 * sin(GetTime() * 5.0));
    Color frame_color = owned ? color :
        can_buy ? (Color){255, 222, 92, 255} :
        needs_renown ? (Color){218, 144, 70, 235} :
        (Color){72, 77, 92, 215};
    Color inner_bg = owned ? (Color){39, 46, 60, 255} :
        can_buy ? (Color){54, 48, 25, 255} :
        needs_renown ? (Color){45, 35, 28, 255} :
        (Color){24, 27, 35, 255};

    if (can_buy)
    {
        float halo_pad = (5.0f + pulse * 4.0f) * tree_zoom;
        if (halo_pad < 2.0f) halo_pad = 2.0f;
        Rectangle halo = {
            frame_rect.x - halo_pad,
            frame_rect.y - halo_pad,
            frame_rect.width + halo_pad * 2.0f,
            frame_rect.height + halo_pad * 2.0f
        };
        DrawRectangleRec(halo, Fade((Color){255, 222, 92, 255}, 0.18f + pulse * 0.14f));
        DrawRectangleLinesEx(halo, 1.0f, Fade((Color){255, 236, 140, 255}, 0.55f + pulse * 0.35f));
    }

    DrawRectangleRec(frame_rect, hovered ? Fade(frame_color, 0.98f) : Fade(frame_color, owned ? 0.78f : (unlocked ? 0.68f : 0.38f)));
    DrawRectangleLinesEx(frame_rect, hovered ? 2.0f : (can_buy ? 2.0f : 1.0f), hovered ? WHITE : frame_color);
    DrawRectangleRec(icon_rect, inner_bg);

    if (icon.id != 0) {
        DrawTexturePro(icon,
                       (Rectangle){0, 0, (float)icon.width, (float)icon.height},
                       icon_rect,
                       (Vector2){0, 0},
                       0,
                       owned || unlocked ? WHITE : (Color){96, 98, 110, 255});
    } else {
        draw_tree_text(node->fallback, icon_rect, tree_scaled_font(7, 5), owned || unlocked ? frame_color : (Color){96, 98, 110, 255}, TEXT_ALIGN_CENTER);
    }

    if (owned) {
        draw_node_badge(frame_rect, "OWNED", (Color){29, 83, 46, 245}, (Color){120, 235, 140, 255}, WHITE);
        DrawCircle((int)(frame_rect.x + frame_rect.width - 3.0f),
                   (int)(frame_rect.y + 3.0f),
                   4.0f,
                   (Color){115, 222, 134, 255});
    }
    else if (can_buy)
    {
        char cost[16];
        snprintf(cost, sizeof(cost), "%dR", node->cost);
        draw_node_badge(frame_rect, "BUY", (Color){125, 89, 24, 250}, (Color){255, 236, 140, 255}, WHITE);
        draw_tree_text(cost,
                       (Rectangle){frame_rect.x + frame_rect.width + 4.0f, frame_rect.y + frame_rect.height * 0.5f - 6.0f, 32.0f, 12.0f},
                       tree_scaled_font(7, 5),
                       (Color){255, 236, 140, 255},
                       TEXT_ALIGN_LEFT);
    }
    else if (needs_renown)
    {
        char cost[16];
        snprintf(cost, sizeof(cost), "%dR", node->cost);
        draw_node_badge(frame_rect, "NEED", (Color){92, 57, 30, 245}, (Color){230, 158, 84, 245}, WHITE);
        draw_tree_text(cost,
                       (Rectangle){frame_rect.x + frame_rect.width + 4.0f, frame_rect.y + frame_rect.height * 0.5f - 6.0f, 32.0f, 12.0f},
                       tree_scaled_font(7, 5),
                       (Color){230, 158, 84, 245},
                       TEXT_ALIGN_LEFT);
    }
    else
    {
        draw_node_badge(frame_rect, "LOCK", (Color){34, 37, 48, 235}, (Color){91, 96, 112, 220}, (Color){150, 154, 170, 235});
    }

    if (tree_zoom >= 0.42f) {
        draw_tree_text(node->label,
                       (Rectangle){frame_rect.x + frame_rect.width * 0.5f - 48.0f * tree_zoom, frame_rect.y + frame_rect.height + 2.0f, 96.0f * tree_zoom, 12.0f * tree_zoom},
                       tree_scaled_font(7, 5),
                       owned || unlocked ? WHITE : (Color){122, 127, 139, 255},
                       TEXT_ALIGN_CENTER);
    }
}

static void draw_tooltip(int node_id) {
    const MetaTreeNode *node = &TREE_NODES[node_id];
    Vector2 mouse = GetMousePosition();
    char detail[48];
    char action[112];
    Rectangle tip = {mouse.x + 12, mouse.y + 10, 250, 76};

    if (tip.x + tip.width > 634) tip.x = mouse.x - tip.width - 12;
    if (tip.y + tip.height > 350) tip.y = mouse.y - tip.height - 10;

    node_detail(node_id, detail, sizeof(detail));
    if (node_complete(node_id)) {
        snprintf(action, sizeof(action), "COMPLETE");
    } else if (node->prerequisite >= 0 && !node_activated(node->prerequisite)) {
        snprintf(action, sizeof(action), "Requires: %s", TREE_NODES[node->prerequisite].title);
    } else if (!class_gate_met(node_id)) {
        snprintf(action, sizeof(action), "Requires 2 %s purchases", branch_name(node->branch));
    } else if (g_state.meta.renown < node->cost) {
        snprintf(action, sizeof(action), "Need %d more Renown", node->cost - g_state.meta.renown);
    } else {
        snprintf(action, sizeof(action), "Click to unlock: %d Renown", node->cost);
    }

    DrawRectangleRec(tip, (Color){20, 24, 34, 248});
    DrawRectangleLinesEx(tip, 1, node_can_buy_now(node_id) ? (Color){255, 236, 140, 255} : branch_color(node->branch));
    draw_tree_text(node->title, (Rectangle){tip.x + 7, tip.y + 5, tip.width - 14, 11}, 8, WHITE, TEXT_ALIGN_LEFT);
    draw_tree_text(node->body, (Rectangle){tip.x + 7, tip.y + 20, tip.width - 14, 23}, 7, (Color){191, 196, 207, 255}, TEXT_ALIGN_LEFT);
    draw_tree_text(detail, (Rectangle){tip.x + 7, tip.y + 48, 82, 10}, 7, (Color){155, 163, 180, 255}, TEXT_ALIGN_LEFT);
    draw_tree_text(action, (Rectangle){tip.x + 7, tip.y + 61, tip.width - 14, 10}, 7, branch_color(node->branch), TEXT_ALIGN_LEFT);
}

static void update_tree_camera(Vector2 mouse, Rectangle viewport) {
    bool mouse_in_viewport = CheckCollisionPointRec(mouse, viewport);
    float wheel = mouse_in_viewport ? GetMouseWheelMove() : 0.0f;

    if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_HOME))
        reset_tree_camera();

    if (wheel != 0.0f) {
        Vector2 anchor = tree_screen_to_world(mouse);
        float next_zoom = clamp_float(tree_zoom * powf(1.16f, wheel), TREE_ZOOM_MIN, TREE_ZOOM_MAX);
        Vector2 viewport_center = tree_viewport_center();
        tree_zoom = next_zoom;
        tree_camera_center.x = anchor.x - (mouse.x - viewport_center.x) / tree_zoom;
        tree_camera_center.y = anchor.y - (mouse.y - viewport_center.y) / tree_zoom;
    }

    bool left_empty_press = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_in_viewport && tree_hovered_node(mouse, viewport) < 0;
    bool drag_started = false;
    if (((IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) && mouse_in_viewport) || left_empty_press) {
        dragging_tree_camera = true;
        drag_started = true;
    }
    bool pan_button_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
                           IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
                           (dragging_tree_camera && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    if (!pan_button_down)
        dragging_tree_camera = false;

    if (dragging_tree_camera && previous_mouse_valid && !drag_started) {
        Vector2 delta = {mouse.x - previous_mouse.x, mouse.y - previous_mouse.y};
        tree_camera_center.x -= delta.x / tree_zoom;
        tree_camera_center.y -= delta.y / tree_zoom;
    }

    float key_pan = 260.0f * GetFrameTime() / tree_zoom;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) tree_camera_center.x += key_pan;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) tree_camera_center.x -= key_pan;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) tree_camera_center.y += key_pan;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) tree_camera_center.y -= key_pan;

    previous_mouse = mouse;
    previous_mouse_valid = true;
}

void meta_shop_screen_update(void) {
    Vector2 mouse = GetMousePosition();
    Rectangle viewport = tree_viewport();
    ensure_back_button();
    button_update(&back_button);

    if (IsKeyPressed(KEY_ESCAPE) || back_button.pressed_this_frame) {
        game_change_screen(SCREEN_TITLE);
        return;
    }

    update_tree_camera(mouse, viewport);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int node_id = tree_hovered_node(mouse, viewport);
        if (node_id >= 0) {
            try_buy_node(node_id);
            return;
        }
    }
}

void meta_shop_screen_draw(void) {
    Rectangle viewport = tree_viewport();
    Vector2 mouse = GetMousePosition();
    int hovered_node = dragging_tree_camera ? -1 : tree_hovered_node(mouse, viewport);

    ClearBackground((Color){13, 17, 26, 255});
    draw_tree_text("RENOWN SKILL TREE", (Rectangle){14, 10, 320, 18}, 15, WHITE, TEXT_ALIGN_LEFT);

    char summary[64];
    snprintf(summary, sizeof(summary), "RENOWN: %d", g_state.meta.renown);
    draw_tree_text(summary, (Rectangle){475, 12, 145, 14}, 11, (Color){232, 193, 100, 255}, TEXT_ALIGN_RIGHT);
    draw_state_legend();

    DrawRectangleRec(viewport, (Color){18, 23, 34, 255});
    DrawRectangleLinesEx(viewport, 1, (Color){57, 65, 82, 255});

    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
    draw_tree_grid(viewport);
    draw_branch_label("ATTACK", 0.0f, -880.0f, META_BRANCH_ATTACK);
    draw_branch_label("DEFENSE", 900.0f, 0.0f, META_BRANCH_DEFENSE);
    draw_branch_label("SUPPORT", 0.0f, 880.0f, META_BRANCH_SUPPORT);
    draw_branch_label("UTILITY", -1120.0f, 0.0f, META_BRANCH_UTILITY);
    for (int i = 0; i < META_NODE_COUNT; i++)
        draw_connection(i);
    for (int i = 0; i < META_NODE_COUNT; i++)
        draw_node(i, i == hovered_node);
    EndScissorMode();

    if (shop_msg[0] != '\0')
        draw_tree_text(shop_msg, (Rectangle){112, 333, 508, 16}, 8, (Color){210, 215, 224, 255}, TEXT_ALIGN_LEFT);
    else {
        char zoom_line[64];
        snprintf(zoom_line, sizeof(zoom_line), "ZOOM %d%%", (int)(tree_zoom * 100.0f + 0.5f));
        draw_tree_text(zoom_line, (Rectangle){528, 333, 92, 16}, 8, (Color){150, 156, 176, 230}, TEXT_ALIGN_RIGHT);
    }

    (void)mouse;
    ensure_back_button();
    button_draw_9slice(&back_button);

    if (hovered_node >= 0)
        draw_tooltip(hovered_node);
}
