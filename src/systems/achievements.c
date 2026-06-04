#include "achievements.h"
#include "systems/party.h"
#include "util/json.h"
#include "util/log.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int first_run;
    int won;
    bool final_area;
    int party_size_eq;
    int party_size_min;
    int party_size_max;
    int deaths_min;
    int deaths_max;
    int relics_min;
    int run_interrupts_min;
    int total_interrupts_min;
    int best_combat_turns_max;
    int floor_min;
    int bosses_min;
    int total_bosses_min;
    int runs_min;
    int wins_min;
    int area_index_min;
    int ascension_min;
    const char *requires_class;
} AchievementCondition;

typedef struct {
    const char *key;
    const char *name;
    const char *description;
    int reward;
    AchievementCondition condition;
    bool loaded;
} AchievementDef;

static AchievementDef achievement_defs[ACH_COUNT];
static AchievementId achievement_order[ACH_COUNT];
static int loaded_count = 0;

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

static void normalize_key(const char *in, char *out, int out_size)
{
    if (!out || out_size <= 0) return;
    out[0] = '\0';
    if (!in) return;

    int start = 0;
    if (strncmp(in, "ACH_", 4) == 0 || strncmp(in, "ach_", 4) == 0)
        start = 4;

    int used = 0;
    for (int i = start; in[i] && used < out_size - 1; i++)
    {
        char c = in[i];
        if (c == ' ' || c == '-')
            c = '_';
        out[used++] = (char)tolower((unsigned char)c);
    }
    out[used] = '\0';
}

static void achievement_condition_defaults(AchievementCondition *condition)
{
    memset(condition, 0, sizeof(*condition));
    condition->first_run = -1;
    condition->won = -1;
    condition->party_size_eq = -1;
    condition->party_size_min = -1;
    condition->party_size_max = -1;
    condition->deaths_min = -1;
    condition->deaths_max = -1;
    condition->relics_min = -1;
    condition->run_interrupts_min = -1;
    condition->total_interrupts_min = -1;
    condition->best_combat_turns_max = -1;
    condition->floor_min = -1;
    condition->bosses_min = -1;
    condition->total_bosses_min = -1;
    condition->runs_min = -1;
    condition->wins_min = -1;
    condition->area_index_min = -1;
    condition->ascension_min = -1;
}

static void achievements_reset(void)
{
    memset(achievement_defs, 0, sizeof(achievement_defs));
    memset(achievement_order, 0, sizeof(achievement_order));
    loaded_count = 0;
    for (int i = 0; i < ACH_COUNT; i++)
        achievement_condition_defaults(&achievement_defs[i].condition);
}

static int json_optional_bool(const JsonValue *object, const char *key)
{
    const JsonValue *value = field(object, key);
    if (!value) return -1;
    return json_bool(value, false) ? 1 : 0;
}

static int json_optional_int(const JsonValue *object, const char *key)
{
    const JsonValue *value = field(object, key);
    if (!value) return -1;
    return json_int(value, -1);
}

static void parse_condition(const JsonValue *json, AchievementCondition *condition)
{
    achievement_condition_defaults(condition);
    if (!json || json->type != JSON_OBJECT)
        return;

    condition->first_run = json_optional_bool(json, "first_run");
    condition->won = json_optional_bool(json, "won");
    condition->final_area = json_bool(field(json, "final_area"), false);
    condition->party_size_eq = json_optional_int(json, "party_size_eq");
    condition->party_size_min = json_optional_int(json, "party_size_min");
    condition->party_size_max = json_optional_int(json, "party_size_max");
    condition->deaths_min = json_optional_int(json, "deaths_min");
    condition->deaths_max = json_optional_int(json, "deaths_max");
    condition->relics_min = json_optional_int(json, "relics_min");
    condition->run_interrupts_min = json_optional_int(json, "run_interrupts_min");
    condition->total_interrupts_min = json_optional_int(json, "total_interrupts_min");
    condition->best_combat_turns_max = json_optional_int(json, "best_combat_turns_max");
    condition->floor_min = json_optional_int(json, "floor_min");
    condition->bosses_min = json_optional_int(json, "bosses_min");
    condition->total_bosses_min = json_optional_int(json, "total_bosses_min");
    condition->runs_min = json_optional_int(json, "runs_min");
    condition->wins_min = json_optional_int(json, "wins_min");
    condition->area_index_min = json_optional_int(json, "area_index_min");
    condition->ascension_min = json_optional_int(json, "ascension_min");
    condition->requires_class = copy_text(json_string(field(json, "requires_class"), ""));
}

static int next_free_slot(void)
{
    for (int i = 0; i < ACH_COUNT; i++)
        if (!achievement_defs[i].loaded)
            return i;
    return -1;
}

