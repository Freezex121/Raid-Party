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
#include "util/log.h"
#include "constants.h"
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

static int active_ascension(void)
{
    int level = g_state.meta.ascension_level;
    if (level < 0) level = 0;
    if (level > META_ASCENSION_MAX) level = META_ASCENSION_MAX;
    return level;
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
        Vector2 pos = layout_enemy_position(count, i);
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

// ── Damage / heal / shield / taunt ──────────────────────────

static int combat_member_index_for_class(CombatState *cs, ClassType ct)
{
    if (!cs || ct == CLASS_NONE) return -1;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive && cs->party.members[i].class == ct)
            return i;
    return -1;
}

static bool combat_class_has_perk(CombatState *cs, ClassType ct, PerkId perk)
{
    int idx = combat_member_index_for_class(cs, ct);
    return idx >= 0 && party_member_has_perk(&cs->party.members[idx], perk);
}

static int combat_class_perk_count(CombatState *cs, ClassType ct, PerkId perk)
{
    int idx = combat_member_index_for_class(cs, ct);
    return idx >= 0 ? party_member_perk_count(&cs->party.members[idx], perk) : 0;
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
    }
}

static void combat_try_rogue_mark_refund(CombatState *cs)
{
    if (!cs || cs->rogue_mark_refund_used)
        return;
    if (!combat_class_has_perk(cs, CLASS_ROGUE, PERK_ROGUE_MARK_REFUND))
        return;
    cs->rogue_mark_refund_used = true;
    if (cs->energy.current < cs->energy.max)
        cs->energy.current++;
    combat_feed_add(cs, "Clean Payday: +1 energy");
}

static void apply_damage_to_enemy(CombatState *cs, int enemy_idx, int damage)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return;

    int before_hp = e->hp;
    int dmg = damage;
    if (e->shield > 0)
    {
        int abs = e->shield >= dmg ? dmg : e->shield;
        e->shield -= abs;
        dmg -= abs;
    }
    e->hp -= dmg;
    if (e->hp < 0) e->hp = 0;

    char buf[16];
    snprintf(buf, sizeof(buf), "-%d", damage);
    ft_spawn_shake((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 10, (Color){ 255, 80, 80, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)e->pos_y, (Color){ 255, 85, 65, 255 }, 6);

    LOG_I(CAT_CARD, "  enemy[%d] %s: %d damage (%d HP)", enemy_idx, e->def->name, damage, e->hp);

    if (before_hp > 0 && e->hp <= 0 &&
        !cs->executioner_used &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_EXECUTIONERS_SEAL))
    {
        cs->executioner_used = true;
        deck_draw(&cs->deck);
        if (cs->energy.current < cs->energy.max)
            cs->energy.current++;
        combat_feed_add(cs, "Executioner's Seal: drew 1, +1 energy");
    }
}

static void apply_shield_to_ally(CombatState *cs, int ally_idx, int amount);

static void apply_heal_to_ally(CombatState *cs, int ally_idx, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;
    int before = pm->hp;
    int overheal = before + amount - pm->max_hp;
    pm->hp += amount;
    if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", amount);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 80, 255, 80, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 95, 245, 135, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d HP (%d)", ally_idx, pm->name, amount, pm->hp);

    if (amount > 0 &&
        cs->resolving_card_class == CLASS_PALADIN &&
        combat_class_has_perk(cs, CLASS_PALADIN, PERK_PALADIN_HEAL_SHIELD))
    {
        apply_shield_to_ally(cs, ally_idx, 1);
        combat_feed_add(cs, "Blessed Mending: +1 Shield");
    }

    if (overheal > 0 &&
        cs->resolving_card_class == CLASS_CLERIC &&
        combat_class_has_perk(cs, CLASS_CLERIC, PERK_CLERIC_OVERHEAL_SHIELD))
    {
        apply_shield_to_ally(cs, ally_idx, 2);
        combat_feed_add(cs, "Overflowing Grace: +2 Shield");
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
    pm->shield += amount;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", amount);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 100, 200, 255, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 115, 190, 255, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d shield (%d)", ally_idx, pm->name, amount, pm->shield);

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
                int share = amount > 2 ? 2 : amount;
                cs->party.members[i].shield += share;
                combat_feed_add(cs, "Sheltered Bulwark: Paladin +%d shield", share);
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
    pm->aggro = 5;
    pm->status_count = 0;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", pm->hp);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    ft_spawn(p.x, p.y, buf, 10, (Color){ 120, 255, 160, 255 });

    LOG_I(CAT_CARD, "  ally[%d] %s: revived at %d HP", ally_idx, pm->name, pm->hp);
}

