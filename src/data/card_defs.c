#include "card_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

#define MAX_JSON_CARDS 192
#define MAX_CARD_EFFECTS_PER_CARD 8

static CardDef all_cards[MAX_JSON_CARDS];
static CardEffect effect_storage[MAX_JSON_CARDS][MAX_CARD_EFFECTS_PER_CARD];
static int all_card_count = 0;

CardDef guardian_cards[CLASS_CARD_COUNT];
CardDef cleric_cards[CLASS_CARD_COUNT];
CardDef mage_cards[CLASS_CARD_COUNT];
CardDef rogue_cards[CLASS_CARD_COUNT];
CardDef shaman_cards[CLASS_CARD_COUNT];
CardDef ranger_cards[CLASS_CARD_COUNT];
CardDef paladin_cards[CLASS_CARD_COUNT];
CardDef warlock_cards[CLASS_CARD_COUNT];
CardDef bard_cards[CLASS_CARD_COUNT];
CardDef utility_cards[MAX_UTILITY_CARDS];
int utility_card_count = 0;

const CardDef *class_card_sets[CLASS_COUNT] = {
    [CLASS_GUARDIAN] = guardian_cards,
    [CLASS_CLERIC]   = cleric_cards,
    [CLASS_MAGE]     = mage_cards,
    [CLASS_ROGUE]    = rogue_cards,
    [CLASS_SHAMAN]   = shaman_cards,
    [CLASS_RANGER]   = ranger_cards,
    [CLASS_PALADIN]  = paladin_cards,
    [CLASS_WARLOCK]  = warlock_cards,
    [CLASS_BARD]     = bard_cards,
};

int class_card_counts[CLASS_COUNT] = { 0 };

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

static CardType parse_card_type(const char *text)
{
    if (text && strcmp(text, "attack") == 0) return CARD_ATTACK;
    if (text && strcmp(text, "power") == 0) return CARD_POWER;
    return CARD_SKILL;
}

static ClassType parse_class(const char *text)
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

static TargetType parse_target(const char *text)
{
    if (text && strcmp(text, "all_enemies") == 0) return TARGET_ALL_ENEMIES;
    if (text && strcmp(text, "ally") == 0) return TARGET_ALLY;
    if (text && strcmp(text, "all_allies") == 0) return TARGET_ALL_ALLIES;
    if (text && strcmp(text, "self") == 0) return TARGET_SELF;
    return TARGET_ENEMY;
}

static StatusType parse_status(const char *text)
{
    if (text && strcmp(text, "burning") == 0) return STATUS_BURNING;
    if (text && strcmp(text, "renew") == 0) return STATUS_RENEW;
    if (text && strcmp(text, "trap") == 0) return STATUS_TRAP;
    if (text && strcmp(text, "totem_heal") == 0) return STATUS_TOTEM_HEAL;
    if (text && strcmp(text, "bleed") == 0) return STATUS_BLEED;
    if (text && strcmp(text, "weakness") == 0) return STATUS_WEAKNESS;
    if (text && strcmp(text, "energy_drain") == 0) return STATUS_ENERGY_DRAIN;
    if (text && strcmp(text, "marked") == 0) return STATUS_MARKED;
    if (text && strcmp(text, "conductive") == 0) return STATUS_CONDUCTIVE;
    if (text && strcmp(text, "blight") == 0) return STATUS_BLIGHT;
    return STATUS_NONE;
}

static CardEffectType parse_effect_type(const char *text)
{
    if (text && strcmp(text, "draw_cards") == 0) return CARD_EFFECT_DRAW_CARDS;
    if (text && strcmp(text, "gain_energy") == 0) return CARD_EFFECT_GAIN_ENERGY;
    if (text && strcmp(text, "revive_target") == 0) return CARD_EFFECT_REVIVE_TARGET;
    if (text && strcmp(text, "apply_status_target_enemy") == 0) return CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY;
    if (text && strcmp(text, "apply_status_target_ally") == 0) return CARD_EFFECT_APPLY_STATUS_TARGET_ALLY;
    if (text && strcmp(text, "apply_status_all_allies") == 0) return CARD_EFFECT_APPLY_STATUS_ALL_ALLIES;
    if (text && strcmp(text, "reset_caster_aggro") == 0) return CARD_EFFECT_RESET_CASTER_AGGRO;
    if (text && strcmp(text, "transfer_aggro_to_guardian") == 0) return CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN;
    return CARD_EFFECT_DRAW_CARDS;
}

static void reset_cards(void)
{
    memset(all_cards, 0, sizeof(all_cards));
    memset(effect_storage, 0, sizeof(effect_storage));
    memset(guardian_cards, 0, sizeof(guardian_cards));
    memset(cleric_cards, 0, sizeof(cleric_cards));
    memset(mage_cards, 0, sizeof(mage_cards));
    memset(rogue_cards, 0, sizeof(rogue_cards));
    memset(shaman_cards, 0, sizeof(shaman_cards));
    memset(ranger_cards, 0, sizeof(ranger_cards));
    memset(paladin_cards, 0, sizeof(paladin_cards));
    memset(warlock_cards, 0, sizeof(warlock_cards));
    memset(bard_cards, 0, sizeof(bard_cards));
    memset(utility_cards, 0, sizeof(utility_cards));
    memset(class_card_counts, 0, sizeof(class_card_counts));
    all_card_count = 0;
    utility_card_count = 0;
}

