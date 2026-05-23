#include "party.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

static const char *class_names[CLASS_COUNT];
static const char *class_roles[CLASS_COUNT];
static int class_hp[CLASS_COUNT];

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

static ClassType parse_class_id(const char *text)
{
    if (text && strcmp(text, "guardian") == 0) return CLASS_GUARDIAN;
    if (text && strcmp(text, "cleric") == 0) return CLASS_CLERIC;
    if (text && strcmp(text, "mage") == 0) return CLASS_MAGE;
    if (text && strcmp(text, "rogue") == 0) return CLASS_ROGUE;
    if (text && strcmp(text, "shaman") == 0) return CLASS_SHAMAN;
    if (text && strcmp(text, "ranger") == 0) return CLASS_RANGER;
    return CLASS_NONE;
}

bool party_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_DRAFT, "%s", error);
        return false;
    }

    const JsonValue *classes = field(root, "classes");
    if (!classes || classes->type != JSON_ARRAY)
    {
        LOG_E(CAT_DRAFT, "%s: classes must be an array", path);
        json_free(root);
        return false;
    }

    int loaded = 0;
    int count = json_array_count(classes);
    for (int i = 0; i < count; i++)
    {
        const JsonValue *item = json_array_get(classes, i);
        if (!item || item->type != JSON_OBJECT) continue;

        ClassType ct = parse_class_id(json_string(field(item, "id"), NULL));
        if (ct < 0 || ct >= CLASS_COUNT) continue;

        class_names[ct] = copy_text(json_string(field(item, "name"), "Unknown"));
        class_roles[ct] = copy_text(json_string(field(item, "role"), ""));
        class_hp[ct] = json_int(field(item, "hp"), 1);
        if (class_hp[ct] < 1) class_hp[ct] = 1;
        loaded++;
    }

    json_free(root);
    LOG_I(CAT_DRAFT, "Loaded %d class definitions from %s", loaded, path);
    return loaded == CLASS_COUNT;
}

const char *class_name(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "Unknown";
    return class_names[ct] ? class_names[ct] : "Unknown";
}

const char *class_role(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_roles[ct] ? class_roles[ct] : "";
}

void party_create(Party *party, int *class_indices, int count)
{
    if (count < 0) count = 0;
    if (count > MAX_PARTY_SIZE) count = MAX_PARTY_SIZE;

    memset(party, 0, sizeof(Party));
    party->count = count;
    for (int i = 0; i < count; i++)
    {
        ClassType ct = (ClassType)class_indices[i];
        if (ct < 0 || ct >= CLASS_COUNT) ct = CLASS_GUARDIAN;
        party->members[i].class = ct;
        strncpy(party->members[i].name, class_names[ct], MAX_PARTY_NAME - 1);
        party->members[i].name[MAX_PARTY_NAME - 1] = '\0';
        int hp = class_hp[ct] > 0 ? class_hp[ct] : 1;
        party->members[i].max_hp = hp;
        party->members[i].hp = hp;
        party->members[i].shield = 0;
        party->members[i].aggro = 0;
        party->members[i].alive = true;
        party->members[i].status_count = 0;
    }
}

int party_lowest_hp(Party *party)
{
    int idx = -1;
    int lowest = 99999;
    for (int i = 0; i < party->count; i++)
    {
        if (!party->members[i].alive) continue;
        int effective = party->members[i].hp;
        if (effective < lowest) { lowest = effective; idx = i; }
    }
    return idx;
}

int party_highest_aggro(Party *party)
{
    int idx = -1;
    int highest = -1;
    for (int i = 0; i < party->count; i++)
    {
        if (!party->members[i].alive) continue;
        if (party->members[i].aggro > highest) { highest = party->members[i].aggro; idx = i; }
    }
    return idx;
}
