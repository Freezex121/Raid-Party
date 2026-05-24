#include "enemy_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

#define MAX_JSON_ENEMIES 64

static EnemyDef enemy_defs[MAX_JSON_ENEMIES];
static int enemy_count = 0;

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

static IntentType parse_intent(const char *text)
{
    if (text && strcmp(text, "attack") == 0) return INTENT_ATTACK;
    if (text && strcmp(text, "tank_buster") == 0) return INTENT_TANK_BUSTER;
    if (text && strcmp(text, "aoe") == 0) return INTENT_AOE;
    if (text && strcmp(text, "wipe") == 0) return INTENT_WIPE;
    if (text && strcmp(text, "buff") == 0) return INTENT_BUFF;
    if (text && strcmp(text, "heal") == 0) return INTENT_HEAL;
    if (text && strcmp(text, "shield") == 0) return INTENT_SHIELD;
    return INTENT_NONE;
}

static StatusType parse_status(const char *text)
{
    if (text && strcmp(text, "burning") == 0) return STATUS_BURNING;
    if (text && strcmp(text, "bleed") == 0) return STATUS_BLEED;
    if (text && strcmp(text, "weakness") == 0) return STATUS_WEAKNESS;
    if (text && strcmp(text, "energy_drain") == 0) return STATUS_ENERGY_DRAIN;
    if (text && strcmp(text, "trap") == 0) return STATUS_TRAP;
    if (text && strcmp(text, "marked") == 0) return STATUS_MARKED;
    if (text && strcmp(text, "conductive") == 0) return STATUS_CONDUCTIVE;
    if (text && strcmp(text, "blight") == 0) return STATUS_BLIGHT;
    return STATUS_NONE;
}

bool enemy_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *enemies = field(root, "enemies");
    if (!enemies || enemies->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: enemies must be an array", path);
        json_free(root);
        return false;
    }

    memset(enemy_defs, 0, sizeof(enemy_defs));
    enemy_count = 0;

    int count = json_array_count(enemies);
    for (int i = 0; i < count && enemy_count < MAX_JSON_ENEMIES; i++)
    {
        const JsonValue *item = json_array_get(enemies, i);
        if (!item || item->type != JSON_OBJECT) continue;

        EnemyDef def = { 0 };
        def.id = copy_text(json_string(field(item, "id"), ""));
        def.name = copy_text(json_string(field(item, "name"), ""));
        def.max_hp = json_int(field(item, "max_hp"), 1);
        def.hp = def.max_hp;

        const JsonValue *abilities = field(item, "abilities");
        int ability_count = json_array_count(abilities);
        if (ability_count > 4) ability_count = 4;
        for (int a = 0; a < ability_count; a++)
        {
            const JsonValue *ability = json_array_get(abilities, a);
            if (!ability || ability->type != JSON_OBJECT) continue;

            EnemyAbility *out = &def.abilities[def.ability_count++];
            out->name = copy_text(json_string(field(ability, "name"), ""));
            out->intent = parse_intent(json_string(field(ability, "intent"), ""));
            out->base_damage = json_int(field(ability, "base_damage"), 0);
            out->cast_time = json_int(field(ability, "cast_time"), 1);
            if (out->cast_time < 1) out->cast_time = 1;
            out->description = copy_text(json_string(field(ability, "description"), ""));
            out->is_wipe = json_bool(field(ability, "is_wipe"), false);
            out->heal_amount = json_int(field(ability, "heal_amount"), 0);
            out->shield_amount = json_int(field(ability, "shield_amount"), 0);
            out->status = parse_status(json_string(field(ability, "status"), ""));
            out->status_amount = json_int(field(ability, "status_amount"), 0);
            out->status_turns = json_int(field(ability, "status_turns"), 0);
        }

        enemy_defs[enemy_count++] = def;
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d enemies from %s", enemy_count, path);
    return enemy_count > 0;
}

const EnemyDef *enemy_def_by_id(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; i < enemy_count; i++)
        if (enemy_defs[i].id && strcmp(enemy_defs[i].id, id) == 0)
            return &enemy_defs[i];
    return NULL;
}

int enemy_defs_loaded_count(void)
{
    return enemy_count;
}