static CardDef *class_storage(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return guardian_cards;
        case CLASS_CLERIC: return cleric_cards;
        case CLASS_MAGE: return mage_cards;
        case CLASS_ROGUE: return rogue_cards;
        case CLASS_SHAMAN: return shaman_cards;
        case CLASS_RANGER: return ranger_cards;
        case CLASS_PALADIN: return paladin_cards;
        case CLASS_WARLOCK: return warlock_cards;
        case CLASS_BARD: return bard_cards;
        default: return NULL;
    }
}

static bool add_to_visible_sets(const CardDef *def, const char *class_text)
{
    if (!def || !class_text) return true;

    if (strcmp(class_text, "utility") == 0)
    {
        if (utility_card_count >= MAX_UTILITY_CARDS)
        {
            LOG_E(CAT_CARD, "Too many utility cards in JSON");
            return false;
        }
        utility_cards[utility_card_count++] = *def;
        return true;
    }

    ClassType ct = def->class;
    CardDef *set = class_storage(ct);
    if (!set) return true;

    if (class_card_counts[ct] >= CLASS_CARD_COUNT)
    {
        LOG_E(CAT_CARD, "Too many cards for class %s", class_text);
        return false;
    }

    set[class_card_counts[ct]++] = *def;
    return true;
}

bool card_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_CARD, "%s", error);
        return false;
    }

    const JsonValue *cards = field(root, "cards");
    if (!cards || cards->type != JSON_ARRAY)
    {
        LOG_E(CAT_CARD, "%s: cards must be an array", path);
        json_free(root);
        return false;
    }

    reset_cards();
    int count = json_array_count(cards);
    for (int i = 0; i < count && all_card_count < MAX_JSON_CARDS; i++)
    {
        const JsonValue *item = json_array_get(cards, i);
        if (!item || item->type != JSON_OBJECT) continue;

        int slot = all_card_count;
        const char *class_text = json_string(field(item, "class"), "utility");
        CardDef def = { 0 };
        def.id = copy_text(json_string(field(item, "id"), ""));
        def.name = copy_text(json_string(field(item, "name"), ""));
        def.type = parse_card_type(json_string(field(item, "type"), "skill"));
        def.class = parse_class(class_text);
        def.cost = json_int(field(item, "cost"), 0);
        def.damage = json_int(field(item, "damage"), 0);
        def.heal = json_int(field(item, "heal"), 0);
        def.heal2 = json_int(field(item, "heal2"), 0);
        def.heal_self = json_bool(field(item, "heal_self"), false);
        def.shield = json_int(field(item, "shield"), 0);
        def.burn_stacks = json_int(field(item, "burn_stacks"), 0);
        def.taunt = json_bool(field(item, "taunt"), false);
        def.taunt_turns = json_int(field(item, "taunt_turns"), 0);
        def.interrupt = json_bool(field(item, "interrupt"), false);
        def.aggro_self = json_int(field(item, "aggro_self"), 0);
        def.exhaust = json_bool(field(item, "exhaust"), false);
        def.consume = json_bool(field(item, "consume"), false);
        def.channel = json_bool(field(item, "channel"), false);
        def.channel_turns = json_int(field(item, "channel_turns"), 0);
        def.echo = json_bool(field(item, "echo"), false);
        def.lifesteal = json_int(field(item, "lifesteal"), 0);
        def.splash = json_int(field(item, "splash"), 0);
        def.retain = json_bool(field(item, "retain"), false);
        def.fleeting = json_bool(field(item, "fleeting"), false);
        def.target = parse_target(json_string(field(item, "target"), "enemy"));
        def.repeat_hits = json_int(field(item, "repeat_hits"), 1);
        if (def.repeat_hits < 1) def.repeat_hits = 1;
        def.description = copy_text(json_string(field(item, "description"), ""));

        const JsonValue *effects = field(item, "effects");
        int effect_count = json_array_count(effects);
        if (effect_count > MAX_CARD_EFFECTS_PER_CARD)
            effect_count = MAX_CARD_EFFECTS_PER_CARD;
        for (int fx = 0; fx < effect_count; fx++)
        {
            const JsonValue *effect = json_array_get(effects, fx);
            if (!effect || effect->type != JSON_OBJECT) continue;
            effect_storage[slot][def.effect_count].type = parse_effect_type(json_string(field(effect, "type"), ""));
            effect_storage[slot][def.effect_count].status = parse_status(json_string(field(effect, "status"), ""));
            effect_storage[slot][def.effect_count].amount = json_int(field(effect, "amount"), 0);
            effect_storage[slot][def.effect_count].turns = json_int(field(effect, "turns"), 0);
            def.effect_count++;
        }
        def.effects = def.effect_count > 0 ? effect_storage[slot] : NULL;

        all_cards[slot] = def;
        all_card_count++;
        add_to_visible_sets(&all_cards[slot], class_text);
    }

    json_free(root);
    LOG_I(CAT_CARD, "Loaded %d cards from %s", all_card_count, path);
    return all_card_count > 0;
}

const CardDef *card_def_by_id(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; i < all_card_count; i++)
        if (all_cards[i].id && strcmp(all_cards[i].id, id) == 0)
            return &all_cards[i];
    return NULL;
}

int card_defs_loaded_count(void)
{
    return all_card_count;
}
