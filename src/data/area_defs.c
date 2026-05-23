#include "area_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

static AreaDef areas[MAX_AREAS];
static int area_count = 0;

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

bool area_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *items = field(root, "areas");
    if (!items || items->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: areas must be an array", path);
        json_free(root);
        return false;
    }

    memset(areas, 0, sizeof(areas));
    area_count = 0;

    int count = json_array_count(items);
    if (count > MAX_AREAS) count = MAX_AREAS;

    for (int i = 0; i < count; i++)
    {
        const JsonValue *item = json_array_get(items, i);
        if (!item || item->type != JSON_OBJECT) continue;

        AreaDef *out = &areas[area_count];
        out->id = copy_text(json_string(field(item, "id"), "area"));
        out->name = copy_text(json_string(field(item, "name"), "Unknown Area"));
        out->description = copy_text(json_string(field(item, "description"), ""));
        out->floor_count = json_int(field(item, "floor_count"), 3);
        out->difficulty_percent = json_int(field(item, "difficulty_percent"), 100);

        if (out->floor_count < 3) out->floor_count = 3;
        if (out->floor_count > 5) out->floor_count = 5;
        if (out->difficulty_percent < 50) out->difficulty_percent = 50;

        area_count++;
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d areas from %s", area_count, path);
    return area_count > 0;
}

int area_defs_count(void)
{
    return area_count;
}

int area_clamp_index(int index)
{
    if (area_count <= 0) return 0;
    if (index < 0) return 0;
    if (index >= area_count) return area_count - 1;
    return index;
}

const AreaDef *area_def(int index)
{
    if (area_count <= 0) return NULL;
    return &areas[area_clamp_index(index)];
}

int area_floor_count(int index)
{
    const AreaDef *area = area_def(index);
    return area ? area->floor_count : 3;
}

float area_difficulty_scale(int index)
{
    const AreaDef *area = area_def(index);
    if (!area) return 1.0f;
    return (float)area->difficulty_percent / 100.0f;
}