static void apply_damage_to_ally(CombatState *cs, int ally_idx, int damage, const char *source)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;

    int remaining = damage;
    if (pm->shield > 0)
    {
        int absorb = pm->shield >= remaining ? remaining : pm->shield;
        pm->shield -= absorb;
        remaining -= absorb;
    }

    int before = pm->hp;
    pm->hp -= remaining;
    if (pm->hp < 0) pm->hp = 0;

    Vector2 p = party_feedback_pos(cs, ally_idx);

    char buf[16];
    snprintf(buf, sizeof(buf), "-%d", before - pm->hp);
    ft_spawn_shake(p.x, p.y, buf, 10, (Color){ 255, 90, 90, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 255, 85, 75, 255 }, 5);

    LOG_I(CAT_COMBAT, "%s hits %s for %d (%d -> %d)", source, pm->name, before - pm->hp, before, pm->hp);
    combat_feed_add(cs, "%s hit %s for %d", source, pm->name, before - pm->hp);

    if (before > pm->hp &&
        pm->hp > 0 &&
        !cs->veil_pin_used &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_VEIL_PIN))
    {
        cs->veil_pin_used = true;
        pm->shield += 6;
        combat_feed_add(cs, "Veil Pin: %s gained 6 Shield", pm->name);
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
        }
        else
        {
            pm->alive = false;
            g_state.run_deaths++;
            log_death_metric(cs, pm, source);
            pm->aggro = 0;
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
    e->hp += amount;
    if (e->hp > e->max_hp) e->hp = e->max_hp;

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
    combat_feed_add(cs, "%s was interrupted", e->def->name);
    ft_spawn_shake((float)(e->pos_x - 21), (float)(e->pos_y - 26), "INTERRUPTED", 10, (Color){ 220, 120, 255, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)(e->pos_y - 15), (Color){ 220, 120, 255, 255 }, 7);
    e->last_interrupted_ability = e->intent.ability_idx;
    e->interrupt_cooldown = 3;
    e->interrupted_recently = true;
    e->intent.ability_idx = -1;
    e->intent.remaining_turns = 0;
    g_state.run_interrupts++;
    return true;
}

static void add_aggro_to_caster(CombatState *cs, ClassType ct, int amount)
{
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].class == ct && cs->party.members[i].alive)
            cs->party.members[i].aggro += amount;
}

static void find_caster(CombatState *cs, ClassType ct, int *out_idx)
{
    *out_idx = -1;
    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].class == ct && cs->party.members[i].alive)
            { *out_idx = i; return; }
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
        // Single-target damage: pick same target enemy_action would use
        if (cs->party.count > 1 && (rand() % 4) == 0)
            tgt_ally = party_random_alive(&cs->party);
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

        // Set type and target based on intent
        switch (thr->intent)
        {
            case INTENT_HEAL:
            case INTENT_SHIELD:
                fake_card.type = CARD_SKILL;
                fake_card.target = thr->intent == INTENT_HEAL ? TARGET_ALLY : TARGET_SELF;
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
            }
        }

        theme_draw_card_art(r, &fake_card, 0);
    }
}

