#include "synergy_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

static SynergyPairDef pairs[MAX_SYNERGY_PAIRS];
static int pair_count = 0;
static SynergyComboDef combos[MAX_SYNERGY_COMBOS];
static int combo_count = 0;
static SynergyCardStatusDef card_status[MAX_CARD_STATUS_INTERACTIONS];
static int card_status_count = 0;

static const JsonValue *field(const JsonValue *object, const char *key)
{
    return json_object_get(object, key);
}

static char *copy_text(const char *text)
{
    if (!text) text = "";
    size_t len = strlen(text);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, text, len + 1);
    return out;
}

static ClassType class_from_string(const char *s)
{
    if (!s) return CLASS_NONE;
    if (strcmp(s, "guardian") == 0) return CLASS_GUARDIAN;
    if (strcmp(s, "cleric") == 0) return CLASS_CLERIC;
    if (strcmp(s, "mage") == 0) return CLASS_MAGE;
    if (strcmp(s, "rogue") == 0) return CLASS_ROGUE;
    if (strcmp(s, "shaman") == 0) return CLASS_SHAMAN;
    if (strcmp(s, "ranger") == 0) return CLASS_RANGER;
    if (strcmp(s, "paladin") == 0) return CLASS_PALADIN;
    if (strcmp(s, "warlock") == 0) return CLASS_WARLOCK;
    if (strcmp(s, "bard") == 0) return CLASS_BARD;
    return CLASS_NONE;
}

static StatusType status_from_string(const char *s)
{
    if (!s) return STATUS_NONE;
    if (strcmp(s, "BURNING") == 0) return STATUS_BURNING;
    if (strcmp(s, "MARKED") == 0) return STATUS_MARKED;
    if (strcmp(s, "CONDUCTIVE") == 0) return STATUS_CONDUCTIVE;
    if (strcmp(s, "BLIGHT") == 0) return STATUS_BLIGHT;
    if (strcmp(s, "TOTEM_HEAL") == 0) return STATUS_TOTEM_HEAL;
    if (strcmp(s, "TRAP") == 0) return STATUS_TRAP;
    return STATUS_NONE;
}

