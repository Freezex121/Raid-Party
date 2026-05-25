#include "encounter_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *area_id;
    int floor_index;
    EncounterDef *normal;
    int normal_count;
    EncounterDef elite;
    EncounterDef boss;
} EncounterFloor;

static EncounterFloor *floors_storage = NULL;
static int floor_layout_count = 0;
static int loaded = 0;

static const JsonValue *field(const JsonValue *object, const char *key)
{
    return json_object_get(object, key);
}

static void free_encounters(void)
{
    for (int i = 0; i < floor_layout_count; i++)
    {
        free(floors_storage[i].area_id);
        free(floors_storage[i].normal);
    }
    free(floors_storage);
    floors_storage = NULL;
    floor_layout_count = 0;
    loaded = 0;
}

static char *copy_text(const char *text)
{
    if (!text || !text[0]) return NULL;
    size_t len = strlen(text);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, text, len + 1);
    return out;
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
    if (json_floor_count <= 0)
    {
        json_free(root);
        return false;
    }

    floor_layout_count = json_floor_count;
    floors_storage = (EncounterFloor *)calloc((size_t)floor_layout_count, sizeof(EncounterFloor));
    if (!floors_storage)
    {
        floor_layout_count = 0;
        json_free(root);
        return false;
    }

    int loaded_floors = 0;
    for (int i = 0; i < json_floor_count; i++)
    {
        const JsonValue *floor_obj = json_array_get(floors, i);
        if (!floor_obj || floor_obj->type != JSON_OBJECT) continue;

        int floor_index = json_int(field(floor_obj, "floor"), i + 1) - 1;
        if (floor_index < 0)
            continue;

        EncounterFloor *out = &floors_storage[i];
        out->floor_index = floor_index;
        const char *area_id = json_string(field(floor_obj, "area"), NULL);
        if (!area_id)
            area_id = json_string(field(floor_obj, "area_id"), NULL);
        out->area_id = copy_text(area_id);

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
        loaded_floors++;
    }

    json_free(root);
    loaded = 1;
    LOG_I(CAT_SCREEN, "Loaded %d encounter floors from %s", loaded_floors, path);
    return loaded_floors > 0;
}

static bool area_matches(const char *layout_area, const char *area_id)
{
    bool layout_empty = !layout_area || !layout_area[0];
    bool area_empty = !area_id || !area_id[0];
    if (layout_empty && area_empty) return true;
    if (layout_empty || area_empty) return false;
    return strcmp(layout_area, area_id) == 0;
}

static EncounterFloor *find_floor_exact(const char *area_id, int floor)
{
    if (floor < 0) floor = 0;
    for (int i = 0; i < floor_layout_count; i++)
        if (floors_storage[i].floor_index == floor &&
            area_matches(floors_storage[i].area_id, area_id))
            return &floors_storage[i];
    return NULL;
}

static EncounterFloor *find_floor(const char *area_id, int floor)
{
    EncounterFloor *data = find_floor_exact(area_id, floor);
    if (data) return data;
    if (area_id && area_id[0])
        return find_floor_exact(NULL, floor);
    return NULL;
}

const EncounterDef *encounter_for_floor(int floor, int index)
{
    return encounter_for_area_floor(NULL, floor, index);
}

int encounter_count_for_floor(int floor)
{
    return encounter_count_for_area_floor(NULL, floor);
}

const EncounterDef *elite_for_floor(int floor)
{
    return elite_for_area_floor(NULL, floor);
}

const EncounterDef *boss_for_floor(int floor)
{
    return boss_for_area_floor(NULL, floor);
}

const EncounterDef *encounter_for_area_floor(const char *area_id, int floor, int index)
{
    if (!loaded) return NULL;
    EncounterFloor *data = find_floor(area_id, floor);
    if (!data) return NULL;
    if (data->normal_count <= 0 || !data->normal) return NULL;
    if (index < 0) index = 0;
    if (index >= data->normal_count) index %= data->normal_count;
    return &data->normal[index];
}

int encounter_count_for_area_floor(const char *area_id, int floor)
{
    EncounterFloor *data = find_floor(area_id, floor);
    return data ? data->normal_count : 0;
}

const EncounterDef *elite_for_area_floor(const char *area_id, int floor)
{
    if (!loaded) return NULL;
    EncounterFloor *data = find_floor(area_id, floor);
    if (!data || data->elite.count <= 0) return NULL;
    return &data->elite;
}

const EncounterDef *boss_for_area_floor(const char *area_id, int floor)
{
    if (!loaded) return NULL;
    EncounterFloor *data = find_floor(area_id, floor);
    if (!data || data->boss.count <= 0) return NULL;
    return &data->boss;
}
