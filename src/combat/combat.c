#include "combat.h"
#include "game.h"
#include "data/area_defs.h"
#include "data/card_defs.h"
#include "data/enemy_defs.h"
#include "data/synergy_defs.h"
#include "systems/relic.h"
#include "systems/telemetry.h"
#include "ui/floating_text.h"
#include "util/text.h"
#include "ui/layout.h"
#include "ui/theme.h"
#include "util/tween.h"
#include "util/shake.h"
#include "util/log.h"
#include "constants.h"
#include "assets.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#define MAX_PARTY_AGGRO 200
#define PARTY_AGGRO_DECAY_PERCENT_PER_TURN 5

static int party_draw_count(int party_count)
{
    if (party_count <= 1) return 3;
    if (party_count == 2) return 4;
    if (party_count >= 5) return 6;
    return 5;
}

static int party_start_energy(int party_count)
{
    if (party_count <= 1) return 2;
    if (party_count == 2) return 3;
    if (party_count >= 5) return 5;
    return 4;
}

static int party_regen(int party_count)
{
    if (party_count <= 2) return 2;
    if (party_count >= 4) return 4;
    return BASE_REGEN;
}

static void check_victory(CombatState *cs);
static void check_defeat(CombatState *cs);
static void combo_prime_clear(CombatState *cs);

static int active_ascension(void)
{
    int level = g_state.meta.ascension_level;
    if (level < 0) level = 0;
    if (level > META_ASCENSION_MAX) level = META_ASCENSION_MAX;
    return level;
}

static int party_member_shield_cap(const PartyMember *pm)
{
    if (!pm || pm->max_hp <= 0) return 0;
    int pct = meta_shield_cap_percent(&g_state.meta);
    int cap = (pm->max_hp * pct + 99) / 100;
    return cap < 1 ? 1 : cap;
}

static int add_party_member_shield_capped(PartyMember *pm, int amount)
{
    if (!pm || amount <= 0) return 0;
    int before = pm->shield;
    int cap = party_member_shield_cap(pm);
    pm->shield += amount;
    if (pm->shield > cap) pm->shield = cap;
    if (pm->shield < 0) pm->shield = 0;
    return pm->shield - before;
}

static void set_party_member_shield_capped(PartyMember *pm, int shield)
{
    if (!pm) return;
    pm->shield = shield;
    int cap = party_member_shield_cap(pm);
    if (pm->shield > cap) pm->shield = cap;
    if (pm->shield < 0) pm->shield = 0;
}

static int clamp_party_aggro(int aggro)
{
    if (aggro < 0) return 0;
    if (aggro > MAX_PARTY_AGGRO) return MAX_PARTY_AGGRO;
    return aggro;
}

static void set_party_member_aggro(PartyMember *pm, int aggro)
{
    if (!pm) return;
    pm->aggro = clamp_party_aggro(aggro);
}

static void add_party_member_aggro(PartyMember *pm, int amount)
{
    if (!pm) return;
    set_party_member_aggro(pm, pm->aggro + amount);
}

static int party_aggro_decay_amount(int aggro)
{
    if (aggro <= 0) return 0;
    int decay = (aggro * PARTY_AGGRO_DECAY_PERCENT_PER_TURN + 99) / 100;
    return decay < 1 ? 1 : decay;
}

static const char *target_type_name(TargetType target)
{
    switch (target)
    {
        case TARGET_ENEMY: return "enemy";
        case TARGET_ALL_ENEMIES: return "all_enemies";
        case TARGET_ALLY: return "ally";
        case TARGET_ALL_ALLIES: return "all_allies";
        case TARGET_SELF: return "self";
    }
    return "unknown";
}

static const char *encounter_type_name(void)
{
    return g_state.encounter_is_boss ? "boss" : (g_state.encounter_is_elite ? "elite" : "normal");
}

static void encounter_id_string(const CombatState *cs, char *out, int out_size)
{
    if (!out || out_size <= 0) return;
    out[0] = '\0';
    if (!cs) return;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        const char *id = cs->enemies[i].def && cs->enemies[i].def->id ? cs->enemies[i].def->id : "unknown";
        if (i > 0) strncat(out, "+", out_size - strlen(out) - 1);
        strncat(out, id, out_size - strlen(out) - 1);
    }
}

static void log_card_play_metric(CombatState *cs, const CardDef *card, int upgrade_level, int paid_cost, int target_enemy, int target_ally)
{
    if (!cs || !card) return;
    char run_id[16], area[16], floor[16], turn[16], cost[16], upgrade[16], exhaust[8], consume[8], enemy[16], ally[16];
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(area, sizeof(area), "%d", g_state.current_area);
    snprintf(floor, sizeof(floor), "%d", g_state.map.floor + 1);
    snprintf(turn, sizeof(turn), "%d", cs->turn);
    snprintf(cost, sizeof(cost), "%d", paid_cost);
    snprintf(upgrade, sizeof(upgrade), "%d", upgrade_level);
    snprintf(exhaust, sizeof(exhaust), "%d", card->exhaust || card->channel ? 1 : 0);
    snprintf(consume, sizeof(consume), "%d", card->consume ? 1 : 0);
    snprintf(enemy, sizeof(enemy), "%d", target_enemy);
    snprintf(ally, sizeof(ally), "%d", target_ally);

    const char *fields[] = {
        run_id,
        area,
        floor,
        encounter_type_name(),
        card->id ? card->id : "",
        class_name(card->class),
        cost,
        turn,
        target_type_name(card->target),
        upgrade,
        exhaust,
        consume,
        enemy,
        ally
    };
    telemetry_csv_append(
        "card_play_metrics.csv",
        "timestamp,run_id,area,floor,encounter,card_id,class,energy_cost,combat_turn,target_type,upgrade_level,exhaust,consume,target_enemy,target_ally",
        fields,
        14);

    char json[512];
    snprintf(json, sizeof(json),
        "\"area\":%d,\"floor\":%d,\"encounter\":\"%s\",\"card_id\":\"%s\",\"class\":\"%s\",\"energy_cost\":%d,\"combat_turn\":%d,\"target_type\":\"%s\",\"upgraded\":%d,\"exhaust\":%s,\"consume\":%s,\"target_enemy\":%d,\"target_ally\":%d",
        g_state.current_area,
        g_state.map.floor + 1,
        encounter_type_name(),
        card->id ? card->id : "",
        class_name(card->class),
        paid_cost,
        cs->turn,
        target_type_name(card->target),
        upgrade_level,
        (card->exhaust || card->channel) ? "true" : "false",
        card->consume ? "true" : "false",
        target_enemy,
        target_ally);
    telemetry_push_json("card_play", json);
}

static void log_death_metric(CombatState *cs, const PartyMember *pm, const char *source)
{
    if (!cs || !pm) return;
    char run_id[16], area[16], floor[16], turn[16], encounter_id[160];
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(area, sizeof(area), "%d", g_state.current_area);
    snprintf(floor, sizeof(floor), "%d", g_state.map.floor + 1);
    snprintf(turn, sizeof(turn), "%d", cs->turn);
    encounter_id_string(cs, encounter_id, sizeof(encounter_id));
    const char *fields[] = {
        run_id,
        area,
        floor,
        encounter_id,
        encounter_type_name(),
        class_name(pm->class),
        turn,
        source ? source : ""
    };
    telemetry_csv_append(
        "death_metrics.csv",
        "timestamp,run_id,area,floor,encounter_id,encounter_type,class,combat_turn,source",
        fields,
        8);

    char json[512];
    snprintf(json, sizeof(json),
        "\"area\":%d,\"floor\":%d,\"encounter_id\":\"%s\",\"encounter_type\":\"%s\",\"class\":\"%s\",\"combat_turn\":%d,\"source\":\"%s\"",
        g_state.current_area,
        g_state.map.floor + 1,
        encounter_id,
        encounter_type_name(),
        class_name(pm->class),
        cs->turn,
        source ? source : "");
    telemetry_push_json("death_event", json);
}

static void deal_cards(Deck *deck, int count)
{
    if (count < 0) count = 0;
    assets_play_sfx(SFX_CARD_DRAW);
    for (int i = 0; i < count; i++)
        if (deck_draw(deck) < 0) break;
}

static void deal_opening_hand(Deck *deck, int party_count, int ascension_level)
{
    int draw = party_draw_count(party_count);
    draw += meta_first_draw_bonus(&g_state.meta);
    if (ascension_level >= 8)
        draw--;
    if (draw < 1) draw = 1;
    deal_cards(deck, draw);
}

static bool card_is(const CardDef *card, const char *id)
{
    return card && card->id && strcmp(card->id, id) == 0;
}

static bool party_has_class(CombatState *cs, ClassType ct)
{
    if (!cs || ct == CLASS_NONE) return false;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive && cs->party.members[i].class == ct)
            return true;
    return false;
}

static bool party_has_pair(CombatState *cs, ClassType a, ClassType b)
{
    return party_has_class(cs, a) && party_has_class(cs, b);
}

static void calc_enemy_positions(EnemyState *enemies, int count)
{
    for (int i = 0; i < count; i++)
    {
        Vector2 pos = layout_enemy_position(MAX_ENEMIES, i);
        enemies[i].pos_x = (int)pos.x;
        enemies[i].pos_y = (int)pos.y;
    }
}

static Vector2 party_feedback_pos(CombatState *cs, int ally_idx)
{
    Rectangle frame = layout_party_frame_rect(cs->party.count, ally_idx);
    return (Vector2){ frame.x + frame.width * 0.5f, frame.y + frame.height + 10.0f };
}

static void advance_turn(CombatState *cs);
static void combat_end_turn_internal(CombatState *cs);
static void boss_add_arena_effect(CombatState *cs, ArenaAuraDef def, int source_enemy);
static void boss_remove_arena_effects_from_source(CombatState *cs, int source_enemy);
int boss_arena_value(const CombatState *cs, ArenaAuraType type);
static bool boss_arena_active(const ArenaEffect *aura);
static void boss_apply_phase_transitions(CombatState *cs, int enemy_idx);
static void boss_on_enemy_hp_damage(CombatState *cs, int enemy_idx, int hp_damage);
static void boss_trigger_reactions(CombatState *cs, ReactionType trigger, const CardDef *card, int source_enemy);
static void boss_clear_invulnerability_if_ready(CombatState *cs);
static void boss_handle_linked_damage(CombatState *cs, int enemy_idx, int hp_damage);
static void boss_tick_linked_revives(CombatState *cs);
static void boss_sync_linked_group(CombatState *cs, int group_idx);
static void combat_init_enemy_state(CombatState *cs, EnemyState *e, const EnemyDef *ed, int slot, float hp_scale);

static void combat_feed_add(CombatState *cs, const char *fmt, ...)
{
    for (int i = 4; i > 0; i--)
    {
        strcpy(cs->action_feed[i], cs->action_feed[i - 1]);
        cs->action_feed_timer[i] = cs->action_feed_timer[i - 1];
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(cs->action_feed[0], sizeof(cs->action_feed[0]), fmt, args);
    va_end(args);
    cs->action_feed_timer[0] = 4.0f;
}

static void combat_set_turn_banner(CombatState *cs, const char *text)
{
    snprintf(cs->turn_banner_text, sizeof(cs->turn_banner_text), "%s", text);
    cs->turn_banner_timer = 0.9f;
}

static void combat_flash_played_card(CombatState *cs, const CardDef *card, int target_enemy, int target_ally)
{
    if (!card) return;

    snprintf(cs->play_flash_text, sizeof(cs->play_flash_text), "%s", card->name);
    cs->play_flash_timer = 0.55f;

    if (target_enemy >= 0 && target_enemy < cs->enemy_count)
    {
        cs->play_flash_x = (float)(cs->enemies[target_enemy].pos_x - 42);
        cs->play_flash_y = (float)(cs->enemies[target_enemy].pos_y - 12);
    }
    else if (target_ally >= 0 && target_ally < cs->party.count)
    {
        Vector2 p = party_feedback_pos(cs, target_ally);
        cs->play_flash_x = p.x - 42.0f;
        cs->play_flash_y = p.y;
    }
    else
    {
        cs->play_flash_x = (float)(VIRT_W / 2 - 42);
        cs->play_flash_y = (float)(VIRT_H / 2);
    }
}

static void combat_flash_echo(CombatState *cs, int target_enemy, int target_ally)
{
    snprintf(cs->play_flash_text, sizeof(cs->play_flash_text), "ECHO");
    cs->play_flash_timer = 0.55f;

    if (target_enemy >= 0 && target_enemy < cs->enemy_count)
    {
        cs->play_flash_x = (float)(cs->enemies[target_enemy].pos_x - 42);
        cs->play_flash_y = (float)(cs->enemies[target_enemy].pos_y - 12);
    }
    else if (target_ally >= 0 && target_ally < cs->party.count)
    {
        Vector2 p = party_feedback_pos(cs, target_ally);
        cs->play_flash_x = p.x - 42.0f;
        cs->play_flash_y = p.y;
    }
    else
    {
        cs->play_flash_x = (float)(VIRT_W / 2 - 42);
        cs->play_flash_y = (float)(VIRT_H / 2);
    }
}

// ── Damage / heal / shield / taunt ──────────────────────────

static bool boss_arena_active(const ArenaEffect *aura)
{
    return aura && aura->type != AURA_NONE && aura->disrupt_turns <= 0;
}

int boss_arena_value(const CombatState *cs, ArenaAuraType type)
{
    if (!cs || type == AURA_NONE) return 0;
    int total = 0;
    for (int i = 0; i < cs->arena_effect_count; i++)
        if (boss_arena_active(&cs->arena_effects[i]) && cs->arena_effects[i].type == type)
            total += cs->arena_effects[i].value;
    return total;
}

static void boss_disrupt_auras(CombatState *cs, int turns)
{
    if (!cs || turns <= 0) return;
    bool any = false;
    for (int i = 0; i < cs->arena_effect_count; i++)
    {
        ArenaEffect *aura = &cs->arena_effects[i];
        if (aura->type == AURA_NONE) continue;
        if (turns > aura->disrupt_turns)
            aura->disrupt_turns = turns;
        any = true;
    }
    if (any)
        combat_feed_add(cs, "Arena aura disrupted");
}

static void boss_remove_arena_effects_from_source(CombatState *cs, int source_enemy)
{
    if (!cs || source_enemy < 0) return;
    bool any = false;
    for (int i = cs->arena_effect_count - 1; i >= 0; i--)
    {
        ArenaEffect *aura = &cs->arena_effects[i];
        if (aura->type == AURA_NONE || aura->source_enemy != source_enemy) continue;
        for (int j = i; j < cs->arena_effect_count - 1; j++)
            cs->arena_effects[j] = cs->arena_effects[j + 1];
        cs->arena_effect_count--;
        any = true;
    }
    if (any)
        combat_feed_add(cs, "Gate aura fades");
}

static void boss_add_arena_effect(CombatState *cs, ArenaAuraDef def, int source_enemy)
{
    if (!cs || def.type == AURA_NONE || def.value == 0) return;

    for (int i = 0; i < cs->arena_effect_count; i++)
    {
        ArenaEffect *aura = &cs->arena_effects[i];
        if (aura->type == def.type && aura->source_enemy == source_enemy)
        {
            aura->value = def.value;
            aura->turns_remaining = def.duration;
            aura->clear_on_damage_threshold = def.clear_on_damage_threshold;
            aura->disrupt_turns = 0;
            aura->damage_this_turn = 0;
            combat_feed_add(cs, "%s aura: %d", enemy_arena_aura_name(def.type), def.value);
            return;
        }
    }

    if (cs->arena_effect_count >= MAX_ARENA_EFFECTS) return;
    ArenaEffect *aura = &cs->arena_effects[cs->arena_effect_count++];
    memset(aura, 0, sizeof(*aura));
    aura->type = def.type;
    aura->value = def.value;
    aura->turns_remaining = def.duration;
    aura->clear_on_damage_threshold = def.clear_on_damage_threshold;
    aura->source_enemy = source_enemy;
    combat_feed_add(cs, "%s aura: %d", enemy_arena_aura_name(def.type), def.value);
}

static void boss_add_accumulator(CombatState *cs, EnemyState *e, int amount, const char *reason)
{
    if (!cs || !e || !e->def || !e->def->accumulate || amount == 0) return;
    int cap = e->def->accumulator_cap > 0 ? e->def->accumulator_cap : 20;
    int before = e->accumulator;
    e->accumulator += amount;
    if (e->accumulator < 0) e->accumulator = 0;
    if (e->accumulator > cap) e->accumulator = cap;
    if (e->accumulator != before && reason && reason[0])
        combat_feed_add(cs, "%s %s: %d", e->def->name, reason, e->accumulator);
}

static int combat_member_index_for_class(CombatState *cs, ClassType ct)
{
    if (!cs || ct == CLASS_NONE) return -1;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive && cs->party.members[i].class == ct)
            return i;
    return -1;
}

static int combat_class_perk_effect_total(CombatState *cs, ClassType ct, const char *effect)
{
    int idx = combat_member_index_for_class(cs, ct);
    return idx >= 0 ? party_member_perk_effect_total(&cs->party.members[idx], effect) : 0;
}

static const char *combat_class_perk_effect_name(CombatState *cs, ClassType ct, const char *effect)
{
    int idx = combat_member_index_for_class(cs, ct);
    return idx >= 0 ? party_member_perk_effect_name(&cs->party.members[idx], effect) : "Perk";
}

static int combat_party_perk_effect_total(CombatState *cs, const char *effect)
{
    if (!cs || !effect || !effect[0])
        return 0;
    int total = 0;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive)
            total += party_member_perk_effect_total(&cs->party.members[i], effect);
    return total;
}

static int combat_lowest_level_living_member(CombatState *cs)
{
    int best = -1;
    for (int i = 0; i < cs->party.count; i++)
    {
        PartyMember *pm = &cs->party.members[i];
        if (!pm->alive) continue;
        if (best < 0 ||
            pm->level < cs->party.members[best].level ||
            (pm->level == cs->party.members[best].level && pm->xp < cs->party.members[best].xp))
            best = i;
    }
    return best;
}

static void combat_award_card_xp(CombatState *cs, const CardDef *card, int paid_cost)
{
    if (!cs || !card || cs->phase != COMBAT_PLAYER_TURN || paid_cost <= 0)
        return;

    int idx = card->class == CLASS_NONE ?
        combat_lowest_level_living_member(cs) :
        combat_member_index_for_class(cs, card->class);
    if (idx < 0)
        return;

    int levels = 0;
    int gained = party_member_gain_xp(&cs->party.members[idx], paid_cost, &levels);
    if (gained <= 0)
        return;

    PartyMember *pm = &cs->party.members[idx];
    combat_feed_add(cs, "%s +%d XP", pm->name, gained);
    if (levels > 0)
    {
        combat_feed_add(cs, "%s reached Level %d!", pm->name, pm->level);
        Vector2 p = party_feedback_pos(cs, idx);
        ft_spawn(p.x - 15.0f, p.y + 8.0f, "LV UP!", 10, (Color){ 105, 245, 140, 255 });
        assets_play_sfx(SFX_LEVEL_UP);
    }
}

static void combat_try_rogue_mark_refund(CombatState *cs)
{
    if (!cs || cs->rogue_mark_refund_used)
        return;
    int refund = combat_class_perk_effect_total(cs, CLASS_ROGUE, "marked_payoff_energy_once");
    if (refund <= 0)
        return;
    cs->rogue_mark_refund_used = true;
    cs->energy.current += refund;
    if (cs->energy.current > cs->energy.max)
        cs->energy.current = cs->energy.max;
    combat_feed_add(cs, "%s: +%d energy",
        combat_class_perk_effect_name(cs, CLASS_ROGUE, "marked_payoff_energy_once"), refund);
}

static void apply_damage_to_ally(CombatState *cs, int ally_idx, int damage, const char *source);
static void apply_shield_to_enemy(CombatState *cs, int enemy_idx, int amount);

static void apply_heal_to_enemy(CombatState *cs, int enemy_idx, int amount);
static int apply_damage_to_enemy(CombatState *cs, int enemy_idx, int damage)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return 0;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return 0;

    if (cs->resolving_ally_idx >= 0)
    {
        int reduction = boss_arena_value(cs, AURA_DAMAGE_REDUCTION);
        if (reduction > 0)
        {
            if (reduction > 95) reduction = 95;
            damage = damage * (100 - reduction) / 100;
            if (damage < 0) damage = 0;
        }
    }

    if (e->invulnerable && damage > 0)
    {
        if (e->invulnerable_clear_damage > 0)
        {
            e->invulnerable_pending_damage += damage;
            if (e->invulnerable_pending_damage >= e->invulnerable_clear_damage)
            {
                e->invulnerable = false;
                e->invulnerable_until = INVULN_NONE;
                e->invulnerable_pending_damage = 0;
                combat_feed_add(cs, "%s's invulnerability broke", e->def->name);
            }
        }
        if (e->invulnerable)
        {
            ft_spawn((float)(e->pos_x - 18), (float)(e->pos_y - 18), "IMMUNE", 10, (Color){ 150, 210, 255, 255 });
            combat_feed_add(cs, "%s is invulnerable", e->def->name);
            return 0;
        }
    }

    int before_hp = e->hp;
    int dmg = damage;
    assets_play_sfx(cs->combo_scale > 1.1f ? SFX_DAMAGE_HEAVY : SFX_DAMAGE);
    if (e->shield > 0)
    {
        int abs = e->shield >= dmg ? dmg : e->shield;
        e->shield -= abs;
        dmg -= abs;
    }
    int hp_damage = 0;
    if (e->linked_group >= 0 && e->linked_group < cs->linked_group_count &&
        cs->linked_groups[e->linked_group].active && cs->linked_groups[e->linked_group].shared_hp)
    {
        LinkedEnemyGroup *group = &cs->linked_groups[e->linked_group];
        before_hp = group->hp;
        group->hp -= dmg;
        if (group->hp < 0) group->hp = 0;
        hp_damage = before_hp - group->hp;
        boss_sync_linked_group(cs, e->linked_group);
    }
    else
    {
        e->hp -= dmg;
        if (e->hp < 0) e->hp = 0;
        hp_damage = before_hp - e->hp;
    }

    // Thorns reflection: if enemy has Thorns and survived, reflect damage
    if (e->hp > 0 && dmg > 0)
    {
        int ti = status_find(e->statuses, e->status_count, STATUS_THORNS);
        if (ti >= 0)
        {
            int thorn_dmg = e->statuses[ti].value;
            int target = party_random_alive(&cs->party);
            if (target >= 0)
            {
                apply_damage_to_ally(cs, target, thorn_dmg, "Thorns");
                combat_feed_add(cs, "Thorns hits %s for %d", cs->party.members[target].name, thorn_dmg);
            }
        }
    }

    if (e->reflect_pct > 0 && e->reflect_turns > 0 && hp_damage > 0)
    {
        int reflected = hp_damage * e->reflect_pct / 100;
        if (e->reflect_cap > 0 && reflected > e->reflect_cap)
            reflected = e->reflect_cap;
        if (reflected > 0)
        {
            if (e->reflect_type == REFLECT_SHIELD_ON_BOSS)
                apply_shield_to_enemy(cs, enemy_idx, reflected);
            else if (cs->resolving_ally_idx >= 0 && cs->resolving_ally_idx < cs->party.count)
            {
                apply_damage_to_ally(cs, cs->resolving_ally_idx, reflected, "Reflect");
                combat_feed_add(cs, "%s reflected %d", e->def->name, reflected);
            }
        }
    }

    char buf[16];
    if (hp_damage > 0)
        snprintf(buf, sizeof(buf), "-%d", hp_damage);
    else
        snprintf(buf, sizeof(buf), "BLOCK");
    ft_spawn((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 10,
        hp_damage > 0 ? (Color){ 255, 80, 80, 255 } : (Color){ 105, 185, 255, 255 });
    shake_trigger(shake_amplitude_for_value(hp_damage > 0 ? hp_damage : damage, 1.5f, 0.22f, 8.0f));
    vfx_spawn_burst((float)e->pos_x, (float)e->pos_y, (Color){ 255, 85, 65, 255 }, 6);

    LOG_I(CAT_CARD, "  enemy[%d] %s: %d damage, %d HP damage (%d HP)", enemy_idx, e->def->name, damage, hp_damage, e->hp);

    if (hp_damage > 0)
    {
        boss_handle_linked_damage(cs, enemy_idx, hp_damage);
        boss_on_enemy_hp_damage(cs, enemy_idx, hp_damage);
    }

    bool pending_linked_revive = e->revive_timer > 0;
    if (before_hp > 0 && e->hp <= 0 &&
        !pending_linked_revive &&
        !cs->executioner_used &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_EXECUTIONERS_SEAL))
    {
        cs->executioner_used = true;
        deck_draw(&cs->deck);
        if (cs->energy.current < cs->energy.max)
            cs->energy.current++;
        combat_feed_add(cs, "Executioner's Seal: drew 1, +1 energy");
    }

    int execute_draw = meta_execute_draw_rank(&g_state.meta);
    if (before_hp > 0 && e->hp <= 0 && !pending_linked_revive && execute_draw > 0 && !cs->meta_execute_used)
    {
        cs->meta_execute_used = true;
        deal_cards(&cs->deck, execute_draw);
        combat_feed_add(cs, "Victory Momentum: drew %d", execute_draw);
    }

    if (before_hp > 0 && e->hp <= 0 && !pending_linked_revive)
    {
        for (int i = 0; i < cs->enemy_count; i++)
        {
            if (i == enemy_idx) continue;
            EnemyState *ally = &cs->enemies[i];
            if (!ally->def || ally->hp <= 0) continue;
            int si = status_find(ally->statuses, ally->status_count, STATUS_MATERNAL_BOND);
            if (si >= 0 && ally->statuses[si].value > 0)
            {
                apply_heal_to_enemy(cs, i, ally->statuses[si].value);
                combat_feed_add(cs, "%s: Maternal Bond heals %d", ally->def->name, ally->statuses[si].value);
            }
        }
    }

    return hp_damage;
}

static void apply_shield_to_ally(CombatState *cs, int ally_idx, int amount);
static void apply_damage_to_ally(CombatState *cs, int ally_idx, int damage, const char *source);

static void apply_heal_to_ally(CombatState *cs, int ally_idx, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;
    int heal_reduction = boss_arena_value(cs, AURA_HEAL_REDUCTION);
    if (heal_reduction > 0 && amount > 0)
    {
        if (heal_reduction > 95) heal_reduction = 95;
        amount = amount * (100 - heal_reduction) / 100;
        if (amount < 0) amount = 0;
    }
    int before = pm->hp;
    int overheal = before + amount - pm->max_hp;
    assets_play_sfx(SFX_HEAL);
    pm->hp += amount;
    if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", amount);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 80, 255, 80, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 95, 245, 135, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d HP (%d)", ally_idx, pm->name, amount, pm->hp);

    int paladin_heal_shield = combat_class_perk_effect_total(cs, CLASS_PALADIN, "heal_grant_shield");
    if (amount > 0 &&
        cs->resolving_card_class == CLASS_PALADIN &&
        paladin_heal_shield > 0)
    {
        apply_shield_to_ally(cs, ally_idx, paladin_heal_shield);
        combat_feed_add(cs, "%s: +%d Shield",
            combat_class_perk_effect_name(cs, CLASS_PALADIN, "heal_grant_shield"),
            paladin_heal_shield);
    }

    int overheal_shield = combat_class_perk_effect_total(cs, CLASS_CLERIC, "overheal_shield");
    if (overheal > 0 &&
        cs->resolving_card_class == CLASS_CLERIC &&
        overheal_shield > 0)
    {
        apply_shield_to_ally(cs, ally_idx, overheal_shield);
        combat_feed_add(cs, "%s: +%d Shield",
            combat_class_perk_effect_name(cs, CLASS_CLERIC, "overheal_shield"),
            overheal_shield);
    }

    if (cs->vengeful_active && cs->vengeful_ally == ally_idx)
    {
        cs->vengeful_active = false;
        combat_feed_add(cs, "Vengeful Retribution erupted");
        ft_spawn(p.x - 24.0f, p.y + 8.0f, "VENGEFUL", 10, (Color){ 245, 155, 80, 255 });
        for (int ei = 0; ei < cs->enemy_count; ei++)
            if (cs->enemies[ei].def && cs->enemies[ei].hp > 0)
                apply_damage_to_enemy(cs, ei, 8);
    }
}

