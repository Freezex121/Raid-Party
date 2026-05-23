#include "encounter_defs.h"
#include "util/json.h"
#include "util/log.h"
#include "systems/map.h"
#include <string.h>

#define MAX_NORMAL_ENCOUNTERS 8

static EncounterDef normal_storage[MAX_FLOORS][MAX_NORMAL_ENCOUNTERS];
static int normal_counts[MAX_FLOORS];
static EncounterDef elite_storage[MAX_FLOORS];
static EncounterDef boss_storage[MAX_FLOORS];
static int loaded = 0;

static const JsonValue *field(const JsonValue *object, const char *key)
{
    return json_object_get(object, key);
}

static bool load_encounter(const JsonValue *object, EncounterDef *out, const char *label)
{
    if (!object || object->type != JSON_OBJECT || !out)
        return false;

    memset(out, 0, sizeof(*out));
    const JsonValue *enemies = field(object, "enemies");
    int count = json_array_count(enemies);
    if (count > MAX_ENEMIES) count = MAX_ENEMIES;
    for (int i = 0; i < count; i++)
    {
        const char *enemy_id = json_string(json_array_get(enemies, i), NULL);
        const EnemyDef *enemy = enemy_def_by_id(enemy_id);
        if (!enemy)
        {
            LOG_E(CAT_SCREEN, "Encounter %s references missing enemy %s", label, enemy_id ? enemy_id : "<null>");
            continue;
        }
        out->enemies[out->count++] = enemy;
    }
    return out->count > 0;
}

bool encounter_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *floors = field(root, "floors");
    if (!floors || floors->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: floors must be an array", path);
        json_free(root);
        return false;
    }

    memset(normal_storage, 0, sizeof(normal_storage));
    memset(normal_counts, 0, sizeof(normal_counts));
    memset(elite_storage, 0, sizeof(elite_storage));
    memset(boss_storage, 0, sizeof(boss_storage));

    int floor_count = json_array_count(floors);
    for (int i = 0; i < floor_count; i++)
    {
        const JsonValue *floor_obj = json_array_get(floors, i);
        if (!floor_obj || floor_obj->type != JSON_OBJECT) continue;

        int floor_index = json_int(field(floor_obj, "floor"), i + 1) - 1;
        if (floor_index < 0 || floor_index >= MAX_FLOORS)
            continue;

        const JsonValue *normal = field(floor_obj, "normal");
        int normal_count = json_array_count(normal);
        if (normal_count > MAX_NORMAL_ENCOUNTERS)
            normal_count = MAX_NORMAL_ENCOUNTERS;
        for (int n = 0; n < normal_count; n++)
        {
            const JsonValue *encounter = json_array_get(normal, n);
            if (load_encounter(encounter, &normal_storage[floor_index][normal_counts[floor_index]], "normal"))
                normal_counts[floor_index]++;
        }

        load_encounter(field(floor_obj, "elite"), &elite_storage[floor_index], "elite");
        load_encounter(field(floor_obj, "boss"), &boss_storage[floor_index], "boss");
    }

    json_free(root);
    loaded = 1;
    LOG_I(CAT_SCREEN, "Loaded encounters from %s", path);
    return true;
}

const EncounterDef *encounter_for_floor(int floor, int index)
{
    if (!loaded) return NULL;
    if (floor < 0) floor = 0;
    if (floor >= MAX_FLOORS) floor = MAX_FLOORS - 1;
    int count = normal_counts[floor];
    if (count <= 0) return NULL;
    if (index < 0) index = 0;
    if (index >= count) index %= count;
    return &normal_storage[floor][index];
}

int encounter_count_for_floor(int floor)
{
    if (floor < 0) floor = 0;
    if (floor >= MAX_FLOORS) floor = MAX_FLOORS - 1;
    return normal_counts[floor];
}

const EncounterDef *elite_for_floor(int floor)
{
    if (!loaded) return NULL;
    if (floor < 0) floor = 0;
    if (floor >= MAX_FLOORS) floor = MAX_FLOORS - 1;
    return &elite_storage[floor];
}

const EncounterDef *boss_for_floor(int floor)
{
    if (!loaded) return NULL;
    if (floor < 0) floor = 0;
    if (floor >= MAX_FLOORS) floor = MAX_FLOORS - 1;
    return &boss_storage[floor];
}
