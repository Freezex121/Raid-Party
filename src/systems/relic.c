#include "relic.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

static RelicDef relic_defs[RELIC_COUNT];
static bool relic_loaded[RELIC_COUNT];
static int loaded_count = 0;

static const char *relic_ids[RELIC_COUNT] = {
    [RELIC_WARD_STONE] = "ward_stone",
    [RELIC_MENDING_BEAD] = "mending_bead",
    [RELIC_BATTLE_DRUM] = "battle_drum",
    [RELIC_SCOUTING_MAP] = "scouting_map",
    [RELIC_GILDED_CHARM] = "gilded_charm",
    [RELIC_VETERAN_SIGIL] = "veteran_sigil",
    [RELIC_PHOENIX_FEATHER] = "phoenix_feather",
    [RELIC_THORNED_AMULET] = "thorned_amulet",
    [RELIC_MIRROR_SHIELD] = "mirror_shield",
    [RELIC_BLOOD_AMBER] = "blood_amber",
    [RELIC_POISON_FANG] = "poison_fang",
    [RELIC_SPIRIT_STONE] = "spirit_stone",
    [RELIC_BANDAGE_ROLL] = "bandage_roll",
    [RELIC_EXPLORER_LANTERN] = "explorer_lantern",
    [RELIC_MANA_GEM] = "mana_gem",
    [RELIC_LUCKY_COIN] = "lucky_coin",
    [RELIC_SCHOLAR_NOTES] = "scholar_notes",
    [RELIC_CRYSTAL_BALL] = "crystal_ball",
    [RELIC_VOID_STONE] = "void_stone",
    [RELIC_TITAN_HEART] = "titan_heart",
    [RELIC_FRUGAL_TOME] = "frugal_tome",
    [RELIC_RABBIT_FOOT] = "rabbit_foot",
    [RELIC_ECHO_BELL] = "echo_bell",
    [RELIC_LEECH_BLADE] = "leech_blade",
    [RELIC_TOXIC_VIAL] = "toxic_vial",
    [RELIC_WHETSTONE] = "whetstone",
    [RELIC_PRAYER_BEADS] = "prayer_beads",
    [RELIC_BALLAST_RING] = "ballast_ring",
    [RELIC_QUICKDRAW_GLOVE] = "quickdraw_glove",
    [RELIC_WARDEN_CREST] = "warden_crest",
    [RELIC_GLASS_CALTROPS] = "glass_caltrops",
    [RELIC_LANTERN_OIL] = "lantern_oil",
    [RELIC_HUNTERS_COMPASS] = "hunters_compass",
    [RELIC_FIELD_RATIONS] = "field_rations",
    [RELIC_VICTORY_PURSE] = "victory_purse",
    [RELIC_RESONANT_CHARM] = "resonant_charm",
    [RELIC_EXECUTIONERS_SEAL] = "executioners_seal",
    [RELIC_BOTTLED_STORM] = "bottled_storm",
    [RELIC_VEIL_PIN] = "veil_pin",
    [RELIC_SPLIT_PRISM] = "split_prism",
    [RELIC_STEADFAST_BANNER] = "steadfast_banner",
    [RELIC_ASHEN_CONTRACT] = "ashen_contract",
    [RELIC_MARK_OF_THE_HUNT] = "mark_of_the_hunt",
    [RELIC_GRAVE_BELL] = "grave_bell",
    [RELIC_SYNERGY_HOURGLASS] = "synergy_hourglass",
    [RELIC_LINGERING_SIGIL] = "lingering_sigil",
};

static char *copy_text(const char *text)
{
    if (!text) text = "";
    size_t len = strlen(text);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, text, len + 1);
    return out;
}

static const JsonValue *field(const JsonValue *object, const char *key)
{
    return json_object_get(object, key);
}

static RelicId relic_id_from_text(const char *id)
{
    if (!id) return RELIC_NONE;
    for (int i = 0; i < RELIC_COUNT; i++)
        if (relic_ids[i] && strcmp(relic_ids[i], id) == 0)
            return (RelicId)i;
    return RELIC_NONE;
}