static void apply_shield_to_ally(CombatState *cs, int ally_idx, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_MIRROR_SHIELD))
        amount += 3;
    if (amount <= 0) return;
    int gained = add_party_member_shield_capped(pm, amount);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    if (gained <= 0)
    {
        ft_spawn(p.x - 8.0f, p.y, "CAP", 10, (Color){ 150, 200, 255, 255 });
        LOG_I(CAT_CARD, "  ally[%d] %s: shield capped at %d", ally_idx, pm->name, pm->shield);
        return;
    }
    assets_play_sfx(SFX_SHIELD);

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", gained);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 100, 200, 255, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 115, 190, 255, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d shield (%d/%d cap)", ally_idx, pm->name, gained, pm->shield, party_member_shield_cap(pm));

    if (pm->class == CLASS_GUARDIAN && party_has_pair(cs, CLASS_GUARDIAN, CLASS_MAGE))
    {
        int living[MAX_ENEMIES];
        int living_count = 0;
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                living[living_count++] = i;
        if (living_count > 0)
        {
            int target = living[rand() % living_count];
            combat_feed_add(cs, "Molten Armor scorched %s", cs->enemies[target].def->name);
            apply_damage_to_enemy(cs, target, 2);
        }
    }

    // ── Sheltered Bulwark: Guardian+Paladin — Guardian shield also shields Paladin ──
    if (pm->class == CLASS_GUARDIAN && party_has_pair(cs, CLASS_GUARDIAN, CLASS_PALADIN))
    {
        for (int i = 0; i < cs->party.count; i++)
        {
            if (cs->party.members[i].class == CLASS_PALADIN && cs->party.members[i].alive)
            {
                int share = gained > 2 ? 2 : gained;
                int paladin_gain = add_party_member_shield_capped(&cs->party.members[i], share);
                if (paladin_gain > 0)
                    combat_feed_add(cs, "Sheltered Bulwark: Paladin +%d shield", paladin_gain);
                else
                    combat_feed_add(cs, "Sheltered Bulwark: Paladin shield capped");
                break;
            }
        }
    }
}

static void revive_ally(CombatState *cs, int ally_idx)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;

    PartyMember *pm = &cs->party.members[ally_idx];
    if (pm->alive) return;

    pm->alive = true;
    pm->hp = pm->max_hp / 2;
    if (pm->hp < 1) pm->hp = 1;
    pm->shield = 0;
    set_party_member_aggro(pm, 5);
    pm->status_count = 0;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", pm->hp);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 120, 255, 160, 255 });
    assets_play_sfx(SFX_PARTY_REVIVED);

    LOG_I(CAT_CARD, "  ally[%d] %s: revived at %d HP", ally_idx, pm->name, pm->hp);
}

static void apply_damage_to_ally(CombatState *cs, int ally_idx, int damage, const char *source)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;

    int remaining = damage;
    assets_play_sfx(cs->combo_scale > 1.1f ? SFX_DAMAGE_HEAVY : SFX_DAMAGE);
    if (pm->shield > 0)
    {
        int absorb = pm->shield >= remaining ? remaining : pm->shield;
        pm->shield -= absorb;
        remaining -= absorb;
    }

    int before = pm->hp;
    pm->hp -= remaining;
    if (pm->hp < 0) pm->hp = 0;

    // Thorns reflection: if ally has Thorns and survived, reflect to a random enemy
    if (pm->hp > 0 && remaining > 0)
    {
        int ti = status_find(pm->statuses, pm->status_count, STATUS_THORNS);
        if (ti >= 0)
        {
            int thorn_dmg = pm->statuses[ti].value;
            int living_enemies = 0;
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                    living_enemies++;
            if (living_enemies > 0)
            {
                int pick = rand() % living_enemies;
                int ei = 0;
                for (int i = 0; i < cs->enemy_count; i++)
                {
                    if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
                    if (ei == pick)
                    {
                        apply_damage_to_enemy(cs, i, thorn_dmg);
                        combat_feed_add(cs, "Thorns hits %s for %d", cs->enemies[i].def->name, thorn_dmg);
                        break;
                    }
                    ei++;
                }
            }
        }
    }

    Vector2 p = party_feedback_pos(cs, ally_idx);

    char buf[16];
    snprintf(buf, sizeof(buf), "-%d", before - pm->hp);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 255, 90, 90, 255 });
    shake_trigger(shake_amplitude_for_value(before - pm->hp, 1.5f, 0.22f, 8.0f));
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 255, 85, 75, 255 }, 5);

    LOG_I(CAT_COMBAT, "%s hits %s for %d (%d -> %d)", source, pm->name, before - pm->hp, before, pm->hp);
    combat_feed_add(cs, "%s hit %s for %d", source, pm->name, before - pm->hp);

    if (before > pm->hp &&
        pm->hp > 0 &&
        !cs->veil_pin_used &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_VEIL_PIN))
    {
        cs->veil_pin_used = true;
        int gained = add_party_member_shield_capped(pm, 6);
        combat_feed_add(cs, "Veil Pin: %s gained 6 Shield", pm->name);
        if (gained < 6) combat_feed_add(cs, "%s's Shield is capped", pm->name);
    }

    if (before > pm->hp &&
        pm->hp > 0 &&
        !cs->meta_emergency_barrier_used &&
        meta_has_emergency_barrier(&g_state.meta))
    {
        cs->meta_emergency_barrier_used = true;
        int gained = add_party_member_shield_capped(pm, 4);
        combat_feed_add(cs, "Emergency Barrier: %s gained 4 Shield", pm->name);
        if (gained < 4) combat_feed_add(cs, "%s's Shield is capped", pm->name);
    }

    if (pm->hp <= 0)
    {
        if (!cs->phoenix_used && relic_has(g_state.relics, g_state.relic_count, RELIC_PHOENIX_FEATHER))
        {
            cs->phoenix_used = true;
            pm->hp = pm->max_hp / 2;
            if (pm->hp < 1) pm->hp = 1;
            pm->shield = 0;
            LOG_I(CAT_COMBAT, "%s saved by Phoenix Feather!", pm->name);
            combat_feed_add(cs, "Phoenix Feather saved %s", pm->name);
            ft_spawn(p.x - 18.0f, p.y + 7.0f, "REVIVED", 10, (Color){ 255, 150, 50, 255 });
            assets_play_sfx(SFX_PARTY_REVIVED);
        }
        else if (!cs->meta_last_stand_used && meta_has_last_stand(&g_state.meta))
        {
            cs->meta_last_stand_used = true;
            pm->hp = 1;
            set_party_member_shield_capped(pm, 6);
            LOG_I(CAT_COMBAT, "%s held on with Last Stand!", pm->name);
            combat_feed_add(cs, "Last Stand saved %s", pm->name);
            ft_spawn(p.x - 18.0f, p.y + 7.0f, "LAST STAND", 10, (Color){ 245, 220, 110, 255 });
            assets_play_sfx(SFX_SHIELD);
        }
        else
        {
            pm->alive = false;
            assets_play_sfx(SFX_PARTY_DOWNED);
            g_state.run_deaths++;
            log_death_metric(cs, pm, source);
            set_party_member_aggro(pm, 0);
            pm->shield = 0;
            LOG_I(CAT_COMBAT, "%s DOWNED! Removing %s cards.", pm->name, class_name(pm->class));
            combat_feed_add(cs, "%s is downed", pm->name);
            deck_remove_class_cards(&cs->deck, pm->class);
            if (relic_has(g_state.relics, g_state.relic_count, RELIC_GRAVE_BELL))
            {
                deal_cards(&cs->deck, 2);
                combat_feed_add(cs, "Grave Bell: drew 2");
            }
            ft_spawn(p.x - 18.0f, p.y + 7.0f, "DOWNED", 10, (Color){ 240, 80, 80, 255 });
        }
    }
}

static void apply_heal_to_enemy(CombatState *cs, int enemy_idx, int amount)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return;

    int before = e->hp;
    assets_play_sfx(SFX_HEAL);
    if (e->linked_group >= 0 && e->linked_group < cs->linked_group_count &&
        cs->linked_groups[e->linked_group].active && cs->linked_groups[e->linked_group].shared_hp)
    {
        LinkedEnemyGroup *group = &cs->linked_groups[e->linked_group];
        before = group->hp;
        group->hp += amount;
        if (group->hp > group->max_hp) group->hp = group->max_hp;
        boss_sync_linked_group(cs, e->linked_group);
    }
    else
    {
        e->hp += amount;
        if (e->hp > e->max_hp) e->hp = e->max_hp;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", e->hp - before);
    ft_spawn((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 10, (Color){ 90, 240, 130, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)(e->pos_y - 4), (Color){ 95, 240, 130, 255 }, 4);
    LOG_I(CAT_COMBAT, "Enemy %s heals for %d (%d -> %d)", e->def->name, e->hp - before, before, e->hp);
    combat_feed_add(cs, "%s healed for %d", e->def->name, e->hp - before);
}

static void apply_shield_to_enemy(CombatState *cs, int enemy_idx, int amount)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return;

    assets_play_sfx(SFX_SHIELD);
    e->shield += amount;
    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", amount);
    ft_spawn((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 10, (Color){ 100, 180, 255, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)(e->pos_y - 4), (Color){ 115, 190, 255, 255 }, 4);
    LOG_I(CAT_COMBAT, "Enemy %s gains %d shield (%d)", e->def->name, amount, e->shield);
    combat_feed_add(cs, "%s gained %d shield", e->def->name, amount);
}

static int enemy_lowest_hp(CombatState *cs)
{
    int idx = -1;
    int lowest = 999999;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (!e->def || e->hp <= 0) continue;
        if (e->hp < lowest)
        {
            lowest = e->hp;
            idx = i;
        }
    }
    return idx;
}

static bool interrupt_enemy(CombatState *cs, int enemy_idx)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return false;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return false;

    if (e->intent.ability_idx < 0)
    {
        ft_spawn((float)(e->pos_x - 14), (float)(e->pos_y - 25), "NO CAST", 10, (Color){ 180, 180, 200, 255 });
        return false;
    }

    const EnemyCardDef *ab = &e->def->cards[e->intent.ability_idx];
    int stage = e->intent.stage_index > 0 ? e->intent.stage_index : 1;
    if (ab->cast_stages > 1 && !(ab->interruptible_stage_mask & (1u << (unsigned int)(stage - 1))))
    {
        ft_spawn((float)(e->pos_x - 14), (float)(e->pos_y - 25), "LOCKED", 10, (Color){ 230, 120, 80, 255 });
        LOG_I(CAT_CARD, "  %s resisted interrupt during stage %d", e->def->name, stage);
        return false;
    }
    if (ab->is_wipe || ab->intent == INTENT_WIPE)
    {
        ft_spawn((float)(e->pos_x - 14), (float)(e->pos_y - 25), "IMMUNE", 10, (Color){ 230, 120, 80, 255 });
        LOG_I(CAT_CARD, "  %s resisted interrupt on %s", e->def->name, ab->name);
        return false;
    }

    if (e->interrupt_cooldown > 0)
    {
        ft_spawn((float)(e->pos_x - 14), (float)(e->pos_y - 25), "READYING", 10, (Color){ 185, 185, 210, 255 });
        LOG_I(CAT_CARD, "  %s resisted repeat interrupt (%d turns)", e->def->name, e->interrupt_cooldown);
        combat_feed_add(cs, "%s resisted repeat interrupt", e->def->name);
        return false;
    }

    LOG_I(CAT_CARD, "  %s interrupted %s", e->def->name, ab->name);
    assets_play_sfx(SFX_INTERRUPT);
    combat_feed_add(cs, "%s was interrupted", e->def->name);
    ft_spawn_shake((float)(e->pos_x - 21), (float)(e->pos_y - 26), "INTERRUPTED", 10, (Color){ 220, 120, 255, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)(e->pos_y - 15), (Color){ 220, 120, 255, 255 }, 7);
    e->last_interrupted_ability = e->intent.ability_idx;
    e->interrupt_cooldown = 3;
    e->interrupted_recently = true;
    e->intent.ability_idx = -1;
    e->intent.remaining_turns = 0;
    g_state.run_interrupts++;
    boss_disrupt_auras(cs, 2);
    return true;
}

static void add_aggro_to_caster(CombatState *cs, ClassType ct, int amount)
{
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].class == ct && cs->party.members[i].alive)
            add_party_member_aggro(&cs->party.members[i], amount);
}

static void find_caster(CombatState *cs, ClassType ct, int *out_idx)
{
    *out_idx = -1;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].class == ct && cs->party.members[i].alive)
            { *out_idx = i; return; }
}

static bool party_class_is_silenced(const CombatState *cs, ClassType ct)
{
    if (!cs || ct == CLASS_NONE) return false;
    int mask = boss_arena_value(cs, AURA_SILENCE_CLASSES);
    if (ct >= 0 && ct < 31 && (mask & (1 << (int)ct)))
        return true;
    for (int i = 0; i < cs->party.count; i++)
    {
        const PartyMember *pm = &cs->party.members[i];
        if (!pm->alive || pm->class != ct) continue;
        if (status_find((StatusEffect *)pm->statuses, pm->status_count, STATUS_SILENCE) >= 0)
            return true;
    }
    return false;
}

int combat_silenced_class_mask(const CombatState *cs)
{
    int mask = boss_arena_value(cs, AURA_SILENCE_CLASSES);
    if (!cs) return mask;
    for (int i = 0; i < cs->party.count; i++)
    {
        const PartyMember *pm = &cs->party.members[i];
        if (!pm->alive || pm->class < 0 || pm->class >= 31) continue;
        if (status_find((StatusEffect *)pm->statuses, pm->status_count, STATUS_SILENCE) >= 0)
            mask |= 1 << (int)pm->class;
    }
    return mask;
}

static Vector2 rect_center(Rectangle r)
{
    return (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}

static Rectangle combat_hand_card_rect(CombatState *cs, int hand_idx)
{
    HandLayout hand_layout = layout_hand(cs->deck.hand_count);
    Rectangle r = layout_hand_card_rect(hand_layout, hand_idx);
    if (hand_idx == cs->target_hand_idx)
        r.y += cs->target_offset;
    else if (hand_idx == cs->hovered_card)
        r.y -= 28.0f;
    return r;
}

static Vector2 combat_discard_target(void)
{
    return rect_center(layout_discard_pile_rect());
}

static Vector2 combat_card_throw_target(CombatState *cs, const CardDef *card, int target_enemy, int target_ally)
{
    if (target_enemy >= 0 && target_enemy < cs->enemy_count)
        return (Vector2){ (float)cs->enemies[target_enemy].pos_x, (float)cs->enemies[target_enemy].pos_y };

    if (target_ally >= 0 && target_ally < cs->party.count)
        return rect_center(layout_party_frame_rect(cs->party.count, target_ally));

    if (card && card->target == TARGET_SELF)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
            return rect_center(layout_party_frame_rect(cs->party.count, caster));
    }

    if (card && card->target == TARGET_ALL_ENEMIES)
    {
        int count = 0;
        Vector2 total = { 0.0f, 0.0f };
        for (int i = 0; i < cs->enemy_count; i++)
        {
            if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
            total.x += (float)cs->enemies[i].pos_x;
            total.y += (float)cs->enemies[i].pos_y;
            count++;
        }
        if (count > 0)
            return (Vector2){ total.x / (float)count, total.y / (float)count };
    }

    if (card && card->target == TARGET_ALL_ALLIES && cs->party.count > 0)
    {
        Rectangle first = layout_party_frame_rect(cs->party.count, 0);
        Rectangle last = layout_party_frame_rect(cs->party.count, cs->party.count - 1);
        return (Vector2){ (first.x + last.x + last.width) * 0.5f, first.y + first.height * 0.5f };
    }

    return combat_discard_target();
}

// ── Enemy card throw animation ────────────────────────────
static void enemy_action(EnemyState *e, CombatState *cs, int target_enemy, int target_ally);

static void combat_spawn_enemy_card_throw(CombatState *cs, EnemyState *e, const EnemyCardDef *cd, int enemy_idx)
{
    int slot = -1;
    for (int i = 0; i < MAX_ENEMY_CARD_THROWS; i++)
    {
        if (!cs->enemy_card_throws[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    Vector2 start = { (float)e->pos_x, (float)e->pos_y };
    Vector2 end;
    int tgt_enemy = -1;
    int tgt_ally = -1;

    if (cd->intent == INTENT_HEAL)
    {
        tgt_enemy = enemy_lowest_hp(cs);
        if (tgt_enemy >= 0 && tgt_enemy < cs->enemy_count && cs->enemies[tgt_enemy].def)
            end = (Vector2){ (float)cs->enemies[tgt_enemy].pos_x, (float)cs->enemies[tgt_enemy].pos_y };
        else
            end = (Vector2){ (float)e->pos_x + 30.0f, (float)e->pos_y + 40.0f };
    }
    else if (cd->intent == INTENT_SHIELD || cd->intent == INTENT_BUFF)
    {
        tgt_enemy = enemy_idx;
        end = (Vector2){ (float)e->pos_x + 30.0f, (float)e->pos_y + 40.0f };
    }
    else if (cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE)
    {
        // Fly to center of party formation
        int alive = 0;
        Vector2 avg = { 0, 0 };
        for (int i = 0; i < cs->party.count; i++)
        {
            if (!cs->party.members[i].alive) continue;
            Rectangle fr = layout_party_frame_rect(cs->party.count, i);
            avg.x += fr.x + fr.width * 0.5f;
            avg.y += fr.y + fr.height * 0.5f;
            alive++;
        }
        if (alive > 0) { avg.x /= (float)alive; avg.y /= (float)alive; end = avg; }
        else end = (Vector2){ VIRT_W / 2.0f, VIRT_H / 2.0f };
    }
    else
    {
        // Single-target damage: pick same target enemy_action will use
        if (e->tethered_ally >= 0 && e->tethered_ally < cs->party.count && cs->party.members[e->tethered_ally].alive)
            tgt_ally = e->tethered_ally;
        else if (cd->target == ENEMY_TARGET_RANDOM)
            tgt_ally = party_random_alive(&cs->party);
        else if (cd->target == ENEMY_TARGET_LOWEST_HP)
            tgt_ally = party_lowest_hp(&cs->party);
        else
            tgt_ally = party_highest_aggro(&cs->party);
        if (tgt_ally >= 0 && tgt_ally < cs->party.count)
            end = rect_center(layout_party_frame_rect(cs->party.count, tgt_ally));
        else
        {
            int alive = 0;
            Vector2 avg = { 0, 0 };
            for (int i = 0; i < cs->party.count; i++)
            {
                if (!cs->party.members[i].alive) continue;
                Rectangle fr = layout_party_frame_rect(cs->party.count, i);
                avg.x += fr.x + fr.width * 0.5f;
                avg.y += fr.y + fr.height * 0.5f;
                alive++;
            }
            if (alive > 0) { avg.x /= (float)alive; avg.y /= (float)alive; end = avg; }
            else end = (Vector2){ VIRT_W / 2.0f, VIRT_H / 2.0f };
        }
    }

    Vector2 mid = { (start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f - 52.0f };

    EnemyCardThrow *thr = &cs->enemy_card_throws[slot];
    thr->active = true;
    assets_play_sfx(SFX_ENEMY_ATTACK);
    thr->enemy_index = enemy_idx;
    thr->card_idx = cd - e->def->cards;
    thr->ability_name = cd->name;
    thr->intent = cd->intent;
    thr->t = 0.0f;
    thr->duration = 0.7f;
    thr->pause_timer = enemy_idx * 0.6f;
    thr->start = start;
    thr->control = mid;
    thr->end = end;
    thr->target_enemy = tgt_enemy;
    thr->target_ally = tgt_ally;

    // Clear intent immediately so cast bar disappears; card_idx is stored for resolution
    e->intent.ability_idx = -1;
    e->intent.remaining_turns = 0;
}

void combat_draw_enemy_card_throws(CombatState *cs)
{
    if (!cs) return;

    for (int i = 0; i < MAX_ENEMY_CARD_THROWS; i++)
    {
        EnemyCardThrow *thr = &cs->enemy_card_throws[i];
        if (!thr->active) continue;

        float t = thr->t;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        float u = 1.0f - t;
        Vector2 p = {
            u * u * thr->start.x + 2.0f * u * t * thr->control.x + t * t * thr->end.x,
            u * u * thr->start.y + 2.0f * u * t * thr->control.y + t * t * thr->end.y
        };

        float w = (float)HAND_CARD_W, h = (float)HAND_CARD_H;
        Rectangle r = { p.x - w * 0.5f, p.y - h * 0.5f, w, h };

        CardDef fake_card = {0};
        fake_card.name = thr->ability_name ? thr->ability_name : "ATTACK";
        fake_card.class = CLASS_NONE;
        fake_card.cost = 0;
        fake_card.description = "";
        int keyword_icon = -1;

        // Set type and target based on intent
        switch (thr->intent)
        {
            case INTENT_HEAL:
            case INTENT_SHIELD:
            case INTENT_AOE_SHIELD:
                fake_card.type = CARD_SKILL;
                if (thr->intent == INTENT_HEAL)
                    fake_card.target = TARGET_ALLY;
                else if (thr->intent == INTENT_AOE_SHIELD)
                    fake_card.target = TARGET_ALL_ALLIES;
                else
                    fake_card.target = TARGET_SELF;
                break;
            case INTENT_BUFF:
                fake_card.type = CARD_POWER;
                fake_card.target = TARGET_SELF;
                break;
            case INTENT_AOE:
            case INTENT_WIPE:
                fake_card.type = CARD_ATTACK;
                fake_card.target = TARGET_ALL_ENEMIES;
                break;
            default:
                fake_card.type = CARD_ATTACK;
                fake_card.target = TARGET_ENEMY;
                break;
        }

        // Get real values from the enemy's card definition
        if (thr->card_idx >= 0 && thr->enemy_index >= 0 && thr->enemy_index < cs->enemy_count)
        {
            EnemyState *et = &cs->enemies[thr->enemy_index];
            if (et->def && thr->card_idx < et->def->card_count)
            {
                const EnemyCardDef *ecd = &et->def->cards[thr->card_idx];
                fake_card.damage = ecd->base_damage;
                fake_card.heal = ecd->heal_amount;
                fake_card.shield = ecd->shield_amount;
                if (ecd->lifesteal_pct > 0)
                    keyword_icon = KW_LIFESTEAL;
                else if (ecd->interrupts)
                    keyword_icon = KW_INTERRUPT;
            }
        }

        theme_draw_card_art(r, &fake_card, 0);
        if (keyword_icon >= 0)
            theme_draw_keyword_icon(r, (KeywordIcon)keyword_icon);
    }
}

bool combat_any_pending(CombatState *cs)
{
    if (!cs) return false;
    if (cs->echo_pending) return true;
    for (int i = 0; i < MAX_ENEMY_CARD_THROWS; i++)
        if (cs->enemy_card_throws[i].active) return true;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].cast_pending) return true;
    return false;
}

static void combat_spawn_card_throw(CombatState *cs, int hand_idx, const CardDef *card, int upgrade_level, int target_enemy, int target_ally)
{
    if (!cs || !card || hand_idx < 0 || hand_idx >= cs->deck.hand_count) return;

    int slot = -1;
    float oldest = -1.0f;
    for (int i = 0; i < MAX_CARD_THROW_ANIMS; i++)
    {
        if (!cs->card_throws[i].active)
        {
            slot = i;
            break;
        }
        if (cs->card_throws[i].t > oldest)
        {
            oldest = cs->card_throws[i].t;
            slot = i;
        }
    }
    if (slot < 0) return;

    Rectangle source = combat_hand_card_rect(cs, hand_idx);
    Vector2 start = rect_center(source);
    Vector2 end = combat_card_throw_target(cs, card, target_enemy, target_ally);
    Vector2 mid = { (start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f - 52.0f };

    CardThrowAnim *anim = &cs->card_throws[slot];
    anim->active = true;
    anim->card = card;
    anim->instance = cs->deck.cards[cs->deck.hand[hand_idx]];
    anim->upgrade_level = upgrade_level;
    anim->seed = (unsigned int)cs->deck.cards[cs->deck.hand[hand_idx]].uid;
    anim->t = 0.0f;
    anim->duration = 0.32f;
    anim->start = start;
    anim->control = mid;
    anim->end = end;
    anim->width = (int)source.width;
    anim->height = (int)source.height;
}

static void combat_update_card_throws(CombatState *cs, float dt)
{
    for (int i = 0; i < MAX_CARD_THROW_ANIMS; i++)
    {
        CardThrowAnim *anim = &cs->card_throws[i];
        if (!anim->active) continue;
        anim->t += dt / anim->duration;
        if (anim->t >= 1.0f)
            anim->active = false;
    }
    for (int i = 0; i < MAX_ENEMY_CARD_THROWS; i++)
    {
        EnemyCardThrow *thr = &cs->enemy_card_throws[i];
        if (!thr->active) continue;
        if (thr->pause_timer > 0.0f)
        {
            thr->pause_timer -= dt;
            if (thr->pause_timer < 0.0f) thr->pause_timer = 0.0f;
        }
        else
        {
            thr->t += dt / thr->duration;
            if (thr->t >= 1.0f)
            {
                thr->active = false;
                // Apply pending enemy action when card reaches target
                if (thr->enemy_index >= 0 && thr->enemy_index < cs->enemy_count)
                {
                    EnemyState *et = &cs->enemies[thr->enemy_index];
                    if (et->def && et->cast_pending)
                    {
                        // Save the next intent (set by deck system in advance_turn)
                        // so it isn't lost when we overwrite ability_idx for enemy_action
                        int saved_idx = et->intent.ability_idx;
                        et->intent.ability_idx = thr->card_idx;
                        enemy_action(et, cs, thr->target_enemy, thr->target_ally);
                        et->intent.ability_idx = saved_idx;
                    }
                }
            }
        }
    }
}

void combat_draw_card_throws(CombatState *cs)
{
    if (!cs) return;

    for (int i = 0; i < MAX_CARD_THROW_ANIMS; i++)
    {
        CardThrowAnim *anim = &cs->card_throws[i];
        if (!anim->active || !anim->card) continue;

        float t = anim->t;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        float u = 1.0f - t;
        Vector2 p = {
            u * u * anim->start.x + 2.0f * u * t * anim->control.x + t * t * anim->end.x,
            u * u * anim->start.y + 2.0f * u * t * anim->control.y + t * t * anim->end.y
        };

        Rectangle r = {
            (float)((int)(p.x - anim->width * 0.5f + 0.5f)),
            (float)((int)(p.y - anim->height * 0.5f + 0.5f)),
            (float)anim->width,
            (float)anim->height
        };
        theme_draw_card_art_seeded(r, anim->card, anim->upgrade_level, anim->seed, -1);
        theme_draw_keyword_badge(r, &anim->instance);
    }
}

static int find_guardian(CombatState *cs)
{
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].class == CLASS_GUARDIAN && cs->party.members[i].alive)
            return i;
    return -1;
}

static const char *status_label(StatusType type)
{
    switch (type)
    {
        case STATUS_NONE:       return "Status";
        case STATUS_BURNING:    return "Burning";
        case STATUS_RENEW:      return "Renew";
        case STATUS_TRAP:       return "Trap";
        case STATUS_TOTEM_HEAL: return "Healing Totem";
        case STATUS_BLEED:      return "Bleed";
        case STATUS_WEAKNESS:   return "Weakness";
        case STATUS_ENERGY_DRAIN: return "Energy Drain";
        case STATUS_MARKED:     return "Marked";
        case STATUS_CONDUCTIVE: return "Conductive";
        case STATUS_BLIGHT:     return "Blight";
    }
    return "Status";
}

static int enemy_status_value(CombatState *cs, int enemy_idx, StatusType status)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count) return 0;
    EnemyState *e = &cs->enemies[enemy_idx];
    int idx = status_find(e->statuses, e->status_count, status);
    return idx >= 0 ? e->statuses[idx].value : 0;
}

static bool enemy_has_status(CombatState *cs, int enemy_idx, StatusType status)
{
    return enemy_status_value(cs, enemy_idx, status) > 0;
}

static void remove_enemy_status(CombatState *cs, int enemy_idx, StatusType status)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    int idx = status_find(e->statuses, e->status_count, status);
    if (idx < 0) return;
    for (int i = idx; i < e->status_count - 1; i++)
        e->statuses[i] = e->statuses[i + 1];
    e->status_count--;
}

static int remove_all_enemy_status(CombatState *cs, StatusType status)
{
    int removed = 0;
    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        while (ei >= 0 && ei < cs->enemy_count && enemy_has_status(cs, ei, status))
        {
            remove_enemy_status(cs, ei, status);
            removed++;
        }
    }
    return removed;
}

