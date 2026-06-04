#include "party.h"
#include "systems/meta.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

static const char *class_names[CLASS_COUNT];
static const char *class_ids[CLASS_COUNT];
static const char *class_roles[CLASS_COUNT];
static const char *class_descriptions[CLASS_COUNT];
static const char *class_hints[CLASS_COUNT];
static const char *class_abbrevs[CLASS_COUNT];
static const char *class_unlock_keys[CLASS_COUNT];
static const char *class_unlock_events[CLASS_COUNT];
static unsigned char class_colors[CLASS_COUNT][3];
static int class_hp[CLASS_COUNT];
static bool class_loaded[CLASS_COUNT];
static ClassType class_order[CLASS_COUNT];
static int class_order_count = 0;
static int next_dynamic_class = CLASS_BUILTIN_COUNT;

static const char *perk_ids[PERK_COUNT];
static const char *perk_names[PERK_COUNT];
static const char *perk_descriptions[PERK_COUNT];
static const char *perk_effects[PERK_COUNT];
static int perk_values[PERK_COUNT];
static int perk_max_stack_values[PERK_COUNT];
static bool perk_class_specific[PERK_COUNT];
static bool perk_loaded[PERK_COUNT];
static PerkId class_perks[CLASS_COUNT][MAX_CLASS_PERKS];
static int class_perk_counts[CLASS_COUNT];
static PerkId generic_perks[PERK_COUNT];
static int generic_perk_count = 0;

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

static ClassType builtin_class_id(const char *text)
{
    if (text && strcmp(text, "guardian") == 0) return CLASS_GUARDIAN;
    if (text && strcmp(text, "cleric") == 0) return CLASS_CLERIC;
    if (text && strcmp(text, "mage") == 0) return CLASS_MAGE;
    if (text && strcmp(text, "rogue") == 0) return CLASS_ROGUE;
    if (text && strcmp(text, "shaman") == 0) return CLASS_SHAMAN;
    if (text && strcmp(text, "ranger") == 0) return CLASS_RANGER;
    if (text && strcmp(text, "paladin") == 0) return CLASS_PALADIN;
    if (text && strcmp(text, "warlock") == 0) return CLASS_WARLOCK;
    if (text && strcmp(text, "bard") == 0) return CLASS_BARD;
    return CLASS_NONE;
}

ClassType class_from_id(const char *id)
{
    if (!id || !id[0]) return CLASS_NONE;
    for (int i = 0; i < CLASS_COUNT; i++)
        if (class_ids[i] && strcmp(class_ids[i], id) == 0)
            return (ClassType)i;
    return builtin_class_id(id);
}

static ClassType class_slot_for_id(const char *id)
{
    ClassType existing = class_from_id(id);
    if (existing != CLASS_NONE)
        return existing;

    if (next_dynamic_class >= CLASS_COUNT)
        return CLASS_NONE;
    return (ClassType)next_dynamic_class++;
}

static void class_defaults(ClassType ct, unsigned char *r, unsigned char *g, unsigned char *b, const char **abbrev)
{
    *r = 130;
    *g = 135;
    *b = 160;
    *abbrev = "--";
    switch (ct)
    {
        case CLASS_GUARDIAN: *r = 74;  *g = 144; *b = 217; *abbrev = "GD"; break;
        case CLASS_CLERIC:   *r = 225; *g = 170; *b = 50;  *abbrev = "CL"; break;
        case CLASS_MAGE:     *r = 155; *g = 89;  *b = 182; *abbrev = "MG"; break;
        case CLASS_ROGUE:    *r = 39;  *g = 174; *b = 96;  *abbrev = "RG"; break;
        case CLASS_SHAMAN:   *r = 230; *g = 126; *b = 34;  *abbrev = "SH"; break;
        case CLASS_RANGER:   *r = 70;  *g = 190; *b = 120; *abbrev = "RA"; break;
        case CLASS_PALADIN:  *r = 240; *g = 210; *b = 95;  *abbrev = "PL"; break;
        case CLASS_WARLOCK:  *r = 125; *g = 70;  *b = 185; *abbrev = "WL"; break;
        case CLASS_BARD:     *r = 235; *g = 95;  *b = 155; *abbrev = "BD"; break;
        default: break;
    }
}

