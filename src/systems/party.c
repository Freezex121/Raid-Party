#include "party.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

static const char *class_names[CLASS_COUNT];
static const char *class_roles[CLASS_COUNT];
static int class_hp[CLASS_COUNT];

typedef struct {
    const char *name;
    const char *description;
} PerkText;

static const PerkText perk_text[PERK_COUNT] = {
    [PERK_HP_PLUS_4] = { "+4 Max HP", "Gain 4 max HP and heal 4." },
    [PERK_STARTING_SHIELD_1] = { "Brace", "Start each combat with +1 Shield." },
    [PERK_CARD_DMG_1] = { "Sharper Cards", "This member's cards deal +1 damage." },
    [PERK_CARD_HEAL_1] = { "Steadier Hands", "This member's cards heal +1 HP." },
    [PERK_CARD_SHIELD_1] = { "Better Guard", "This member's cards grant +1 Shield." },
    [PERK_GUARDIAN_TAUNT_SHIELD] = { "Anchor Stance", "First Taunt each combat grants +4 Shield." },
    [PERK_CLERIC_OVERHEAL_SHIELD] = { "Overflowing Grace", "Overhealing with Cleric cards grants 2 Shield." },
    [PERK_MAGE_FIRST_SPELL_DMG] = { "Opening Spark", "First Mage damage card each turn deals +1 damage." },
    [PERK_ROGUE_MARK_REFUND] = { "Clean Payday", "First Rogue Mark payoff each combat refunds 1 Energy." },
    [PERK_SHAMAN_EXTEND_STATUS] = { "Lingering Rite", "First Shaman Conductive or Totem each combat lasts +1 turn." },
    [PERK_RANGER_MARKED_DMG] = { "True Shot", "First Ranger hit against Marked each combat deals +2 damage." },
    [PERK_PALADIN_HEAL_SHIELD] = { "Blessed Mending", "Paladin heals also grant 1 Shield." },
    [PERK_WARLOCK_BLIGHT_BOOST] = { "Deeper Rot", "First Warlock Blight each combat gains +1 value." },
    [PERK_BARD_FIRST_DRAW] = { "Encore", "First Bard card each combat draws 1 card." }
};

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
    if (text && strcmp(text, "paladin") == 0) return CLASS_PALADIN;
    if (text && strcmp(text, "warlock") == 0) return CLASS_WARLOCK;
    if (text && strcmp(text, "bard") == 0) return CLASS_BARD;
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
    if (!member || perk < 0 || perk >= PERK_COUNT)
        return 0;
    int count = 0;
    for (int i = 0; i < member->perk_count && i < MAX_MEMBER_PERKS; i++)
        if (member->perks[i] == (int)perk)
            count++;
    return count;
}

bool party_member_add_perk(PartyMember *member, PerkId perk)
{
    if (!member || perk < 0 || perk >= PERK_COUNT)
        return false;
    if (member->perk_count >= MAX_MEMBER_PERKS)
        return false;
    if (perk_is_class_specific(perk) && party_member_has_perk(member, perk))
        return false;

    member->perks[member->perk_count++] = (int)perk;
    if (perk == PERK_HP_PLUS_4)
    {
        member->max_hp += 4;
        member->hp += 4;
        if (member->hp > member->max_hp)
            member->hp = member->max_hp;
    }
    return true;
}

bool perk_is_class_specific(PerkId perk)
{
    return perk >= PERK_GUARDIAN_TAUNT_SHIELD && perk <= PERK_BARD_FIRST_DRAW;
}

PerkId perk_for_class(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return PERK_GUARDIAN_TAUNT_SHIELD;
        case CLASS_CLERIC: return PERK_CLERIC_OVERHEAL_SHIELD;
        case CLASS_MAGE: return PERK_MAGE_FIRST_SPELL_DMG;
        case CLASS_ROGUE: return PERK_ROGUE_MARK_REFUND;
        case CLASS_SHAMAN: return PERK_SHAMAN_EXTEND_STATUS;
        case CLASS_RANGER: return PERK_RANGER_MARKED_DMG;
        case CLASS_PALADIN: return PERK_PALADIN_HEAL_SHIELD;
        case CLASS_WARLOCK: return PERK_WARLOCK_BLIGHT_BOOST;
        case CLASS_BARD: return PERK_BARD_FIRST_DRAW;
        default: return PERK_COUNT;
    }
}

const char *perk_name(PerkId perk)
{
    if (perk < 0 || perk >= PERK_COUNT)
        return "Unknown Perk";
    return perk_text[perk].name ? perk_text[perk].name : "Unknown Perk";
}

const char *perk_description(PerkId perk)
{
    if (perk < 0 || perk >= PERK_COUNT)
        return "";
    return perk_text[perk].description ? perk_text[perk].description : "";
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
