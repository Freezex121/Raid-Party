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

static EnemyTargetType parse_target(const char *text)
{
    if (!text) return ENEMY_TARGET_TANK;
    if (strcmp(text, "tank") == 0) return ENEMY_TARGET_TANK;
    if (strcmp(text, "random") == 0) return ENEMY_TARGET_RANDOM;
    if (strcmp(text, "all") == 0) return ENEMY_TARGET_ALL;
    if (strcmp(text, "aoe") == 0) return ENEMY_TARGET_ALL;
    if (strcmp(text, "self") == 0) return ENEMY_TARGET_SELF;
    if (strcmp(text, "lowest_hp") == 0) return ENEMY_TARGET_LOWEST_HP;
    return ENEMY_TARGET_TANK;
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
        def.hand_size = json_int(field(item, "hand_size"), 2);
        def.energy_per_turn = json_int(field(item, "energy_per_turn"), 2);

        const JsonValue *cards = field(item, "cards");
        int card_count = json_array_count(cards);
        if (card_count > MAX_ENEMY_CARDS) card_count = MAX_ENEMY_CARDS;
        for (int a = 0; a < card_count; a++)
        {
            const JsonValue *card = json_array_get(cards, a);
            if (!card || card->type != JSON_OBJECT) continue;

            EnemyCardDef *out = &def.cards[def.card_count++];
            out->name = copy_text(json_string(field(card, "name"), ""));
            out->intent = parse_intent(json_string(field(card, "intent"), ""));
            out->target = parse_target(json_string(field(card, "target"), "tank"));
            out->cost = json_int(field(card, "cost"), 1);
            out->base_damage = json_int(field(card, "base_damage"), 0);
            out->cast_time = json_int(field(card, "cast_time"), 1);
            if (out->cast_time < 1) out->cast_time = 1;
            out->description = copy_text(json_string(field(card, "description"), ""));
            out->is_wipe = json_bool(field(card, "is_wipe"), false);
            out->heal_amount = json_int(field(card, "heal_amount"), 0);
            out->shield_amount = json_int(field(card, "shield_amount"), 0);
            out->status = parse_status(json_string(field(card, "status"), ""));
            out->status_amount = json_int(field(card, "status_amount"), 0);
            out->status_turns = json_int(field(card, "status_turns"), 0);
            out->count = json_int(field(card, "count"), 1);
            if (out->count < 1) out->count = 1;
        }

        if (def.card_count <= 0)
        {
            LOG_W(CAT_SCREEN, "Enemy %s has no cards, skipping", def.id ? def.id : "unknown");
            continue;
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