static void reset_class_defs(void)
{
    memset((void *)class_ids, 0, sizeof(class_ids));
    memset((void *)class_names, 0, sizeof(class_names));
    memset((void *)class_roles, 0, sizeof(class_roles));
    memset((void *)class_descriptions, 0, sizeof(class_descriptions));
    memset((void *)class_hints, 0, sizeof(class_hints));
    memset((void *)class_abbrevs, 0, sizeof(class_abbrevs));
    memset((void *)class_unlock_keys, 0, sizeof(class_unlock_keys));
    memset((void *)class_unlock_events, 0, sizeof(class_unlock_events));
    memset(class_colors, 0, sizeof(class_colors));
    memset(class_hp, 0, sizeof(class_hp));
    memset(class_loaded, 0, sizeof(class_loaded));
    memset(class_order, 0, sizeof(class_order));
    class_order_count = 0;
    next_dynamic_class = CLASS_BUILTIN_COUNT;
}

static void reset_perk_defs(void)
{
    memset((void *)perk_ids, 0, sizeof(perk_ids));
    memset((void *)perk_names, 0, sizeof(perk_names));
    memset((void *)perk_descriptions, 0, sizeof(perk_descriptions));
    memset((void *)perk_effects, 0, sizeof(perk_effects));
    memset(perk_values, 0, sizeof(perk_values));
    memset(perk_max_stack_values, 0, sizeof(perk_max_stack_values));
    memset(perk_class_specific, 0, sizeof(perk_class_specific));
    memset(perk_loaded, 0, sizeof(perk_loaded));
    for (int i = 0; i < CLASS_COUNT; i++)
    {
        class_perk_counts[i] = 0;
        for (int p = 0; p < MAX_CLASS_PERKS; p++)
            class_perks[i][p] = PERK_INVALID;
    }
    generic_perk_count = 0;
}

bool party_defs_load_json(const char *path)
{
    reset_class_defs();

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

        const char *id = json_string(field(item, "id"), NULL);
        ClassType ct = class_slot_for_id(id);
        if (ct < 0 || ct >= CLASS_COUNT) continue;

        unsigned char r, g, b;
        const char *abbrev;
        class_defaults(ct, &r, &g, &b, &abbrev);
        const JsonValue *color = field(item, "color");
        if (color && color->type == JSON_ARRAY && json_array_count(color) >= 3)
        {
            r = (unsigned char)json_int(json_array_get(color, 0), r);
            g = (unsigned char)json_int(json_array_get(color, 1), g);
            b = (unsigned char)json_int(json_array_get(color, 2), b);
        }

        class_ids[ct] = copy_text(id);
        class_names[ct] = copy_text(json_string(field(item, "name"), "Unknown"));
        class_roles[ct] = copy_text(json_string(field(item, "role"), ""));
        class_descriptions[ct] = copy_text(json_string(field(item, "description"), json_string(field(item, "tagline"), "")));
        class_hints[ct] = copy_text(json_string(field(item, "hint"), class_descriptions[ct] ? class_descriptions[ct] : ""));
        class_abbrevs[ct] = copy_text(json_string(field(item, "abbrev"), abbrev));
        class_unlock_keys[ct] = copy_text(json_string(field(item, "unlock"), ""));
        class_unlock_events[ct] = copy_text(json_string(field(item, "unlock_event"), ""));
        meta_content_register_unlock_event(class_unlock_keys[ct], class_unlock_events[ct]);
        class_colors[ct][0] = r;
        class_colors[ct][1] = g;
        class_colors[ct][2] = b;
        class_hp[ct] = json_int(field(item, "hp"), 1);
        if (class_hp[ct] < 1) class_hp[ct] = 1;
        if (!class_loaded[ct] && class_order_count < CLASS_COUNT)
            class_order[class_order_count++] = ct;
        class_loaded[ct] = true;
        loaded++;
    }

    json_free(root);
    LOG_I(CAT_DRAFT, "Loaded %d class definitions from %s", loaded, path);
    return loaded > 0;
}