static int count_enemies_with_status(CombatState *cs, StatusType status)
{
    int count = 0;
    for (int ei = 0; ei < cs->enemy_count; ei++)
        if (enemy_has_status(cs, ei, status))
            count++;
    return count;
}

static int count_enemy_synergy_statuses(CombatState *cs)
{
    int count = 0;
    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        if (!cs->enemies[ei].def || cs->enemies[ei].hp <= 0) continue;
        if (enemy_has_status(cs, ei, STATUS_MARKED)) count++;
        if (enemy_has_status(cs, ei, STATUS_CONDUCTIVE)) count++;
        if (enemy_has_status(cs, ei, STATUS_BLIGHT)) count++;
    }
    return count;
}

static bool is_enemy_synergy_status(StatusType type)
{
    return type == STATUS_MARKED || type == STATUS_CONDUCTIVE || type == STATUS_BLIGHT;
}

static int extend_enemy_synergy_statuses(CombatState *cs, int turns)
{
    if (turns > 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_LINGERING_SIGIL))
        turns++;

    int extended = 0;
    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        EnemyState *e = &cs->enemies[ei];
        if (!e->def || e->hp <= 0) continue;
        for (int s = 0; s < e->status_count; s++)
        {
            StatusType type = e->statuses[s].type;
            if (!is_enemy_synergy_status(type))
                continue;
            e->statuses[s].turns += turns;
            extended++;
        }
    }
    return extended;
}

static void apply_status_to_enemy(CombatState *cs, int enemy_idx, StatusType status, int turns, int amount)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return;

    if (turns > 0 && is_enemy_synergy_status(status) &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_LINGERING_SIGIL))
        turns++;
    int shaman_extend = combat_class_perk_effect_total(cs, CLASS_SHAMAN, "extend_shaman_status_once");
    if (turns > 0 &&
        status == STATUS_CONDUCTIVE &&
        cs->resolving_card_class == CLASS_SHAMAN &&
        !cs->shaman_extend_status_used &&
        shaman_extend > 0)
    {
        turns += shaman_extend;
        cs->shaman_extend_status_used = true;
        combat_feed_add(cs, "%s: +%d turn",
            combat_class_perk_effect_name(cs, CLASS_SHAMAN, "extend_shaman_status_once"),
            shaman_extend);
    }
    int blight_bonus = combat_class_perk_effect_total(cs, CLASS_WARLOCK, "first_blight_bonus");
    if (status == STATUS_BLIGHT &&
        cs->resolving_card_class == CLASS_WARLOCK &&
        !cs->warlock_blight_boost_used &&
        blight_bonus > 0)
    {
        amount += blight_bonus;
        cs->warlock_blight_boost_used = true;
        combat_feed_add(cs, "%s: +%d BLIGHT",
            combat_class_perk_effect_name(cs, CLASS_WARLOCK, "first_blight_bonus"),
            blight_bonus);
    }

    // ── Call of Nature: Shaman+Bard — CONDUCTIVE also applies MARKED ──
    if (status == STATUS_CONDUCTIVE && turns > 0 &&
        party_has_pair(cs, CLASS_SHAMAN, CLASS_BARD))
    {
        status_apply(e->statuses, &e->status_count, STATUS_MARKED, 1, 1);
        combat_feed_add(cs, "Call of Nature: +MARKED");
    }

    // ── Deadly Poison: Ranger+Warlock — BLIGHT deals immediate damage ──
    if (status == STATUS_BLIGHT && amount > 0 &&
        party_has_pair(cs, CLASS_RANGER, CLASS_WARLOCK))
    {
        int dmg = amount * turns;
        apply_damage_to_enemy(cs, enemy_idx, dmg);
        combat_feed_add(cs, "Deadly Poison: +%d immediate", dmg);
    }

    // ── Status amplification buff ──
    if (cs->player_status_amp > 0 && amount > 0)
        amount = amount * (100 + cs->player_status_amp) / 100;

    status_apply(e->statuses, &e->status_count, status, turns, amount);
    LOG_I(CAT_CARD, "  enemy[%d]: +%s (%d for %d turns)", enemy_idx, status_label(status), amount, turns);
    combat_feed_add(cs, "%s: %s", e->def->name, status_label(status));
}

static void apply_status_to_ally(CombatState *cs, int ally_idx, StatusType status, int turns, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;

    int shaman_extend = combat_class_perk_effect_total(cs, CLASS_SHAMAN, "extend_shaman_status_once");
    if (turns > 0 &&
        status == STATUS_TOTEM_HEAL &&
        cs->resolving_card_class == CLASS_SHAMAN &&
        !cs->shaman_extend_status_used &&
        shaman_extend > 0)
    {
        turns += shaman_extend;
        cs->shaman_extend_status_used = true;
        combat_feed_add(cs, "%s: +%d turn",
            combat_class_perk_effect_name(cs, CLASS_SHAMAN, "extend_shaman_status_once"),
            shaman_extend);
    }

    // ── Status amplification buff ──
    if (cs->player_status_amp > 0 && amount > 0)
        amount = amount * (100 + cs->player_status_amp) / 100;

    status_apply(pm->statuses, &pm->status_count, status, turns, amount);
    LOG_I(CAT_CARD, "  ally[%d]: +%s (%d for %d turns)", ally_idx, status_label(status), amount, turns);
    combat_feed_add(cs, "%s: %s", pm->name, status_label(status));
}

static void apply_enemy_interrupt_effects(CombatState *cs, EnemyState *e, const EnemyCardDef *cd, int target_ally)
{
    if (!cs || !cd || !cd->interrupts)
        return;

    if (cs->channel_card)
    {
        combat_feed_add(cs, "%s was silenced!", cs->channel_card->name);
        cs->channel_card = NULL;
        cs->channel_remaining = 0;
        cs->channel_class = CLASS_NONE;
    }

    combo_prime_clear(cs);
    cs->combo_class = CLASS_NONE;
    cs->combo_count = 0;

    bool any_silenced = false;
    if (cd->target == ENEMY_TARGET_ALL || cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE)
    {
        for (int i = 0; i < cs->party.count; i++)
        {
            if (!cs->party.members[i].alive) continue;
            apply_status_to_ally(cs, i, STATUS_SILENCE, 1, 0);
            any_silenced = true;
        }
    }
    else
    {
        int target = target_ally;
        if (e->tethered_ally >= 0 && e->tethered_ally < cs->party.count && cs->party.members[e->tethered_ally].alive)
            target = e->tethered_ally;
        if (target < 0 || target >= cs->party.count || !cs->party.members[target].alive)
        {
            if (cd->target == ENEMY_TARGET_RANDOM)
                target = party_random_alive(&cs->party);
            else if (cd->target == ENEMY_TARGET_LOWEST_HP)
                target = party_lowest_hp(&cs->party);
            else
                target = party_highest_aggro(&cs->party);
        }
        if (target >= 0)
        {
            apply_status_to_ally(cs, target, STATUS_SILENCE, 1, 0);
            any_silenced = true;
        }
    }

    if (any_silenced)
        combat_feed_add(cs, "%s silenced cards for a turn", e && e->def ? e->def->name : "Enemy");
}

static void apply_card_effect(CombatState *cs, const CardDef *card, const CardEffect *effect, int target_enemy, int target_ally)
{
    if (!cs || !card || !effect) return;

    switch (effect->type)
    {
        case CARD_EFFECT_DRAW_CARDS:
            for (int d = 0; d < effect->amount; d++)
                deck_draw(&cs->deck);
            LOG_I(CAT_CARD, "  %s: drew %d cards", card->name, effect->amount);
            combat_feed_add(cs, "Drew %d cards", effect->amount);
            break;

        case CARD_EFFECT_GAIN_ENERGY:
            cs->energy.current += effect->amount;
            if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
            LOG_I(CAT_CARD, "  %s: +%d energy (%d/%d)", card->name, effect->amount, cs->energy.current, cs->energy.max);
            combat_feed_add(cs, "Gained %d energy", effect->amount);
            break;

        case CARD_EFFECT_REVIVE_TARGET:
            if (target_ally >= 0)
                revive_ally(cs, target_ally);
            break;

        case CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY:
            apply_status_to_enemy(cs, target_enemy, effect->status, effect->turns, effect->amount);
            break;

        case CARD_EFFECT_APPLY_STATUS_TARGET_ALLY:
            apply_status_to_ally(cs, target_ally, effect->status, effect->turns, effect->amount);
            break;

        case CARD_EFFECT_APPLY_STATUS_ALL_ALLIES:
        {
            int turns = effect->turns;
            if (effect->status == STATUS_TOTEM_HEAL && party_has_pair(cs, CLASS_GUARDIAN, CLASS_SHAMAN))
            {
                turns += 2;
                combat_feed_add(cs, "Earthen Bulwark extended Totem");
            }
            for (int i = 0; i < cs->party.count; i++)
                apply_status_to_ally(cs, i, effect->status, turns, effect->amount);
            break;
        }

        case CARD_EFFECT_RESET_CASTER_AGGRO:
        {
            int caster = -1;
            find_caster(cs, card->class, &caster);
            if (caster >= 0)
            {
                set_party_member_aggro(&cs->party.members[caster], 0);
                ft_spawn(22.0f, 176.0f, "AGGRO RESET", 10, (Color){ 120, 220, 160, 255 });
                LOG_I(CAT_CARD, "  %s: aggro reset", card->name);
                combat_feed_add(cs, "%s reset aggro", cs->party.members[caster].name);
                if (card->class == CLASS_ROGUE && party_has_pair(cs, CLASS_CLERIC, CLASS_ROGUE))
                {
                    combat_feed_add(cs, "Shadow Mend healed %s", cs->party.members[caster].name);
                    apply_heal_to_ally(cs, caster, 8);
                }
            }
            break;
        }

        case CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN:
        {
            if (target_ally < 0 || target_ally >= cs->party.count) break;
            int guardian = find_guardian(cs);
            int transfer = cs->party.members[target_ally].aggro;
            if (guardian >= 0 && guardian != target_ally)
            {
                add_party_member_aggro(&cs->party.members[guardian], transfer);
                set_party_member_aggro(&cs->party.members[target_ally], 0);
                ft_spawn(22.0f, 176.0f, "AGGRO TRANSFER", 10, (Color){ 180, 180, 220, 255 });
                LOG_I(CAT_CARD, "  %s: moved %d aggro to Guardian", card->name, transfer);
                combat_feed_add(cs, "Aggro moved to Guardian");
            }
            else
            {
                set_party_member_aggro(&cs->party.members[target_ally], 0);
                LOG_I(CAT_CARD, "  %s: reduced ally aggro by %d", card->name, transfer);
                combat_feed_add(cs, "Aggro cleared");
            }
            if (card->class == CLASS_ROGUE && party_has_pair(cs, CLASS_CLERIC, CLASS_ROGUE))
            {
                int caster = -1;
                find_caster(cs, card->class, &caster);
                if (caster >= 0)
                {
                    combat_feed_add(cs, "Shadow Mend healed %s", cs->party.members[caster].name);
                    apply_heal_to_ally(cs, caster, 8);
                }
            }
            break;
        }
        case CARD_EFFECT_GAIN_BUFF:
        {
            float mult = 1.0f + (float)effect->amount / 100.0f;
            cs->player_damage_mult *= mult;
            if (effect->turns > cs->player_buff_turns)
                cs->player_buff_turns = effect->turns;
            combat_feed_add(cs, "Damage buffed x%.2f for %d turns", mult, effect->turns);
            break;
        }
        case CARD_EFFECT_EXTRA_DRAW:
        {
            cs->player_extra_draw += effect->amount;
            if (effect->turns > cs->player_extra_draw_turns)
                cs->player_extra_draw_turns = effect->turns;
            combat_feed_add(cs, "Extra draw +%d for %d turns", effect->amount, effect->turns);
            break;
        }
        case CARD_EFFECT_GAIN_STATUS_AMP:
        {
            cs->player_status_amp += effect->amount;
            if (effect->turns > cs->player_status_amp_turns)
                cs->player_status_amp_turns = effect->turns;
            combat_feed_add(cs, "Status amp +%d%% for %d turns", effect->amount, effect->turns);
            break;
        }
    }
}

static void apply_card_effect_chain(CombatState *cs, const CardDef *card, int target_enemy, int target_ally)
{
    if (!card || !card->effects || card->effect_count <= 0) return;
    for (int i = 0; i < card->effect_count; i++)
        apply_card_effect(cs, card, &card->effects[i], target_enemy, target_ally);
}

static int echo_half_value(int value)
{
    if (value <= 0) return 0;
    int half = (value + 1) / 2;
    return half > 0 ? half : 1;
}

static void apply_echo_card_effect(CombatState *cs, const CardDef *card, const CardEffect *effect, int target_enemy, int target_ally)
{
    if (!cs || !card || !effect) return;

    int amount = echo_half_value(effect->amount);
    int turns = echo_half_value(effect->turns);

    switch (effect->type)
    {
        case CARD_EFFECT_DRAW_CARDS:
            for (int d = 0; d < amount; d++)
                deck_draw(&cs->deck);
            if (amount > 0)
                combat_feed_add(cs, "Echo drew %d", amount);
            break;

        case CARD_EFFECT_GAIN_ENERGY:
            if (amount > 0)
            {
                cs->energy.current += amount;
                if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
                combat_feed_add(cs, "Echo gained %d energy", amount);
            }
            break;

        case CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY:
            if (target_enemy >= 0)
                apply_status_to_enemy(cs, target_enemy, effect->status, turns, amount);
            break;

        case CARD_EFFECT_APPLY_STATUS_TARGET_ALLY:
            if (target_ally >= 0)
                apply_status_to_ally(cs, target_ally, effect->status, turns, amount);
            break;

        case CARD_EFFECT_APPLY_STATUS_ALL_ALLIES:
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    apply_status_to_ally(cs, i, effect->status, turns, amount);
            break;

        case CARD_EFFECT_GAIN_BUFF:
            if (amount > 0)
            {
                float mult = 1.0f + (float)amount / 100.0f;
                cs->player_damage_mult *= mult;
                if (turns > cs->player_buff_turns)
                    cs->player_buff_turns = turns;
                combat_feed_add(cs, "Echo damage buff x%.2f", mult);
            }
            break;

        case CARD_EFFECT_EXTRA_DRAW:
            if (amount > 0)
            {
                cs->player_extra_draw += amount;
                if (turns > cs->player_extra_draw_turns)
                    cs->player_extra_draw_turns = turns;
                combat_feed_add(cs, "Echo extra draw +%d", amount);
            }
            break;

        case CARD_EFFECT_GAIN_STATUS_AMP:
            if (amount > 0)
            {
                cs->player_status_amp += amount;
                if (turns > cs->player_status_amp_turns)
                    cs->player_status_amp_turns = turns;
                combat_feed_add(cs, "Echo status amp +%d%%", amount);
            }
            break;

        case CARD_EFFECT_REVIVE_TARGET:
        case CARD_EFFECT_RESET_CASTER_AGGRO:
        case CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN:
            break;
    }
}

static int echo_pick_enemy_target(CombatState *cs, int preferred)
{
    if (preferred >= 0 && preferred < cs->enemy_count &&
        cs->enemies[preferred].def && cs->enemies[preferred].hp > 0)
        return preferred;

    int alive[MAX_ENEMIES], alive_count = 0;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].def && cs->enemies[i].hp > 0)
            alive[alive_count++] = i;
    return alive_count > 0 ? alive[rand() % alive_count] : -1;
}

static int echo_pick_ally_target(CombatState *cs, int preferred)
{
    if (preferred >= 0 && preferred < cs->party.count && cs->party.members[preferred].alive)
        return preferred;

    int alive[MAX_PARTY_SIZE], alive_count = 0;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive)
            alive[alive_count++] = i;
    return alive_count > 0 ? alive[rand() % alive_count] : -1;
}

static void combat_schedule_echo(CombatState *cs, const CardDef *card, int upgrade_level, int target_enemy, int target_ally)
{
    if (!cs || !card)
        return;
    cs->echo_pending = true;
    cs->echo_timer = ECHO_DELAY_SECONDS;
    cs->echo_card = card;
    cs->echo_upgrade_level = upgrade_level;
    cs->echo_target_enemy = target_enemy;
    cs->echo_target_ally = target_ally;
}

static void combat_resolve_pending_echo(CombatState *cs)
{
    if (!cs || !cs->echo_pending)
        return;

    const CardDef *card = cs->echo_card;
    int upgrade_level = cs->echo_upgrade_level;
    int target_enemy = -1;
    int target_ally = -1;
    bool resolved = false;

    cs->echo_pending = false;
    cs->echo_card = NULL;
    cs->echo_timer = 0.0f;

    if (!card)
        return;

    if (card->target == TARGET_ENEMY || card->target == TARGET_ALL_ENEMIES)
        target_enemy = echo_pick_enemy_target(cs, cs->echo_target_enemy);
    else if (card->target == TARGET_ALLY || card->target == TARGET_ALL_ALLIES)
        target_ally = echo_pick_ally_target(cs, cs->echo_target_ally);

    combat_flash_echo(cs, target_enemy, target_ally);
    cs->resolving_card_class = card->class;

    int half_dmg = echo_half_value(card_damage(card, upgrade_level));
    int half_hl = echo_half_value(card_heal(card, upgrade_level));
    int half_sh = echo_half_value(card_shield(card, upgrade_level));

    if (half_dmg > 0)
    {
        if (card->target == TARGET_ALL_ENEMIES)
        {
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                {
                    apply_damage_to_enemy(cs, i, half_dmg);
                    resolved = true;
                }
        }
        else if (target_enemy >= 0)
        {
            apply_damage_to_enemy(cs, target_enemy, half_dmg);
            resolved = true;
        }
    }

    if (card->target == TARGET_ALL_ALLIES)
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
            {
                if (half_hl > 0) apply_heal_to_ally(cs, i, half_hl);
                if (half_sh > 0) apply_shield_to_ally(cs, i, half_sh);
                resolved = resolved || half_hl > 0 || half_sh > 0;
            }
    }
    else if (card->target == TARGET_ALLY && target_ally >= 0)
    {
        if (half_hl > 0) apply_heal_to_ally(cs, target_ally, half_hl);
        if (half_sh > 0) apply_shield_to_ally(cs, target_ally, half_sh);
        resolved = resolved || half_hl > 0 || half_sh > 0;
    }
    else
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            if (half_hl > 0) apply_heal_to_ally(cs, caster, half_hl);
            if (half_sh > 0) apply_shield_to_ally(cs, caster, half_sh);
            resolved = resolved || half_hl > 0 || half_sh > 0;
        }
    }

    if (card->effects && card->effect_count > 0)
    {
        for (int i = 0; i < card->effect_count; i++)
        {
            apply_echo_card_effect(cs, card, &card->effects[i], target_enemy, target_ally);
            resolved = true;
        }
    }

    cs->resolving_card_class = CLASS_NONE;

    if (resolved)
    {
        LOG_D(CAT_CARD, "Echo: resolving copy at 50%% after delay");
        combat_feed_add(cs, "Echo copy resolves");
    }

    check_defeat(cs);
    if (cs->phase == COMBAT_DEFEAT) return;
    check_victory(cs);
}

static bool card_is_heal_card(const CardDef *card)
{
    if (!card) return false;
    return card->heal > 0 || card->heal2 > 0 ||
        card_has_effect(card, CARD_EFFECT_REVIVE_TARGET) ||
        card_has_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ALLY) ||
        card_has_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES);
}

static bool card_is_attack_card(const CardDef *card)
{
    return card && card->type == CARD_ATTACK && card->damage > 0;
}

static bool card_is_aoe_card(const CardDef *card)
{
    if (!card) return false;
    return card->target == TARGET_ALL_ENEMIES ||
        (card->channel && card->target == TARGET_SELF && card->damage > 0);
}

static bool card_is_fire_spell(const CardDef *card)
{
    if (!card || card->class != CLASS_MAGE || card->damage <= 0) return false;
    return true;
}

static int combat_effective_card_cost(CombatState *cs, const CardDef *card)
{
    if (!cs || !card) return 0;
    // Check data-driven combos for free_cost
    for (int i = 0; i < synergy_combo_count(); i++)
    {
        const SynergyComboDef *c = synergy_combo_by_index(i);
        if (!c || !c->free_cost) continue;
        if (cs->combo_prime_index == i)
        {
            if (strcmp(c->consume_card_type, "heal") == 0 && card_is_heal_card(card))
                return 0;
            if (strcmp(c->consume_card_type, "mage_fire_spell") == 0 && card_is_fire_spell(card))
                return 0;
        }
    }
    int cost = card->cost;
    int cost_up = boss_arena_value(cs, AURA_COST_UP);
    if (cs->temp_card_cost_turns > 0)
        cost_up += cs->temp_card_cost_delta;
    cost += cost_up;
    if (cost < 0) cost = 0;
    return cost;
}

static bool preview_combo_consumes(const CombatState *cs, const SynergyComboDef *c, const CardDef *card, int dmg)
{
    if (!cs || !c || !card) return false;
    if (strcmp(c->consume_card_type, "heal") == 0 && card_is_heal_card(card)) return true;
    if (strcmp(c->consume_card_type, "attack") == 0 && card_is_attack_card(card)) return true;
    if (strcmp(c->consume_card_type, "aoe") == 0 && card_is_aoe_card(card)) return true;
    if (strcmp(c->consume_card_type, "damage") == 0 && dmg > 0) return true;
    if (strcmp(c->consume_card_type, "group_heal_or_shield") == 0 && card->target == TARGET_ALL_ALLIES) return true;
    if (strcmp(c->consume_card_type, "mage_fire_spell") == 0 && card_is_fire_spell(card)) return true;
    if (strcmp(c->consume_card_type, "warlock_damage") == 0 && card->class == CLASS_WARLOCK && dmg > 0) return true;
    if (strcmp(c->consume_card_type, "paladin") == 0 && card->class == CLASS_PALADIN) return true;
    return false;
}

static void preview_card_live_values(const CombatState *cs, const CardInstance *inst, int target_enemy, int *out_dmg, int *out_heal, int *out_shield, bool *out_dark_refrain)
{
    const CardDef *card = inst ? inst->def : NULL;
    int upgrade_level = inst ? inst->upgrade_level : 0;
    int dmg = card_damage(card, upgrade_level) + meta_dmg_bonus(&g_state.meta);
    int hl = card_heal(card, upgrade_level);
    int sh = card_shield(card, upgrade_level) + meta_shield_bonus(&g_state.meta);
    bool dark_refrain = false;

    if (card && card->class != CLASS_NONE)
    {
        if (dmg > 0) dmg += combat_class_perk_effect_total((CombatState *)cs, card->class, "card_damage");
        if (hl > 0) hl += combat_class_perk_effect_total((CombatState *)cs, card->class, "card_heal");
        if (sh > 0) sh += combat_class_perk_effect_total((CombatState *)cs, card->class, "card_shield");
    }
    if (dmg > 0 && card && card->class == CLASS_WARLOCK)
        dmg += meta_warlock_damage_bonus(&g_state.meta);
    if (sh > 0 && card && card->class == CLASS_PALADIN)
        sh += meta_paladin_shield_bonus(&g_state.meta);
    if (hl > 0)
        hl += meta_heal_bonus(&g_state.meta);
    if (dmg > 0 && !cs->meta_opening_damage_used)
        dmg += meta_opening_damage_bonus(&g_state.meta);
    if (dmg > 0 && (g_state.encounter_is_elite || g_state.encounter_is_boss))
        dmg += meta_elite_damage_bonus(&g_state.meta);
    if (dmg > 0 && g_state.encounter_is_boss)
        dmg += meta_boss_damage_bonus(&g_state.meta);
    if (dmg > 0 && target_enemy >= 0 && target_enemy < cs->enemy_count)
    {
        const EnemyState *target = &cs->enemies[target_enemy];
        if (target->def && target->hp > 0 && target->hp * 2 <= target->max_hp)
            dmg += meta_weak_enemy_damage_bonus(&g_state.meta);
    }

    if (dmg > 0 && card_is_attack_card(card) && relic_has(g_state.relics, g_state.relic_count, RELIC_WHETSTONE))
        dmg += 1;
    if (dmg > 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_GILDED_BLADE))
    {
        int bonus = g_state.gold / 50;
        if (bonus > 6) bonus = 6;
        if (bonus > 0) dmg += bonus;
    }
    if (dmg > 0 && target_enemy >= 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_MARK_OF_THE_HUNT) &&
        enemy_has_status((CombatState *)cs, target_enemy, STATUS_MARKED))
        dmg += 2;
    if (hl > 0 && card_is_heal_card(card) && relic_has(g_state.relics, g_state.relic_count, RELIC_PRAYER_BEADS))
        hl += 2;
    if (sh > 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_BALLAST_RING))
        sh += 2;

    if (cs->combo_prime_index >= 0 && cs->combo_prime_index < synergy_combo_count())
    {
        const SynergyComboDef *c = synergy_combo_by_index(cs->combo_prime_index);
        if (preview_combo_consumes(cs, c, card, dmg))
        {
            if (c->multiply_damage > 1.0f) dmg = (int)(dmg * c->multiply_damage);
            if (c->multiply_heal > 1.0f) hl = (int)(hl * c->multiply_heal);
            if (c->multiply_shield > 1.0f) sh = (int)(sh * c->multiply_shield);
            if (c->apply_status && strcmp(c->apply_status, "BLIGHT") == 0 &&
                strcmp(c->consume_card_type, "warlock_damage") == 0)
                dark_refrain = true;
        }
    }

    if (!cs->ambush_used && dmg > 0 && party_has_pair((CombatState *)cs, CLASS_ROGUE, CLASS_RANGER))
        dmg = (int)(dmg * 1.5f);
    if (!cs->magical_might_used && dmg > 0 && party_has_pair((CombatState *)cs, CLASS_MAGE, CLASS_WARLOCK) &&
        card && (card->class == CLASS_MAGE || card->class == CLASS_WARLOCK))
        dmg += 3;
    if (cs->player_damage_mult > 1.0f && dmg > 0)
    {
        dmg = (int)(dmg * cs->player_damage_mult);
        if (dmg < 1) dmg = 1;
    }
    int arena_reduction = boss_arena_value(cs, AURA_DAMAGE_REDUCTION);
    if (arena_reduction > 0 && dmg > 0)
    {
        if (arena_reduction > 95) arena_reduction = 95;
        dmg = dmg * (100 - arena_reduction) / 100;
    }

    int caster = -1;
    if (card) find_caster((CombatState *)cs, card->class, &caster);
    if (caster >= 0)
    {
        int weak_idx = status_find((StatusEffect *)cs->party.members[caster].statuses, cs->party.members[caster].status_count, STATUS_WEAKNESS);
        if (weak_idx >= 0 && dmg > 0)
        {
            int pct = cs->party.members[caster].statuses[weak_idx].value;
            if (pct < 0) pct = 0;
            if (pct > 90) pct = 90;
            dmg = (dmg * (100 - pct)) / 100;
        }
    }

    bool marked_target = target_enemy >= 0 && enemy_has_status((CombatState *)cs, target_enemy, STATUS_MARKED);
    bool blighted_target = target_enemy >= 0 && enemy_has_status((CombatState *)cs, target_enemy, STATUS_BLIGHT);
    if (card_is(card, "rng_pounce") && marked_target)
        dmg += 4;
    if (card_is(card, "rog_evis") && marked_target)
        dmg += 8;
    if (card_is(card, "grd_shield_slam") && blighted_target)
        sh += 6;
    if (card_is(card, "pal_aegis_aura"))
    {
        int blighted = count_enemies_with_status((CombatState *)cs, STATUS_BLIGHT);
        if (blighted > 0) sh += blighted * 3;
    }
    if (card_is(card, "brd_finale"))
    {
        int synergies = count_enemy_synergy_statuses((CombatState *)cs);
        if (synergies > 0)
        {
            int bonus = synergies * 2;
            hl += bonus;
            sh += bonus;
        }
    }

    int mage_first_damage = combat_class_perk_effect_total((CombatState *)cs, CLASS_MAGE, "first_mage_damage_each_turn");
    if (dmg > 0 && card && card->class == CLASS_MAGE && !cs->mage_first_spell_used && mage_first_damage > 0)
        dmg += mage_first_damage;

    int ranger_marked_damage = combat_class_perk_effect_total((CombatState *)cs, CLASS_RANGER, "first_marked_damage");
    if (dmg > 0 && card && card->class == CLASS_RANGER && marked_target && !cs->ranger_marked_dmg_used && ranger_marked_damage > 0)
        dmg += ranger_marked_damage;

    if (out_dmg) *out_dmg = dmg;
    if (out_heal) *out_heal = hl;
    if (out_shield) *out_shield = sh;
    if (out_dark_refrain) *out_dark_refrain = dark_refrain;
}