bool combat_any_pending(CombatState *cs)
{
    if (!cs) return false;
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
        theme_draw_card_art_seeded(r, anim->card, anim->upgrade_level, anim->seed);
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
    if (turns > 0 &&
        status == STATUS_CONDUCTIVE &&
        cs->resolving_card_class == CLASS_SHAMAN &&
        !cs->shaman_extend_status_used &&
        combat_class_has_perk(cs, CLASS_SHAMAN, PERK_SHAMAN_EXTEND_STATUS))
    {
        turns++;
        cs->shaman_extend_status_used = true;
        combat_feed_add(cs, "Lingering Rite: +1 turn");
    }
    if (status == STATUS_BLIGHT &&
        cs->resolving_card_class == CLASS_WARLOCK &&
        !cs->warlock_blight_boost_used &&
        combat_class_has_perk(cs, CLASS_WARLOCK, PERK_WARLOCK_BLIGHT_BOOST))
    {
        amount++;
        cs->warlock_blight_boost_used = true;
        combat_feed_add(cs, "Deeper Rot: +1 BLIGHT");
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

    status_apply(e->statuses, &e->status_count, status, turns, amount);
    LOG_I(CAT_CARD, "  enemy[%d]: +%s (%d for %d turns)", enemy_idx, status_label(status), amount, turns);
    combat_feed_add(cs, "%s: %s", e->def->name, status_label(status));
}

static void apply_status_to_ally(CombatState *cs, int ally_idx, StatusType status, int turns, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;

    if (turns > 0 &&
        status == STATUS_TOTEM_HEAL &&
        cs->resolving_card_class == CLASS_SHAMAN &&
        !cs->shaman_extend_status_used &&
        combat_class_has_perk(cs, CLASS_SHAMAN, PERK_SHAMAN_EXTEND_STATUS))
    {
        turns++;
        cs->shaman_extend_status_used = true;
        combat_feed_add(cs, "Lingering Rite: +1 turn");
    }

    status_apply(pm->statuses, &pm->status_count, status, turns, amount);
    LOG_I(CAT_CARD, "  ally[%d]: +%s (%d for %d turns)", ally_idx, status_label(status), amount, turns);
    combat_feed_add(cs, "%s: %s", pm->name, status_label(status));
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
                cs->party.members[caster].aggro = 0;
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
                cs->party.members[guardian].aggro += transfer;
                cs->party.members[target_ally].aggro = 0;
                ft_spawn(22.0f, 176.0f, "AGGRO TRANSFER", 10, (Color){ 180, 180, 220, 255 });
                LOG_I(CAT_CARD, "  %s: moved %d aggro to Guardian", card->name, transfer);
                combat_feed_add(cs, "Aggro moved to Guardian");
            }
            else
            {
                cs->party.members[target_ally].aggro = 0;
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
    }
}

