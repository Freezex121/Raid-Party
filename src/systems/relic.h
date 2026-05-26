#ifndef RELIC_H
#define RELIC_H

#include <stdbool.h>

#define MAX_RUN_RELICS 48
#define RELIC_REWARD_CHOICES 3

typedef enum {
    RELIC_NONE = -1,
    RELIC_WARD_STONE,
    RELIC_MENDING_BEAD,
    RELIC_BATTLE_DRUM,
    RELIC_SCOUTING_MAP,
    RELIC_GILDED_CHARM,
    RELIC_VETERAN_SIGIL,
    RELIC_PHOENIX_FEATHER,
    RELIC_THORNED_AMULET,
    RELIC_MIRROR_SHIELD,
    RELIC_BLOOD_AMBER,
    RELIC_POISON_FANG,
    RELIC_SPIRIT_STONE,
    RELIC_BANDAGE_ROLL,
    RELIC_EXPLORER_LANTERN,
    RELIC_MANA_GEM,
    RELIC_LUCKY_COIN,
    RELIC_SCHOLAR_NOTES,
    RELIC_CRYSTAL_BALL,
    RELIC_VOID_STONE,
    RELIC_TITAN_HEART,
    RELIC_FRUGAL_TOME,
    RELIC_RABBIT_FOOT,
    RELIC_ECHO_BELL,
    RELIC_LEECH_BLADE,
    RELIC_TOXIC_VIAL,
    RELIC_WHETSTONE,
    RELIC_PRAYER_BEADS,
    RELIC_BALLAST_RING,
    RELIC_QUICKDRAW_GLOVE,
    RELIC_WARDEN_CREST,
    RELIC_GLASS_CALTROPS,
    RELIC_LANTERN_OIL,
    RELIC_HUNTERS_COMPASS,
    RELIC_FIELD_RATIONS,
    RELIC_VICTORY_PURSE,
    RELIC_RESONANT_CHARM,
    RELIC_EXECUTIONERS_SEAL,
    RELIC_BOTTLED_STORM,
    RELIC_VEIL_PIN,
    RELIC_SPLIT_PRISM,
    RELIC_STEADFAST_BANNER,
    RELIC_ASHEN_CONTRACT,
    RELIC_MARK_OF_THE_HUNT,
    RELIC_GRAVE_BELL,
    RELIC_SYNERGY_HOURGLASS,
    RELIC_LINGERING_SIGIL,
    RELIC_GILDED_BLADE,
    RELIC_PROSPERITY_CHARM,
    RELIC_GOLDEN_IDOL,
    RELIC_HOARDERS_SCALES,
    RELIC_FATES_INTEREST,
    RELIC_COUNT
} RelicId;

typedef struct {
    RelicId id;
    const char *name;
    const char *icon;
    const char *description;
    int rarity;
} RelicDef;

const char *relic_id_string(RelicId id);
bool relic_defs_load_json(const char *path);
const RelicDef *relic_def(RelicId id);
int relic_loaded_count(void);
bool relic_has(const RelicId *owned, int count, RelicId id);
bool relic_add_unique(RelicId *owned, int *count, RelicId id);
RelicId relic_random_unowned(const RelicId *owned, int count);
RelicId relic_random_unowned_by_rarity(const RelicId *owned, int count, int max_rarity);
int relic_generate_choices(const RelicId *owned, int count, RelicId *out, int max_choices);
int relic_generate_choices_by_rarity(const RelicId *owned, int count, RelicId *out, int max_choices, int max_rarity);

#endif