static void preview_apply_enemy_damage(CombatCardPreview *out, int enemy_idx, int raw_damage, int temp_hp[], int temp_shield[], bool counted[])
{
    if (!out || enemy_idx < 0 || enemy_idx >= MAX_ENEMIES || raw_damage <= 0) return;
    if (temp_hp[enemy_idx] <= 0) return;

    if (!counted[enemy_idx])
    {
        counted[enemy_idx] = true;
        out->targets++;
    }

    out->raw_damage_total += raw_damage;
    int remaining = raw_damage;
    int blocked = temp_shield[enemy_idx] >= remaining ? remaining : temp_shield[enemy_idx];
    temp_shield[enemy_idx] -= blocked;
    remaining -= blocked;
    out->blocked_total += blocked;

    int hp_damage = temp_hp[enemy_idx] >= remaining ? remaining : temp_hp[enemy_idx];
    temp_hp[enemy_idx] -= hp_damage;
    out->hp_damage_total += hp_damage;
}

static int preview_shield_gain_for_ally(const CombatState *cs, int ally_idx, int amount)
{
    if (!cs || ally_idx < 0 || ally_idx >= cs->party.count || amount <= 0) return 0;
    const PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return 0;
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_MIRROR_SHIELD))
        amount += 3;
    int cap = party_member_shield_cap_for_percent(pm, meta_shield_cap_percent(&g_state.meta));
    int room = cap - pm->shield;
    if (room <= 0) return 0;
    return amount < room ? amount : room;
}

static int preview_heal_gain_for_ally(const CombatState *cs, int ally_idx, int amount)
{
    if (!cs || ally_idx < 0 || ally_idx >= cs->party.count || amount <= 0) return 0;
    const PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return 0;
    int room = pm->max_hp - pm->hp;
    if (room <= 0) return 0;
    return amount < room ? amount : room;
}

static int preview_blight_immediate_damage(const CombatState *cs, const CardDef *card, int amount, int turns, bool *warlock_boost_available)
{
    if (!cs || amount <= 0 || turns <= 0) return 0;
    if (!party_has_pair((CombatState *)cs, CLASS_RANGER, CLASS_WARLOCK)) return 0;
    if (card && card->class == CLASS_WARLOCK && warlock_boost_available && *warlock_boost_available)
    {
        int bonus = combat_class_perk_effect_total((CombatState *)cs, CLASS_WARLOCK, "first_blight_bonus");
        if (bonus > 0)
        {
            amount += bonus;
            *warlock_boost_available = false;
        }
    }
    return amount * turns;
}

bool combat_card_preview(const CombatState *cs, int hand_idx, int target_enemy, int target_ally, CombatCardPreview *out)
{
    if (!cs || !out || hand_idx < 0 || hand_idx >= cs->deck.hand_count) return false;
    memset(out, 0, sizeof(*out));

    const CardInstance *inst = &cs->deck.cards[cs->deck.hand[hand_idx]];
    const CardDef *card = inst->def;
    if (!card) return false;

    int dmg = 0, hl = 0, sh = 0;
    bool dark_refrain = false;
    preview_card_live_values(cs, inst, target_enemy, &dmg, &hl, &sh, &dark_refrain);
    out->damage_per_hit = dmg;
    out->hits = card_repeat_hits(card);
    if (out->hits < 1) out->hits = 1;
    out->will_interrupt = card_instance_has_interrupt(inst);
    out->will_channel = card->channel;
    out->will_echo = card_instance_has_echo(inst) ||
        (!cs->echo_used && card->cost >= 1 && relic_has(g_state.relics, g_state.relic_count, RELIC_ECHO_BELL));

    int temp_hp[MAX_ENEMIES] = { 0 };
    int temp_shield[MAX_ENEMIES] = { 0 };
    bool counted[MAX_ENEMIES] = { false };
    for (int i = 0; i < cs->enemy_count && i < MAX_ENEMIES; i++)
    {
        temp_hp[i] = cs->enemies[i].hp;
        temp_shield[i] = cs->enemies[i].shield;
    }

    bool warlock_boost_available = !cs->warlock_blight_boost_used;

    if (!card->channel && dmg > 0)
    {
        if (card_is(card, "wlk_dark_harvest"))
        {
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0 && enemy_has_status((CombatState *)cs, i, STATUS_BLIGHT))
                    preview_apply_enemy_damage(out, i, dmg, temp_hp, temp_shield, counted);
        }
        else if (card->target == TARGET_ALL_ENEMIES)
        {
            for (int i = 0; i < cs->enemy_count; i++)
            {
                if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
                int per_target_damage = dmg;
                if (card_is(card, "mag_meteor"))
                {
                    int conductive = enemy_status_value((CombatState *)cs, i, STATUS_CONDUCTIVE);
                    if (conductive > 0) per_target_damage += conductive * 10;
                }
                if (card_is(card, "wlk_hellfire") && enemy_has_status((CombatState *)cs, i, STATUS_BLIGHT))
                    per_target_damage += 4;
                preview_apply_enemy_damage(out, i, per_target_damage, temp_hp, temp_shield, counted);
                if (dark_refrain && temp_hp[i] > 0)
                    preview_apply_enemy_damage(out, i, preview_blight_immediate_damage(cs, card, 2, 3, &warlock_boost_available), temp_hp, temp_shield, counted);
            }
        }
        else if (card->target == TARGET_ENEMY && target_enemy >= 0 && target_enemy < cs->enemy_count)
        {
            preview_apply_enemy_damage(out, target_enemy, dmg * out->hits, temp_hp, temp_shield, counted);

            if (!cs->split_prism_used && relic_has(g_state.relics, g_state.relic_count, RELIC_SPLIT_PRISM))
            {
                int splash = dmg / 2;
                if (splash < 1) splash = 1;
                for (int dir = -1; dir <= 1; dir += 2)
                {
                    int arc = target_enemy + dir;
                    if (arc < 0 || arc >= cs->enemy_count) continue;
                    if (!cs->enemies[arc].def || cs->enemies[arc].hp <= 0) continue;
                    preview_apply_enemy_damage(out, arc, splash, temp_hp, temp_shield, counted);
                }
            }
            if (card_is(card, "mag_fireball") && enemy_has_status((CombatState *)cs, target_enemy, STATUS_CONDUCTIVE))
            {
                for (int dir = -1; dir <= 1; dir += 2)
                {
                    int arc = target_enemy + dir;
                    if (arc < 0 || arc >= cs->enemy_count) continue;
                    if (!cs->enemies[arc].def || cs->enemies[arc].hp <= 0) continue;
                    preview_apply_enemy_damage(out, arc, dmg / 2, temp_hp, temp_shield, counted);
                }
            }
            if (card_is(card, "rog_shadow") && enemy_has_status((CombatState *)cs, target_enemy, STATUS_CONDUCTIVE))
            {
                for (int i = 0; i < cs->enemy_count; i++)
                {
                    if (i == target_enemy) continue;
                    if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
                    if (!enemy_has_status((CombatState *)cs, i, STATUS_CONDUCTIVE)) continue;
                    preview_apply_enemy_damage(out, i, dmg / 2, temp_hp, temp_shield, counted);
                }
            }
            if (card_is(card, "shm_chain_lightning"))
            {
                int jumps = 0;
                for (int i = 0; i < cs->enemy_count && jumps < 2; i++)
                {
                    if (i == target_enemy) continue;
                    if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
                    preview_apply_enemy_damage(out, i, dmg, temp_hp, temp_shield, counted);
                    jumps++;
                }
            }
            if (dark_refrain && temp_hp[target_enemy] > 0)
                preview_apply_enemy_damage(out, target_enemy, preview_blight_immediate_damage(cs, card, 2, 3, &warlock_boost_available), temp_hp, temp_shield, counted);
            if (card->effects && card->effect_count > 0 && temp_hp[target_enemy] > 0)
            {
                for (int i = 0; i < card->effect_count; i++)
                {
                    const CardEffect *effect = &card->effects[i];
                    if (effect->type != CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY || effect->status != STATUS_BLIGHT) continue;
                    preview_apply_enemy_damage(out, target_enemy, preview_blight_immediate_damage(cs, card, effect->amount, effect->turns, &warlock_boost_available), temp_hp, temp_shield, counted);
                }
            }
        }
    }

    if (card->target == TARGET_ALL_ALLIES)
    {
        for (int i = 0; i < cs->party.count; i++)
        {
            if (!cs->party.members[i].alive) continue;
            out->targets++;
            out->heal_total += preview_heal_gain_for_ally(cs, i, hl);
            out->shield_total += preview_shield_gain_for_ally(cs, i, sh);
        }
    }
    else if (card->target == TARGET_SELF)
    {
        int caster = -1;
        find_caster((CombatState *)cs, card->class, &caster);
        if (caster >= 0)
        {
            out->targets = 1;
            out->heal_total += preview_heal_gain_for_ally(cs, caster, hl);
            out->shield_total += preview_shield_gain_for_ally(cs, caster, sh);
        }
        else
        {
            out->invalid_target = true;
        }
    }
    else if (card->target == TARGET_ALLY && target_ally >= 0 && target_ally < cs->party.count)
    {
        const PartyMember *pm = &cs->party.members[target_ally];
        bool revive = card_has_effect(card, CARD_EFFECT_REVIVE_TARGET);
        if (!pm->alive && !revive)
            out->invalid_target = true;
        else if (revive && pm->alive)
            out->invalid_target = true;
        else if (revive && !pm->alive)
        {
            out->targets = 1;
            out->revive_hp = pm->max_hp / 2;
            if (out->revive_hp < 1) out->revive_hp = 1;
        }
        else
        {
            out->targets = 1;
            out->heal_total += preview_heal_gain_for_ally(cs, target_ally, hl);
            out->shield_total += preview_shield_gain_for_ally(cs, target_ally, sh);
        }
    }
    else if (card->target == TARGET_ENEMY && target_enemy < 0)
    {
        out->invalid_target = true;
    }

    return true;
}

static int boss_enemy_damage_value(const CombatState *cs, const EnemyState *e, const EnemyCardDef *cd)
{
    if (!cs || !e || !e->def || !cd) return 0;
    float scale = cs->floor_scale * cs->enemy_damage_scale * cs->enemy_damage_buff_scale;
    if (e->phase_damage_scale_delta != 0.0f)
        scale *= 1.0f + e->phase_damage_scale_delta;

    int aura_damage_up = boss_arena_value(cs, AURA_ENEMY_DAMAGE_UP);
    if (aura_damage_up > 0)
        scale *= 1.0f + (float)aura_damage_up / 100.0f;

    for (int i = 0; i < cs->enemy_count; i++)
    {
        const EnemyState *leader = &cs->enemies[i];
        if (leader == e || !leader->def || leader->hp <= 0) continue;
        for (int b = 0; b < leader->def->minion_buff_count; b++)
        {
            const MinionBuffDef *buff = &leader->def->minion_buffs[b];
            if (buff->effect == MINION_BUFF_DAMAGE_BOOST && buff->value > 0)
                scale *= 1.0f + (float)buff->value / 100.0f;
        }
    }

    const EnemyDef *def = e->def;
    if (def->rage_scale > 0.0f && def->rage_hp_threshold > 0 && e->max_hp > 0)
    {
        int hp_pct = e->hp * 100 / e->max_hp;
        if (hp_pct <= def->rage_hp_threshold)
        {
            float hp_ratio = (float)e->hp / (float)e->max_hp;
            float bonus = 0.0f;
            if (def->rage_formula == RAGE_FORMULA_STEP)
                bonus = def->rage_scale;
            else if (def->rage_formula == RAGE_FORMULA_EXPONENTIAL)
                bonus = def->rage_scale * (1.0f - hp_ratio) * (1.0f - hp_ratio);
            else
                bonus = def->rage_scale * (1.0f - hp_ratio);
            float max_mult = def->rage_max_mult > 0.0f ? def->rage_max_mult : 1.0f + def->rage_scale;
            float mult = 1.0f + bonus;
            if (mult > max_mult) mult = max_mult;
            scale *= mult;
        }
    }

    int damage = (int)((float)cd->base_damage * scale);
    if (cd->damage_per_player_energy_drained > 0)
        damage += cd->damage_per_player_energy_drained * cs->player_energy_drained_this_turn;

    int trap_idx = status_find((StatusEffect *)e->statuses, e->status_count, STATUS_TRAP);
    if (trap_idx >= 0)
    {
        damage -= e->statuses[trap_idx].value;
        if (damage < 0) damage = 0;
    }
    return damage;
}

bool combat_enemy_ability_preview(const CombatState *cs, int enemy_idx, CombatEnemyAbilityPreview *out)
{
    if (!cs || !out || enemy_idx < 0 || enemy_idx >= cs->enemy_count) return false;
    memset(out, 0, sizeof(*out));
    const EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0 || e->intent.ability_idx < 0 || e->intent.ability_idx >= e->def->card_count)
        return false;

    const EnemyCardDef *cd = &e->def->cards[e->intent.ability_idx];
    int damage = boss_enemy_damage_value(cs, e, cd);

    out->damage_per_hit = damage;
    out->repeats = cd->repeats < 1 ? 1 : cd->repeats;
    out->heal = cd->heal_amount;
    out->shield = cd->shield_amount;
    if (cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE)
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                out->targets++;
        out->total_damage = damage;
    }
    else if (damage > 0)
    {
        out->targets = 1;
        out->total_damage = damage * out->repeats;
    }
    return true;
}

static void combo_prime_set(CombatState *cs, const SynergyComboDef *def)
{
    if (!cs || !def) return;
    int turns = def->combo_turns;
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_SYNERGY_HOURGLASS))
        turns++;
    if (turns < 0) turns = 0;
    cs->combo_prime_index = (int)(def - synergy_combo_by_index(0));
    cs->combo_prime_turns_remaining = turns;
    snprintf(cs->synergy_banner_title, sizeof(cs->synergy_banner_title), "%s", def->title);
    snprintf(cs->synergy_banner_subtitle, sizeof(cs->synergy_banner_subtitle), "%s", def->subtitle);
    cs->synergy_banner_timer = 1.35f;
    cs->synergy_flash_timer = 0.32f;
    assets_play_sfx(SFX_SYNERGY_TRIGGER);
    cs->combo_scale = 1.0f;
    cs->combo_tween = tween_create(&cs->combo_scale, 1.35f, 0.12f, EASE_OUT_BACK);
    tween_chain(cs->combo_tween, &cs->combo_scale, 1.0f, 0.35f, EASE_OUT_ELASTIC);
    combat_feed_add(cs, "%s primed", def->title);
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_RESONANT_CHARM) &&
        cs->energy.current < cs->energy.max)
    {
        cs->energy.current++;
        combat_feed_add(cs, "Resonant Charm: +1 energy");
    }
}

static void combo_prime_clear(CombatState *cs)
{
    if (!cs) return;
    cs->combo_prime_index = -1;
    cs->combo_prime_turns_remaining = 0;
}

static void combo_check_chain(CombatState *cs, ClassType previous, ClassType current)
{
    if (!cs || previous == CLASS_NONE || current == CLASS_NONE || previous == current) return;

    for (int i = 0; i < synergy_combo_count(); i++)
    {
        const SynergyComboDef *def = synergy_combo_by_index(i);
        if (def && def->prev_class == previous && def->next_class == current)
        {
            combo_prime_set(cs, def);
            return;
        }
    }
}

static void apply_relic_combat_start(CombatState *cs)
{
    if (!cs) return;

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_BATTLE_DRUM))
    {
        cs->energy.current += 1;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        combat_feed_add(cs, "Battle Drum: +1 energy");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_ASHEN_CONTRACT))
    {
        cs->energy.current += 1;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        for (int i = 0; i < cs->party.count; i++)
        {
            PartyMember *pm = &cs->party.members[i];
            if (!pm->alive) continue;
            pm->hp -= 2;
            if (pm->hp < 1) pm->hp = 1;
        }
        combat_feed_add(cs, "Ashen Contract: +1 energy, -2 HP");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_SCOUTING_MAP))
    {
        deck_draw(&cs->deck);
        combat_feed_add(cs, "Scouting Map: drew 1");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_DUELIST_SIGIL) && cs->party.count == 1)
    {
        cs->energy.current += 1;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        deck_draw(&cs->deck);
        combat_feed_add(cs, "Duelist Sigil: +1 energy, drew 1");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_QUICKDRAW_GLOVE))
    {
        deck_draw(&cs->deck);
        combat_feed_add(cs, "Quickdraw Glove: drew 1");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_WARD_STONE))
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                add_party_member_shield_capped(&cs->party.members[i], 4);
        combat_feed_add(cs, "Ward Stone: +4 Shield");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_FELLOWSHIP_STANDARD) && cs->party.count >= 4)
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                add_party_member_shield_capped(&cs->party.members[i], 5);
        combat_feed_add(cs, "Fellowship Standard: +5 Shield");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_PROSPERITY_CHARM))
    {
        int shield = g_state.gold / 20;
        if (shield > 0)
        {
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    add_party_member_shield_capped(&cs->party.members[i], shield);
            combat_feed_add(cs, "Prosperity Charm: +%d Shield", shield);
        }
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_GOLDEN_IDOL))
    {
        int hp = g_state.gold / 25;
        if (hp > 0)
        {
            for (int i = 0; i < cs->party.count; i++)
            {
                PartyMember *pm = &cs->party.members[i];
                pm->max_hp += hp;
                if (pm->alive)
                    pm->hp += hp;
            }
            combat_feed_add(cs, "Golden Idol: +%d max HP", hp);
        }
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_WARDEN_CREST))
    {
        int lowest = party_lowest_hp(&cs->party);
        if (lowest >= 0)
        {
            add_party_member_shield_capped(&cs->party.members[lowest], 8);
            combat_feed_add(cs, "Warden Crest: +8 Shield");
        }
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_MENDING_BEAD))
    {
        for (int i = 0; i < cs->party.count; i++)
        {
            PartyMember *pm = &cs->party.members[i];
            if (!pm->alive) continue;
            pm->hp += 4;
            if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;
        }
        combat_feed_add(cs, "Mending Bead: +4 HP");
    }

    // Spirit Stone: +1 energy per owned relic (max +3)
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_SPIRIT_STONE))
    {
        int bonus = g_state.relic_count;
        if (bonus > 3) bonus = 3;
        cs->energy.current += bonus;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        combat_feed_add(cs, "Spirit Stone: +%d energy", bonus);
    }

    // Mana Gem bonus
    if (cs->mana_gem_bonus > 0)
    {
        cs->energy.max += cs->mana_gem_bonus;
    }

}

// ── Card resolver ───────────────────────────────────────────