static void apply_card_effect_chain(CombatState *cs, const CardDef *card, int target_enemy, int target_ally)
{
    if (!card || !card->effects || card->effect_count <= 0) return;
    for (int i = 0; i < card->effect_count; i++)
        apply_card_effect(cs, card, &card->effects[i], target_enemy, target_ally);
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
    return card->cost;
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

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_QUICKDRAW_GLOVE))
    {
        deck_draw(&cs->deck);
        combat_feed_add(cs, "Quickdraw Glove: drew 1");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_WARD_STONE))
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                cs->party.members[i].shield += 4;
        combat_feed_add(cs, "Ward Stone: +4 Shield");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_PROSPERITY_CHARM))
    {
        int shield = g_state.gold / 20;
        if (shield > 0)
        {
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    cs->party.members[i].shield += shield;
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
            cs->party.members[lowest].shield += 8;
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
    combat_award_card_xp(cs, card, paid_cost);

    int upgrade_level = inst->upgrade_level;
    log_card_play_metric(cs, card, upgrade_level, paid_cost, target_enemy, target_ally);

    int dmg = card_damage(card, upgrade_level) + meta_dmg_bonus(&g_state.meta);
    int hl  = card_heal(card, upgrade_level);
    int sh  = card_shield(card, upgrade_level) + meta_shield_bonus(&g_state.meta);
    if (card->class != CLASS_NONE)
    {
        if (dmg > 0) dmg += combat_class_perk_count(cs, card->class, PERK_CARD_DMG_1);
        if (hl > 0) hl += combat_class_perk_count(cs, card->class, PERK_CARD_HEAL_1);
        if (sh > 0) sh += combat_class_perk_count(cs, card->class, PERK_CARD_SHIELD_1);
    }

    LOG_I(CAT_CARD, "Playing %s (enemy=%d, ally=%d) upgrade_level=%d channel=%d", card->name, target_enemy, target_ally, upgrade_level, card->channel);
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

    if (card->class == CLASS_BARD &&
        !cs->bard_first_draw_used &&
        combat_class_has_perk(cs, CLASS_BARD, PERK_BARD_FIRST_DRAW))
    {
        cs->bard_first_draw_used = true;
        deck_draw(&cs->deck);
        combat_feed_add(cs, "Encore: drew 1");
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
    bool echo_this_card = card->echo ||
        (!cs->echo_used && card->cost >= 1 && relic_has(g_state.relics, g_state.relic_count, RELIC_ECHO_BELL));
    if (echo_this_card)
    {
        cs->echo_used = true;
        combat_feed_add(cs, "%s echoed", card->name);
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
        dmg = card_damage(card, upgrade_level) + meta_dmg_bonus(&g_state.meta) +
            combat_class_perk_count(cs, card->class, PERK_CARD_DMG_1);
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
            apply_damage_to_enemy(cs, ei, dmg);
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
    if (dmg > 0 &&
        card->class == CLASS_MAGE &&
        !cs->mage_first_spell_used &&
        combat_class_has_perk(cs, CLASS_MAGE, PERK_MAGE_FIRST_SPELL_DMG))
    {
        dmg += 1;
        cs->mage_first_spell_used = true;
        combat_feed_add(cs, "Opening Spark: +1 damage");
    }
    if (dmg > 0 &&
        card->class == CLASS_RANGER &&
        marked_target &&
        !cs->ranger_marked_dmg_used &&
        combat_class_has_perk(cs, CLASS_RANGER, PERK_RANGER_MARKED_DMG))
    {
        dmg += 2;
        cs->ranger_marked_dmg_used = true;
        combat_feed_add(cs, "True Shot: +2 damage");
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
                    apply_damage_to_enemy(cs, i, per_target_damage);
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
                apply_damage_to_enemy(cs, target_enemy, dmg);

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
                    apply_damage_to_enemy(cs, arc, splash);
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
                    apply_damage_to_enemy(cs, arc, dmg / 2);
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
                    apply_damage_to_enemy(cs, ei, dmg / 2);
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
                    apply_damage_to_enemy(cs, ei, dmg);
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

    if (card->interrupt && target_enemy >= 0)
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

    if (card->taunt)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            int highest = 0;
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive && cs->party.members[i].aggro > highest)
                    highest = cs->party.members[i].aggro;
            cs->party.members[caster].aggro = highest + 30;
            LOG_I(CAT_CARD, "Taunt: caster aggro set to %d", cs->party.members[caster].aggro);
            if (!cs->guardian_taunt_shield_used &&
                combat_class_has_perk(cs, CLASS_GUARDIAN, PERK_GUARDIAN_TAUNT_SHIELD))
            {
                cs->guardian_taunt_shield_used = true;
                apply_shield_to_ally(cs, caster, 4);
                combat_feed_add(cs, "Anchor Stance: +4 Shield");
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
    if (have_sneaky_steal && dmg > 0)
    {
        int caster = -1;
        find_caster(cs, card->class, &caster);
        if (caster >= 0)
        {
            int steal = dmg / 10;
            if (steal < 1) steal = 1;
            apply_heal_to_ally(cs, caster, steal);
            combat_feed_add(cs, "Sneaky Steal: healed %d", steal);
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
    else if (card->exhaust)
        deck_exhaust_index(&cs->deck, hand_idx);
    else
        deck_discard_index(&cs->deck, hand_idx);

    if (card->class != CLASS_NONE)
    {
        combo_check_chain(cs, previous_class, card->class);
        cs->last_played_class = card->class;
    }

    // ── Echo: resolve again at 50% if flagged ──
    if (echo_this_card && echo_this_card == true)
    {
        echo_this_card = false;
        int half_dmg = card->damage / 2;
        int half_hl = card->heal / 2;
        int half_sh = card->shield / 2;
        // Create a temporary card def with halved values
        CardDef echo_card = *card;
        echo_card.damage = half_dmg;
        echo_card.heal = half_hl;
        echo_card.shield = half_sh;
        echo_card.echo = false; // prevent infinite loop
        // Find a random valid target
        int echo_target_enemy = -1;
        int echo_target_ally = -1;
        if (card->target == TARGET_ENEMY || card->target == TARGET_ALL_ENEMIES)
        {
            // Pick a random living enemy
            int alive[MAX_ENEMIES], alive_count = 0;
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                    alive[alive_count++] = i;
            if (alive_count > 0)
                echo_target_enemy = alive[rand() % alive_count];
        }
        else if (card->target == TARGET_ALLY || card->target == TARGET_ALL_ALLIES || card->target == TARGET_SELF)
        {
            int alive[MAX_PARTY_SIZE], alive_count = 0;
            for (int i = 0; i < cs->party.count; i++)
                if (cs->party.members[i].alive)
                    alive[alive_count++] = i;
            if (alive_count > 0)
                echo_target_ally = alive[rand() % alive_count];
            if (card->target == TARGET_SELF && echo_target_ally >= 0)
                echo_target_ally = 0;
        }
        if (echo_target_enemy >= 0 || echo_target_ally >= 0)
        {
            LOG_D(CAT_CARD, "Echo: resolving copy at 50%%");
            combat_feed_add(cs, "Echo copy resolves");
            resolve_card_on_target(cs, hand_idx, echo_target_enemy, echo_target_ally, 0);
            // Restore original card def pointer for chain detection
            cs->deck.cards[cs->deck.hand[hand_idx]].def = card;
        }
    }

    cs->resolving_card_class = CLASS_NONE;
}

// ── Check win/loss ──────────────────────────────────────────

static void check_victory(CombatState *cs)
{
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].def && cs->enemies[i].hp > 0) return;
    cs->phase = COMBAT_VICTORY;
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
    strcpy(cs->result_message, "Party wiped. Click to continue.");
    combat_feed_add(cs, "Party wiped");
    LOG_I(CAT_COMBAT, "=== DEFEAT ===");
}

// ── Enemy AI ────────────────────────────────────────────────

static void enemy_action(EnemyState *e, CombatState *cs, int target_enemy, int target_ally)
{
    e->cast_pending = false;
    if (!e->def) return;
    int ci = e->intent.ability_idx;
    if (ci < 0 || ci >= e->def->card_count) return;

    const EnemyCardDef *cd = &e->def->cards[ci];
    int damage = (int)(cd->base_damage * cs->floor_scale * cs->enemy_damage_scale);

    // Snare Trap reduces damage
    int trap_idx = status_find(e->statuses, e->status_count, STATUS_TRAP);
    if (trap_idx >= 0)
    {
        damage -= e->statuses[trap_idx].value;
        if (damage < 0) damage = 0;
        LOG_I(CAT_CARD, "  Snare Trap reduces damage by %d", e->statuses[trap_idx].value);
    }

    if (cd->intent == INTENT_HEAL)
    {
        if (target_enemy >= 0) apply_heal_to_enemy(cs, target_enemy, cd->heal_amount);
        return;
    }

    if (cd->intent == INTENT_SHIELD || cd->intent == INTENT_BUFF)
    {
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), cd->shield_amount);
        return;
    }

    if (cd->intent == INTENT_AOE || cd->intent == INTENT_WIPE)
    {
        for (int i = 0; i < cs->party.count; i++)
        {
            apply_damage_to_ally(cs, i, damage, e->def->name);
            if (cd->status != STATUS_NONE && cd->status_turns > 0)
                apply_status_to_ally(cs, i, cd->status, cd->status_turns, cd->status_amount);
        }
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET))
            apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);
        check_defeat(cs);
        return;
    }

    int repeats = 1;
    if (strcmp(cd->name, "Dual Strike") == 0 || strcmp(cd->name, "Rapid Strikes") == 0)
        repeats = 2;

    for (int hit = 0; hit < repeats; hit++)
    {
        int target = target_ally;
        if (target < 0 || target >= cs->party.count || !cs->party.members[target].alive)
            target = party_highest_aggro(&cs->party);
        if (target < 0) break;
        apply_damage_to_ally(cs, target, damage, e->def->name);
        if (hit == 0 && cd->status != STATUS_NONE && cd->status_turns > 0)
            apply_status_to_ally(cs, target, cd->status, cd->status_turns, cd->status_amount);
    }
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET) && damage > 0)
        apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);

    if (cd->shield_amount > 0 && e->hp > 0)
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), cd->shield_amount);

    if (cd->heal_amount > 0 && e->hp > 0)
        apply_heal_to_enemy(cs, (int)(e - cs->enemies), cd->heal_amount);

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
    if (e->interrupted_recently && e->last_interrupted_ability == card_idx)
        cast--;
    if (cast < 1) cast = 1;
    return cast;
}