bool perk_defs_load_json(const char *path)
{
    reset_perk_defs();

    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_DRAFT, "%s", error);
        return false;
    }

    const JsonValue *items = field(root, "perks");
    if (!items || items->type != JSON_ARRAY)
    {
        LOG_E(CAT_DRAFT, "%s: perks must be an array", path);
        json_free(root);
        return false;
    }

    int loaded = 0;
    int count = json_array_count(items);
    for (int i = 0; i < count; i++)
    {
        const JsonValue *item = json_array_get(items, i);
        if (!item || item->type != JSON_OBJECT)
            continue;

        int index = json_int(field(item, "index"), -1);
        PerkId perk = (PerkId)index;
        if (perk < 0 || perk >= PERK_COUNT)
        {
            LOG_E(CAT_DRAFT, "%s: invalid perk index at row %d", path, i);
            continue;
        }
        if (perk_loaded[perk])
        {
            LOG_E(CAT_DRAFT, "%s: duplicate perk index %d", path, index);
            continue;
        }

        const char *id = json_string(field(item, "id"), "");
        const char *name = json_string(field(item, "name"), "");
        const char *description = json_string(field(item, "description"), "");
        const char *effect = json_string(field(item, "effect"), "");
        if (!id[0] || !name[0] || !description[0] || !effect[0])
        {
            LOG_E(CAT_DRAFT, "%s: perk index %d is missing id, name, description, or effect", path, index);
            continue;
        }
        bool duplicate_id = false;
        for (int prev = 0; prev < PERK_COUNT; prev++)
        {
            if (perk_loaded[prev] && perk_ids[prev] && strcmp(perk_ids[prev], id) == 0)
            {
                duplicate_id = true;
                break;
            }
        }
        if (duplicate_id)
        {
            LOG_E(CAT_DRAFT, "%s: duplicate perk id '%s'", path, id);
            continue;
        }

        const char *class_id = json_string(field(item, "class"), "");
        ClassType ct = class_from_id(class_id);
        if (class_id[0] && ct == CLASS_NONE)
        {
            LOG_E(CAT_DRAFT, "%s: perk index %d has unknown class '%s'", path, index, class_id);
            continue;
        }
        if (ct != CLASS_NONE)
        {
            if (class_perk_counts[ct] >= MAX_CLASS_PERKS)
            {
                LOG_E(CAT_DRAFT, "%s: class '%s' has too many perks assigned", path, class_id);
                continue;
            }
        }

        perk_ids[perk] = copy_text(id);
        perk_names[perk] = copy_text(name);
        perk_descriptions[perk] = copy_text(description);
        perk_effects[perk] = copy_text(effect);
        perk_values[perk] = json_int(field(item, "value"), 1);

        if (ct != CLASS_NONE)
        {
            class_perks[ct][class_perk_counts[ct]++] = perk;
            perk_class_specific[perk] = true;
        }

        const char *pool = json_string(field(item, "pool"), "");
        if (strcmp(pool, "generic") == 0 && generic_perk_count < PERK_COUNT)
            generic_perks[generic_perk_count++] = perk;

        int default_max = ct != CLASS_NONE ? 1 : MAX_MEMBER_PERKS;
        perk_max_stack_values[perk] = json_int(field(item, "max_stacks"), default_max);
        if (perk_max_stack_values[perk] < 1)
            perk_max_stack_values[perk] = 1;
        perk_loaded[perk] = true;

        loaded++;
    }

    json_free(root);

    if (generic_perk_count <= 0)
    {
        LOG_E(CAT_DRAFT, "%s: no generic perks defined", path);
        return false;
    }

    LOG_I(CAT_DRAFT, "Loaded %d perk definitions from %s", loaded, path);
    return loaded > 0;
}