const char *relic_id_string(RelicId id)
{
    if (id < 0 || id >= RELIC_COUNT) return NULL;
    return relic_ids[id];
}

bool relic_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *relics = field(root, "relics");
    if (!relics || relics->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: relics must be an array", path);
        json_free(root);
        return false;
    }

    memset(relic_defs, 0, sizeof(relic_defs));
    memset(relic_loaded, 0, sizeof(relic_loaded));
    loaded_count = 0;

    int count = json_array_count(relics);
    for (int i = 0; i < count; i++)
    {
        const JsonValue *item = json_array_get(relics, i);
        if (!item || item->type != JSON_OBJECT) continue;

        const char *id_text = json_string(field(item, "id"), NULL);
        RelicId id = relic_id_from_text(id_text);
        if (id == RELIC_NONE)
        {
            LOG_W(CAT_SCREEN, "Skipping unknown relic id %s", id_text ? id_text : "<null>");
            continue;
        }

        relic_defs[id].id = id;
        relic_defs[id].name = copy_text(json_string(field(item, "name"), ""));
        relic_defs[id].icon = copy_text(json_string(field(item, "icon"), ""));
        relic_defs[id].description = copy_text(json_string(field(item, "description"), ""));
        relic_defs[id].rarity = json_int(field(item, "rarity"), 1);
        if (relic_defs[id].rarity < 1) relic_defs[id].rarity = 1;
        if (!relic_loaded[id])
        {
            relic_loaded[id] = true;
            loaded_count++;
        }
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d relics from %s", loaded_count, path);
    return loaded_count > 0;
}

const RelicDef *relic_def(RelicId id)
{
    if (id < 0 || id >= RELIC_COUNT || !relic_loaded[id]) return NULL;
    return &relic_defs[id];
}

int relic_loaded_count(void)
{
    return loaded_count;
}

bool relic_has(const RelicId *owned, int count, RelicId id)
{
    if (!owned || id < 0 || id >= RELIC_COUNT) return false;
    for (int i = 0; i < count; i++)
        if (owned[i] == id)
            return true;
    return false;
}

bool relic_add_unique(RelicId *owned, int *count, RelicId id)
{
    if (!owned || !count || id < 0 || id >= RELIC_COUNT || !relic_loaded[id]) return false;
    if (*count >= MAX_RUN_RELICS) return false;
    if (relic_has(owned, *count, id)) return false;
    owned[*count] = id;
    (*count)++;
    return true;
}

RelicId relic_random_unowned(const RelicId *owned, int count)
{
    return relic_random_unowned_by_rarity(owned, count, 99);
}

RelicId relic_random_unowned_by_rarity(const RelicId *owned, int count, int max_rarity)
{
    RelicId pool[RELIC_COUNT];
    int pool_count = 0;
    for (int i = 0; i < RELIC_COUNT; i++)
    {
        RelicId id = (RelicId)i;
        if (relic_loaded[id] && relic_defs[id].rarity <= max_rarity && !relic_has(owned, count, id))
            pool[pool_count++] = id;
    }

    if (pool_count <= 0) return RELIC_NONE;
    return pool[rand() % pool_count];
}

int relic_generate_choices(const RelicId *owned, int count, RelicId *out, int max_choices)
{
    return relic_generate_choices_by_rarity(owned, count, out, max_choices, 99);
}

int relic_generate_choices_by_rarity(const RelicId *owned, int count, RelicId *out, int max_choices, int max_rarity)
{
    if (!out || max_choices <= 0) return 0;

    RelicId pool[RELIC_COUNT];
    int pool_count = 0;
    for (int i = 0; i < RELIC_COUNT; i++)
    {
        RelicId id = (RelicId)i;
        if (relic_loaded[id] && relic_defs[id].rarity <= max_rarity && !relic_has(owned, count, id))
            pool[pool_count++] = id;
    }

    int choice_count = pool_count < max_choices ? pool_count : max_choices;
    for (int i = 0; i < choice_count; i++)
    {
        int pick = i + rand() % (pool_count - i);
        RelicId tmp = pool[i];
        pool[i] = pool[pick];
        pool[pick] = tmp;
        out[i] = pool[i];
    }

    return choice_count;
}