bool achievements_load_json(const char *path)
{
    achievements_reset();

    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *items = field(root, "achievements");
    if (!items || items->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: achievements must be an array", path);
        json_free(root);
        return false;
    }

    int count = json_array_count(items);
    for (int i = 0; i < count; i++)
    {
        const JsonValue *item = json_array_get(items, i);
        if (!item || item->type != JSON_OBJECT) continue;

        const char *key = json_string(field(item, "key"), "");
        if (!key || !key[0])
            continue;

        int slot = json_optional_int(item, "slot");
        if (slot < 0)
            slot = next_free_slot();
        if (slot < 0 || slot >= ACH_COUNT)
        {
            LOG_E(CAT_SCREEN, "%s: achievement '%s' has invalid slot %d", path, key, slot);
            continue;
        }

        AchievementDef *def = &achievement_defs[slot];
        def->key = copy_text(key);
        def->name = copy_text(json_string(field(item, "name"), "Achievement"));
        def->description = copy_text(json_string(field(item, "description"), ""));
        def->reward = json_int(field(item, "renown"), 0);
        parse_condition(field(item, "condition"), &def->condition);
        def->loaded = true;
        if (loaded_count < ACH_COUNT)
            achievement_order[loaded_count++] = slot;
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d achievement definitions from %s", loaded_count, path);
    return loaded_count > 0;
}

int achievement_count(void)
{
    return loaded_count;
}

AchievementId achievement_at(int order_index)
{
    if (order_index < 0 || order_index >= loaded_count)
        return ACH_COUNT;
    return achievement_order[order_index];
}

bool achievement_loaded(AchievementId id)
{
    return id >= 0 && id < ACH_COUNT && achievement_defs[id].loaded;
}

static bool stats_party_has_class(const AchievementRunStats *stats, const char *class_id_text)
{
    if (!stats || !class_id_text || !class_id_text[0])
        return true;

    ClassType ct = class_from_id(class_id_text);
    if (ct == CLASS_NONE)
        return false;

    if (!stats->party_classes)
        return false;

    for (int i = 0; i < stats->party_size; i++)
        if ((ClassType)stats->party_classes[i] == ct)
            return true;
    return false;
}

bool achievement_condition_met(AchievementId id, const AchievementRunStats *stats)
{
    if (!achievement_loaded(id) || !stats)
        return false;

    const AchievementCondition *c = &achievement_defs[id].condition;
    if (c->first_run >= 0 && stats->first_run != (c->first_run != 0)) return false;
    if (c->won >= 0 && stats->won != (c->won != 0)) return false;
    if (c->final_area && (!stats->won || stats->area_count <= 0 || stats->area_index < stats->area_count - 1)) return false;
    if (c->party_size_eq >= 0 && stats->party_size != c->party_size_eq) return false;
    if (c->party_size_min >= 0 && stats->party_size < c->party_size_min) return false;
    if (c->party_size_max >= 0 && stats->party_size > c->party_size_max) return false;
    if (c->deaths_min >= 0 && stats->deaths < c->deaths_min) return false;
    if (c->deaths_max >= 0 && stats->deaths > c->deaths_max) return false;
    if (c->relics_min >= 0 && stats->relic_count < c->relics_min) return false;
    if (c->run_interrupts_min >= 0 && stats->run_interrupts < c->run_interrupts_min) return false;
    if (c->total_interrupts_min >= 0 && stats->total_interrupts < c->total_interrupts_min) return false;
    if (c->best_combat_turns_max >= 0 && (stats->best_combat_turns <= 0 || stats->best_combat_turns > c->best_combat_turns_max)) return false;
    if (c->floor_min >= 0 && stats->floor_reached < c->floor_min) return false;
    if (c->bosses_min >= 0 && stats->bosses_defeated < c->bosses_min) return false;
    if (c->total_bosses_min >= 0 && stats->total_bosses_defeated < c->total_bosses_min) return false;
    if (c->runs_min >= 0 && stats->runs_completed < c->runs_min) return false;
    if (c->wins_min >= 0 && stats->wins < c->wins_min) return false;
    if (c->area_index_min >= 0 && (!stats->won || stats->area_index < c->area_index_min)) return false;
    if (c->ascension_min >= 0 && (!stats->won || stats->ascension_level < c->ascension_min)) return false;
    if (!stats_party_has_class(stats, c->requires_class)) return false;
    return true;
}

const char *achievement_name(AchievementId id)
{
    if (!achievement_loaded(id)) return "Achievement";
    return achievement_defs[id].name ? achievement_defs[id].name : "Achievement";
}

const char *achievement_key(AchievementId id)
{
    if (!achievement_loaded(id)) return "";
    return achievement_defs[id].key ? achievement_defs[id].key : "";
}

AchievementId achievement_from_key(const char *key)
{
    char normalized[48];
    normalize_key(key, normalized, sizeof(normalized));
    if (!normalized[0]) return ACH_COUNT;

    for (int i = 0; i < ACH_COUNT; i++)
    {
        if (!achievement_defs[i].loaded) continue;
        char candidate[48];
        normalize_key(achievement_key(i), candidate, sizeof(candidate));
        if (strcmp(normalized, candidate) == 0)
            return i;
    }
    return ACH_COUNT;
}

const char *achievement_desc(AchievementId id)
{
    if (!achievement_loaded(id)) return "";
    return achievement_defs[id].description ? achievement_defs[id].description : "";
}

int achievement_reward(AchievementId id)
{
    if (!achievement_loaded(id)) return 0;
    return achievement_defs[id].reward;
}