bool synergy_defs_load_json(const char *path)
{
    char error[192] = "";
    JsonValue *root = json_load_file(path, error, sizeof(error));
    if (!root)
    {
        LOG_E(CAT_SCREEN, "%s", error);
        return false;
    }

    memset(pairs, 0, sizeof(pairs));
    memset(combos, 0, sizeof(combos));
    memset(card_status, 0, sizeof(card_status));
    pair_count = 0;
    combo_count = 0;
    card_status_count = 0;

    // ── Pair Passives ──
    const JsonValue *pair_array = field(root, "pair_passives");
    if (pair_array && pair_array->type == JSON_ARRAY)
    {
        int count = json_array_count(pair_array);
        for (int i = 0; i < count && pair_count < MAX_SYNERGY_PAIRS; i++)
        {
            const JsonValue *item = json_array_get(pair_array, i);
            if (!item || item->type != JSON_OBJECT) continue;
            SynergyPairDef *p = &pairs[pair_count];
            p->id = copy_text(json_string(field(item, "id"), ""));
            const JsonValue *classes = field(item, "classes");
            if (classes && classes->type == JSON_ARRAY && json_array_count(classes) >= 2)
            {
                p->class_a = copy_text(json_string(json_array_get(classes, 0), ""));
                p->class_b = copy_text(json_string(json_array_get(classes, 1), ""));
            }
            p->trigger = copy_text(json_string(field(item, "trigger"), ""));
            p->on_class = copy_text(json_string(field(item, "on_class"), NULL));
            p->on_status = copy_text(json_string(field(item, "on_status"), NULL));
            p->effect = copy_text(json_string(field(item, "effect"), ""));
            p->value = (float)json_int(field(item, "value"), 0);
            const JsonValue *val_field = field(item, "value");
            if (val_field && val_field->type == JSON_NUMBER)
                p->value = (float)val_field->as.number;
            p->once_per_combat = json_bool(field(item, "once_per_combat"), false);
            p->name = copy_text(json_string(field(item, "name"), ""));
            p->desc = copy_text(json_string(field(item, "desc"), ""));
            pair_count++;
        }
    }

    // ── Combos ──
    const JsonValue *combo_array = field(root, "combos");
    if (combo_array && combo_array->type == JSON_ARRAY)
    {
        int count = json_array_count(combo_array);
        for (int i = 0; i < count && combo_count < MAX_SYNERGY_COMBOS; i++)
        {
            const JsonValue *item = json_array_get(combo_array, i);
            if (!item || item->type != JSON_OBJECT) continue;
            SynergyComboDef *c = &combos[combo_count];
            c->id = copy_text(json_string(field(item, "id"), ""));
            c->prev_class = class_from_string(json_string(field(item, "prev_class"), ""));
            c->next_class = class_from_string(json_string(field(item, "next_class"), ""));
            c->title = copy_text(json_string(field(item, "title"), ""));
            c->subtitle = copy_text(json_string(field(item, "subtitle"), ""));
            c->consume_card_type = copy_text(json_string(field(item, "consume_card_type"), ""));
            c->free_cost = json_bool(field(item, "free_cost"), false);
            c->multiply_damage = (float)(field(item, "multiply_damage") ? field(item, "multiply_damage")->as.number : 1.0);
            c->multiply_heal = (float)(field(item, "multiply_heal") ? field(item, "multiply_heal")->as.number : 1.0);
            c->multiply_shield = (float)(field(item, "multiply_shield") ? field(item, "multiply_shield")->as.number : 1.0);
            c->apply_status = copy_text(json_string(field(item, "apply_status"), NULL));
            c->status_turns = json_int(field(item, "status_turns"), 0);
            c->status_value = json_int(field(item, "status_value"), 0);
            c->special_effect = copy_text(json_string(field(item, "special_effect"), NULL));
            c->combo_turns = json_int(field(item, "combo_turns"), 0);
            combo_count++;
        }
    }

    // ── Card-Status Interactions ──
    const JsonValue *csi_array = field(root, "card_status_interactions");
    if (csi_array && csi_array->type == JSON_ARRAY)
    {
        int count = json_array_count(csi_array);
        for (int i = 0; i < count && card_status_count < MAX_CARD_STATUS_INTERACTIONS; i++)
        {
            const JsonValue *item = json_array_get(csi_array, i);
            if (!item || item->type != JSON_OBJECT) continue;
            SynergyCardStatusDef *s = &card_status[card_status_count];
            s->card_id = copy_text(json_string(field(item, "card"), ""));
            s->status = status_from_string(json_string(field(item, "status"), ""));
            s->consume = json_bool(field(item, "consume"), false);
            s->effect = copy_text(json_string(field(item, "effect"), ""));
            s->value = json_int(field(item, "value"), 0);
            s->turns = json_int(field(item, "turns"), 0);
            card_status_count++;
        }
    }

    json_free(root);
    LOG_I(CAT_SCREEN, "Loaded %d pair passives, %d combos, %d card-status interactions from %s",
          pair_count, combo_count, card_status_count, path);
    return true;
}

// ── Pair passive lookups ──

int synergy_pair_count(void) { return pair_count; }
const SynergyPairDef *synergy_pair_by_index(int index)
{
    if (index < 0 || index >= pair_count) return NULL;
    return &pairs[index];
}

int synergy_pair_count_for_trigger(const char *trigger)
{
    if (!trigger) return 0;
    int count = 0;
    for (int i = 0; i < pair_count; i++)
        if (pairs[i].trigger && strcmp(pairs[i].trigger, trigger) == 0)
            count++;
    return count;
}

// ── Combo lookups ──

int synergy_combo_count(void) { return combo_count; }
const SynergyComboDef *synergy_combo_by_index(int index)
{
    if (index < 0 || index >= combo_count) return NULL;
    return &combos[index];
}

const SynergyComboDef *synergy_combo_for_chain(ClassType prev, ClassType next)
{
    for (int i = 0; i < combo_count; i++)
        if (combos[i].prev_class == prev && combos[i].next_class == next)
            return &combos[i];
    return NULL;
}

// ── Card-status interaction lookups ──

int synergy_card_status_count(void) { return card_status_count; }
const SynergyCardStatusDef *synergy_card_status_by_index(int index)
{
    if (index < 0 || index >= card_status_count) return NULL;
    return &card_status[index];
}

const SynergyCardStatusDef *synergy_card_status_for_card(const char *card_id)
{
    if (!card_id) return NULL;
    for (int i = 0; i < card_status_count; i++)
        if (card_status[i].card_id && strcmp(card_status[i].card_id, card_id) == 0)
            return &card_status[i];
    return NULL;
}