static void resolve_card_on_target(CombatState *cs, int hand_idx, int target_enemy, int target_ally, int paid_cost)
{
    if (hand_idx < 0 || hand_idx >= cs->deck.hand_count) return;

    CardInstance *inst = &cs->deck.cards[cs->deck.hand[hand_idx]];
    const CardDef *card = inst->def;
    if (!card || !card->name) return;
    int played_uid = inst->uid;
    cs->resolving_card_class = card->class;
    int resolving_ally = -1;
    find_caster(cs, card->class, &resolving_ally);
    cs->resolving_ally_idx = resolving_ally;
    if (resolving_ally >= 0)
        cs->last_player_attacker = resolving_ally;
    combat_award_card_xp(cs, card, paid_cost);

    int upgrade_level = inst->upgrade_level;
    log_card_play_metric(cs, card, upgrade_level, paid_cost, target_enemy, target_ally);

    int dmg = card_damage(card, upgrade_level) + meta_dmg_bonus(&g_state.meta);
    int hl  = card_heal(card, upgrade_level);
    int sh  = card_shield(card, upgrade_level) + meta_shield_bonus(&g_state.meta);
    if (card->class != CLASS_NONE)
    {
        if (dmg > 0) dmg += combat_class_perk_effect_total(cs, card->class, "card_damage");
        if (hl > 0) hl += combat_class_perk_effect_total(cs, card->class, "card_heal");
        if (sh > 0) sh += combat_class_perk_effect_total(cs, card->class, "card_shield");
    }
    if (dmg > 0 && card->class == CLASS_WARLOCK)
        dmg += meta_warlock_damage_bonus(&g_state.meta);
    if (sh > 0 && card->class == CLASS_PALADIN)
        sh += meta_paladin_shield_bonus(&g_state.meta);
    if (hl > 0)
        hl += meta_heal_bonus(&g_state.meta);
    if (dmg > 0 && !cs->meta_opening_damage_used)
    {
        int opening = meta_opening_damage_bonus(&g_state.meta);
        if (opening > 0)
        {
            dmg += opening;
            cs->meta_opening_damage_used = true;
            combat_feed_add(cs, "Opening Strike: +%d damage", opening);
        }
    }
    if (dmg > 0 && (g_state.encounter_is_elite || g_state.encounter_is_boss))
        dmg += meta_elite_damage_bonus(&g_state.meta);
    if (dmg > 0 && g_state.encounter_is_boss)
        dmg += meta_boss_damage_bonus(&g_state.meta);
    if (dmg > 0 && target_enemy >= 0 && target_enemy < cs->enemy_count)
    {
        EnemyState *target = &cs->enemies[target_enemy];
        if (target->def && target->hp > 0 && target->hp * 2 <= target->max_hp)
            dmg += meta_weak_enemy_damage_bonus(&g_state.meta);
    }

    LOG_I(CAT_CARD, "Playing %s (enemy=%d, ally=%d) upgrade_level=%d channel=%d", card->name, target_enemy, target_ally, upgrade_level, card->channel);
    assets_play_sfx(SFX_CARD_PLAY);
    combat_spawn_card_throw(cs, hand_idx, card, upgrade_level, target_enemy, target_ally);
    combat_flash_played_card(cs, card, target_enemy, target_ally);
    combat_feed_add(cs, "Played %s", card->name);

    // ── Combo check ─────────────────────────────────────────
    if (dmg > 0 && card_is_attack_card(card) && relic_has(g_state.relics, g_state.relic_count, RELIC_WHETSTONE))
        dmg += 1;
    if (dmg > 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_GILDED_BLADE))
    {
        int bonus = g_state.gold / 50;
        if (bonus > 6) bonus = 6;
        if (bonus > 0)
            dmg += bonus;
    }
    if (dmg > 0 && target_enemy >= 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_MARK_OF_THE_HUNT) &&
        enemy_has_status(cs, target_enemy, STATUS_MARKED))
        dmg += 2;
    if (hl > 0 && card_is_heal_card(card) && relic_has(g_state.relics, g_state.relic_count, RELIC_PRAYER_BEADS))
        hl += 2;
    if (sh > 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_BALLAST_RING))
        sh += 2;

    ClassType previous_class = cs->last_played_class;
    bool arcane_assault_flag = false;
    bool dark_refrain_flag = false;
    bool absolution_flag = false;

    // ── Combo consume: check data-driven combos ──
    if (cs->combo_prime_index >= 0 && cs->combo_prime_index < synergy_combo_count())
    {
        const SynergyComboDef *c = synergy_combo_by_index(cs->combo_prime_index);
        if (c)
        {
            bool consumed = false;
            if (strcmp(c->consume_card_type, "heal") == 0 && card_is_heal_card(card))
                consumed = true;
            else if (strcmp(c->consume_card_type, "attack") == 0 && card_is_attack_card(card))
                consumed = true;
            else if (strcmp(c->consume_card_type, "aoe") == 0 && card_is_aoe_card(card))
                consumed = true;
            else if (strcmp(c->consume_card_type, "damage") == 0 && dmg > 0)
                consumed = true;
            else if (strcmp(c->consume_card_type, "group_heal_or_shield") == 0 && card->target == TARGET_ALL_ALLIES)
                consumed = true;
            else if (strcmp(c->consume_card_type, "mage_fire_spell") == 0 && card_is_fire_spell(card))
                consumed = true;
            else if (strcmp(c->consume_card_type, "warlock_damage") == 0 && card->class == CLASS_WARLOCK && dmg > 0)
                consumed = true;
            else if (strcmp(c->consume_card_type, "paladin") == 0 && card->class == CLASS_PALADIN)
                consumed = true;

            if (consumed)
            {
                if (c->multiply_damage > 1.0f)
                    dmg = (int)(dmg * c->multiply_damage);
                if (c->multiply_heal > 1.0f)
                    hl = (int)(hl * c->multiply_heal);
                if (c->multiply_shield > 1.0f)
                    sh = (int)(sh * c->multiply_shield);
                if (c->apply_status)
                {
                    StatusType st = STATUS_NONE;
                    if (strcmp(c->apply_status, "BURNING") == 0) st = STATUS_BURNING;
                    else if (strcmp(c->apply_status, "BLIGHT") == 0) st = STATUS_BLIGHT;
                    if (st != STATUS_NONE)
                    {
                        if (strcmp(c->consume_card_type, "attack") == 0)
                            arcane_assault_flag = true;
                        else if (strcmp(c->consume_card_type, "warlock_damage") == 0)
                            dark_refrain_flag = true;
                        else
                            apply_status_to_enemy(cs, target_enemy, st, c->status_turns, c->status_value);
                    }
                }
                if (c->special_effect && strcmp(c->special_effect, "consume_blight_heal_party") == 0)
                {
                    absolution_flag = true;
                }
                combat_feed_add(cs, "%s consumed", c->title);
                combo_prime_clear(cs);
            }
        }
    }

    if (!cs->ambush_used && dmg > 0 && party_has_pair(cs, CLASS_ROGUE, CLASS_RANGER))
    {
        dmg = (int)(dmg * 1.5f);
        cs->ambush_used = true;
        combat_feed_add(cs, "Ambush: first strike +50%%");
    }

    // ── Magical Might: Mage+Warlock first spell +3 damage ──
    if (!cs->magical_might_used && dmg > 0 && party_has_pair(cs, CLASS_MAGE, CLASS_WARLOCK) &&
        (card->class == CLASS_MAGE || card->class == CLASS_WARLOCK))
    {
        dmg += 3;
        cs->magical_might_used = true;
        combat_feed_add(cs, "Magical Might: +3 damage");
    }

    // ── Sneaky Steal: Rogue+Paladin 10% lifesteal on all damage ──
    bool have_sneaky_steal = (dmg > 0 && party_has_pair(cs, CLASS_ROGUE, CLASS_PALADIN));
    int card_hp_damage_dealt = 0;

    int bard_draw = 0;
    int bard_perk_draw = combat_class_perk_effect_total(cs, CLASS_BARD, "first_bard_draw");
    if (bard_perk_draw > bard_draw)
        bard_draw = bard_perk_draw;
    if (meta_bard_draw_bonus(&g_state.meta) > bard_draw)
        bard_draw = meta_bard_draw_bonus(&g_state.meta);
    if (card->class == CLASS_BARD &&
        !cs->bard_first_draw_used &&
        bard_draw > 0)
    {
        cs->bard_first_draw_used = true;
        deal_cards(&cs->deck, bard_draw);
        combat_feed_add(cs, "%s: drew %d",
            bard_perk_draw > 0 ? combat_class_perk_effect_name(cs, CLASS_BARD, "first_bard_draw") : "Harmony",
            bard_draw);
    }

    // ── Blood Amber: lose 1 HP, gain 1 energy per card played ──
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_BLOOD_AMBER))
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            cs->party.members[caster].hp -= 1;
            if (cs->party.members[caster].hp < 0) cs->party.members[caster].hp = 0;
            cs->energy.current += 1;
            if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
            combat_feed_add(cs, "Blood Amber: -1 HP, +1 energy");
        }
    }

    // ── Echo: resolve card again at 50% (from card keyword or Echo Bell relic) ──
    bool echo_from_card = card_instance_has_echo(inst);
    bool echo_from_bell = !echo_from_card &&
        !cs->echo_used &&
        card->cost >= 1 &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_ECHO_BELL);
    bool echo_this_card = echo_from_card || echo_from_bell;
    if (echo_from_bell)
        cs->echo_used = true;
    if (echo_this_card)
        combat_feed_add(cs, "%s will echo", card->name);

    // ── Player damage buff multiplier ──
    if (cs->player_damage_mult > 1.0f && dmg > 0)
    {
        dmg = (int)(dmg * cs->player_damage_mult);
        if (dmg < 1) dmg = 1;
    }

    int caster_for_debuff = -1;
    find_caster(cs, card->class, &caster_for_debuff);
    if (caster_for_debuff >= 0)
    {
        int weak_idx = status_find(cs->party.members[caster_for_debuff].statuses, cs->party.members[caster_for_debuff].status_count, STATUS_WEAKNESS);
        if (weak_idx >= 0 && dmg > 0)
        {
            int pct = cs->party.members[caster_for_debuff].statuses[weak_idx].value;
            if (pct < 0) pct = 0;
            if (pct > 90) pct = 90;
            dmg = (dmg * (100 - pct)) / 100;
            combat_feed_add(cs, "Weakness reduced damage");
        }
    }

    bool marked_target = target_enemy >= 0 && enemy_has_status(cs, target_enemy, STATUS_MARKED);
    bool conductive_target = target_enemy >= 0 && enemy_has_status(cs, target_enemy, STATUS_CONDUCTIVE);
    bool blighted_target = target_enemy >= 0 && enemy_has_status(cs, target_enemy, STATUS_BLIGHT);
    bool consume_marked = false;
    bool consume_blight = false;
    bool arcane_assault = arcane_assault_flag;
    bool dark_refrain = dark_refrain_flag;
    bool absolution = absolution_flag;
    int extra_lowest_heal = 0;
    int extra_caster_heal = 0;
    int blight_consumed_count = 0;
    bool rogue_mark_payoff = false;

    if (card_is(card, "rng_pounce") && marked_target)
    {
        dmg += 4;
        combat_feed_add(cs, "[MARKED] Pounce found the opening");
    }
    if (card_is(card, "rog_evis") && marked_target)
    {
        dmg += 8;
        consume_marked = true;
        rogue_mark_payoff = true;
        combat_feed_add(cs, "[MARKED] Eviscerate +8 damage");
    }
    if (card_is(card, "clr_smite") && marked_target)
    {
        extra_lowest_heal += 8;
        consume_marked = true;
        combat_feed_add(cs, "[MARKED] Smite extra heal");
    }
    if (card_is(card, "pal_holy_strike") && marked_target)
    {
        extra_lowest_heal += 6;
        consume_marked = true;
        combat_feed_add(cs, "[MARKED] Holy Strike healed lowest ally");
    }
    if (card_is(card, "grd_shield_slam") && blighted_target)
    {
        sh += 6;
        consume_blight = true;
        combat_feed_add(cs, "[BLIGHT] Shield Slam +6 Shield");
    }
    if (card_is(card, "clr_holy_fire") && blighted_target)
    {
        extra_caster_heal += 10;
        consume_blight = true;
        combat_feed_add(cs, "[BLIGHT] Holy Fire heals caster");
    }
    if (card_is(card, "pal_judgment") && blighted_target)
    {
        apply_status_to_enemy(cs, target_enemy, STATUS_TRAP, 2, 4);
        consume_blight = true;
        combat_feed_add(cs, "[BLIGHT] Judgment applied Trap");
    }
    if (card_is(card, "pal_aegis_aura"))
    {
        int blighted = count_enemies_with_status(cs, STATUS_BLIGHT);
        if (blighted > 0)
        {
            sh += blighted * 3;
            combat_feed_add(cs, "[BLIGHT] Aegis Aura +%d Shield", blighted * 3);
        }
    }
    if (card_is(card, "wlk_shadow_bolt") && conductive_target)
    {
        apply_status_to_enemy(cs, target_enemy, STATUS_BLIGHT, 3, 2);
        combat_feed_add(cs, "[CONDUCTIVE] Shadow Bolt applied BLIGHT");
    }
    if (card_is(card, "wlk_drain_life") && marked_target)
    {
        extra_lowest_heal += 4;
        combat_feed_add(cs, "[MARKED] Drain Life healed lowest ally");
    }
    if (card_is(card, "brd_battle_hymn"))
    {
        int extended = extend_enemy_synergy_statuses(cs, 1);
        if (extended > 0)
            combat_feed_add(cs, "Battle Hymn extended %d synergies", extended);
    }
    if (card_is(card, "brd_finale"))
    {
        int synergies = count_enemy_synergy_statuses(cs);
        if (synergies > 0)
        {
            int bonus = synergies * 2;
            hl += bonus;
            sh += bonus;
            combat_feed_add(cs, "Finale amplified by %d synergies", synergies);
        }
    }
    if (card_is(card, "wlk_dark_harvest"))
    {
        int hit_count = 0;
        for (int ei = 0; ei < cs->enemy_count; ei++)
        {
            if (!cs->enemies[ei].def || cs->enemies[ei].hp <= 0) continue;
            if (!enemy_has_status(cs, ei, STATUS_BLIGHT)) continue;
            card_hp_damage_dealt += apply_damage_to_enemy(cs, ei, dmg);
            hit_count++;
        }
        if (hit_count > 0)
        {
            int heal = hit_count * 3;
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    apply_heal_to_ally(cs, i, heal);
            combat_feed_add(cs, "[BLIGHT] Dark Harvest hit %d", hit_count);
        }
        dmg = 0;
    }
    if (card_is(card, "grd_vengeful_retribution"))
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            cs->vengeful_active = true;
            cs->vengeful_ally = caster;
            combat_feed_add(cs, "Vengeful Retribution armed");
            ft_spawn(22.0f, 176.0f, "VENGEFUL", 10, (Color){ 245, 155, 80, 255 });
        }
    }

    // ── Utility card effects ────────────────────────────────
    // Channel cards don't resolve immediately — they start a channel instead
    int mage_first_damage = combat_class_perk_effect_total(cs, CLASS_MAGE, "first_mage_damage_each_turn");
    if (dmg > 0 &&
        card->class == CLASS_MAGE &&
        !cs->mage_first_spell_used &&
        mage_first_damage > 0)
    {
        dmg += mage_first_damage;
        cs->mage_first_spell_used = true;
        combat_feed_add(cs, "%s: +%d damage",
            combat_class_perk_effect_name(cs, CLASS_MAGE, "first_mage_damage_each_turn"),
            mage_first_damage);
    }
    int ranger_marked_damage = combat_class_perk_effect_total(cs, CLASS_RANGER, "first_marked_damage");
    if (dmg > 0 &&
        card->class == CLASS_RANGER &&
        marked_target &&
        !cs->ranger_marked_dmg_used &&
        ranger_marked_damage > 0)
    {
        dmg += ranger_marked_damage;
        cs->ranger_marked_dmg_used = true;
        combat_feed_add(cs, "%s: +%d damage",
            combat_class_perk_effect_name(cs, CLASS_RANGER, "first_marked_damage"),
            ranger_marked_damage);
    }

    if (!card->channel)
    {
        if (card->target == TARGET_ALL_ENEMIES)
        {
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                {
                    int per_target_damage = dmg;
                    if (card_is(card, "mag_meteor"))
                    {
                        int conductive = enemy_status_value(cs, i, STATUS_CONDUCTIVE);
                        if (conductive > 0)
                        {
                            per_target_damage += conductive * 10;
                            remove_enemy_status(cs, i, STATUS_CONDUCTIVE);
                            combat_feed_add(cs, "[CONDUCTIVE] Meteor consumed charge");
                        }
                    }
                    if (card_is(card, "wlk_hellfire") && enemy_has_status(cs, i, STATUS_BLIGHT))
                    {
                        per_target_damage += 4;
                        combat_feed_add(cs, "[BLIGHT] Hellfire burned hotter");
                    }
                    card_hp_damage_dealt += apply_damage_to_enemy(cs, i, per_target_damage);
                    if (arcane_assault)
                        apply_status_to_enemy(cs, i, STATUS_BURNING, 3, 2);
                    if (dark_refrain)
                        apply_status_to_enemy(cs, i, STATUS_BLIGHT, 3, 2);
                    if (card_is(card, "brd_dissonance"))
                        apply_status_to_enemy(cs, i, STATUS_CONDUCTIVE, 2, 1);
                }
        }
        else if (dmg > 0 && target_enemy >= 0)
        {
            int repeat_hits = card_repeat_hits(card);

            for (int hit = 0; hit < repeat_hits; hit++)
                card_hp_damage_dealt += apply_damage_to_enemy(cs, target_enemy, dmg);

            if (!cs->split_prism_used &&
                relic_has(g_state.relics, g_state.relic_count, RELIC_SPLIT_PRISM))
            {
                int splash = dmg / 2;
                if (splash < 1) splash = 1;
                int splashed = 0;
                for (int dir = -1; dir <= 1; dir += 2)
                {
                    int arc = target_enemy + dir;
                    if (arc < 0 || arc >= cs->enemy_count) continue;
                    if (!cs->enemies[arc].def || cs->enemies[arc].hp <= 0) continue;
                    card_hp_damage_dealt += apply_damage_to_enemy(cs, arc, splash);
                    splashed++;
                }
                if (splashed > 0)
                {
                    cs->split_prism_used = true;
                    combat_feed_add(cs, "Split Prism: splashed %d", splashed);
                }
            }

            if (card_is(card, "mag_missiles") && marked_target)
            {
                deck_draw(&cs->deck);
                combat_feed_add(cs, "[MARKED] Arcane Missiles drew 1");
            }
            if (card_is(card, "mag_fireball") && conductive_target)
            {
                for (int dir = -1; dir <= 1; dir += 2)
                {
                    int arc = target_enemy + dir;
                    if (arc < 0 || arc >= cs->enemy_count) continue;
                    if (!cs->enemies[arc].def || cs->enemies[arc].hp <= 0) continue;
                    card_hp_damage_dealt += apply_damage_to_enemy(cs, arc, dmg / 2);
                    combat_feed_add(cs, "[CONDUCTIVE] Fireball arced");
                }
            }
            if (card_is(card, "rog_shadow") && conductive_target)
            {
                for (int ei = 0; ei < cs->enemy_count; ei++)
                {
                    if (ei == target_enemy) continue;
                    if (!cs->enemies[ei].def || cs->enemies[ei].hp <= 0) continue;
                    if (!enemy_has_status(cs, ei, STATUS_CONDUCTIVE)) continue;
                    card_hp_damage_dealt += apply_damage_to_enemy(cs, ei, dmg / 2);
                    combat_feed_add(cs, "[CONDUCTIVE] Shadow Strike chained");
                }
            }
            if (card_is(card, "shm_chain_lightning"))
            {
                int jumps = 0;
                for (int ei = 0; ei < cs->enemy_count && jumps < 2; ei++)
                {
                    if (ei == target_enemy) continue;
                    if (!cs->enemies[ei].def || cs->enemies[ei].hp <= 0) continue;
                    card_hp_damage_dealt += apply_damage_to_enemy(cs, ei, dmg);
                    apply_status_to_enemy(cs, ei, STATUS_CONDUCTIVE, 2, 1);
                    jumps++;
                }
                if (jumps > 0)
                    combat_feed_add(cs, "[CONDUCTIVE] Chain Lightning jumped");
            }
            if (arcane_assault)
                apply_status_to_enemy(cs, target_enemy, STATUS_BURNING, 3, 2);
            if (dark_refrain)
                apply_status_to_enemy(cs, target_enemy, STATUS_BLIGHT, 3, 2);
        }

        if (arcane_assault)
        {
            combo_prime_clear(cs);
            combat_feed_add(cs, "Arcane Assault: Burning applied");
        }
        if (dark_refrain)
        {
            combo_prime_clear(cs);
            combat_feed_add(cs, "Dark Refrain: BLIGHT applied");
        }

    if (card_instance_has_interrupt(inst) && target_enemy >= 0)
        interrupt_enemy(cs, target_enemy);

    if (card->target == TARGET_ALL_ALLIES)
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
            {
                if (hl > 0) apply_heal_to_ally(cs, i, hl);
                if (sh > 0) apply_shield_to_ally(cs, i, sh);
            }
    }
    else if (card->target == TARGET_SELF)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            if (hl > 0) apply_heal_to_ally(cs, caster, hl);
            if (sh > 0) apply_shield_to_ally(cs, caster, sh);
        }
    }
    else if (target_ally >= 0)
    {
        if (hl > 0) apply_heal_to_ally(cs, target_ally, hl);
        if (sh > 0) apply_shield_to_ally(cs, target_ally, sh);
    }
    else
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            if (sh > 0) apply_shield_to_ally(cs, caster, sh);
            if (hl > 0 && card->heal_self) apply_heal_to_ally(cs, caster, hl);
        }
    }

    if (card_instance_has_taunt(inst))
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            int highest = 0;
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive && cs->party.members[i].aggro > highest)
                    highest = cs->party.members[i].aggro;
            set_party_member_aggro(&cs->party.members[caster], highest + 30);
            for (int ei = 0; ei < cs->enemy_count; ei++)
            {
                EnemyState *enemy = &cs->enemies[ei];
                if (!enemy->def || enemy->hp <= 0) continue;
                if (enemy->tethered_ally >= 0 && enemy->tether_transferrable)
                {
                    enemy->tethered_ally = caster;
                    if (enemy->tether_turns < 1) enemy->tether_turns = 1;
                    combat_feed_add(cs, "Tether moved to %s", cs->party.members[caster].name);
                }
            }
            LOG_I(CAT_CARD, "Taunt: caster aggro set to %d", cs->party.members[caster].aggro);
            assets_play_sfx(SFX_TAUNT);
            int taunt_shield = combat_class_perk_effect_total(cs, CLASS_GUARDIAN, "first_taunt_shield");
            if (!cs->guardian_taunt_shield_used && taunt_shield > 0)
            {
                cs->guardian_taunt_shield_used = true;
                apply_shield_to_ally(cs, caster, taunt_shield);
                combat_feed_add(cs, "%s: +%d Shield",
                    combat_class_perk_effect_name(cs, CLASS_GUARDIAN, "first_taunt_shield"),
                    taunt_shield);
            }
        }
    }

    apply_card_effect_chain(cs, card, target_enemy, target_ally);

    if (card->aggro_self > 0)
        add_aggro_to_caster(cs, card->class, card->aggro_self);

    if (card->heal2 > 0 && target_ally >= 0)
    {
        int second_hp = 99999;
        int second = -1;
        for (int i = 0; i < cs->party.count; i++)
        {
            if (i != target_ally && cs->party.members[i].alive && cs->party.members[i].hp < second_hp)
            {
                second_hp = cs->party.members[i].hp;
                second = i;
            }
        }
        if (second >= 0) apply_heal_to_ally(cs, second, card->heal2);
    }

    if (hl > 0 && dmg > 0 && card->target == TARGET_ENEMY && !card->heal_self)
    {
        int lowest = party_lowest_hp(&cs->party);
        if (lowest >= 0) apply_heal_to_ally(cs, lowest, hl);
    }

    if (card_is(card, "rog_backstab") && marked_target)
    {
        cs->energy.current += 1;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        rogue_mark_payoff = true;
        combat_feed_add(cs, "[MARKED] Backstab refunded 1 energy");
    }
    if (rogue_mark_payoff)
        combat_try_rogue_mark_refund(cs);
    if (extra_lowest_heal > 0)
    {
        int lowest = party_lowest_hp(&cs->party);
        if (lowest >= 0) apply_heal_to_ally(cs, lowest, extra_lowest_heal);
    }
    if (extra_caster_heal > 0)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0) apply_heal_to_ally(cs, caster, extra_caster_heal);
    }
    if (consume_marked && target_enemy >= 0)
    {
        remove_enemy_status(cs, target_enemy, STATUS_MARKED);
        ft_spawn((float)(cs->enemies[target_enemy].pos_x - 22), (float)(cs->enemies[target_enemy].pos_y - 35), "MARKED!", 10, (Color){ 245, 220, 75, 255 });
    }
    if (consume_blight && target_enemy >= 0)
    {
        remove_enemy_status(cs, target_enemy, STATUS_BLIGHT);
        blight_consumed_count++;
        ft_spawn((float)(cs->enemies[target_enemy].pos_x - 22), (float)(cs->enemies[target_enemy].pos_y - 35), "BLIGHT!", 10, (Color){ 190, 95, 230, 255 });
        if (party_has_pair(cs, CLASS_PALADIN, CLASS_WARLOCK))
        {
            int lowest = party_lowest_hp(&cs->party);
            if (lowest >= 0)
            {
                apply_heal_to_ally(cs, lowest, 4);
                combat_feed_add(cs, "Absolution healed lowest ally");
            }
        }
    }
    if (absolution)
    {
        int consumed = remove_all_enemy_status(cs, STATUS_BLIGHT) + blight_consumed_count;
        combo_prime_clear(cs);
        if (consumed > 0)
        {
            int heal = consumed * 3;
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    apply_heal_to_ally(cs, i, heal);
            combat_feed_add(cs, "Absolution consumed %d BLIGHT", consumed);
        }
    }
    } // end if(!card->channel)

    // ── Musical Mend: Cleric+Bard — first heal each turn draws 1 card ──
    if (!cs->musical_mend_used && hl > 0 && party_has_pair(cs, CLASS_CLERIC, CLASS_BARD))
    {
        deck_draw(&cs->deck);
        cs->musical_mend_used = true;
        combat_feed_add(cs, "Musical Mend: drew 1");
    }

    // ── Sneaky Steal: Rogue+Paladin — all damage 10% lifesteal ──
    if (have_sneaky_steal && card_hp_damage_dealt > 0)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            int steal = card_hp_damage_dealt / 10;
            if (steal < 1) steal = 1;
            apply_heal_to_ally(cs, caster, steal);
            combat_feed_add(cs, "Sneaky Steal: healed %d", steal);
        }
    }

    // ── Card lifesteal: heal caster by percent of HP damage dealt ──
    int lifesteal_pct = card_instance_lifesteal(inst);
    if (lifesteal_pct > 0 && card_hp_damage_dealt > 0)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            int lifesteal = (card_hp_damage_dealt * lifesteal_pct + 99) / 100;
            if (lifesteal < 1) lifesteal = 1;
            apply_heal_to_ally(cs, caster, lifesteal);
            combat_feed_add(cs, "%s leeched %d HP", card->name, lifesteal);
        }
    }

    // ── Self-HP cost: caster takes damage as a cost ──
    if (card->self_hp_cost > 0)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            apply_damage_to_ally(cs, caster, card->self_hp_cost, card->name);
            combat_feed_add(cs, "%s cost %d HP", card->name, card->self_hp_cost);
        }
    }

    if (card->channel)
    {
        cs->channel_card = card;
        cs->channel_remaining = card->channel_turns;
        cs->channel_class = card->class;
        deck_exhaust_index(&cs->deck, hand_idx);
        LOG_I(CAT_CARD, "  %s starts channeling for %d turns!", card->name, card->channel_turns);
    }

    else if (card->consume)
    {
        deck_remove_card_by_uid(&cs->deck, played_uid);
        deck_remove_card_by_uid(&g_state.run_deck, played_uid);
        combat_feed_add(cs, "%s consumed", card->name);
    }
    else if (card_instance_has_fleeting(inst))
        deck_exhaust_index(&cs->deck, hand_idx);
    else if (card_instance_has_exhaust(inst) || card->exhaust)
        deck_exhaust_index(&cs->deck, hand_idx);
    else
        deck_discard_index(&cs->deck, hand_idx);

    if (card->class != CLASS_NONE)
    {
        combo_check_chain(cs, previous_class, card->class);
        cs->last_played_class = card->class;
    }

    if (echo_this_card)
    {
        combat_schedule_echo(cs, card, upgrade_level, target_enemy, target_ally);
        echo_this_card = false;
    }

    // ── Echo: resolve again at 50% if flagged ──
    if (false && echo_this_card)
    {
        int half_dmg = card_damage(card, upgrade_level) / 2;
        int half_hl = card_heal(card, upgrade_level) / 2;
        int half_sh = card_shield(card, upgrade_level) / 2;
        int echo_target_enemy = -1;
        int echo_target_ally = -1;
        bool resolved = false;

        if (card->target == TARGET_ENEMY || card->target == TARGET_ALL_ENEMIES)
        {
            int alive[MAX_ENEMIES], alive_count = 0;
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                    alive[alive_count++] = i;
            if (alive_count > 0)
                echo_target_enemy = alive[rand() % alive_count];
        }
        else if (card->target == TARGET_ALLY || card->target == TARGET_ALL_ALLIES)
        {
            int alive[MAX_PARTY_SIZE], alive_count = 0;
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    alive[alive_count++] = i;
            if (alive_count > 0)
                echo_target_ally = alive[rand() % alive_count];
        }

        if (half_dmg > 0)
        {
            if (card->target == TARGET_ALL_ENEMIES)
            {
                for (int i = 0; i < cs->enemy_count; i++)
                    if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                        apply_damage_to_enemy(cs, i, half_dmg);
                resolved = true;
            }
            else if (echo_target_enemy >= 0)
            {
                apply_damage_to_enemy(cs, echo_target_enemy, half_dmg);
                resolved = true;
            }
        }

        if (card->target == TARGET_ALL_ALLIES)
        {
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                {
                    if (half_hl > 0) apply_heal_to_ally(cs, i, half_hl);
                    if (half_sh > 0) apply_shield_to_ally(cs, i, half_sh);
                    resolved = resolved || half_hl > 0 || half_sh > 0;
                }
        }
        else if (card->target == TARGET_ALLY && echo_target_ally >= 0)
        {
            if (half_hl > 0) apply_heal_to_ally(cs, echo_target_ally, half_hl);
            if (half_sh > 0) apply_shield_to_ally(cs, echo_target_ally, half_sh);
            resolved = resolved || half_hl > 0 || half_sh > 0;
        }
        else
        {
            int caster = -1;
            find_caster(cs, card->class, &caster);
            if (caster >= 0)
            {
                if (half_hl > 0) apply_heal_to_ally(cs, caster, half_hl);
                if (half_sh > 0) apply_shield_to_ally(cs, caster, half_sh);
                resolved = resolved || half_hl > 0 || half_sh > 0;
            }
        }

        if (resolved)
        {
            LOG_D(CAT_CARD, "Echo: resolving copy at 50%%");
            combat_feed_add(cs, "Echo copy resolves");
        }
    }

    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        EnemyState *enemy = &cs->enemies[ei];
        if (!enemy->def || enemy->hp <= 0) continue;
        boss_add_accumulator(cs, enemy, enemy->def->gain_on_player_card_play, "charge");
        if (card->type == CARD_ATTACK || card_hp_damage_dealt > 0)
            boss_add_accumulator(cs, enemy, enemy->def->gain_on_player_attack, "charge");
        if (card->type == CARD_SKILL || card->type == CARD_POWER)
            boss_add_accumulator(cs, enemy, enemy->def->gain_on_player_skill, "charge");
        if (sh > 0)
            boss_add_accumulator(cs, enemy, enemy->def->gain_on_player_shield, "charge");
        if (hl > 0)
            boss_add_accumulator(cs, enemy, enemy->def->gain_on_player_heal, "charge");
    }
    boss_trigger_reactions(cs, REACTION_ON_CARD_PLAYED, card, -1);
    if (card->type == CARD_ATTACK || card_hp_damage_dealt > 0)
        boss_trigger_reactions(cs, REACTION_ON_PLAYER_ATTACK, card, -1);
    if (card->type == CARD_SKILL || card->type == CARD_POWER)
        boss_trigger_reactions(cs, REACTION_ON_PLAYER_SKILL, card, -1);
    if (sh > 0)
        boss_trigger_reactions(cs, REACTION_ON_PLAYER_SHIELD, card, -1);
    if (hl > 0)
        boss_trigger_reactions(cs, REACTION_ON_PLAYER_HEAL, card, -1);

    cs->resolving_card_class = CLASS_NONE;
    cs->resolving_ally_idx = -1;
}

// ── Check win/loss ──────────────────────────────────────────

static void check_victory(CombatState *cs)
{
    if (cs->echo_pending) return;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].def && cs->enemies[i].hp > 0) return;
    cs->phase = COMBAT_VICTORY;
    assets_play_sfx(SFX_VICTORY);
    assets_stop_music();
    strcpy(cs->result_message, "VICTORY! Click to continue.");
    combat_feed_add(cs, "Victory");
    if (g_state.run_best_combat_turns <= 0 || cs->turn < g_state.run_best_combat_turns)
        g_state.run_best_combat_turns = cs->turn;

    // Gold popup immediately on victory
    if (!cs->gold_spawned)
    {
        int gold_gain = g_state.encounter_is_boss ? 50 : (g_state.encounter_is_elite ? 25 : 10);
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_GILDED_CHARM))
            gold_gain += 8;
        if ((g_state.encounter_is_elite || g_state.encounter_is_boss) &&
            relic_has(g_state.relics, g_state.relic_count, RELIC_VICTORY_PURSE))
            gold_gain += 5;
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_HOARDERS_SCALES))
            gold_gain += g_state.gold / 20;
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_RABBIT_FOOT) && (rand() % 10) == 0)
            gold_gain *= 2;
        cs->gold_reward = gold_gain;
        assets_play_sfx(SFX_GOLD_PICKUP);
        ft_spawn_gold(gold_gain);
        cs->gold_spawned = true;
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_LEECH_BLADE))
    {
        int lowest = party_lowest_hp(&cs->party);
        if (lowest >= 0)
        {
            apply_heal_to_ally(cs, lowest, 5);
            combat_feed_add(cs, "Leech Blade: healed lowest ally");
        }
    }

    LOG_I(CAT_COMBAT, "=== VICTORY ===");
}

static void check_defeat(CombatState *cs)
{
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive) return;
    cs->phase = COMBAT_DEFEAT;
    assets_play_sfx(SFX_DEFEAT);
    assets_stop_music();
    strcpy(cs->result_message, "Party wiped. Click to continue.");
    combat_feed_add(cs, "Party wiped");
    LOG_I(CAT_COMBAT, "=== DEFEAT ===");
}

// ── Enemy AI ────────────────────────────────────────────────

static int boss_party_status_stacks(CombatState *cs, int ally_idx, StatusType status)
{
    if (!cs || ally_idx < 0 || ally_idx >= cs->party.count) return 0;
    PartyMember *pm = &cs->party.members[ally_idx];
    int idx = status_find(pm->statuses, pm->status_count, status);
    return idx >= 0 ? pm->statuses[idx].value : 0;
}

static int boss_consume_party_status(CombatState *cs, int ally_idx, StatusType status, bool clear)
{
    if (!cs || ally_idx < 0 || ally_idx >= cs->party.count) return 0;
    PartyMember *pm = &cs->party.members[ally_idx];
    int idx = status_find(pm->statuses, pm->status_count, status);
    if (idx < 0) return 0;
    int stacks = pm->statuses[idx].value;
    if (clear)
    {
        for (int i = idx; i < pm->status_count - 1; i++)
            pm->statuses[i] = pm->statuses[i + 1];
        pm->status_count--;
    }
    return stacks;
}

static void boss_double_party_status(CombatState *cs, StatusType status)
{
    if (!cs || status == STATUS_NONE) return;
    bool any = false;
    for (int i = 0; i < cs->party.count; i++)
    {
        PartyMember *pm = &cs->party.members[i];
        if (!pm->alive) continue;
        int idx = status_find(pm->statuses, pm->status_count, status);
        if (idx >= 0)
        {
            pm->statuses[idx].value *= 2;
            any = true;
        }
    }
    if (any)
        combat_feed_add(cs, "Statuses festered");
}

static void boss_detonate_ally(CombatState *cs, EnemyState *e, const EnemyCardDef *cd, int ally_idx)
{
    if (!cs || !e || !cd || cd->detonate_status == STATUS_NONE) return;
    if (ally_idx < 0 || ally_idx >= cs->party.count || !cs->party.members[ally_idx].alive) return;
    int stacks = boss_consume_party_status(cs, ally_idx, cd->detonate_status, cd->detonate_clear);
    if (stacks <= 0) return;
    if (cd->detonate_mult > 0)
        apply_damage_to_ally(cs, ally_idx, stacks * cd->detonate_mult, e->def ? e->def->name : "Detonate");
    if (cd->detonate_heal_mult > 0)
        apply_heal_to_enemy(cs, (int)(e - cs->enemies), stacks * cd->detonate_heal_mult);
    if (cd->detonate_shield_mult > 0)
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), stacks * cd->detonate_shield_mult);
    combat_feed_add(cs, "%s detonated %d stacks", e->def ? e->def->name : "Enemy", stacks);
}

static void boss_apply_detonation(CombatState *cs, EnemyState *e, const EnemyCardDef *cd, int target_ally)
{
    if (!cd || cd->detonate_status == STATUS_NONE) return;
    if (cd->detonate_target == DETONATE_ALL || cd->target == ENEMY_TARGET_ALL || cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE)
    {
        for (int i = 0; i < cs->party.count; i++)
            boss_detonate_ally(cs, e, cd, i);
    }
    else
    {
        if (target_ally < 0 || target_ally >= cs->party.count || !cs->party.members[target_ally].alive)
            target_ally = party_highest_aggro(&cs->party);
        boss_detonate_ally(cs, e, cd, target_ally);
    }
}

