#include "enemy_defs.h"
#include "util/json.h"
#include "util/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_JSON_ENEMIES 128

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
    if (text && strcmp(text, "buff_attack") == 0) return INTENT_BUFF_ATTACK;
    if (text && strcmp(text, "aoe_shield") == 0) return INTENT_AOE_SHIELD;
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
    if (text && strcmp(text, "thorns") == 0) return STATUS_THORNS;
    if (text && strcmp(text, "death_mark") == 0) return STATUS_DEATH_MARK;
    if (text && strcmp(text, "silence") == 0) return STATUS_SILENCE;
    if (text && strcmp(text, "maternal_bond") == 0) return STATUS_MATERNAL_BOND;
    return STATUS_NONE;
}

static float json_float_value(const JsonValue *value, float fallback)
{
    if (!value || value->type != JSON_NUMBER) return fallback;
    return (float)value->as.number;
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

ArenaAuraType enemy_parse_arena_aura_type(const char *text)
{
    if (!text) return AURA_NONE;
    if (strcmp(text, "damage_tick") == 0) return AURA_DAMAGE_TICK;
    if (strcmp(text, "heal_tick") == 0) return AURA_HEAL_TICK;
    if (strcmp(text, "shield_tick") == 0) return AURA_SHIELD_TICK;
    if (strcmp(text, "cost_up") == 0) return AURA_COST_UP;
    if (strcmp(text, "cost_down") == 0) return AURA_COST_DOWN;
    if (strcmp(text, "damage_reduction") == 0) return AURA_DAMAGE_REDUCTION;
    if (strcmp(text, "heal_reduction") == 0) return AURA_HEAL_REDUCTION;
    if (strcmp(text, "draw_reduction") == 0) return AURA_DRAW_REDUCTION;
    if (strcmp(text, "energy_drain") == 0) return AURA_ENERGY_DRAIN;
    if (strcmp(text, "enemy_damage_up") == 0) return AURA_ENEMY_DAMAGE_UP;
    if (strcmp(text, "silence_classes") == 0) return AURA_SILENCE_CLASSES;
    return AURA_NONE;
}

const char *enemy_arena_aura_name(ArenaAuraType type)
{
    switch (type)
    {
        case AURA_DAMAGE_TICK: return "Damage Tick";
        case AURA_HEAL_TICK: return "Enemy Regen";
        case AURA_SHIELD_TICK: return "Enemy Shield";
        case AURA_COST_UP: return "Cost Up";
        case AURA_COST_DOWN: return "Cost Down";
        case AURA_DAMAGE_REDUCTION: return "Damage Down";
        case AURA_HEAL_REDUCTION: return "Heal Down";
        case AURA_DRAW_REDUCTION: return "Draw Down";
        case AURA_ENERGY_DRAIN: return "Energy Drain";
        case AURA_ENEMY_DAMAGE_UP: return "Enemy Damage";
        case AURA_SILENCE_CLASSES: return "Silence";
        default: return "Aura";
    }
}

const char *enemy_arena_aura_description(ArenaAuraType type)
{
    switch (type)
    {
        case AURA_DAMAGE_TICK: return "Deals damage to your party at the start of each turn.";
        case AURA_HEAL_TICK: return "Heals all enemies at the start of each turn.";
        case AURA_SHIELD_TICK: return "Grants shield to all enemies at the start of each turn.";
        case AURA_COST_UP: return "Increases the energy cost of all your cards.";
        case AURA_COST_DOWN: return "Decreases the energy cost of enemy abilities.";
        case AURA_DAMAGE_REDUCTION: return "Reduces all damage your party deals to enemies.";
        case AURA_HEAL_REDUCTION: return "Reduces all healing your party receives.";
        case AURA_DRAW_REDUCTION: return "Reduces the number of cards drawn each turn.";
        case AURA_ENERGY_DRAIN: return "Drains your energy at the start of each turn, reducing available energy.";
        case AURA_ENEMY_DAMAGE_UP: return "Increases all damage dealt by enemies.";
        case AURA_SILENCE_CLASSES: return "Silences specific classes, preventing them from playing cards.";
        default: return "An unknown arena effect is active.";
    }
}

const char *enemy_reaction_type_name(ReactionType type)
{
    switch (type)
    {
        case REACTION_ON_PLAYER_ATTACK: return "Attack";
        case REACTION_ON_PLAYER_SKILL: return "Skill";
        case REACTION_ON_PLAYER_SHIELD: return "Shield";
        case REACTION_ON_PLAYER_HEAL: return "Heal";
        case REACTION_ON_DAMAGE_TAKEN: return "Damage";
        case REACTION_ON_CARD_PLAYED: return "Card";
        case REACTION_ON_TURN_END_ENERGY_UNSPENT: return "Unspent";
        default: return "Reaction";
    }
}

static InvulnerableUntilType parse_invulnerable_until(const char *text)
{
    if (!text) return INVULN_NONE;
    if (strcmp(text, "all_adds_dead") == 0) return INVULN_ALL_ADDS_DEAD;
    if (strcmp(text, "damage_threshold") == 0) return INVULN_DAMAGE_THRESHOLD;
    if (strcmp(text, "turns") == 0) return INVULN_TURNS;
    if (strcmp(text, "card_played") == 0) return INVULN_CARD_PLAYED;
    return INVULN_NONE;
}

static TetherTargetType parse_tether_target(const char *text)
{
    if (!text) return TETHER_RANDOM;
    if (strcmp(text, "lowest_hp") == 0) return TETHER_LOWEST_HP;
    if (strcmp(text, "lowest_shield") == 0) return TETHER_LOWEST_SHIELD;
    if (strcmp(text, "last_attacker") == 0) return TETHER_LAST_ATTACKER;
    return TETHER_RANDOM;
}

static DetonateTargetType parse_detonate_target(const char *text)
{
    if (!text) return DETONATE_SINGLE;
    if (strcmp(text, "self") == 0) return DETONATE_SELF;
    if (strcmp(text, "all") == 0) return DETONATE_ALL;
    return DETONATE_SINGLE;
}

static CursePileType parse_curse_pile(const char *text)
{
    if (!text) return CURSE_PILE_DRAW;
    if (strcmp(text, "discard") == 0) return CURSE_PILE_DISCARD;
    if (strcmp(text, "hand") == 0) return CURSE_PILE_HAND;
    return CURSE_PILE_DRAW;
}

static ReactionType parse_reaction_type(const char *text)
{
    if (!text) return REACTION_NONE;
    if (strcmp(text, "on_player_attack") == 0) return REACTION_ON_PLAYER_ATTACK;
    if (strcmp(text, "on_player_skill") == 0) return REACTION_ON_PLAYER_SKILL;
    if (strcmp(text, "on_player_shield") == 0) return REACTION_ON_PLAYER_SHIELD;
    if (strcmp(text, "on_player_heal") == 0) return REACTION_ON_PLAYER_HEAL;
    if (strcmp(text, "on_damage_taken") == 0) return REACTION_ON_DAMAGE_TAKEN;
    if (strcmp(text, "on_card_played") == 0) return REACTION_ON_CARD_PLAYED;
    if (strcmp(text, "on_turn_end_energy_unspent") == 0) return REACTION_ON_TURN_END_ENERGY_UNSPENT;
    return REACTION_NONE;
}

static ReflectType parse_reflect_type(const char *text)
{
    if (!text) return REFLECT_DAMAGE;
    if (strcmp(text, "status") == 0) return REFLECT_STATUS;
    if (strcmp(text, "shield_on_boss") == 0) return REFLECT_SHIELD_ON_BOSS;
    return REFLECT_DAMAGE;
}

static EnemyAIOverride parse_ai_override(const char *text)
{
    if (!text) return AI_WEIGHTED;
    if (strcmp(text, "pattern") == 0) return AI_PATTERN;
    if (strcmp(text, "reactive") == 0) return AI_REACTIVE;
    return AI_WEIGHTED;
}

static ThresholdResetType parse_threshold_reset(const char *text)
{
    if (!text) return THRESHOLD_RESET_END_OF_TURN;
    if (strcmp(text, "per_combat") == 0) return THRESHOLD_RESET_PER_COMBAT;
    if (strcmp(text, "per_phase") == 0) return THRESHOLD_RESET_PER_PHASE;
    return THRESHOLD_RESET_END_OF_TURN;
}

static RageFormulaType parse_rage_formula(const char *text)
{
    if (!text) return RAGE_FORMULA_LINEAR;
    if (strcmp(text, "exponential") == 0) return RAGE_FORMULA_EXPONENTIAL;
    if (strcmp(text, "step") == 0) return RAGE_FORMULA_STEP;
    return RAGE_FORMULA_LINEAR;
}

static MinionBuffType parse_minion_buff(const char *text)
{
    if (text && strcmp(text, "shield_per_turn") == 0) return MINION_BUFF_SHIELD_PER_TURN;
    return MINION_BUFF_DAMAGE_BOOST;
}

static ArenaAuraDef parse_aura_def(const JsonValue *object)
{
    ArenaAuraDef out = { 0 };
    if (!object || object->type != JSON_OBJECT) return out;
    out.type = enemy_parse_arena_aura_type(json_string(field(object, "type"), json_string(field(object, "aura_type"), "")));
    out.value = json_int(field(object, "value"), 0);
    out.duration = json_int(field(object, "duration"), 0);
    out.clear_on_damage_threshold = json_int(field(object, "clear_on_damage_threshold"), 0);
    return out;
}

static void parse_int_array(const JsonValue *array, int *out, int *out_count, int max_count)
{
    if (!out || !out_count) return;
    *out_count = 0;
    int count = json_array_count(array);
    if (count > max_count) count = max_count;
    for (int i = 0; i < count; i++)
        out[(*out_count)++] = json_int(json_array_get(array, i), 0);
}

static void parse_threshold_response(DamageThresholdDef *out, const char *text)
{
    if (!out || !text || !text[0]) return;
    char key[48] = "";
    int value = 0;
    if (sscanf(text, "%47[^:]:%d", key, &value) != 2) return;
    if (strcmp(key, "gain_shield") == 0) out->gain_shield = value;
    else if (strcmp(key, "heal") == 0) out->heal = value;
    else if (strcmp(key, "retaliate") == 0) out->retaliate = value;
    else if (strcmp(key, "gain_energy") == 0) out->gain_energy = value;
    else if (strcmp(key, "apply_weakness") == 0)
    {
        out->apply_status = STATUS_WEAKNESS;
        out->status_turns = 1;
        out->status_value = value;
    }
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
        def.accumulate = json_bool(field(item, "accumulate"), false);
        def.accumulator_cap = json_int(field(item, "accumulator_cap"), 20);
        if (def.accumulator_cap < 1) def.accumulator_cap = 20;
        def.gain_on_player_card_play = json_int(field(item, "gain_on_player_card_play"), 0);
        def.gain_on_player_attack = json_int(field(item, "gain_on_player_attack"), 0);
        def.gain_on_player_skill = json_int(field(item, "gain_on_player_skill"), 0);
        def.gain_on_player_shield = json_int(field(item, "gain_on_player_shield"), 0);
        def.gain_on_player_heal = json_int(field(item, "gain_on_player_heal"), 0);
        def.gain_on_boss_attacked = json_int(field(item, "gain_on_boss_attacked"), 0);
        def.gain_on_turn_start = json_int(field(item, "gain_on_turn_start"), 0);
        def.gain_on_ally_damaged = json_int(field(item, "gain_on_ally_damaged"), 0);
        def.decay_per_turn = json_int(field(item, "decay_per_turn"), 0);
        def.decay_on_defend = json_bool(field(item, "decay_on_defend"), false);
        def.linked_group_id = copy_text(json_string(field(item, "linked_group_id"), ""));
        def.linked_revive = json_bool(field(item, "linked_revive"), false);
        def.linked_revive_hp = json_int(field(item, "linked_revive_hp"), 50);
        def.linked_revive_turns = json_int(field(item, "linked_revive_turns"), 0);
        def.linked_revive_stagger = json_int(field(item, "linked_revive_stagger"), 0);
        parse_int_array(field(item, "opening_sequence"), def.opening_sequence, &def.opening_count, MAX_ENEMY_SEQUENCE);
        parse_int_array(field(item, "pattern_cycle"), def.pattern_cycle, &def.pattern_count, MAX_ENEMY_SEQUENCE);
        def.pattern_loop = json_bool(field(item, "pattern_loop"), true);
        def.ai_override = parse_ai_override(json_string(field(item, "ai_override"), "weighted"));
        def.threshold_reset = parse_threshold_reset(json_string(field(item, "threshold_reset"), "end_of_turn"));
        def.rage_scale = json_float_value(field(item, "rage_scale"), 0.0f);
        def.rage_hp_threshold = json_int(field(item, "rage_hp_threshold"), 0);
        def.rage_max_mult = json_float_value(field(item, "rage_max_mult"), 0.0f);
        def.rage_formula = parse_rage_formula(json_string(field(item, "rage_formula"), "linear"));
        def.start_aura = parse_aura_def(field(item, "arena_aura"));
        if (def.start_aura.type != AURA_NONE)
            def.start_auras[def.start_aura_count++] = def.start_aura;
        const JsonValue *start_auras = field(item, "arena_auras");
        int start_aura_count = json_array_count(start_auras);
        if (start_aura_count > MAX_ENEMY_AURAS) start_aura_count = MAX_ENEMY_AURAS;
        for (int aa = 0; aa < start_aura_count && def.start_aura_count < MAX_ENEMY_AURAS; aa++)
        {
            ArenaAuraDef aura = parse_aura_def(json_array_get(start_auras, aa));
            if (aura.type != AURA_NONE)
                def.start_auras[def.start_aura_count++] = aura;
        }

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
            out->repeats = json_int(field(card, "repeats"), 1);
            if (out->repeats < 1) out->repeats = 1;
            out->lifesteal_pct = json_int(field(card, "lifesteal_pct"), 0);
            out->self_damage = json_int(field(card, "self_damage"), 0);
            out->buff_damage = json_int(field(card, "buff_damage"), 0);
            out->buff_turns = json_int(field(card, "buff_turns"), 0);
            out->interrupts = json_bool(field(card, "interrupts"), false);
            out->enrage_allies = json_bool(field(card, "enrage_allies"), false);
            out->spend = json_int(field(card, "spend"), 0);
            out->spend_all = json_bool(field(card, "spend_all"), false);
            out->damage_per_spent = json_int(field(card, "damage_per_spent"), 0);
            out->heal_per_spent = json_int(field(card, "heal_per_spent"), 0);
            out->shield_per_spent = json_int(field(card, "shield_per_spent"), 0);
            out->status_per_spent = json_int(field(card, "status_per_spent"), 0);
            out->detonate_status = parse_status(json_string(field(card, "detonate_status"), ""));
            out->detonate_mult = json_int(field(card, "detonate_mult"), 0);
            out->detonate_target = parse_detonate_target(json_string(field(card, "detonate_target"), out->target == ENEMY_TARGET_ALL ? "all" : "single"));
            out->detonate_heal_mult = json_int(field(card, "detonate_heal_mult"), 0);
            out->detonate_shield_mult = json_int(field(card, "detonate_shield_mult"), 0);
            out->detonate_clear = json_bool(field(card, "detonate_clear"), out->detonate_status != STATUS_NONE);
            out->double_status_on_party = json_bool(field(card, "double_status_on_party"), false);
            out->double_status = parse_status(json_string(field(card, "double_status"), "blight"));
            out->tether = json_bool(field(card, "tether"), false);
            out->tether_duration = json_int(field(card, "tether_duration"), 1);
            out->tether_target = parse_tether_target(json_string(field(card, "tether_target"), "random"));
            out->tether_transferrable = json_bool(field(card, "tether_transferrable"), true);
            out->set_invulnerable = json_bool(field(card, "set_invulnerable"), false);
            out->invulnerable_until = parse_invulnerable_until(json_string(field(card, "invulnerable_until"), ""));
            out->invulnerable_turns = json_int(field(card, "invulnerable_turns"), 0);
            out->invulnerable_clear_on_damage = json_int(field(card, "invulnerable_clear_on_damage"), 0);
            out->summon_id = copy_text(json_string(field(card, "summon_id"), ""));
            out->summon_count = json_int(field(card, "summon_count"), 0);
            out->phase_gate_hp_pct = json_int(field(card, "phase_gate_hp_pct"), 0);
            out->gate_summon_id = copy_text(json_string(field(card, "gate_summon_id"), ""));
            out->gate_summon_count = json_int(field(card, "gate_summon_count"), 0);
            out->gate_aura = parse_aura_def(field(card, "gate_arena_aura"));
            if (out->gate_aura.type == AURA_NONE && field(card, "gate_aura_type"))
            {
                out->gate_aura.type = enemy_parse_arena_aura_type(json_string(field(card, "gate_aura_type"), ""));
                out->gate_aura.value = json_int(field(card, "gate_aura_value"), 0);
                out->gate_aura.duration = json_int(field(card, "gate_aura_duration"), 0);
            }
            out->gate_deck_override = copy_text(json_string(field(card, "gate_deck_override"), ""));
            out->curse_count = json_int(field(card, "curse_count"), 0);
            out->curse_card_id = copy_text(json_string(field(card, "curse_card_id"), "dazed"));
            out->curse_pile = parse_curse_pile(json_string(field(card, "curse_pile"), "draw"));
            out->steal_card = json_bool(field(card, "steal_card"), false);
            out->steal_return_turns = json_int(field(card, "steal_return_turns"), 2);
            out->steal_use_against = json_bool(field(card, "steal_use_against"), false);
            out->exhaust_from_deck = json_int(field(card, "exhaust_from_deck"), json_int(field(card, "exhaust_count"), 0));
            out->force_discard = json_int(field(card, "force_discard"), 0);
            out->increase_costs = json_int(field(card, "increase_costs"), 0);
            out->decrease_hand_size = json_int(field(card, "decrease_hand_size"), 0);
            out->set_player_energy_zero = json_bool(field(card, "set_player_energy_zero"), false);
            out->damage_per_player_energy_drained = json_int(field(card, "damage_per_player_energy_drained"), 0);
            out->apply_arena_aura = parse_aura_def(field(card, "apply_arena_aura"));
            out->reaction_type = parse_reaction_type(json_string(field(card, "reaction_type"), ""));
            out->reaction_trigger = json_int(field(card, "reaction_trigger"), 1);
            if (out->reaction_trigger < 1) out->reaction_trigger = 1;
            out->reaction_once = json_bool(field(card, "reaction_once"), true);
            out->reaction_priority = json_int(field(card, "reaction_priority"), 0);
            out->reflect_pct = json_int(field(card, "reflect_pct"), 0);
            out->reflect_type = parse_reflect_type(json_string(field(card, "reflect_type"), "damage"));
            out->reflect_turns = json_int(field(card, "reflect_turns"), 0);
            out->reflect_cap = json_int(field(card, "reflect_cap"), 0);
            out->cast_stages = json_int(field(card, "cast_stages"), 1);
            if (out->cast_stages < 1) out->cast_stages = 1;
            if (out->cast_stages > MAX_ENEMY_CAST_STAGES) out->cast_stages = MAX_ENEMY_CAST_STAGES;
            const JsonValue *labels = field(card, "stage_labels");
            int label_count = json_array_count(labels);
            if (label_count > MAX_ENEMY_CAST_STAGES) label_count = MAX_ENEMY_CAST_STAGES;
            for (int si = 0; si < label_count; si++)
                out->stage_labels[si] = copy_text(json_string(json_array_get(labels, si), ""));
            const JsonValue *effects = field(card, "stage_effects");
            int effect_count = json_array_count(effects);
            if (effect_count > MAX_ENEMY_CAST_STAGES) effect_count = MAX_ENEMY_CAST_STAGES;
            for (int si = 0; si < effect_count; si++)
            {
                const JsonValue *stage = json_array_get(effects, si);
                if (!stage || stage->type != JSON_OBJECT) continue;
                CastStageEffectDef *stage_out = &out->stage_effects[out->stage_effect_count++];
                stage_out->turn = json_int(field(stage, "turn"), si + 1);
                stage_out->effect = copy_text(json_string(field(stage, "effect"), ""));
                stage_out->value = json_int(field(stage, "value"), 0);
                stage_out->enemy_id = copy_text(json_string(field(stage, "enemy_id"), json_string(field(stage, "id"), "")));
                stage_out->count = json_int(field(stage, "count"), 1);
            }
            const JsonValue *interruptible = field(card, "interruptible_stages");
            int interrupt_count = json_array_count(interruptible);
            for (int si = 0; si < interrupt_count; si++)
            {
                int stage = json_int(json_array_get(interruptible, si), 0);
                if (stage > 0 && stage <= 31)
                    out->interruptible_stage_mask |= 1u << (unsigned int)(stage - 1);
            }
            if (interrupt_count <= 0)
                out->interruptible_stage_mask = 0xffffffffu;
            out->stage_warning = json_bool(field(card, "stage_warning"), false);
        }

        const JsonValue *phases = field(item, "phases");
        int phase_count = json_array_count(phases);
        if (phase_count > MAX_ENEMY_PHASES) phase_count = MAX_ENEMY_PHASES;
        for (int p = 0; p < phase_count; p++)
        {
            const JsonValue *phase = json_array_get(phases, p);
            if (!phase || phase->type != JSON_OBJECT) continue;
            EnemyPhaseDef *out = &def.phases[def.phase_count++];
            out->trigger_hp_pct = json_int(field(phase, "trigger_hp_pct"), 50);

            const JsonValue *on_enter = field(phase, "on_enter");
            if (on_enter && on_enter->type == JSON_OBJECT)
            {
                out->gain_shield = json_int(field(on_enter, "gain_shield"), 0);
                out->gain_energy = json_int(field(on_enter, "gain_energy"), 0);
                out->heal_pct = json_int(field(on_enter, "heal_pct"), 0);
                out->set_invulnerable = json_bool(field(on_enter, "set_invulnerable"), false);
                out->invulnerable_turns = json_int(field(on_enter, "invulnerable_turns"), 0);
                out->swap_deck = copy_text(json_string(field(on_enter, "swap_deck"), ""));
                const JsonValue *party_status = field(on_enter, "apply_status_to_party");
                if (party_status && party_status->type == JSON_OBJECT)
                {
                    out->party_status = parse_status(json_string(field(party_status, "type"), ""));
                    out->party_status_turns = json_int(field(party_status, "turns"), 0);
                    out->party_status_value = json_int(field(party_status, "value"), 0);
                }
                const JsonValue *spawn = field(on_enter, "spawn_enemies");
                if (spawn && spawn->type == JSON_OBJECT)
                {
                    out->spawn_id = copy_text(json_string(field(spawn, "id"), ""));
                    out->spawn_count = json_int(field(spawn, "count"), 0);
                }
                out->aura = parse_aura_def(field(on_enter, "apply_arena_aura"));
                if (out->aura.type != AURA_NONE)
                    out->auras[out->aura_count++] = out->aura;
                const JsonValue *auras = field(on_enter, "apply_arena_auras");
                int aura_count = json_array_count(auras);
                if (aura_count > MAX_ENEMY_AURAS) aura_count = MAX_ENEMY_AURAS;
                for (int aa = 0; aa < aura_count && out->aura_count < MAX_ENEMY_AURAS; aa++)
                {
                    ArenaAuraDef aura = parse_aura_def(json_array_get(auras, aa));
                    if (aura.type != AURA_NONE)
                        out->auras[out->aura_count++] = aura;
                }
            }

            const JsonValue *modify = field(phase, "modify_stats");
            if (modify && modify->type == JSON_OBJECT)
            {
                out->energy_per_turn_delta = json_int(field(modify, "energy_per_turn_delta"), 0);
                out->hand_size_delta = json_int(field(modify, "hand_size_delta"), 0);
                out->damage_scale_delta = json_float_value(field(modify, "damage_scale_delta"), 0.0f);
                out->cast_time_reduction = json_int(field(modify, "cast_time_reduction"), 0);
            }
            parse_int_array(field(phase, "new_cards"), out->new_cards, &out->new_card_count, MAX_ENEMY_PHASE_CARD_REFS);
            parse_int_array(field(phase, "remove_cards"), out->remove_cards, &out->remove_card_count, MAX_ENEMY_PHASE_CARD_REFS);
        }

        const JsonValue *reactive_cards = field(item, "reactive_cards");
        int reactive_count = json_array_count(reactive_cards);
        if (reactive_count > MAX_ENEMY_REACTIVE_CARDS) reactive_count = MAX_ENEMY_REACTIVE_CARDS;
        for (int r = 0; r < reactive_count; r++)
        {
            const JsonValue *reactive = json_array_get(reactive_cards, r);
            if (!reactive || reactive->type != JSON_OBJECT) continue;
            ReactiveCardDef *out = &def.reactive_cards[def.reactive_count++];
            out->card_index = json_int(field(reactive, "card_index"), 0);
            out->condition = copy_text(json_string(field(reactive, "condition"), "always"));
        }

        const JsonValue *minion_buffs = field(item, "minion_buffs");
        int buff_count = json_array_count(minion_buffs);
        if (buff_count > MAX_ENEMY_MINION_BUFFS) buff_count = MAX_ENEMY_MINION_BUFFS;
        for (int b = 0; b < buff_count; b++)
        {
            const JsonValue *buff = json_array_get(minion_buffs, b);
            if (!buff || buff->type != JSON_OBJECT) continue;
            MinionBuffDef *out = &def.minion_buffs[def.minion_buff_count++];
            out->effect = parse_minion_buff(json_string(field(buff, "effect"), "damage_boost"));
            out->value = json_int(field(buff, "value"), 0);
        }
        def.minion_buff_tooltip = copy_text(json_string(field(item, "minion_buff_tooltip"), ""));

        const JsonValue *thresholds = field(item, "damage_thresholds");
        int threshold_count = json_array_count(thresholds);
        if (threshold_count > MAX_ENEMY_THRESHOLDS) threshold_count = MAX_ENEMY_THRESHOLDS;
        for (int t = 0; t < threshold_count; t++)
        {
            const JsonValue *threshold = json_array_get(thresholds, t);
            if (!threshold || threshold->type != JSON_OBJECT) continue;
            DamageThresholdDef *out = &def.damage_thresholds[def.damage_threshold_count++];
            out->threshold = json_int(field(threshold, "threshold"), 0);
            parse_threshold_response(out, json_string(field(threshold, "response"), ""));
            out->gain_shield = json_int(field(threshold, "gain_shield"), out->gain_shield);
            out->heal = json_int(field(threshold, "heal"), out->heal);
            out->retaliate = json_int(field(threshold, "retaliate"), out->retaliate);
            out->gain_energy = json_int(field(threshold, "gain_energy"), out->gain_energy);
            out->clear_debuffs = json_bool(field(threshold, "clear_debuffs"), out->clear_debuffs);
            out->apply_status = parse_status(json_string(field(threshold, "apply_status"), ""));
            out->status_turns = json_int(field(threshold, "status_turns"), out->status_turns);
            out->status_value = json_int(field(threshold, "status_value"), out->status_value);
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

const EnemyDef *enemy_def_by_index(int index)
{
    if (index < 0 || index >= enemy_count) return NULL;
    return &enemy_defs[index];
}

int enemy_defs_loaded_count(void)
{
    return enemy_count;
}