int party_class_count(void)
{
    return class_order_count;
}

ClassType party_class_at(int order_index)
{
    if (order_index < 0 || order_index >= class_order_count)
        return CLASS_NONE;
    return class_order[order_index];
}

bool class_is_loaded(ClassType ct)
{
    return ct >= 0 && ct < CLASS_COUNT && class_loaded[ct];
}

const char *class_id(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_ids[ct] ? class_ids[ct] : "";
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

const char *class_description(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_descriptions[ct] ? class_descriptions[ct] : "";
}

const char *class_hint(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_hints[ct] ? class_hints[ct] : "";
}

const char *class_abbrev(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "??";
    return class_abbrevs[ct] ? class_abbrevs[ct] : "??";
}

unsigned char class_color_r(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return 130;
    return class_loaded[ct] ? class_colors[ct][0] : 130;
}

unsigned char class_color_g(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return 135;
    return class_loaded[ct] ? class_colors[ct][1] : 135;
}

unsigned char class_color_b(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return 160;
    return class_loaded[ct] ? class_colors[ct][2] : 160;
}

const char *class_unlock_key(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_unlock_keys[ct] ? class_unlock_keys[ct] : "";
}

const char *class_unlock_event(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return "";
    return class_unlock_events[ct] ? class_unlock_events[ct] : "";
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
        if (ct < 0 || ct >= CLASS_COUNT || !class_is_loaded(ct)) ct = party_class_at(0);
        if (ct < 0 || ct >= CLASS_COUNT) ct = CLASS_GUARDIAN;
        party->members[i].class = ct;
        strncpy(party->members[i].name, class_name(ct), MAX_PARTY_NAME - 1);
        party->members[i].name[MAX_PARTY_NAME - 1] = '\0';
        int hp = class_hp[ct] > 0 ? class_hp[ct] : 1;
        party->members[i].max_hp = hp;
        party->members[i].hp = hp;
        party->members[i].shield = 0;
        party->members[i].aggro = 0;
        party->members[i].level = 1;
        party->members[i].xp = 0;
        party->members[i].combat_xp = 0;
        party->members[i].pending_levels = 0;
        for (int p = 0; p < MAX_MEMBER_PERKS; p++)
            party->members[i].perks[p] = -1;
        party->members[i].perk_count = 0;
        party->members[i].alive = true;
        party->members[i].status_count = 0;
    }
}

int xp_for_level(int level)
{
    if (level < 1) level = 1;
    if (level >= MAX_LEVEL) return 0;
    return (10 * level) * (level / 2) + 5;
}

int xp_total_for_level(int level)
{
    if (level <= 1) return 0;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    int total = 0;
    for (int i = 1; i < level; i++)
        total += xp_for_level(i);
    return total;
}

int party_member_xp_into_level(const PartyMember *member)
{
    if (!member || member->level <= 1)
        return member ? member->xp : 0;
    int current_start = xp_total_for_level(member->level);
    int progress = member->xp - current_start;
    return progress > 0 ? progress : 0;
}

int party_member_gain_xp(PartyMember *member, int amount, int *levels_gained)
{
    if (levels_gained) *levels_gained = 0;
    if (!member || amount <= 0 || member->level >= MAX_LEVEL)
        return 0;

    int remaining = MAX_COMBAT_XP - member->combat_xp;
    if (remaining <= 0)
        return 0;
    int gained = amount < remaining ? amount : remaining;
    member->xp += gained;
    member->combat_xp += gained;

    int levels = 0;
    while (member->level < MAX_LEVEL)
    {
        int threshold = xp_total_for_level(member->level + 1);
        if (threshold <= 0 || member->xp < threshold)
            break;
        member->level++;
        member->pending_levels++;
        levels++;
    }

    if (levels_gained) *levels_gained = levels;
    return gained;
}

bool party_member_has_perk(const PartyMember *member, PerkId perk)
{
    return party_member_perk_count(member, perk) > 0;
}

