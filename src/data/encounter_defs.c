#include "encounter_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    EncounterDef *normal;
    int normal_count;
    EncounterDef elite;
    EncounterDef boss;
} EncounterFloor;

static EncounterFloor *floors_storage = NULL;
static int floors_count = 0;
static int loaded = 0;

static const JsonValue *field(const JsonValue *object, const char *key)
{
    return json_object_get(object, key);
}

static void free_encounters(void)
{
    for (int i = 0; i < floors_count; i++)
        free(floors_storage[i].normal);
    free(floors_storage);
    floors_storage = NULL;
    floors_count = 0;
    loaded = 0;
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

    free_encounters();

    int json_floor_count = json_array_count(floors);
    int max_floor_index = -1;
    for (int i = 0; i < json_floor_count; i++)
    {
        const JsonValue *floor_obj = json_array_get(floors, i);
        if (!floor_obj || floor_obj->type != JSON_OBJECT) continue;
        int floor_index = json_int(field(floor_obj, "floor"), i + 1) - 1;
        if (floor_index > max_floor_index)
            max_floor_index = floor_index;
    }

    if (max_floor_index < 0)
    {
        json_free(root);
        return false;
    }

    floors_count = max_floor_index + 1;
    floors_storage = (EncounterFloor *)calloc((size_t)floors_count, sizeof(EncounterFloor));
    if (!floors_storage)
    {
        floors_count = 0;
        json_free(root);
        return false;
    }

    for (int i = 0; i < json_floor_count; i++)
    {
        const JsonValue *floor_obj = json_array_get(floors, i);
        if (!floor_obj || floor_obj->type != JSON_OBJECT) continue;

        int floor_index = json_int(field(floor_obj, "floor"), i + 1) - 1;
        if (floor_index < 0 || floor_index >= floors_count)
            continue;

        EncounterFloor *out = &floors_storage[floor_index];
        const JsonValue *normal = field(floor_obj, "normal");
        int normal_count = json_array_count(normal);
        if (normal_count > 0)
        {
            out->normal = (EncounterDef *)calloc((size_t)normal_count, sizeof(EncounterDef));
            if (out->normal)
            {
                for (int n = 0; n < normal_count; n++)
                {
                    const JsonValue *encounter = json_array_get(normal, n);
                    if (load_encounter(encounter, &out->normal[out->normal_count], "normal"))
                        out->normal_count++;
                }
            }
        }

        load_encounter(field(floor_obj, "elite"), &out->elite, "elite");
        load_encounter(field(floor_obj, "boss"), &out->boss, "boss");
    }

    json_free(root);
    loaded = 1;
    LOG_I(CAT_SCREEN, "Loaded %d encounter floors from %s", floors_count, path);
    return floors_count > 0;
}

static int clamp_floor(int floor)
{
    if (floor < 0) floor = 0;
    if (floors_count <= 0) return -1;
    if (floor >= floors_count) floor = floors_count - 1;
    return floor;
}

const EncounterDef *encounter_for_floor(int floor, int index)
{
    if (!loaded) return NULL;
    floor = clamp_floor(floor);
    if (floor < 0) return NULL;
    EncounterFloor *data = &floors_storage[floor];
    if (data->normal_count <= 0 || !data->normal) return NULL;
    if (index < 0) index = 0;
    if (index >= data->normal_count) index %= data->normal_count;
    return &data->normal[index];
}

int encounter_count_for_floor(int floor)
{
    floor = clamp_floor(floor);
    if (floor < 0) return 0;
    return floors_storage[floor].normal_count;
}

const EncounterDef *elite_for_floor(int floor)
{
    if (!loaded) return NULL;
    floor = clamp_floor(floor);
    if (floor < 0 || floors_storage[floor].elite.count <= 0) return NULL;
    return &floors_storage[floor].elite;
}

const EncounterDef *boss_for_floor(int floor)
{
    if (!loaded) return NULL;
    floor = clamp_floor(floor);
    if (floor < 0 || floors_storage[floor].boss.count <= 0) return NULL;
    return &floors_storage[floor].boss;
}