static int boss_pick_tether_target(CombatState *cs, const EnemyCardDef *cd)
{
    if (!cs || !cd) return -1;
    if (cd->tether_target == TETHER_LAST_ATTACKER && cs->last_player_attacker >= 0 &&
        cs->last_player_attacker < cs->party.count && cs->party.members[cs->last_player_attacker].alive)
        return cs->last_player_attacker;
    if (cd->tether_target == TETHER_LOWEST_HP)
        return party_lowest_hp(&cs->party);
    if (cd->tether_target == TETHER_LOWEST_SHIELD)
    {
        int best = -1;
        int shield = 99999;
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive && cs->party.members[i].shield < shield)
            {
                shield = cs->party.members[i].shield;
                best = i;
            }
        return best;
    }
    return party_random_alive(&cs->party);
}

static void boss_apply_tether(CombatState *cs, EnemyState *e, const EnemyCardDef *cd)
{
    if (!cs || !e || !cd || !cd->tether) return;
    int target = boss_pick_tether_target(cs, cd);
    if (target < 0) return;
    int highest = 0;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive && cs->party.members[i].aggro > highest)
            highest = cs->party.members[i].aggro;
    e->tethered_ally = target;
    e->tether_turns = cd->tether_duration > 0 ? cd->tether_duration : 1;
    e->tether_transferrable = cd->tether_transferrable;
    set_party_member_aggro(&cs->party.members[target], highest + 50);
    combat_feed_add(cs, "%s tethered %s", e->def->name, cs->party.members[target].name);
}

static void boss_add_curse_card(CombatState *cs, const char *card_id, CursePileType pile)
{
    if (!cs || !card_id || !card_id[0]) return;
    const CardDef *curse = card_def_by_id(card_id);
    if (!curse) return;
    int before_count = cs->deck.card_count;
    deck_add_card(&cs->deck, curse);
    if (cs->deck.card_count <= before_count || cs->deck.draw_count <= 0) return;
    int card_idx = cs->deck.draw[--cs->deck.draw_count];
    if (pile == CURSE_PILE_HAND && cs->deck.hand_count < MAX_HAND_SIZE)
        cs->deck.hand[cs->deck.hand_count++] = card_idx;
    else if (pile == CURSE_PILE_DISCARD && cs->deck.discard_count < MAX_DECK_SIZE)
        cs->deck.discard[cs->deck.discard_count++] = card_idx;
    else
        cs->deck.draw[cs->deck.draw_count++] = card_idx;
}

static void boss_shuffle_curses(CombatState *cs, const EnemyCardDef *cd)
{
    if (!cs || !cd || cd->curse_count <= 0) return;
    const char *curse_id = cd->curse_card_id && cd->curse_card_id[0] ? cd->curse_card_id : "dazed";
    for (int i = 0; i < cd->curse_count; i++)
        boss_add_curse_card(cs, curse_id, cd->curse_pile);
    combat_feed_add(cs, "Cursed deck: +%d %s", cd->curse_count, curse_id);
}

static void boss_exhaust_from_draw(CombatState *cs, int count)
{
    if (!cs || count <= 0) return;
    int exhausted = 0;
    for (int i = 0; i < count && cs->deck.draw_count > 0 && cs->deck.exhaust_count < MAX_DECK_SIZE; i++)
    {
        int pick = rand() % cs->deck.draw_count;
        int card_idx = cs->deck.draw[pick];
        cs->deck.draw[pick] = cs->deck.draw[--cs->deck.draw_count];
        cs->deck.exhaust[cs->deck.exhaust_count++] = card_idx;
        exhausted++;
    }
    if (exhausted > 0)
        combat_feed_add(cs, "Exhausted %d cards from draw", exhausted);
}

static void boss_force_discard(CombatState *cs, int count)
{
    if (!cs || count <= 0) return;
    int discarded = 0;
    for (int i = 0; i < count && cs->deck.hand_count > 0; i++)
    {
        int pick = rand() % cs->deck.hand_count;
        deck_discard_index(&cs->deck, pick);
        discarded++;
    }
    if (discarded > 0)
        combat_feed_add(cs, "Discarded %d cards", discarded);
}

static void boss_steal_one_card(CombatState *cs, int owner_enemy, int return_turns, bool use_against_party)
{
    if (!cs || cs->deck.hand_count <= 0) return;
    int slot = -1;
    for (int i = 0; i < MAX_STOLEN_CARDS; i++)
        if (!cs->stolen_cards[i].active) { slot = i; break; }
    if (slot < 0) return;

    int hand_idx = rand() % cs->deck.hand_count;
    int card_idx = cs->deck.hand[hand_idx];
    for (int i = hand_idx; i < cs->deck.hand_count - 1; i++)
        cs->deck.hand[i] = cs->deck.hand[i + 1];
    cs->deck.hand_count--;

    StolenCardState *stolen = &cs->stolen_cards[slot];
    stolen->active = true;
    stolen->card_idx = card_idx;
    stolen->return_turns = return_turns > 0 ? return_turns : 2;
    stolen->use_against_party = use_against_party;
    stolen->owner_enemy = owner_enemy;
    const CardDef *card = cs->deck.cards[card_idx].def;
    combat_feed_add(cs, "Stole %s", card ? card->name : "a card");
}

static void boss_return_stolen_card(CombatState *cs, int slot)
{
    if (!cs || slot < 0 || slot >= MAX_STOLEN_CARDS || !cs->stolen_cards[slot].active) return;
    int card_idx = cs->stolen_cards[slot].card_idx;
    if (card_idx >= 0 && card_idx < cs->deck.card_count && cs->deck.discard_count < MAX_DECK_SIZE)
        cs->deck.discard[cs->deck.discard_count++] = card_idx;
    cs->stolen_cards[slot].active = false;
}

static void boss_use_stolen_cards_against_party(CombatState *cs, int owner_enemy)
{
    if (!cs) return;
    for (int i = 0; i < MAX_STOLEN_CARDS; i++)
    {
        StolenCardState *stolen = &cs->stolen_cards[i];
        if (!stolen->active || !stolen->use_against_party) continue;
        if (owner_enemy >= 0 && stolen->owner_enemy != owner_enemy) continue;
        if (stolen->card_idx < 0 || stolen->card_idx >= cs->deck.card_count) continue;
        const CardInstance *inst = &cs->deck.cards[stolen->card_idx];
        const CardDef *card = inst->def;
        if (!card) continue;
        int damage = card_damage(card, inst->upgrade_level);
        int shield = card_shield(card, inst->upgrade_level);
        int heal = card_heal(card, inst->upgrade_level);
        if (damage > 0)
        {
            if (card->target == TARGET_ALL_ENEMIES || card->target == TARGET_ALL_ALLIES)
            {
                for (int a = 0; a < cs->party.count; a++)
                    if (cs->party.members[a].alive)
                        apply_damage_to_ally(cs, a, damage, card->name);
            }
            else
            {
                int target = party_highest_aggro(&cs->party);
                if (target >= 0)
                    apply_damage_to_ally(cs, target, damage, card->name);
            }
        }
        if (shield > 0 && owner_enemy >= 0)
            apply_shield_to_enemy(cs, owner_enemy, shield);
        if (heal > 0 && owner_enemy >= 0)
            apply_heal_to_enemy(cs, owner_enemy, heal);
        combat_feed_add(cs, "Echoed stolen %s", card->name);
        boss_return_stolen_card(cs, i);
    }
}

static void boss_apply_enemy_card_side_effects(CombatState *cs, EnemyState *e, const EnemyCardDef *cd, int target_ally)
{
    if (!cs || !e || !e->def || !cd) return;
    int enemy_idx = (int)(e - cs->enemies);

    if (cd->set_invulnerable)
    {
        e->invulnerable = true;
        e->invulnerable_until = cd->invulnerable_until;
        e->invulnerable_turns = cd->invulnerable_turns;
        e->invulnerable_clear_damage = cd->invulnerable_clear_on_damage;
        e->invulnerable_pending_damage = 0;
        combat_feed_add(cs, "%s became invulnerable", e->def->name);
    }
    if (cd->reflect_pct > 0)
    {
        e->reflect_pct = cd->reflect_pct;
        e->reflect_type = cd->reflect_type;
        e->reflect_turns = cd->reflect_turns > 0 ? cd->reflect_turns : 1;
        e->reflect_cap = cd->reflect_cap;
        combat_feed_add(cs, "%s raised a reflect ward", e->def->name);
    }
    if (cd->summon_count > 0 && cd->summon_id && cd->summon_id[0])
        combat_spawn_enemy(cs, cd->summon_id, cd->summon_count);
    if (cd->gate_summon_count > 0 && cd->gate_summon_id && cd->gate_summon_id[0])
        combat_spawn_enemy(cs, cd->gate_summon_id, cd->gate_summon_count);
    if (cd->gate_aura.type != AURA_NONE)
        boss_add_arena_effect(cs, cd->gate_aura, enemy_idx);
    if (cd->apply_arena_aura.type != AURA_NONE)
        boss_add_arena_effect(cs, cd->apply_arena_aura, enemy_idx);
    if (cd->curse_count > 0)
        boss_shuffle_curses(cs, cd);
    if (cd->steal_card)
        boss_steal_one_card(cs, enemy_idx, cd->steal_return_turns, true);
    if (cd->steal_use_against)
        boss_use_stolen_cards_against_party(cs, enemy_idx);
    if (cd->exhaust_from_deck > 0)
        boss_exhaust_from_draw(cs, cd->exhaust_from_deck);
    if (cd->force_discard > 0)
        boss_force_discard(cs, cd->force_discard);
    if (cd->increase_costs != 0)
    {
        cs->temp_card_cost_delta += cd->increase_costs;
        cs->temp_card_cost_turns = 1;
        combat_feed_add(cs, "Card costs changed by %+d", cd->increase_costs);
    }
    if (cd->decrease_hand_size > 0)
    {
        cs->temp_hand_size_delta += cd->decrease_hand_size;
        cs->temp_hand_size_turns = 1;
        combat_feed_add(cs, "Hand size reduced by %d", cd->decrease_hand_size);
    }
    if (cd->set_player_energy_zero && cs->energy.current > 0)
    {
        cs->player_energy_drained_this_turn += cs->energy.current;
        cs->energy.current = 0;
        combat_feed_add(cs, "Energy set to 0");
    }
    if (cd->double_status_on_party)
        boss_double_party_status(cs, cd->double_status);
    boss_apply_tether(cs, e, cd);
    boss_apply_detonation(cs, e, cd, target_ally);
}

static void boss_apply_cast_stage_effects(CombatState *cs, EnemyState *e, const EnemyCardDef *cd, int stage)
{
    if (!cs || !e || !cd || stage <= 0) return;
    int enemy_idx = (int)(e - cs->enemies);
    for (int i = 0; i < cd->stage_effect_count; i++)
    {
        const CastStageEffectDef *effect = &cd->stage_effects[i];
        if (effect->turn != stage || !effect->effect) continue;
        if (strcmp(effect->effect, "damage_tick") == 0)
        {
            for (int a = 0; a < cs->party.count; a++)
                if (cs->party.members[a].alive)
                    apply_damage_to_ally(cs, a, effect->value, e->def ? e->def->name : "Ritual");
        }
        else if (strcmp(effect->effect, "spawn_enemy") == 0)
        {
            const char *id = effect->enemy_id && effect->enemy_id[0] ? effect->enemy_id : cd->summon_id;
            int count = effect->count > 0 ? effect->count : 1;
            combat_spawn_enemy(cs, id, count);
        }
        else if (strcmp(effect->effect, "shield") == 0)
        {
            apply_shield_to_enemy(cs, enemy_idx, effect->value);
        }
        else if (strcmp(effect->effect, "heal") == 0)
        {
            apply_heal_to_enemy(cs, enemy_idx, effect->value);
        }
        combat_feed_add(cs, "%s ritual stage %d", e->def ? e->def->name : "Enemy", stage);
    }
}

static bool boss_reactive_condition_met(CombatState *cs, EnemyState *e, int card_idx, const char *condition)
{
    if (!cs || !e || !e->def || card_idx < 0 || card_idx >= e->def->card_count) return false;
    if (!condition || !condition[0] || strcmp(condition, "always") == 0) return true;
    if (strncmp(condition, "player_energy >=", 17) == 0)
    {
        int value = atoi(condition + 17);
        return cs->energy.current >= value;
    }
    if (strncmp(condition, "party_hp_pct <", 14) == 0)
    {
        int value = atoi(condition + 14);
        int hp = 0, max_hp = 0;
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
            {
                hp += cs->party.members[i].hp;
                max_hp += cs->party.members[i].max_hp;
            }
        return max_hp > 0 && hp * 100 / max_hp < value;
    }
    if (strncmp(condition, "status_count_on_party >", 23) == 0)
    {
        int value = atoi(condition + 23);
        int count = 0;
        for (int i = 0; i < cs->party.count; i++)
            count += cs->party.members[i].status_count;
        return count > value;
    }
    return false;
}

static void boss_resolve_reaction_card(CombatState *cs, int enemy_idx, int card_idx)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0 || card_idx < 0 || card_idx >= e->def->card_count) return;

    EnemyIntent saved_intent = e->intent;
    bool saved_pending = e->cast_pending;
    e->intent.ability_idx = card_idx;
    e->intent.remaining_turns = 0;
    e->cast_pending = false;
    combat_feed_add(cs, "%s reacts: %s", e->def->name, e->def->cards[card_idx].name);
    enemy_action(e, cs, -1, -1);
    e->intent = saved_intent;
    e->cast_pending = saved_pending;
}

static void boss_trigger_reactions(CombatState *cs, ReactionType trigger, const CardDef *card, int source_enemy)
{
    if (!cs || trigger == REACTION_NONE) return;
    if (cs->phase != COMBAT_PLAYER_TURN && trigger != REACTION_ON_TURN_END_ENERGY_UNSPENT)
        return;

    int best_enemy = -1;
    int best_card = -1;
    int best_priority = -9999;

    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        EnemyState *e = &cs->enemies[ei];
        if (!e->def || e->hp <= 0) continue;
        if (source_enemy >= 0 && source_enemy != ei && trigger == REACTION_ON_DAMAGE_TAKEN) continue;
        for (int ci = 0; ci < e->def->card_count; ci++)
        {
            const EnemyCardDef *cd = &e->def->cards[ci];
            if (cd->reaction_type != trigger) continue;
            if (cd->reaction_once && e->reaction_fired[ci]) continue;
            e->reaction_counts[ci]++;
            int required = cd->reaction_trigger > 0 ? cd->reaction_trigger : 1;
            if (e->reaction_counts[ci] < required)
            {
                combat_feed_add(cs, "%s stirs (%s %d/%d)", e->def->name, enemy_reaction_type_name(trigger), e->reaction_counts[ci], required);
                continue;
            }
            if (cd->reaction_priority > best_priority)
            {
                best_enemy = ei;
                best_card = ci;
                best_priority = cd->reaction_priority;
            }
        }

        if (e->def->ai_override == AI_REACTIVE && e->def->reactive_count > 0 && trigger == REACTION_ON_CARD_PLAYED)
        {
            for (int r = 0; r < e->def->reactive_count; r++)
            {
                int ci = e->def->reactive_cards[r].card_index;
                if (boss_reactive_condition_met(cs, e, ci, e->def->reactive_cards[r].condition))
                {
                    best_enemy = ei;
                    best_card = ci;
                    best_priority = 9999;
                    break;
                }
            }
        }
    }

    if (best_enemy >= 0 && best_card >= 0)
    {
        EnemyState *e = &cs->enemies[best_enemy];
        e->reaction_counts[best_card] = 0;
        e->reaction_fired[best_card] = true;
        boss_resolve_reaction_card(cs, best_enemy, best_card);
    }
    (void)card;
}

static void boss_sync_linked_group(CombatState *cs, int group_idx)
{
    if (!cs || group_idx < 0 || group_idx >= cs->linked_group_count) return;
    LinkedEnemyGroup *group = &cs->linked_groups[group_idx];
    if (!group->active || !group->shared_hp) return;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (!e->def || e->linked_group != group_idx) continue;
        e->max_hp = group->max_hp;
        e->hp = group->hp;
        if (e->hp <= 0)
        {
            e->hp = 0;
            e->intent.ability_idx = -1;
            e->intent.remaining_turns = 0;
        }
    }
}

static bool boss_linked_has_living_partner(const CombatState *cs, int enemy_idx)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count) return false;
    const EnemyState *src = &cs->enemies[enemy_idx];
    if (!src->def || src->linked_group < 0) return false;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        const EnemyState *other = &cs->enemies[i];
        if (i != enemy_idx && other->def && other->linked_group == src->linked_group && other->hp > 0)
            return true;
    }
    return false;
}

static bool boss_linked_group_has_living_member(const CombatState *cs, int group_idx)
{
    if (!cs || group_idx < 0) return false;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        const EnemyState *e = &cs->enemies[i];
        if (e->def && e->linked_group == group_idx && e->hp > 0)
            return true;
    }
    return false;
}

static void boss_cancel_linked_revives_if_group_down(CombatState *cs, int group_idx)
{
    if (!cs || group_idx < 0 || boss_linked_group_has_living_member(cs, group_idx)) return;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (e->def && e->linked_group == group_idx)
        {
            e->revive_timer = 0;
            e->revive_stagger_damage = 0;
        }
    }
}

static void boss_handle_linked_damage(CombatState *cs, int enemy_idx, int hp_damage)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count || hp_damage <= 0) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->linked_group < 0 || e->linked_group >= cs->linked_group_count) return;
    LinkedEnemyGroup *group = &cs->linked_groups[e->linked_group];
    if (!group->active || group->shared_hp) return;

    if (e->hp > 0)
    {
        for (int i = 0; i < cs->enemy_count; i++)
        {
            EnemyState *reviver = &cs->enemies[i];
            if (i == enemy_idx || !reviver->def || reviver->linked_group != e->linked_group) continue;
            if (reviver->revive_timer <= 0) continue;
            int stagger = reviver->def->linked_revive_stagger;
            if (stagger <= 0) continue;
            reviver->revive_stagger_damage += hp_damage;
            while (reviver->revive_stagger_damage >= stagger)
            {
                reviver->revive_stagger_damage -= stagger;
                reviver->revive_timer++;
                combat_feed_add(cs, "%s's revival delayed", reviver->def->name);
            }
        }
    }
    else if (e->def->linked_revive && e->revive_timer <= 0 && boss_linked_has_living_partner(cs, enemy_idx))
    {
        e->revive_timer = e->def->linked_revive_turns > 0 ? e->def->linked_revive_turns : 2;
        e->revive_stagger_damage = 0;
        e->cast_pending = false;
        e->intent.ability_idx = -1;
        e->intent.remaining_turns = 0;
        e->intent.stage_index = 0;
        combat_feed_add(cs, "%s will revive in %d", e->def->name, e->revive_timer);
    }
    boss_cancel_linked_revives_if_group_down(cs, e->linked_group);
}

static void boss_tick_linked_revives(CombatState *cs)
{
    if (!cs) return;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (!e->def || e->revive_timer <= 0) continue;
        if (!boss_linked_has_living_partner(cs, i))
        {
            e->revive_timer = 0;
            e->revive_stagger_damage = 0;
            continue;
        }
        e->revive_timer--;
        if (e->revive_timer > 0) continue;
        if (!boss_linked_has_living_partner(cs, i)) continue;
        int pct = e->def->linked_revive_hp > 0 ? e->def->linked_revive_hp : 35;
        int revive_hp = e->max_hp * pct / 100;
        if (revive_hp < 1) revive_hp = 1;
        e->hp = revive_hp;
        e->shield = 0;
        e->revive_stagger_damage = 0;
        e->cast_pending = false;
        e->intent.ability_idx = -1;
        e->intent.remaining_turns = 0;
        e->intent.stage_index = 0;
        ft_spawn((float)(e->pos_x - 18), (float)(e->pos_y - 18), "REVIVE", 10, (Color){ 140, 230, 170, 255 });
        combat_feed_add(cs, "%s revived", e->def->name);
    }
}

static void boss_remove_card_from_enemy_piles(EnemyState *e, int card_idx)
{
    if (!e || card_idx < 0) return;
    for (int i = e->hand_count - 1; i >= 0; i--)
        if (e->hand[i] == card_idx)
        {
            for (int j = i; j < e->hand_count - 1; j++) e->hand[j] = e->hand[j + 1];
            e->hand_count--;
        }
    for (int i = e->deck_count - 1; i >= 0; i--)
        if (e->deck[i] == card_idx)
            e->deck[i] = e->deck[--e->deck_count];
    for (int i = e->discard_count - 1; i >= 0; i--)
        if (e->discard[i] == card_idx)
            e->discard[i] = e->discard[--e->discard_count];
}

static void boss_apply_phase_transitions(CombatState *cs, int enemy_idx)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0 || e->max_hp <= 0) return;

    if (e->def->phase_count <= 0)
    {
        if (g_state.encounter_is_boss && e->phase == 0 && e->hp * 2 <= e->max_hp)
        {
            e->phase = 1;
            e->shield += 10;
            if (e->intent.ability_idx >= 0 && e->intent.remaining_turns > 1)
                e->intent.remaining_turns--;
            combat_feed_add(cs, "%s enraged", e->def->name);
        }
        return;
    }

    while (e->phase < e->def->phase_count)
    {
        const EnemyPhaseDef *phase = &e->def->phases[e->phase];
        int hp_pct = e->hp * 100 / e->max_hp;
        if (hp_pct > phase->trigger_hp_pct) break;

        e->phase++;
        e->shield += phase->gain_shield;
        e->energy_current += phase->gain_energy;
        if (phase->heal_pct > 0)
        {
            int heal = e->max_hp * phase->heal_pct / 100;
            apply_heal_to_enemy(cs, enemy_idx, heal);
        }
        if (phase->party_status != STATUS_NONE)
            for (int i = 0; i < cs->party.count; i++)
                apply_status_to_ally(cs, i, phase->party_status, phase->party_status_turns, phase->party_status_value);
        if (phase->set_invulnerable)
        {
            e->invulnerable = true;
            e->invulnerable_until = phase->invulnerable_turns > 0 ? INVULN_TURNS : INVULN_ALL_ADDS_DEAD;
            e->invulnerable_turns = phase->invulnerable_turns;
            combat_feed_add(cs, "%s became invulnerable", e->def->name);
        }
        if (phase->spawn_count > 0 && phase->spawn_id && phase->spawn_id[0])
            combat_spawn_enemy(cs, phase->spawn_id, phase->spawn_count);
        for (int i = 0; i < phase->aura_count; i++)
            boss_add_arena_effect(cs, phase->auras[i], enemy_idx);
        e->phase_energy_delta += phase->energy_per_turn_delta;
        e->energy_max += phase->energy_per_turn_delta;
        if (e->energy_max < 0) e->energy_max = 0;
        e->phase_hand_delta += phase->hand_size_delta;
        e->phase_damage_scale_delta += phase->damage_scale_delta;
        e->phase_cast_time_reduction += phase->cast_time_reduction;
        for (int i = 0; i < phase->new_card_count && e->discard_count < ENEMY_DECK_SIZE; i++)
            if (phase->new_cards[i] >= 0 && phase->new_cards[i] < e->def->card_count)
                e->discard[e->discard_count++] = phase->new_cards[i];
        for (int i = 0; i < phase->remove_card_count; i++)
            boss_remove_card_from_enemy_piles(e, phase->remove_cards[i]);
        if (e->def->threshold_reset == THRESHOLD_RESET_PER_PHASE)
            e->thresholds_triggered_mask = 0;
        combat_feed_add(cs, "%s entered phase %d", e->def->name, e->phase + 1);
    }
}

static void boss_on_enemy_hp_damage(CombatState *cs, int enemy_idx, int hp_damage)
{
    if (!cs || enemy_idx < 0 || enemy_idx >= cs->enemy_count || hp_damage <= 0) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def) return;

    boss_add_accumulator(cs, e, e->def->gain_on_boss_attacked, "charge");
    for (int i = 0; i < cs->enemy_count; i++)
        if (i != enemy_idx && cs->enemies[i].def && cs->enemies[i].hp > 0)
            boss_add_accumulator(cs, &cs->enemies[i], cs->enemies[i].def->gain_on_ally_damaged, "charge");

    if (cs->resolving_ally_idx >= 0)
    {
        for (int a = 0; a < cs->arena_effect_count; a++)
        {
            ArenaEffect *aura = &cs->arena_effects[a];
            if (aura->type == AURA_NONE || aura->clear_on_damage_threshold <= 0) continue;
            aura->damage_this_turn += hp_damage;
            if (aura->damage_this_turn >= aura->clear_on_damage_threshold)
            {
                aura->disrupt_turns = 1;
                aura->damage_this_turn = 0;
                combat_feed_add(cs, "%s aura faltered", enemy_arena_aura_name(aura->type));
            }
        }
    }

    e->turn_damage_received += hp_damage;
    for (int i = 0; i < e->def->damage_threshold_count && i < 31; i++)
    {
        if (e->thresholds_triggered_mask & (1u << (unsigned int)i)) continue;
        const DamageThresholdDef *threshold = &e->def->damage_thresholds[i];
        if (threshold->threshold <= 0 || e->turn_damage_received < threshold->threshold) continue;
        e->thresholds_triggered_mask |= 1u << (unsigned int)i;
        if (threshold->gain_shield > 0) apply_shield_to_enemy(cs, enemy_idx, threshold->gain_shield);
        if (threshold->heal > 0) apply_heal_to_enemy(cs, enemy_idx, threshold->heal);
        if (threshold->gain_energy > 0) e->energy_current += threshold->gain_energy;
        if (threshold->clear_debuffs) status_clear(e->statuses, &e->status_count);
        if (threshold->retaliate > 0)
        {
            int target = cs->resolving_ally_idx >= 0 ? cs->resolving_ally_idx : party_random_alive(&cs->party);
            if (target >= 0)
                apply_damage_to_ally(cs, target, threshold->retaliate, e->def->name);
        }
        if (threshold->apply_status != STATUS_NONE)
        {
            int target = cs->resolving_ally_idx >= 0 ? cs->resolving_ally_idx : party_random_alive(&cs->party);
            if (target >= 0)
                apply_status_to_ally(cs, target, threshold->apply_status, threshold->status_turns, threshold->status_value);
        }
        combat_feed_add(cs, "%s crossed a damage threshold", e->def->name);
    }

    boss_trigger_reactions(cs, REACTION_ON_DAMAGE_TAKEN, NULL, enemy_idx);
    boss_apply_phase_transitions(cs, enemy_idx);
}

static void boss_clear_invulnerability_if_ready(CombatState *cs)
{
    if (!cs) return;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (!e->def || e->hp <= 0 || !e->invulnerable) continue;
        if (e->invulnerable_until == INVULN_TURNS && e->invulnerable_turns <= 0)
        {
            e->invulnerable = false;
            e->invulnerable_until = INVULN_NONE;
            combat_feed_add(cs, "%s is vulnerable", e->def->name);
        }
        else if (e->invulnerable_until == INVULN_ALL_ADDS_DEAD)
        {
            bool any_add = false;
            for (int j = 0; j < cs->enemy_count; j++)
                if (j != i && cs->enemies[j].def && cs->enemies[j].hp > 0)
                    any_add = true;
            if (!any_add)
            {
                e->invulnerable = false;
                e->invulnerable_until = INVULN_NONE;
                boss_remove_arena_effects_from_source(cs, i);
                combat_feed_add(cs, "%s is vulnerable", e->def->name);
            }
        }
    }
}

