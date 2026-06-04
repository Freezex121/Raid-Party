#include "relic.h"
#include "util/json.h"
#include "util/log.h"
#include "game.h"
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
    [RELIC_GILDED_BLADE] = "gilded_blade",
    [RELIC_PROSPERITY_CHARM] = "prosperity_charm",
    [RELIC_GOLDEN_IDOL] = "golden_idol",
    [RELIC_HOARDERS_SCALES] = "hoarders_scales",
    [RELIC_FATES_INTEREST] = "fates_interest",
    [RELIC_FURY_CHARM] = "fury_charm",
    [RELIC_WAR_DRUM] = "war_drum",
    [RELIC_RAGING_HEART] = "raging_heart",
    [RELIC_SCROLL_INSIGHT] = "scroll_insight",
    [RELIC_TOME_KNOWLEDGE] = "tome_knowledge",
    [RELIC_GRIM_SCYTHE] = "grim_scythe",
    [RELIC_SOUL_REAPER] = "soul_reaper",
    [RELIC_LEECHING_FANG] = "leeching_fang",
    [RELIC_BLOOD_PACT] = "blood_pact",
    [RELIC_DUELIST_SIGIL] = "duelist_sigil",
    [RELIC_FELLOWSHIP_STANDARD] = "fellowship_standard",
    [RELIC_CHRONICLE_QUILL] = "chronicle_quill",
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

static ClassType class_from_string(const char *text)
{
    return class_from_id(text);
}

const char *relic_id_string(RelicId id)
{
    if (id < 0 || id >= RELIC_COUNT) return NULL;
    return relic_ids[id];
}

static bool relic_meets_party_requirements(RelicId id)
{
    // Class requirement
    ClassType req = relic_defs[id].requires_class;
    if (req != CLASS_NONE)
    {
        bool ok = false;
        for (int p = 0; p < g_state.run_party.count; p++)
            if (g_state.run_party.members[p].alive && g_state.run_party.members[p].class == req)
                { ok = true; break; }
        if (!ok) return false;
    }

    // Pair requirement (both classes needed)
    ClassType a = relic_defs[id].requires_pair[0];
    ClassType b = relic_defs[id].requires_pair[1];
    if (a != CLASS_NONE && b != CLASS_NONE)
    {
        bool has_a = false, has_b = false;
        for (int p = 0; p < g_state.run_party.count; p++)
        {
            if (!g_state.run_party.members[p].alive) continue;
            if (g_state.run_party.members[p].class == a) has_a = true;
            if (g_state.run_party.members[p].class == b) has_b = true;
        }
        if (!has_a || !has_b) return false;
    }

    return true;
}

static bool relic_can_appear_in_rewards(RelicId id, const RelicId *owned, int count, int max_rarity)
{
    return relic_loaded[id] &&
        relic_defs[id].rarity <= max_rarity &&
        !relic_has(owned, count, id) &&
        meta_relic_available(&g_state.meta, id) &&
        meta_content_active(&g_state.meta, relic_defs[id].unlock_key) &&
        relic_meets_party_requirements(id);
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
        relic_defs[id].unlock_key = copy_text(json_string(field(item, "unlock"), ""));
        relic_defs[id].unlock_event = copy_text(json_string(field(item, "unlock_event"), ""));
        meta_content_register_unlock_event(relic_defs[id].unlock_key, relic_defs[id].unlock_event);
        relic_defs[id].rarity = json_int(field(item, "rarity"), 1);
        if (relic_defs[id].rarity < 1) relic_defs[id].rarity = 1;

        // Parse class requirement
        const char *req = json_string(field(item, "requires"), NULL);
        relic_defs[id].requires_class = class_from_string(req);

        // Parse pair requirement
        relic_defs[id].requires_pair[0] = CLASS_NONE;
        relic_defs[id].requires_pair[1] = CLASS_NONE;
        const JsonValue *pair = field(item, "requires_pair");
        if (pair && pair->type == JSON_ARRAY && json_array_count(pair) >= 2)
        {
            relic_defs[id].requires_pair[0] = class_from_string(json_string(json_array_get(pair, 0), NULL));
            relic_defs[id].requires_pair[1] = class_from_string(json_string(json_array_get(pair, 1), NULL));
        }

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
        if (relic_can_appear_in_rewards(id, owned, count, max_rarity))
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
        if (relic_can_appear_in_rewards(id, owned, count, max_rarity))
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