int party_member_perk_count(const PartyMember *member, PerkId perk)
{
    if (!member || !perk_is_loaded(perk))
        return 0;
    int count = 0;
    for (int i = 0; i < member->perk_count && i < MAX_MEMBER_PERKS; i++)
        if (member->perks[i] == (int)perk)
            count++;
    return count;
}

bool party_member_add_perk(PartyMember *member, PerkId perk)
{
    if (!member || !perk_is_loaded(perk))
        return false;
    if (member->perk_count >= MAX_MEMBER_PERKS)
        return false;
    if (party_member_perk_count(member, perk) >= perk_max_stacks(perk))
        return false;

    member->perks[member->perk_count++] = (int)perk;
    int hp_bonus = 0;
    if (perk_effects[perk] && strcmp(perk_effects[perk], "max_hp") == 0)
        hp_bonus = perk_values[perk];
    if (hp_bonus > 0)
    {
        member->max_hp += hp_bonus;
        member->hp += hp_bonus;
        if (member->hp > member->max_hp)
            member->hp = member->max_hp;
    }
    return true;
}

bool perk_is_loaded(PerkId perk)
{
    return perk >= 0 && perk < PERK_COUNT && perk_loaded[perk];
}

bool perk_is_class_specific(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return false;
    return perk_class_specific[perk];
}

int perk_max_stacks(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return 0;
    return perk_max_stack_values[perk] > 0 ? perk_max_stack_values[perk] : 1;
}

int perk_class_count(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT)
        return 0;
    return class_perk_counts[ct];
}

PerkId perk_class_at(ClassType ct, int index)
{
    if (ct < 0 || ct >= CLASS_COUNT || index < 0 || index >= class_perk_counts[ct])
        return PERK_INVALID;
    return class_perks[ct][index];
}

PerkId perk_for_class(ClassType ct)
{
    return perk_class_at(ct, 0);
}

const char *perk_id(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return "";
    return perk_ids[perk] ? perk_ids[perk] : "";
}

const char *perk_name(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return "Unknown Perk";
    return perk_names[perk] ? perk_names[perk] : "Unknown Perk";
}

const char *perk_description(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return "";
    return perk_descriptions[perk] ? perk_descriptions[perk] : "";
}

const char *perk_effect(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return "";
    return perk_effects[perk] ? perk_effects[perk] : "";
}

int perk_effect_value(PerkId perk)
{
    if (!perk_is_loaded(perk))
        return 0;
    return perk_values[perk];
}

int party_member_perk_effect_total(const PartyMember *member, const char *effect)
{
    if (!member || !effect || !effect[0])
        return 0;
    int total = 0;
    for (int i = 0; i < member->perk_count && i < MAX_MEMBER_PERKS; i++)
    {
        PerkId perk = member->perks[i];
        if (!perk_is_loaded(perk))
            continue;
        if (perk_effects[perk] && strcmp(perk_effects[perk], effect) == 0)
            total += perk_values[perk];
    }
    return total;
}

const char *party_member_perk_effect_name(const PartyMember *member, const char *effect)
{
    if (!member || !effect || !effect[0])
        return "Perk";
    for (int i = 0; i < member->perk_count && i < MAX_MEMBER_PERKS; i++)
    {
        PerkId perk = member->perks[i];
        if (!perk_is_loaded(perk))
            continue;
        if (perk_effects[perk] && strcmp(perk_effects[perk], effect) == 0)
            return perk_name(perk);
    }
    return "Perk";
}

int perk_generic_count(void)
{
    return generic_perk_count;
}

PerkId perk_generic_at(int index)
{
    if (index < 0 || index >= generic_perk_count)
        return PERK_INVALID;
    return generic_perks[index];
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

int party_random_alive(Party *party)
{
    int alive[MAX_PARTY_SIZE];
    int count = 0;
    for (int i = 0; i < party->count; i++)
        if (party->members[i].alive)
            alive[count++] = i;
    if (count == 0) return -1;
    return alive[rand() % count];
}