static void enemy_action(EnemyState *e, CombatState *cs, int target_enemy, int target_ally)
{
    e->cast_pending = false;
    if (!e->def) return;
    int ci = e->intent.ability_idx;
    if (ci < 0 || ci >= e->def->card_count) return;

    const EnemyCardDef *cd = &e->def->cards[ci];
    int damage = boss_enemy_damage_value(cs, e, cd);
    int spent_accumulator = 0;
    if (e->def->accumulate && (cd->spend_all || cd->spend > 0))
    {
        spent_accumulator = cd->spend_all ? e->accumulator : cd->spend;
        if (spent_accumulator > e->accumulator) spent_accumulator = e->accumulator;
        if (spent_accumulator < 0) spent_accumulator = 0;
        e->accumulator -= spent_accumulator;
        if (spent_accumulator > 0)
            combat_feed_add(cs, "%s spent %d charge", e->def->name, spent_accumulator);
    }
    if (spent_accumulator > 0 && cd->damage_per_spent > 0)
        damage += spent_accumulator * cd->damage_per_spent;

    boss_apply_enemy_card_side_effects(cs, e, cd, target_ally);
    bool attacking_card = cd->intent == INTENT_ATTACK || cd->intent == INTENT_TANK_BUSTER ||
        cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE;
    if (e->def->decay_on_defend && !attacking_card)
        boss_add_accumulator(cs, e, -1, "");

    if (cd->intent == INTENT_HEAL)
    {
        int heal = cd->heal_amount + spent_accumulator * cd->heal_per_spent;
        if (target_enemy >= 0) apply_heal_to_enemy(cs, target_enemy, heal);
        return;
    }

    if (cd->intent == INTENT_SHIELD || cd->intent == INTENT_BUFF)
    {
        int shield = cd->shield_amount + spent_accumulator * cd->shield_per_spent;
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), shield);
        if (cd->status != STATUS_NONE && cd->status_turns > 0)
            status_apply(e->statuses, &e->status_count, cd->status, cd->status_turns, cd->status_amount + spent_accumulator * cd->status_per_spent);
        if (cd->enrage_allies)
        {
            for (int i = 0; i < cs->enemy_count; i++)
                if (i != (int)(e - cs->enemies) && cs->enemies[i].def && cs->enemies[i].hp > 0)
                    apply_shield_to_enemy(cs, i, shield);
        }
        return;
    }

    // ── BUFF ATTACK: increase enemy damage output ──
    if (cd->intent == INTENT_BUFF_ATTACK)
    {
        float mult = 1.0f + (float)cd->buff_damage / 100.0f;
        cs->enemy_damage_buff_scale *= mult;
        if (cd->buff_turns > cs->enemy_damage_buff_turns)
            cs->enemy_damage_buff_turns = cd->buff_turns;
        combat_feed_add(cs, "%s buffed damage x%.2f", e->def->name, mult);
        return;
    }

    // ── AOE SHIELD: shields all living enemies ──
    if (cd->intent == INTENT_AOE_SHIELD)
    {
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                apply_shield_to_enemy(cs, i, cd->shield_amount);
        check_defeat(cs);
        return;
    }

    if (cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE)
    {
        int total_damage = 0;
        for (int i = 0; i < cs->party.count; i++)
        {
            apply_damage_to_ally(cs, i, damage, e->def->name);
            if (cd->status != STATUS_NONE && cd->status_turns > 0)
                apply_status_to_ally(cs, i, cd->status, cd->status_turns, cd->status_amount + spent_accumulator * cd->status_per_spent);
            total_damage += damage;
        }
        if (cd->lifesteal_pct > 0)
        {
            int lifesteal = total_damage * cd->lifesteal_pct / 100;
            if (lifesteal > 0 && e->hp > 0)
            {
                apply_heal_to_enemy(cs, (int)(e - cs->enemies), lifesteal);
                combat_feed_add(cs, "%s leeched %d HP", e->def->name, lifesteal);
            }
        }
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET))
            apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);
        apply_enemy_interrupt_effects(cs, e, cd, target_ally);
        check_defeat(cs);
        return;
    }

    int repeats = cd->repeats;
    if (repeats < 1) repeats = 1;

    for (int hit = 0; hit < repeats; hit++)
    {
        int target = target_ally;
        if (target < 0 || target >= cs->party.count || !cs->party.members[target].alive)
            target = party_highest_aggro(&cs->party);
        if (target < 0) break;
        apply_damage_to_ally(cs, target, damage, e->def->name);
        if (hit == 0 && cd->status != STATUS_NONE && cd->status_turns > 0)
            apply_status_to_ally(cs, target, cd->status, cd->status_turns, cd->status_amount + spent_accumulator * cd->status_per_spent);
    }
    if (cd->lifesteal_pct > 0 && damage > 0 && e->hp > 0)
    {
        int lifesteal = damage * repeats * cd->lifesteal_pct / 100;
        if (lifesteal > 0)
        {
            apply_heal_to_enemy(cs, (int)(e - cs->enemies), lifesteal);
            combat_feed_add(cs, "%s leeched %d HP", e->def->name, lifesteal);
        }
    }
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET) && damage > 0)
        apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);

    if ((cd->shield_amount > 0 || (spent_accumulator > 0 && cd->shield_per_spent > 0)) && e->hp > 0)
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), cd->shield_amount + spent_accumulator * cd->shield_per_spent);

    if ((cd->heal_amount > 0 || (spent_accumulator > 0 && cd->heal_per_spent > 0)) && e->hp > 0)
        apply_heal_to_enemy(cs, (int)(e - cs->enemies), cd->heal_amount + spent_accumulator * cd->heal_per_spent);

    // ── Self-damage (sacrifice HP for power) ──
    if (cd->self_damage > 0 && e->hp > 0)
    {
        int sd = (int)(cd->self_damage * cs->floor_scale);
        e->hp -= sd;
        if (e->hp < 0) e->hp = 0;
        combat_feed_add(cs, "%s sacrificed %d HP", e->def->name, sd);
    }

    // ── Interrupt player channeling/combo ──
    apply_enemy_interrupt_effects(cs, e, cd, target_ally);

    // ── Enrage allies (apply shield to other enemies) ──
    if (cd->enrage_allies && cd->shield_amount > 0)
    {
        int half = cd->shield_amount > 1 ? cd->shield_amount / 2 : 1;
        for (int i = 0; i < cs->enemy_count; i++)
            if (i != (int)(e - cs->enemies) && cs->enemies[i].def && cs->enemies[i].hp > 0)
                apply_shield_to_enemy(cs, i, half);
    }

    check_defeat(cs);
}

static int living_party_count(CombatState *cs)
{
    int count = 0;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive)
            count++;
    return count;
}

static int enemy_cast_time(CombatState *cs, EnemyState *e, int card_idx)
{
    if (card_idx < 0 || card_idx >= e->def->card_count) return 1;
    int cast = e->def->cards[card_idx].cast_time;
    int asc = active_ascension();
    if (asc >= 4)
        cast--;
    if (g_state.encounter_is_boss && e->phase >= 1)
        cast--;
    cast -= e->phase_cast_time_reduction;
    if (e->interrupted_recently && e->last_interrupted_ability == card_idx)
        cast--;
    if (e->def->cards[card_idx].cast_stages > cast)
        cast = e->def->cards[card_idx].cast_stages;
    if (cast < 1) cast = 1;
    return cast;
}


// ── Turn progression ────────────────────────────────────────

int combat_sudden_death_damage(int completed_turn)
{
    if (completed_turn < SUDDEN_DEATH_START_TURN) return 0;
    return 1 + completed_turn - SUDDEN_DEATH_START_TURN;
}

static bool apply_sudden_death(CombatState *cs, int completed_turn)
{
    int damage = combat_sudden_death_damage(completed_turn);
    if (damage <= 0) return false;

    combat_feed_add(cs, "Sudden Death: everyone takes %d", damage);
    ft_spawn((float)(VIRT_W / 2 - 40), 74.0f, "SUDDEN DEATH", 10, (Color){ 255, 70, 70, 255 });
    shake_trigger(shake_amplitude_for_value(damage, 2.0f, 0.35f, 8.0f));

    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive)
            apply_damage_to_ally(cs, i, damage, "Sudden Death");

    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].def && cs->enemies[i].hp > 0)
            apply_damage_to_enemy(cs, i, damage);

    check_defeat(cs);
    if (cs->phase == COMBAT_DEFEAT) return true;
    check_victory(cs);
    return cs->phase == COMBAT_VICTORY;
}

static void advance_turn(CombatState *cs)
{
    cs->turn++;
    LOG_I(CAT_COMBAT, "=== Turn %d ===", cs->turn);
    if (apply_sudden_death(cs, cs->turn - 1))
        return;
    cs->player_energy_drained_this_turn = 0;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        if (!e->def) continue;
        if (e->def->threshold_reset == THRESHOLD_RESET_END_OF_TURN)
        {
            e->turn_damage_received = 0;
            e->thresholds_triggered_mask = 0;
        }
    }
    boss_tick_linked_revives(cs);
    if (cs->temp_card_cost_turns > 0)
    {
        cs->temp_card_cost_turns--;
        if (cs->temp_card_cost_turns <= 0)
            cs->temp_card_cost_delta = 0;
    }
    if (cs->temp_hand_size_turns > 0)
    {
        cs->temp_hand_size_turns--;
        if (cs->temp_hand_size_turns <= 0)
            cs->temp_hand_size_delta = 0;
    }
    for (int i = 0; i < MAX_STOLEN_CARDS; i++)
    {
        if (!cs->stolen_cards[i].active) continue;
        cs->stolen_cards[i].return_turns--;
        if (cs->stolen_cards[i].return_turns <= 0)
            boss_return_stolen_card(cs, i);
    }
    for (int i = cs->arena_effect_count - 1; i >= 0; i--)
    {
        ArenaEffect *aura = &cs->arena_effects[i];
        aura->damage_this_turn = 0;
        if (aura->disrupt_turns > 0)
            aura->disrupt_turns--;
        if (aura->turns_remaining > 0)
        {
            aura->turns_remaining--;
            if (aura->turns_remaining <= 0)
            {
                for (int j = i; j < cs->arena_effect_count - 1; j++)
                    cs->arena_effects[j] = cs->arena_effects[j + 1];
                cs->arena_effect_count--;
            }
        }
    }
    int aura_damage = boss_arena_value(cs, AURA_DAMAGE_TICK);
    if (aura_damage > 0)
    {
        combat_feed_add(cs, "Arena deals %d damage", aura_damage);
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                apply_damage_to_ally(cs, i, aura_damage, "Arena");
    }
    int aura_heal = boss_arena_value(cs, AURA_HEAL_TICK);
    if (aura_heal > 0)
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                apply_heal_to_enemy(cs, i, aura_heal);
    int aura_shield = boss_arena_value(cs, AURA_SHIELD_TICK);
    if (aura_shield > 0)
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                apply_shield_to_enemy(cs, i, aura_shield);
    if (cs->turn == SUDDEN_DEATH_START_TURN)
        combat_feed_add(cs, "Sudden Death begins after this turn");
    cs->last_played_class = CLASS_NONE;
    if (cs->combo_prime_index >= 0 && cs->combo_prime_turns_remaining > 0)
    {
        cs->combo_prime_turns_remaining--;
        if (cs->combo_prime_turns_remaining <= 0)
            combo_prime_clear(cs);
        else
            combat_feed_add(cs, "Synergy Hourglass: prime carried over");
    }
    else
    {
        combo_prime_clear(cs);
    }
    cs->combo_count = 0;
    cs->combo_class = CLASS_NONE;
    cs->combo_last_cost = -1;
    cs->vengeful_active = false;
    cs->vengeful_ally = -1;
    cs->mage_first_spell_used = false; cs->musical_mend_used = false; cs->magical_might_used = false;

    // ── Tick channel ────────────────────────────────────────
    if (cs->channel_card && cs->channel_remaining > 0)
    {
        cs->channel_remaining--;
        LOG_I(CAT_COMBAT, "  %s channeling: %d turns remaining", cs->channel_card->name, cs->channel_remaining);

        if (cs->channel_remaining <= 0)
        {
            const CardDef *cc = cs->channel_card;
            LOG_I(CAT_CARD, "  Channel complete! Resolving %s...", cc->name);
            // Resolve channel card effects directly
            if (cc->damage > 0)
                for (int ei = 0; ei < cs->enemy_count; ei++)
                    if (cs->enemies[ei].def && cs->enemies[ei].hp > 0)
                        apply_damage_to_enemy(cs, ei, card_damage(cc, 0));
            if (cc->heal > 0)
                for (int i = 0; i < cs->party.count; i++)
                    if (cs->party.members[i].alive)
                        apply_heal_to_ally(cs, i, card_heal(cc, 0));
            if (cc->shield > 0)
                for (int i = 0; i < cs->party.count; i++)
                    if (cs->party.members[i].alive)
                        apply_shield_to_ally(cs, i, card_shield(cc, 0));
            cs->channel_card = NULL;
            cs->channel_class = CLASS_NONE;
        }
    }

    // ── Tick status effects ──────────────────────────────────
    // Burning damage on enemies
    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        if (!cs->enemies[ei].def || cs->enemies[ei].hp <= 0) continue;
        int bi = status_find(cs->enemies[ei].statuses, cs->enemies[ei].status_count, STATUS_BURNING);
        if (bi >= 0)
        {
            int burn_dmg = cs->enemies[ei].statuses[bi].value;
            assets_play_sfx(SFX_BURN_TICK);
            cs->enemies[ei].hp -= burn_dmg;
            if (cs->enemies[ei].hp < 0) cs->enemies[ei].hp = 0;
            char bbuf[16];
            snprintf(bbuf, sizeof(bbuf), "-%d", burn_dmg);
            ft_spawn((float)(cs->enemies[ei].pos_x + 4), (float)(cs->enemies[ei].pos_y - 18), bbuf, 10, (Color){ 255, 150, 50, 255 });
            vfx_spawn_burst((float)cs->enemies[ei].pos_x, (float)(cs->enemies[ei].pos_y - 5), (Color){ 255, 135, 45, 255 }, 4);
            LOG_I(CAT_CARD, "  enemy[%d] Burning: %d damage", ei, burn_dmg);
        }
    }

    // Bleed damage on allies
    for (int i = 0; i < cs->party.count; i++)
    {
        if (!cs->party.members[i].alive) continue;
        int bi = status_find(cs->party.members[i].statuses, cs->party.members[i].status_count, STATUS_BLEED);
        if (bi >= 0)
        {
            int bleed_dmg = cs->party.members[i].statuses[bi].value;
            assets_play_sfx(SFX_BLEED_TICK);
            apply_damage_to_ally(cs, i, bleed_dmg, "Bleed");
        }
    }

    // Renew healing on allies
    for (int i = 0; i < cs->party.count; i++)
    {
        if (!cs->party.members[i].alive) continue;
        int ri = status_find(cs->party.members[i].statuses, cs->party.members[i].status_count, STATUS_RENEW);
        if (ri >= 0)
        {
            int heal = cs->party.members[i].statuses[ri].value;
            cs->party.members[i].hp += heal;
            if (cs->party.members[i].hp > cs->party.members[i].max_hp)
                cs->party.members[i].hp = cs->party.members[i].max_hp;
            LOG_I(CAT_CARD, "  ally[%d] Renew: +%d HP", i, heal);
        }
    }

    // Healing Totem
    for (int i = 0; i < cs->party.count; i++)
    {
        if (!cs->party.members[i].alive) continue;
        int ti = status_find(cs->party.members[i].statuses, cs->party.members[i].status_count, STATUS_TOTEM_HEAL);
        if (ti >= 0)
        {
            int heal = cs->party.members[i].statuses[ti].value;
            if (party_has_pair(cs, CLASS_CLERIC, CLASS_SHAMAN))
                heal += 2;
            cs->party.members[i].hp += heal;
            if (cs->party.members[i].hp > cs->party.members[i].max_hp)
                cs->party.members[i].hp = cs->party.members[i].max_hp;
            LOG_I(CAT_CARD, "  ally[%d] Totem: +%d HP", i, heal);
        }
    }

    // ── Death Mark tick on enemies (damages for stacks, ticks down by 2) ──
    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        if (!cs->enemies[ei].def || cs->enemies[ei].hp <= 0) continue;
        int di = status_find(cs->enemies[ei].statuses, cs->enemies[ei].status_count, STATUS_DEATH_MARK);
        if (di >= 0)
        {
            int stacks = cs->enemies[ei].statuses[di].value;
            if (stacks > 0)
            {
                apply_damage_to_enemy(cs, ei, stacks);
                LOG_I(CAT_CARD, "  enemy[%d] Death Mark: %d damage", ei, stacks);
            }
            cs->enemies[ei].statuses[di].value -= 2;
            if (cs->enemies[ei].statuses[di].value <= 0)
            {
                for (int j = di; j < cs->enemies[ei].status_count - 1; j++)
                    cs->enemies[ei].statuses[j] = cs->enemies[ei].statuses[j + 1];
                cs->enemies[ei].status_count--;
            }
        }
    }

    // Tick all statuses down and remove expired
    for (int ei = 0; ei < cs->enemy_count; ei++)
        status_tick(cs->enemies[ei].statuses, &cs->enemies[ei].status_count);
    for (int i = 0; i < cs->party.count; i++)
        status_tick(cs->party.members[i].statuses, &cs->party.members[i].status_count);

    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive)
        {
            int decay = party_aggro_decay_amount(cs->party.members[i].aggro);
            add_party_member_aggro(&cs->party.members[i], -decay);
        }

    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        EnemyState *e = &cs->enemies[ei];
        if (!e->def || e->hp <= 0) continue;

        if (e->interrupt_cooldown > 0)
            e->interrupt_cooldown--;

        boss_add_accumulator(cs, e, e->def->gain_on_turn_start, "charge");
        if (e->def->decay_per_turn > 0)
            boss_add_accumulator(cs, e, -e->def->decay_per_turn, "");
        if (e->reflect_turns > 0)
        {
            e->reflect_turns--;
            if (e->reflect_turns <= 0)
                e->reflect_pct = 0;
        }
        if (e->tether_turns > 0)
        {
            e->tether_turns--;
            if (e->tether_turns <= 0 || e->tethered_ally < 0 ||
                e->tethered_ally >= cs->party.count || !cs->party.members[e->tethered_ally].alive)
                e->tethered_ally = -1;
        }
        if (e->invulnerable && e->invulnerable_until == INVULN_TURNS && e->invulnerable_turns > 0)
            e->invulnerable_turns--;

        for (int leader_idx = 0; leader_idx < cs->enemy_count; leader_idx++)
        {
            EnemyState *leader = &cs->enemies[leader_idx];
            if (leader_idx == ei || !leader->def || leader->hp <= 0) continue;
            for (int b = 0; b < leader->def->minion_buff_count; b++)
                if (leader->def->minion_buffs[b].effect == MINION_BUFF_SHIELD_PER_TURN &&
                    leader->def->minion_buffs[b].value > 0)
                    apply_shield_to_enemy(cs, ei, leader->def->minion_buffs[b].value);
        }

        boss_apply_phase_transitions(cs, ei);

        if (e->intent.ability_idx >= 0 && e->intent.remaining_turns <= 0 && !e->cast_pending)
        {
            int ci = e->intent.ability_idx;
            if (ci >= 0 && ci < e->def->card_count)
            {
                combat_spawn_enemy_card_throw(cs, e, &e->def->cards[ci], ei);
                e->cast_pending = true;
            }
        }

        if (e->intent.ability_idx >= 0 && e->intent.remaining_turns > 0)
        {
            e->intent.remaining_turns--;
            int ci = e->intent.ability_idx;
            if (ci >= 0 && ci < e->def->card_count)
            {
                int total = e->intent.index > 0 ? e->intent.index : enemy_cast_time(cs, e, ci);
                int stage = total - e->intent.remaining_turns;
                if (stage < 1) stage = 1;
                if (stage > e->def->cards[ci].cast_stages) stage = e->def->cards[ci].cast_stages;
                if (stage != e->intent.stage_index)
                    e->intent.stage_index = stage;
                boss_apply_cast_stage_effects(cs, e, &e->def->cards[ci], stage);
            }
            if (e->intent.remaining_turns <= 0)
            {
                if (ci >= 0 && ci < e->def->card_count)
                {
                    combat_spawn_enemy_card_throw(cs, e, &e->def->cards[ci], ei);
                    e->cast_pending = true;
                }
            }
        }

        // Enemy deck system: if no intent, refill energy, draw hand, pick best card
        if (e->intent.ability_idx < 0 && e->def && e->def->card_count > 0)
        {
            // Refill energy
            e->energy_current += e->energy_max;
            if (e->energy_current > e->energy_max * 2)
                e->energy_current = e->energy_max * 2;

            // Draw cards up to hand size
            int hand_size = e->def->hand_size + e->phase_hand_delta;
            if (hand_size < 1) hand_size = 1;
            if (hand_size > MAX_ENEMY_HAND) hand_size = MAX_ENEMY_HAND;
            while (e->hand_count < hand_size)
            {
                if (e->deck_count <= 0)
                {
                    // Reshuffle discard into deck
                    for (int d = 0; d < e->discard_count; d++)
                        e->deck[d] = e->discard[d];
                    e->deck_count = e->discard_count;
                    e->discard_count = 0;
                    for (int s = e->deck_count - 1; s > 0; s--)
                    {
                        int j = rand() % (s + 1);
                        int tmp = e->deck[s];
                        e->deck[s] = e->deck[j];
                        e->deck[j] = tmp;
                    }
                }
                if (e->deck_count > 0)
                {
                    e->hand[e->hand_count++] = e->deck[--e->deck_count];
                }
                else break;
            }

            int forced_ci = -1;
            if (e->opening_cursor < e->def->opening_count)
                forced_ci = e->def->opening_sequence[e->opening_cursor++];
            else if ((e->def->ai_override == AI_PATTERN || e->def->pattern_count > 0) && e->def->pattern_count > 0)
            {
                int cursor = e->pattern_cursor;
                if (cursor >= e->def->pattern_count)
                {
                    if (e->def->pattern_loop)
                        cursor = 0;
                    else
                        cursor = rand() % e->def->pattern_count;
                }
                forced_ci = e->def->pattern_cycle[cursor];
                e->pattern_cursor = cursor + 1;
            }
            if (forced_ci >= 0 && forced_ci < e->def->card_count)
            {
                int effective_cost = e->def->cards[forced_ci].cost - boss_arena_value(cs, AURA_COST_DOWN);
                if (effective_cost < 0) effective_cost = 0;
                if (effective_cost <= e->energy_current)
                {
                    e->intent.ability_idx = forced_ci;
                    e->intent.remaining_turns = enemy_cast_time(cs, e, forced_ci);
                    e->intent.index = e->intent.remaining_turns;
                    e->intent.stage_index = 1;
                    assets_play_sfx(g_state.encounter_is_boss ? SFX_BOSS_CAST_WARNING : SFX_ENEMY_CAST_WARNING);
                    e->current_ability++;
                    e->energy_current -= effective_cost;
                    if (e->discard_count < ENEMY_DECK_SIZE)
                        e->discard[e->discard_count++] = forced_ci;
                    for (int h = 0; h < e->hand_count; h++)
                        if (e->hand[h] == forced_ci)
                        {
                            for (int j = h; j < e->hand_count - 1; j++)
                                e->hand[j] = e->hand[j + 1];
                            e->hand_count--;
                            break;
                        }
                    continue;
                }
            }

            // Score each card in hand and play the best affordable one
            int alive_party = living_party_count(cs);
            bool low_hp = e->hp * 100 <= e->max_hp * 40;
            int best_idx = -1;
            int best_score = -9999;
            for (int h = 0; h < e->hand_count; h++)
            {
                int ci = e->hand[h];
                if (ci < 0 || ci >= e->def->card_count) continue;
                const EnemyCardDef *cd = &e->def->cards[ci];
                int effective_cost = cd->cost - boss_arena_value(cs, AURA_COST_DOWN);
                if (effective_cost < 0) effective_cost = 0;
                if (effective_cost > e->energy_current) continue;
                int score = (rand() % 8) + cd->base_damage;
                if (alive_party > 1 && cd->intent == INTENT_AOE)
                    score += 24;
                if (low_hp && (cd->intent == INTENT_HEAL || cd->intent == INTENT_SHIELD || cd->intent == INTENT_BUFF))
                    score += 28;
                if (!low_hp && (cd->intent == INTENT_HEAL || cd->intent == INTENT_SHIELD))
                    score -= 10;
                if (cd->status != STATUS_NONE && alive_party > 0)
                    score += 8;
                if (cd->intent == INTENT_WIPE && e->phase <= 0)
                    score -= 12;
                if (score > best_score)
                {
                    best_score = score;
                    best_idx = h;
                }
            }

            if (best_idx >= 0)
            {
                int ci = e->hand[best_idx];
                e->intent.ability_idx = ci;
                e->intent.remaining_turns = enemy_cast_time(cs, e, ci);
                e->intent.index = e->intent.remaining_turns;
                e->intent.stage_index = 1;
                assets_play_sfx(g_state.encounter_is_boss ? SFX_BOSS_CAST_WARNING : SFX_ENEMY_CAST_WARNING);
                e->current_ability++;
                int effective_cost = e->def->cards[ci].cost - boss_arena_value(cs, AURA_COST_DOWN);
                if (effective_cost < 0) effective_cost = 0;
                e->energy_current -= effective_cost;
                // Add to discard pile
                if (e->discard_count < ENEMY_DECK_SIZE)
                    e->discard[e->discard_count++] = ci;
                // Remove from hand
                for (int h = best_idx; h < e->hand_count - 1; h++)
                    e->hand[h] = e->hand[h + 1];
                e->hand_count--;
            }
        }
    }

    boss_clear_invulnerability_if_ready(cs);

    // Damage buff decay per turn
    if (cs->enemy_damage_buff_turns > 0)
    {
        cs->enemy_damage_buff_turns--;
        if (cs->enemy_damage_buff_turns <= 0)
        {
            cs->enemy_damage_buff_scale = 1.0f;
            combat_feed_add(cs, "Enemy damage buff faded");
        }
    }

    // Player damage buff decay per turn
    if (cs->player_buff_turns > 0)
    {
        cs->player_buff_turns--;
        if (cs->player_buff_turns <= 0)
        {
            cs->player_damage_mult = 1.0f;
            combat_feed_add(cs, "Damage buff faded");
        }
    }

    // Extra draw decay per turn
    if (cs->player_extra_draw_turns > 0)
    {
        cs->player_extra_draw_turns--;
        if (cs->player_extra_draw_turns <= 0)
        {
            cs->player_extra_draw = 0;
            combat_feed_add(cs, "Extra draw faded");
        }
    }

    // Status amp decay per turn
    if (cs->player_status_amp_turns > 0)
    {
        cs->player_status_amp_turns--;
        if (cs->player_status_amp_turns <= 0)
        {
            cs->player_status_amp = 0;
            combat_feed_add(cs, "Status amp faded");
        }
    }

    check_defeat(cs);
    if (cs->phase == COMBAT_DEFEAT) return;
    check_victory(cs);
    if (cs->phase == COMBAT_VICTORY) return;

    energy_refresh(&cs->energy);
    int drain = 0;
    for (int i = 0; i < cs->party.count; i++)
    {
        if (!cs->party.members[i].alive) continue;
        int di = status_find(cs->party.members[i].statuses, cs->party.members[i].status_count, STATUS_ENERGY_DRAIN);
        if (di >= 0 && cs->party.members[i].statuses[di].value > drain)
            drain = cs->party.members[i].statuses[di].value;
    }
    if (drain > 0)
    {
        int before = cs->energy.current;
        cs->energy.current -= drain;
        if (cs->energy.current < 0) cs->energy.current = 0;
        cs->player_energy_drained_this_turn += before - cs->energy.current;
        combat_feed_add(cs, "Energy drain: -%d", drain);
    }
    int aura_drain = boss_arena_value(cs, AURA_ENERGY_DRAIN);
    if (aura_drain > 0)
    {
        int before = cs->energy.current;
        cs->energy.current -= aura_drain;
        if (cs->energy.current < 0) cs->energy.current = 0;
        cs->player_energy_drained_this_turn += before - cs->energy.current;
        combat_feed_add(cs, "Arena drain: -%d", aura_drain);
    }
    bool boon_active = cs->boon_turns_remaining > 0;
    if (boon_active && cs->boon_energy_bonus > 0)
    {
        cs->energy.current += cs->boon_energy_bonus;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        combat_feed_add(cs, "Shop boon: +%d energy", cs->boon_energy_bonus);
    }
    // Discard hand, but keep retain cards
    {
        assets_play_sfx(SFX_CARD_DISCARD);
        for (int i = cs->deck.hand_count - 1; i >= 0; i--)
        {
            CardInstance *ci = &cs->deck.cards[cs->deck.hand[i]];
            if (!ci->def || !card_instance_has_retain(ci))
                deck_discard_index(&cs->deck, i);
        }
    }
    if (boon_active && cs->boon_draw_bonus > 0)
        combat_feed_add(cs, "Shop boon: +%d draw", cs->boon_draw_bonus);
    int draw_count = cs->turn_draw_count + (boon_active ? cs->boon_draw_bonus : 0) + cs->player_extra_draw;
    draw_count -= boss_arena_value(cs, AURA_DRAW_REDUCTION);
    if (cs->temp_hand_size_turns > 0)
        draw_count -= cs->temp_hand_size_delta;
    if (draw_count < 1) draw_count = 1;
    deal_cards(&cs->deck, draw_count);
    if (boon_active)
        cs->boon_turns_remaining--;

    cs->target_mode = TGT_NONE;
    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        EnemyState *e = &cs->enemies[ei];
        if (!e->def) continue;
        memset(e->reaction_counts, 0, sizeof(e->reaction_counts));
        memset(e->reaction_fired, 0, sizeof(e->reaction_fired));
    }
    cs->phase = COMBAT_PLAYER_TURN;
    combat_set_turn_banner(cs, "PLAYER TURN");
}