// ── Turn progression ────────────────────────────────────────

static void advance_turn(CombatState *cs)
{
    cs->turn++;
    LOG_I(CAT_COMBAT, "=== Turn %d ===", cs->turn);
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

    // Tick all statuses down and remove expired
    for (int ei = 0; ei < cs->enemy_count; ei++)
        status_tick(cs->enemies[ei].statuses, &cs->enemies[ei].status_count);
    for (int i = 0; i < cs->party.count; i++)
        status_tick(cs->party.members[i].statuses, &cs->party.members[i].status_count);

    for (int i = 0; i < cs->party.count; i++)
        if (cs->party.members[i].alive)
        {
            cs->party.members[i].aggro -= 5;
            if (cs->party.members[i].aggro < 0) cs->party.members[i].aggro = 0;
        }

    for (int ei = 0; ei < cs->enemy_count; ei++)
    {
        EnemyState *e = &cs->enemies[ei];
        if (!e->def || e->hp <= 0) continue;

        if (e->interrupt_cooldown > 0)
            e->interrupt_cooldown--;

        if (g_state.encounter_is_boss && e->phase == 0 && e->hp * 2 <= e->max_hp)
        {
            e->phase = 1;
            e->shield += 10;
            if (e->intent.ability_idx >= 0 && e->intent.remaining_turns > 1)
                e->intent.remaining_turns--;
            combat_feed_add(cs, "%s enraged", e->def->name);
        }

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
            if (e->intent.remaining_turns <= 0)
            {
                int ci = e->intent.ability_idx;
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
            int hand_size = e->def->hand_size;
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
                if (cd->cost > e->energy_current) continue;
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
                e->current_ability++;
                e->energy_current -= e->def->cards[ci].cost;
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
        cs->energy.current -= drain;
        if (cs->energy.current < 0) cs->energy.current = 0;
        combat_feed_add(cs, "Energy drain: -%d", drain);
    }
    bool boon_active = cs->boon_turns_remaining > 0;
    if (boon_active && cs->boon_energy_bonus > 0)
    {
        cs->energy.current += cs->boon_energy_bonus;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        combat_feed_add(cs, "Shop boon: +%d energy", cs->boon_energy_bonus);
    }
    deck_discard_hand(&cs->deck);
    if (boon_active && cs->boon_draw_bonus > 0)
        combat_feed_add(cs, "Shop boon: +%d draw", cs->boon_draw_bonus);
    deal_cards(&cs->deck, cs->turn_draw_count + (boon_active ? cs->boon_draw_bonus : 0));
    if (boon_active)
        cs->boon_turns_remaining--;

    cs->target_mode = TGT_NONE;
    cs->phase = COMBAT_PLAYER_TURN;
    combat_set_turn_banner(cs, "PLAYER TURN");
}

static void combat_end_turn_internal(CombatState *cs)
{
    if (cs->deck.hand_count <= 0 &&
        relic_has(g_state.relics, g_state.relic_count, RELIC_STEADFAST_BANNER))
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                cs->party.members[i].shield += 4;
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
    cs->turn_draw_count = party_draw_count(cs->party.count) + meta_first_draw_bonus(&g_state.meta);
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
    LOG_T("  hand dealt: hand_count=%d draw_count=%d", cs->deck.hand_count, cs->deck.draw_count);

    int start_energy = party_start_energy(cs->party.count);
    if (asc >= 2) start_energy--;
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
    if (asc >= 1) cs->enemy_damage_scale += 0.10f;
    if (asc >= 6) cs->enemy_damage_scale += 0.15f;
    if (asc >= 10) cs->enemy_damage_scale += 0.25f;
    cs->phoenix_used = false;
    cs->echo_used = false;
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

    for (int i = 0; i < cs->party.count; i++)
    {
        PartyMember *pm = &cs->party.members[i];
        if (!pm->alive) continue;
        int shield_perks = party_member_perk_count(pm, PERK_STARTING_SHIELD_1);
        if (shield_perks > 0)
        {
            pm->shield += shield_perks;
            combat_feed_add(cs, "%s: +%d starting Shield", pm->name, shield_perks);
        }
    }

    LOG_T("  setting up enemies");

    cs->enemy_count = (encounter && encounter->count > 0) ? encounter->count : 1;
    int party_enemy_cap = cs->party.count <= 1 ? 1 : (cs->party.count == 2 ? 2 : MAX_ENEMIES);
    if (cs->enemy_count > party_enemy_cap)
        cs->enemy_count = party_enemy_cap;
    if (cs->enemy_count > MAX_ENEMIES) cs->enemy_count = MAX_ENEMIES;

    for (int i = 0; i < cs->enemy_count; i++)
    {
        const EnemyDef *ed = (encounter && encounter->enemies[i]) ? encounter->enemies[i] : enemy_def_by_id("flame_imp");
        if (!ed) continue;
        LOG_T("  enemy[%d]: %s HP=%d", i, ed->name, ed->hp);
        int scaled_hp = (int)(ed->hp * cs->floor_scale);
        cs->enemies[i].def = ed;
        cs->enemies[i].hp = scaled_hp;
        cs->enemies[i].max_hp = scaled_hp;
        cs->enemies[i].shield = 0;
        cs->enemies[i].current_ability = i * 2;
        cs->enemies[i].last_interrupted_ability = -1;
        cs->enemies[i].interrupt_cooldown = 0;
        cs->enemies[i].interrupted_recently = false;
        cs->enemies[i].cast_pending = false;
        cs->enemies[i].phase = 0;
        cs->enemies[i].intent.ability_idx = -1;
        cs->enemies[i].intent.remaining_turns = 0;

        // Build and shuffle the enemy's card deck
        cs->enemies[i].energy_current = 0;
        cs->enemies[i].energy_max = ed->energy_per_turn;
        cs->enemies[i].deck_count = 0;
        cs->enemies[i].deck_top = 0;
        cs->enemies[i].hand_count = 0;
        cs->enemies[i].discard_count = 0;
        int deck_pos = 0;
        for (int c = 0; c < ed->card_count; c++)
        {
            for (int copy = 0; copy < ed->cards[c].count && deck_pos < ENEMY_DECK_SIZE; copy++)
                cs->enemies[i].deck[deck_pos++] = c;
        }
        cs->enemies[i].deck_count = deck_pos;
        // Fisher-Yates shuffle
        for (int s = deck_pos - 1; s > 0; s--)
        {
            int j = rand() % (s + 1);
            int tmp = cs->enemies[i].deck[s];
            cs->enemies[i].deck[s] = cs->enemies[i].deck[j];
            cs->enemies[i].deck[j] = tmp;
        }
    }
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

void combat_end_turn(CombatState *cs)
{
    if (cs->target_mode != TGT_NONE)
    {
        cs->energy.current += cs->target_paid_cost;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        cs->target_mode = TGT_NONE;
        cs->target_hand_idx = -1;
        cs->target_paid_cost = 0;
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
        case TARGET_ALL_ENEMIES:
            cs->target_mode = TGT_SELECT_ENEMY;
            break;
        case TARGET_ALLY:
        case TARGET_ALL_ALLIES:
            cs->target_mode = TGT_SELECT_ALLY;
            break;
        case TARGET_SELF:
        {
            int caster = -1;
            find_caster(cs, cs->deck.cards[cs->deck.hand[hand_idx]].def->class, &caster);
            resolve_card_on_target(cs, hand_idx, -1, caster >= 0 ? caster : 0, cs->target_paid_cost);
            cs->target_mode = TGT_NONE;
            cs->target_hand_idx = -1;
            cs->target_paid_cost = 0;
            check_victory(cs);
            if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);
            if (cs->phase == COMBAT_DEFEAT) return;
            break;
        }
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
        return;
    }

    int effective_cost = combat_effective_card_cost(cs, card);
    if (cs->energy.current < effective_cost) return;

    energy_spend(&cs->energy, effective_cost);
    cs->target_paid_cost = effective_cost;

    LOG_D(CAT_CARD, "handle_card_click: card=%s target=%d energy=%d", card->name, (int)card->target, cs->energy.current);

    if (card->target == TARGET_ENEMY || card->target == TARGET_ALLY)
    {
        LOG_D(CAT_CARD, "  -> entering targeting mode");
        start_targeting(cs, hand_idx, card->target);
    }
    else
    {
        LOG_D(CAT_CARD, "  -> resolving immediately (target=%d)", (int)card->target);
        resolve_card_on_target(cs, hand_idx, -1, -1, effective_cost);
        cs->target_paid_cost = 0;
        check_victory(cs);
        if (cs->phase == COMBAT_VICTORY) return;
        check_defeat(cs);
        if (cs->phase == COMBAT_DEFEAT) return;
    }
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
            const CardDef *card = cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def;
            (void)card;
            cs->energy.current += cs->target_paid_cost;
            if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_paid_cost = 0; cs->target_offset = 0;
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
            const CardDef *card = cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def;
            (void)card;
            cs->energy.current += cs->target_paid_cost;
            if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_paid_cost = 0; cs->target_offset = 0;
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
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                handle_card_click(cs, i);
            break;
        }
    }

    Rectangle end_btn = layout_end_turn_button();
    if (CheckCollisionPointRec(mouse, end_btn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        combat_end_turn_internal(cs);
}




