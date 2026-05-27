#include "event_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 32

static EventDef events[MAX_EVENTS];
static int event_count = 0;

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

static EventEffectType parse_effect(const char *text)
{
    if (text && strcmp(text, "heal_party") == 0) return EVENT_EFFECT_HEAL_PARTY;
    if (text && strcmp(text, "gain_gold_add_curse") == 0) return EVENT_EFFECT_GAIN_GOLD_ADD_CURSE;
    if (text && strcmp(text, "remove_card") == 0) return EVENT_EFFECT_REMOVE_CARD;
    if (text && strcmp(text, "upgrade_random_card_hurt_party") == 0) return EVENT_EFFECT_UPGRADE_RANDOM_CARD_HURT_PARTY;
    if (text && strcmp(text, "pay_gold_gain_relic") == 0) return EVENT_EFFECT_PAY_GOLD_GAIN_RELIC;
    if (text && strcmp(text, "pay_gold_add_card") == 0) return EVENT_EFFECT_PAY_GOLD_ADD_CARD;
    if (text && strcmp(text, "gain_gold") == 0) return EVENT_EFFECT_GAIN_GOLD;
    if (text && strcmp(text, "pay_gold_upgrade_random_card") == 0) return EVENT_EFFECT_PAY_GOLD_UPGRADE_RANDOM_CARD;
    if (text && strcmp(text, "gain_gold_hurt_party") == 0) return EVENT_EFFECT_GAIN_GOLD_HURT_PARTY;
    if (text && strcmp(text, "add_card_add_curse") == 0) return EVENT_EFFECT_ADD_CARD_ADD_CURSE;
    if (text && strcmp(text, "gain_relic_add_curse") == 0) return EVENT_EFFECT_GAIN_RELIC_ADD_CURSE;
    if (text && strcmp(text, "duplicate_random_card_hurt_party") == 0) return EVENT_EFFECT_DUPLICATE_RANDOM_CARD_HURT_PARTY;
    if (text && strcmp(text, "transform_random_card") == 0) return EVENT_EFFECT_TRANSFORM_RANDOM_CARD;
    if (text && strcmp(text, "gain_max_hp") == 0) return EVENT_EFFECT_GAIN_MAX_HP;
    return EVENT_EFFECT_NONE;
}

bool event_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    const JsonValue *json_events = field(root, "events");
    if (!json_events || json_events->type != JSON_ARRAY)
    {
        LOG_E(CAT_SCREEN, "%s: events must be an array", path);
        json_free(root);
        return false;
    }

    memset(events, 0, sizeof(events));
    event_count = 0;

    int count = json_array_count(json_events);
    for (int i = 0; i < count && event_count < MAX_EVENTS; i++)
    {
        const JsonValue *item = json_array_get(json_events, i);
        if (!item || item->type != JSON_OBJECT) continue;

        EventDef def = { 0 };
        def.id = copy_text(json_string(field(item, "id"), ""));
        def.name = copy_text(json_string(field(item, "name"), ""));
        def.body = copy_text(json_string(field(item, "body"), ""));

        const JsonValue *choices = field(item, "choices");
        int choice_count = json_array_count(choices);
        if (choice_count > EVENT_CHOICE_COUNT)
            choice_count = EVENT_CHOICE_COUNT;
        for (int c = 0; c < choice_count; c++)
        {
            const JsonValue *choice = json_array_get(choices, c);
            if (!choice || choice->type != JSON_OBJECT) continue;

            EventChoiceDef *out = &def.choices[def.choice_count++];
            out->label = copy_text(json_string(field(choice, "label"), ""));
            out->description = copy_text(json_string(field(choice, "description"), ""));
            out->effect = parse_effect(json_string(field(choice, "effect"), "none"));
            out->amount = json_int(field(choice, "amount"), 0);
            out->gold = json_int(field(choice, "gold"), 0);
            out->hp_loss = json_int(field(choice, "hp_loss"), 0);
            out->curse = copy_text(json_string(field(choice, "curse"), ""));
        }

        events[event_count++] = def;
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d events from %s", event_count, path);
    return event_count > 0;
}

int event_defs_count(void)
{
    return event_count;
}

const EventDef *event_def_by_index(int index)
{
    if (event_count <= 0) return NULL;
    if (index < 0) index = 0;
    index %= event_count;
    return &events[index];
}

const EventDef *event_def_by_id(const char *id)
{
    if (!id || !id[0] || event_count <= 0) return NULL;
    for (int i = 0; i < event_count; i++)
        if (events[i].id && strcmp(events[i].id, id) == 0)
            return &events[i];
    return NULL;
}