static void combat_end_turn_internal(CombatState *cs)
{
    if (cs->energy.current > 0)
        boss_trigger_reactions(cs, REACTION_ON_TURN_END_ENERGY_UNSPENT, NULL, -1);

    if (cs->deck.hand_count <= 0 &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_STEADFAST_BANNER))
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                add_party_member_shield_capped(&cs->party.members[i], 4);
        combat_feed_add(cs, "Steadfast Banner: +4 Shield");
    }

    cs->target_mode = TGT_NONE;
    cs->phase = COMBAT_ENEMY_TURN;
    cs->enemy_banner_timer = 0.55f;
    cs->end_turn_flash = 0.35f;
    combat_feed_add(cs, "Enemy actions resolve");
    advance_turn(cs);
}

// ── Public API ──────────────────────────────────────────────

static void combat_init_enemy_state(CombatState *cs, EnemyState *e, const EnemyDef *ed, int slot, float hp_scale)
{
    if (!cs || !e || !ed) return;
    memset(e, 0, sizeof(*e));
    int scaled_hp = (int)(ed->hp * hp_scale);
    if (scaled_hp < 1) scaled_hp = 1;
    e->def = ed;
    e->hp = scaled_hp;
    e->max_hp = scaled_hp;
    e->shield = 0;
    e->current_ability = slot * 2;
    e->last_interrupted_ability = -1;
    e->interrupt_cooldown = 0;
    e->interrupted_recently = false;
    e->cast_pending = false;
    e->phase = 0;
    e->intent.ability_idx = -1;
    e->intent.remaining_turns = 0;
    e->intent.index = 0;
    e->intent.stage_index = 0;
    e->tethered_ally = -1;
    e->linked_group = -1;
    e->energy_current = 0;
    e->energy_max = ed->energy_per_turn;
    e->deck_count = 0;
    e->deck_top = 0;
    e->hand_count = 0;
    e->discard_count = 0;
    int deck_pos = 0;
    for (int c = 0; c < ed->card_count; c++)
    {
        for (int copy = 0; copy < ed->cards[c].count && deck_pos < ENEMY_DECK_SIZE; copy++)
            e->deck[deck_pos++] = c;
    }
    e->deck_count = deck_pos;
    for (int s = deck_pos - 1; s > 0; s--)
    {
        int j = rand() % (s + 1);
        int tmp = e->deck[s];
        e->deck[s] = e->deck[j];
        e->deck[j] = tmp;
    }
}

static void boss_rebuild_linked_groups(CombatState *cs)
{
    if (!cs) return;
    memset(cs->linked_groups, 0, sizeof(cs->linked_groups));
    cs->linked_group_count = 0;
    for (int i = 0; i < cs->enemy_count; i++)
    {
        EnemyState *e = &cs->enemies[i];
        e->linked_group = -1;
        if (!e->def || !e->def->linked_group_id || !e->def->linked_group_id[0]) continue;
        int group_idx = -1;
        for (int g = 0; g < cs->linked_group_count; g++)
            if (strcmp(cs->linked_groups[g].id, e->def->linked_group_id) == 0)
                group_idx = g;
        if (group_idx < 0 && cs->linked_group_count < MAX_LINKED_GROUPS)
        {
            group_idx = cs->linked_group_count++;
            LinkedEnemyGroup *group = &cs->linked_groups[group_idx];
            group->active = true;
            group->shared_hp = true;
            snprintf(group->id, sizeof(group->id), "%s", e->def->linked_group_id);
            group->hp = 0;
            group->max_hp = 0;
        }
        if (group_idx >= 0)
        {
            e->linked_group = group_idx;
            if (e->def->linked_revive)
                cs->linked_groups[group_idx].shared_hp = false;
            cs->linked_groups[group_idx].hp += e->hp;
            cs->linked_groups[group_idx].max_hp += e->max_hp;
        }
    }
    for (int g = 0; g < cs->linked_group_count; g++)
        boss_sync_linked_group(cs, g);
}

int combat_spawn_enemy(CombatState *cs, const char *enemy_id, int count)
{
    if (!cs || !enemy_id || !enemy_id[0] || count <= 0) return 0;
    const EnemyDef *ed = enemy_def_by_id(enemy_id);
    if (!ed) return 0;
    int spawned = 0;
    for (int n = 0; n < count; n++)
    {
        int slot = -1;
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (!cs->enemies[i].def || cs->enemies[i].hp <= 0)
            {
                slot = i;
                break;
            }
        }
        if (slot < 0) break;
        if (slot >= cs->enemy_count)
            cs->enemy_count = slot + 1;
        combat_init_enemy_state(cs, &cs->enemies[slot], ed, slot, cs->floor_scale);
        cs->enemies[slot].pos_x = (int)layout_enemy_position(MAX_ENEMIES, slot).x;
        cs->enemies[slot].pos_y = (int)layout_enemy_position(MAX_ENEMIES, slot).y;
        spawned++;
    }
    if (spawned > 0)
    {
        boss_rebuild_linked_groups(cs);
        combat_feed_add(cs, "Spawned %d %s", spawned, ed->name);
    }
    return spawned;
}

void combat_start(CombatState *cs, const Party *party, const EncounterDef *encounter)
{
    LOG_T("combat_start BEGIN");
    LOG_T("  party_count=%d encounter_count=%d", party ? party->count : 0, encounter ? encounter->count : 0);

    memset(cs, 0, sizeof(CombatState));
    LOG_T("  memset done");

    if (party && party->count > 0)
        memcpy(&cs->party, party, sizeof(Party));
    else
    {
        int fallback_classes[3] = { CLASS_GUARDIAN, CLASS_CLERIC, CLASS_MAGE };
        party_create(&cs->party, fallback_classes, 3);
    }
    for (int i = 0; i < cs->party.count; i++)
    {
        PartyMember *pm = &cs->party.members[i];
        if (pm->level < 1) pm->level = 1;
        if (pm->level > MAX_LEVEL) pm->level = MAX_LEVEL;
        pm->combat_xp = 0;
        if (pm->perk_count < 0) pm->perk_count = 0;
        if (pm->perk_count > MAX_MEMBER_PERKS) pm->perk_count = MAX_MEMBER_PERKS;
    }
    LOG_T("  party created: %d members, run_deck has %d cards", cs->party.count, g_state.run_deck.card_count);

    memcpy(&cs->deck, &g_state.run_deck, sizeof(Deck));
    deck_prepare_for_combat(&cs->deck);
    for (int i = 0; i < cs->party.count; i++)
        if (!cs->party.members[i].alive)
            deck_remove_class_cards(&cs->deck, cs->party.members[i].class);
    LOG_T("  deck copied: card_count=%d draw_count=%d", cs->deck.card_count, cs->deck.draw_count);

    int asc = active_ascension();
    cs->turn_draw_count = party_draw_count(cs->party.count) +
        meta_first_draw_bonus(&g_state.meta) +
        combat_party_perk_effect_total(cs, "turn_draw");
    if (asc >= 8)
        cs->turn_draw_count--;
    if (cs->turn_draw_count < 1) cs->turn_draw_count = 1;
    cs->boon_energy_bonus = g_state.next_combat_energy_bonus;
    cs->boon_draw_bonus = g_state.next_combat_draw_bonus;
    cs->boon_turns_remaining = g_state.next_combat_boon_turns;
    g_state.next_combat_energy_bonus = 0;
    g_state.next_combat_draw_bonus = 0;
    g_state.next_combat_boon_turns = 0;

    deal_opening_hand(&cs->deck, cs->party.count, asc);
    int perk_start_draw = combat_party_perk_effect_total(cs, "combat_start_draw");
    if (perk_start_draw > 0)
        deal_cards(&cs->deck, perk_start_draw);
    LOG_T("  hand dealt: hand_count=%d draw_count=%d", cs->deck.hand_count, cs->deck.draw_count);

    int start_energy = party_start_energy(cs->party.count);
    if (asc >= 2) start_energy--;
    start_energy += meta_starting_energy_bonus(&g_state.meta);
    start_energy += combat_party_perk_effect_total(cs, "combat_start_energy");
    if (start_energy < 0) start_energy = 0;
    int regen = party_regen(cs->party.count);
    LOG_T("  calling energy_init(%d, %d, %d)", start_energy, MAX_ENERGY, regen);
    energy_init(&cs->energy, start_energy, MAX_ENERGY, regen);
    LOG_T("  energy: %d/%d regen=%d", cs->energy.current, cs->energy.max, cs->energy.regen);

    cs->turn = 0;
    cs->hovered_card = -1;
    cs->target_mode = TGT_NONE;
    cs->target_hand_idx = -1;
    cs->target_paid_cost = 0;
    cs->hovered_enemy = -1;
    cs->hovered_ally = -1;
    cs->gold_spawned = false;
    cs->floor_scale = area_difficulty_scale(g_state.current_area) * (1.0f + 0.12f * (float)g_state.map.floor);
    if (asc >= 3)
        cs->floor_scale *= 1.15f;
    cs->enemy_damage_scale = 1.0f;
    cs->enemy_damage_buff_scale = 1.0f;
    cs->enemy_damage_buff_turns = 0;
    cs->resolving_ally_idx = -1;
    cs->last_player_attacker = -1;
    cs->linked_group_count = 0;
    cs->arena_effect_count = 0;
    cs->temp_card_cost_delta = 0;
    cs->temp_card_cost_turns = 0;
    cs->temp_hand_size_delta = 0;
    cs->temp_hand_size_turns = 0;
    cs->player_energy_drained_this_turn = 0;
    cs->player_damage_mult = 1.0f;
    cs->player_buff_turns = 0;
    cs->player_extra_draw = 0;
    cs->player_extra_draw_turns = 0;
    cs->player_status_amp = 0;
    cs->player_status_amp_turns = 0;
    if (asc >= 1) cs->enemy_damage_scale += 0.10f;
    if (asc >= 6) cs->enemy_damage_scale += 0.15f;
    if (asc >= 10) cs->enemy_damage_scale += 0.25f;
    cs->phoenix_used = false;
    cs->echo_used = false;
    cs->meta_opening_damage_used = false;
    cs->meta_execute_used = false;
    cs->meta_emergency_barrier_used = false;
    cs->meta_last_stand_used = false;
    cs->mana_gem_bonus = relic_has(g_state.relics, g_state.relic_count, RELIC_MANA_GEM) ? 1 : 0;
    cs->channel_card = NULL;
    cs->channel_remaining = 0;
    cs->channel_class = CLASS_NONE;
    cs->resolving_card_class = CLASS_NONE;
    cs->target_offset = 0;
    cs->target_offset_tween = -1;
    cs->combo_class = CLASS_NONE;
    cs->combo_last_cost = -1;
    cs->combo_count = 0;
    cs->last_played_class = CLASS_NONE;
    cs->combo_prime_index = -1;
    cs->combo_prime_turns_remaining = 0;
    cs->combo_scale = 1.0f;
    cs->combo_tween = -1;
    cs->combo_shake = 0;
    cs->turn_banner_timer = 0.0f;
    cs->turn_banner_text[0] = '\0';
    cs->enemy_banner_timer = 0.0f;
    cs->end_turn_flash = 0.0f;
    cs->play_flash_timer = 0.0f;
    cs->play_flash_text[0] = '\0';
    cs->synergy_banner_timer = 0.0f;
    cs->synergy_flash_timer = 0.0f;
    cs->synergy_banner_title[0] = '\0';
    cs->synergy_banner_subtitle[0] = '\0';
    cs->ambush_used = false;
    cs->vengeful_active = false;
    cs->guardian_taunt_shield_used = false;
    cs->mage_first_spell_used = false; cs->musical_mend_used = false; cs->magical_might_used = false;
    cs->rogue_mark_refund_used = false;
    cs->shaman_extend_status_used = false;
    cs->ranger_marked_dmg_used = false;
    cs->warlock_blight_boost_used = false;
    cs->bard_first_draw_used = false;
    cs->deadly_poison_used = false;
    cs->vengeful_ally = -1;
    for (int i = 0; i < 5; i++)
    {
        cs->action_feed[i][0] = '\0';
        cs->action_feed_timer[i] = 0.0f;
    }

    apply_relic_combat_start(cs);

    int meta_start_shield = meta_combat_start_shield(&g_state.meta);
    if (meta_start_shield > 0)
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                add_party_member_shield_capped(&cs->party.members[i], meta_start_shield);
        combat_feed_add(cs, "Skill Tree: +%d starting Shield", meta_start_shield);
    }

    for (int i = 0; i < cs->party.count; i++)
    {
        PartyMember *pm = &cs->party.members[i];
        if (!pm->alive) continue;
        int shield_perks = party_member_perk_effect_total(pm, "combat_start_shield");
        if (shield_perks > 0)
        {
            add_party_member_shield_capped(pm, shield_perks);
            combat_feed_add(cs, "%s: +%d starting Shield", pm->name, shield_perks);
        }
    }

    LOG_T("  setting up enemies");

    cs->enemy_count = (encounter && encounter->count > 0) ? encounter->count : 1;
    int party_enemy_cap = g_state.debug_admin_mode ? MAX_ENEMIES : (cs->party.count <= 1 ? 1 : (cs->party.count == 2 ? 2 : MAX_ENEMIES));
    if (cs->enemy_count > party_enemy_cap)
        cs->enemy_count = party_enemy_cap;
    if (cs->enemy_count > MAX_ENEMIES) cs->enemy_count = MAX_ENEMIES;

    if (cs->enemy_count == 1)
    {
        const EnemyDef *ed = (encounter && encounter->enemies[0]) ? encounter->enemies[0] : enemy_def_by_id("flame_imp");
        LOG_T("  enemy: %s HP=%d", ed ? ed->name : "null", ed ? ed->hp : 0);
        if (ed)
        {
            combat_init_enemy_state(cs, &cs->enemies[1], ed, 1, cs->floor_scale);
            for (int a = 0; a < ed->start_aura_count; a++)
                boss_add_arena_effect(cs, ed->start_auras[a], 1);
        }
        cs->enemy_count = 2;
    }
    else if (cs->enemy_count == 2)
    {
        const EnemyDef *ed0 = (encounter && encounter->enemies[0]) ? encounter->enemies[0] : enemy_def_by_id("flame_imp");
        const EnemyDef *ed1 = (encounter && encounter->enemies[1]) ? encounter->enemies[1] : enemy_def_by_id("flame_imp");
        if (ed0)
        {
            combat_init_enemy_state(cs, &cs->enemies[0], ed0, 0, cs->floor_scale);
            for (int a = 0; a < ed0->start_aura_count; a++)
                boss_add_arena_effect(cs, ed0->start_auras[a], 0);
        }
        if (ed1)
        {
            combat_init_enemy_state(cs, &cs->enemies[2], ed1, 2, cs->floor_scale);
            for (int a = 0; a < ed1->start_aura_count; a++)
                boss_add_arena_effect(cs, ed1->start_auras[a], 2);
        }
        cs->enemy_count = 3;
    }
    else
    {
        const EnemyDef *enemy_defs[3];
        for (int i = 0; i < 3; i++)
            enemy_defs[i] = (encounter && encounter->enemies[i]) ? encounter->enemies[i] : enemy_def_by_id("flame_imp");
        if (cs->enemy_count == 3)
        {
            int strongest = 0;
            for (int i = 1; i < 3; i++)
                if (enemy_defs[i] && enemy_defs[strongest] && enemy_defs[i]->max_hp > enemy_defs[strongest]->max_hp)
                    strongest = i;
            const EnemyDef *tmp = enemy_defs[1];
            enemy_defs[1] = enemy_defs[strongest];
            enemy_defs[strongest] = tmp;
        }
        for (int i = 0; i < cs->enemy_count; i++)
        {
            const EnemyDef *ed = enemy_defs[i];
            if (!ed) continue;
            LOG_T("  enemy[%d]: %s HP=%d", i, ed->name, ed->hp);
            combat_init_enemy_state(cs, &cs->enemies[i], ed, i, cs->floor_scale);
            for (int a = 0; a < ed->start_aura_count; a++)
                boss_add_arena_effect(cs, ed->start_auras[a], i);
        }
    }
    boss_rebuild_linked_groups(cs);
    calc_enemy_positions(cs->enemies, cs->enemy_count);
    LOG_T("  enemy positions calculated");

    // ── Enemy-affecting relic effects ───────────────────────────
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_POISON_FANG))
    {
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                status_apply(cs->enemies[i].statuses, &cs->enemies[i].status_count, STATUS_BURNING, 3, 2);
        combat_feed_add(cs, "Poison Fang: +2 Burning");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_VOID_STONE))
    {
        for (int i = 0; i < cs->enemy_count; i++)
            cs->enemies[i].shield = 0;
        combat_feed_add(cs, "Void Stone: removed enemy shields");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_TOXIC_VIAL))
    {
        int living = 0;
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0) living++;
        if (living > 0)
        {
            int pick = rand() % living;
            int idx = 0;
            for (int i = 0; i < cs->enemy_count; i++)
            {
                if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
                if (idx == pick)
                {
                    status_apply(cs->enemies[i].statuses, &cs->enemies[i].status_count, STATUS_BURNING, 3, 3);
                    break;
                }
                idx++;
            }
        }
        combat_feed_add(cs, "Toxic Vial: applied Burning");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_LANTERN_OIL))
    {
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                status_apply(cs->enemies[i].statuses, &cs->enemies[i].status_count, STATUS_BURNING, 3, 1);
        combat_feed_add(cs, "Lantern Oil: +1 Burning");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_GLASS_CALTROPS))
    {
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                status_apply(cs->enemies[i].statuses, &cs->enemies[i].status_count, STATUS_TRAP, 2, 2);
        combat_feed_add(cs, "Glass Caltrops: trapped enemies");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_HUNTERS_COMPASS))
    {
        int living = 0;
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0) living++;
        if (living > 0)
        {
            int pick = rand() % living;
            int idx = 0;
            for (int i = 0; i < cs->enemy_count; i++)
            {
                if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
                if (idx == pick)
                {
                    status_apply(cs->enemies[i].statuses, &cs->enemies[i].status_count, STATUS_MARKED, 2, 1);
                    break;
                }
                idx++;
            }
            combat_feed_add(cs, "Hunter's Compass: marked target");
        }
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_BOTTLED_STORM))
    {
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                status_apply(cs->enemies[i].statuses, &cs->enemies[i].status_count, STATUS_CONDUCTIVE, 2, 1);
        combat_feed_add(cs, "Bottled Storm: enemies Conductive");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_TITAN_HEART) && g_state.titan_heart_bonus == 0)
    {
        for (int i = 0; i < cs->party.count; i++)
            cs->party.members[i].max_hp += 10;
        g_state.titan_heart_bonus = 10;
    }

    LOG_T("  calling advance_turn");
    advance_turn(cs);

    // Crystal Ball: +1 cast time on enemies' current intent (after advance_turn sets them)
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_CRYSTAL_BALL))
    {
        for (int i = 0; i < cs->enemy_count; i++)
        {
            EnemyState *e = &cs->enemies[i];
            if (!e->def || e->hp <= 0 || e->intent.ability_idx < 0) continue;
            e->intent.remaining_turns++;
        }
        combat_feed_add(cs, "Crystal Ball: delayed enemies");
    }

    LOG_T("combat_start END");
}

static void cancel_targeting(CombatState *cs)
{
    if (!cs) return;
    cs->energy.current += cs->target_paid_cost;
    if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
    cs->target_mode = TGT_NONE;
    cs->target_hand_idx = -1;
    cs->target_paid_cost = 0;
    cs->target_offset = 0.0f;
}

void combat_end_turn(CombatState *cs)
{
    if (cs->target_mode != TGT_NONE)
    {
        cancel_targeting(cs);
        return;
    }
    combat_end_turn_internal(cs);
}

// ── Input handling ──────────────────────────────────────────

static void start_targeting(CombatState *cs, int hand_idx, TargetType tt)
{
    cs->target_hand_idx = hand_idx;
    cs->hovered_enemy = -1;
    cs->hovered_ally = -1;

    // Animate the selected card upward
    cs->target_offset = 0;
    cs->target_offset_tween = tween_create(&cs->target_offset, -40.0f, 0.2f, EASE_OUT_BACK);

    switch (tt)
    {
        case TARGET_ENEMY:
            cs->target_mode = TGT_SELECT_ENEMY;
            break;
        case TARGET_ALLY:
            cs->target_mode = TGT_SELECT_ALLY;
            break;
        case TARGET_ALL_ENEMIES:
        case TARGET_ALL_ALLIES:
        case TARGET_SELF:
            cs->target_mode = TGT_CONFIRM_CARD;
            break;
        default:
            cs->target_mode = TGT_NONE;
            cs->target_hand_idx = -1;
            break;
    }
}

static void handle_card_click(CombatState *cs, int hand_idx)
{
    if (hand_idx < 0 || hand_idx >= cs->deck.hand_count) return;
    const CardDef *card = cs->deck.cards[cs->deck.hand[hand_idx]].def;
    if (!card || !card->name) return;

    // Block cards from a channeling class
    if (cs->channel_card && cs->channel_class == card->class)
    {
        LOG_D(CAT_CARD, "  Cannot play %s — %s is channeling", card->name, class_name(card->class));
        assets_play_sfx(SFX_ERROR);
        return;
    }

    if (party_class_is_silenced(cs, card->class))
    {
        LOG_D(CAT_CARD, "  Cannot play %s - %s is silenced", card->name, class_name(card->class));
        combat_feed_add(cs, "%s cards are silenced", class_name(card->class));
        ft_spawn(244.0f, 222.0f, "SILENCED", 10, (Color){ 190, 130, 255, 255 });
        assets_play_sfx(SFX_ERROR);
        return;
    }

    int effective_cost = combat_effective_card_cost(cs, card);
    if (cs->energy.current < effective_cost)
    {
        assets_play_sfx(SFX_ERROR);
        return;
    }

    energy_spend(&cs->energy, effective_cost);
    cs->target_paid_cost = effective_cost;

    LOG_D(CAT_CARD, "handle_card_click: card=%s target=%d energy=%d", card->name, (int)card->target, cs->energy.current);

    LOG_D(CAT_CARD, "  -> entering preview mode");
    start_targeting(cs, hand_idx, card->target);
}

static int hit_test_enemies(CombatState *cs, Vector2 mouse)
{
    for (int i = 0; i < cs->enemy_count; i++)
    {
        if (!cs->enemies[i].def || cs->enemies[i].hp <= 0) continue;
        Rectangle r = layout_enemy_hit_rect((Vector2){ (float)cs->enemies[i].pos_x, (float)cs->enemies[i].pos_y });
        if (CheckCollisionPointRec(mouse, r)) return i;
    }
    return -1;
}

static int hit_test_party(CombatState *cs, Vector2 mouse)
{
    for (int i = 0; i < cs->party.count; i++)
    {
        Rectangle r = layout_party_frame_rect(cs->party.count, i);
        if (CheckCollisionPointRec(mouse, r)) return i;
    }
    return -1;
}

static bool card_can_target_ally(const CardDef *card, PartyMember *member)
{
    if (!card || !member) return false;
    if (card_has_effect(card, CARD_EFFECT_REVIVE_TARGET))
        return !member->alive;
    return member->alive;
}

void combat_update(CombatState *cs)
{
    LOG_T("combat_update: phase=%d hand=%d energy=%d", cs->phase, cs->deck.hand_count, cs->energy.current);
    float dt = GetFrameTime();
    if (cs->turn_banner_timer > 0.0f) cs->turn_banner_timer -= dt;
    if (cs->enemy_banner_timer > 0.0f) cs->enemy_banner_timer -= dt;
    if (cs->end_turn_flash > 0.0f) cs->end_turn_flash -= dt;
    if (cs->play_flash_timer > 0.0f) cs->play_flash_timer -= dt;
    if (cs->synergy_banner_timer > 0.0f) cs->synergy_banner_timer -= dt;
    if (cs->synergy_flash_timer > 0.0f) cs->synergy_flash_timer -= dt;
    for (int i = 0; i < 5; i++)
        if (cs->action_feed_timer[i] > 0.0f)
            cs->action_feed_timer[i] -= dt;
    combat_update_card_throws(cs, dt);
    if (cs->echo_pending)
    {
        cs->echo_timer -= dt;
        if (cs->echo_timer <= 0.0f)
            combat_resolve_pending_echo(cs);
    }

    if (cs->phase == COMBAT_VICTORY || cs->phase == COMBAT_DEFEAT)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            cs->phase = COMBAT_TURN_START;
        return;
    }

    if (cs->phase != COMBAT_PLAYER_TURN) { LOG_T("CU: not player turn, returning"); return; }

    // Block player input while enemy cards are resolving
    if (combat_any_pending(cs)) { return; }

    LOG_T("CU: getting mouse");
    Vector2 mouse = GetMousePosition();
    int prev_hovered_card = cs->hovered_card;
    cs->hovered_card = -1;

    // ── Targeting mode ──────────────────────────────────────
    if (cs->target_mode == TGT_SELECT_ENEMY)
    {
        cs->hovered_enemy = hit_test_enemies(cs, mouse);
        if (cs->hovered_enemy >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            int paid_cost = cs->target_paid_cost;
            resolve_card_on_target(cs, cs->target_hand_idx, cs->hovered_enemy, -1, paid_cost);
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_paid_cost = 0; cs->target_offset = 0;
            check_victory(cs); if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);  if (cs->phase == COMBAT_DEFEAT) return;
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            cancel_targeting(cs);
        }
        return;
    }

    if (cs->target_mode == TGT_SELECT_ALLY)
    {
        cs->hovered_ally = hit_test_party(cs, mouse);
        if (cs->hovered_ally >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            const CardDef *card = cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def;
            if (!card_can_target_ally(card, &cs->party.members[cs->hovered_ally]))
            {
                assets_play_sfx(SFX_ERROR);
                ft_spawn(244.0f, 222.0f, "INVALID TARGET", 10, (Color){ 230, 90, 90, 255 });
                return;
            }
            int paid_cost = cs->target_paid_cost;
            resolve_card_on_target(cs, cs->target_hand_idx, -1, cs->hovered_ally, paid_cost);
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_paid_cost = 0; cs->target_offset = 0;
            check_victory(cs); if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);  if (cs->phase == COMBAT_DEFEAT) return;
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            cancel_targeting(cs);
        }
        return;
    }

    if (cs->target_mode == TGT_CONFIRM_CARD)
    {
        if (cs->target_hand_idx < 0 || cs->target_hand_idx >= cs->deck.hand_count)
        {
            cancel_targeting(cs);
            return;
        }

        Rectangle selected = combat_hand_card_rect(cs, cs->target_hand_idx);
        if (CheckCollisionPointRec(mouse, selected))
            cs->hovered_card = cs->target_hand_idx;

        if (CheckCollisionPointRec(mouse, selected) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            int hand_idx = cs->target_hand_idx;
            int paid_cost = cs->target_paid_cost;
            int target_ally = -1;
            const CardDef *card = cs->deck.cards[cs->deck.hand[hand_idx]].def;
            if (card && card->target == TARGET_SELF)
                find_caster(cs, card->class, &target_ally);

            resolve_card_on_target(cs, hand_idx, -1, target_ally, paid_cost);
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_paid_cost = 0; cs->target_offset = 0;
            check_victory(cs); if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);  if (cs->phase == COMBAT_DEFEAT) return;
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            cancel_targeting(cs);
        }
        return;
    }

    // ── Normal card interaction ────────────────────────────
    HandLayout hand_layout = layout_hand(cs->deck.hand_count);

    for (int i = cs->deck.hand_count - 1; i >= 0; i--)
    {
        Rectangle r = layout_hand_card_rect(hand_layout, i);
        if (CheckCollisionPointRec(mouse, r))
        {
            cs->hovered_card = i;
            if (cs->hovered_card != prev_hovered_card)
                assets_play_sfx(SFX_CARD_HOVER);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                handle_card_click(cs, i);
            break;
        }
    }

    Rectangle end_btn = layout_end_turn_button();
    if (CheckCollisionPointRec(mouse, end_btn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        combat_end_turn_internal(cs);
}




