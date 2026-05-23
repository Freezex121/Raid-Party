#include "combat.h"
#include "game.h"
#include "data/area_defs.h"
#include "data/card_defs.h"
#include "data/enemy_defs.h"
#include "systems/relic.h"
#include "ui/floating_text.h"
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

static void deal_opening_hand(Deck *deck)
{
    for (int i = 0; i < 5; i++)
        if (deck_draw(deck) < 0) break;
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

static void apply_damage_to_enemy(CombatState *cs, int enemy_idx, int damage)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return;

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
    ft_spawn_shake((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 8, (Color){ 255, 80, 80, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)e->pos_y, (Color){ 255, 85, 65, 255 }, 6);

    LOG_I(CAT_CARD, "  enemy[%d] %s: %d damage (%d HP)", enemy_idx, e->def->name, damage, e->hp);
}

static void apply_heal_to_ally(CombatState *cs, int ally_idx, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;
    pm->hp += amount;
    if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;

    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", amount);
    Vector2 p = party_feedback_pos(cs, ally_idx);
    ft_spawn(p.x, p.y, buf, 8, (Color){ 80, 255, 80, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 95, 245, 135, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d HP (%d)", ally_idx, pm->name, amount, pm->hp);
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
    ft_spawn(p.x, p.y, buf, 8, (Color){ 100, 200, 255, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 115, 190, 255, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d shield (%d)", ally_idx, pm->name, amount, pm->shield);
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
    ft_spawn(p.x, p.y, buf, 9, (Color){ 120, 255, 160, 255 });

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
    ft_spawn_shake(p.x, p.y, buf, 8, (Color){ 255, 90, 90, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 255, 85, 75, 255 }, 5);

    LOG_I(CAT_COMBAT, "%s hits %s for %d (%d -> %d)", source, pm->name, before - pm->hp, before, pm->hp);
    combat_feed_add(cs, "%s hit %s for %d", source, pm->name, before - pm->hp);

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
            ft_spawn(p.x - 18.0f, p.y + 7.0f, "REVIVED", 7, (Color){ 255, 150, 50, 255 });
        }
        else
        {
            pm->alive = false;
            pm->aggro = 0;
            pm->shield = 0;
            LOG_I(CAT_COMBAT, "%s DOWNED! Removing %s cards.", pm->name, class_name(pm->class));
            combat_feed_add(cs, "%s is downed", pm->name);
            deck_remove_class_cards(&cs->deck, pm->class);
            ft_spawn(p.x - 18.0f, p.y + 7.0f, "DOWNED", 8, (Color){ 240, 80, 80, 255 });
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
    ft_spawn((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 7, (Color){ 90, 240, 130, 255 });
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
    ft_spawn((float)(e->pos_x - 7), (float)(e->pos_y - 18), buf, 7, (Color){ 100, 180, 255, 255 });
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
        ft_spawn((float)(e->pos_x - 14), (float)(e->pos_y - 25), "NO CAST", 6, (Color){ 180, 180, 200, 255 });
        return false;
    }

    const EnemyAbility *ab = &e->def->abilities[e->intent.ability_idx];
    if (ab->is_wipe || ab->intent == INTENT_WIPE)
    {
        ft_spawn((float)(e->pos_x - 14), (float)(e->pos_y - 25), "IMMUNE", 6, (Color){ 230, 120, 80, 255 });
        LOG_I(CAT_CARD, "  %s resisted interrupt on %s", e->def->name, ab->name);
        return false;
    }

    LOG_I(CAT_CARD, "  %s interrupted %s", e->def->name, ab->name);
    combat_feed_add(cs, "%s was interrupted", e->def->name);
    ft_spawn_shake((float)(e->pos_x - 21), (float)(e->pos_y - 26), "INTERRUPTED", 7, (Color){ 220, 120, 255, 255 });
    vfx_spawn_burst((float)e->pos_x, (float)(e->pos_y - 15), (Color){ 220, 120, 255, 255 }, 7);
    e->intent.ability_idx = -1;
    e->intent.remaining_turns = 0;
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

static void combat_spawn_card_throw(CombatState *cs, int hand_idx, const CardDef *card, bool upgraded, int target_enemy, int target_ally)
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
    anim->upgraded = upgraded;
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
        theme_draw_card_art(r, anim->card, anim->upgraded);
        DrawRectangleLinesEx(r, 1.0f, (Color){ 255, 245, 190, 220 });
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
    }
    return "Status";
}

static void apply_status_to_enemy(CombatState *cs, int enemy_idx, StatusType status, int turns, int amount)
{
    if (enemy_idx < 0 || enemy_idx >= cs->enemy_count) return;
    EnemyState *e = &cs->enemies[enemy_idx];
    if (!e->def || e->hp <= 0) return;

    status_apply(e->statuses, &e->status_count, status, turns, amount);
    LOG_I(CAT_CARD, "  enemy[%d]: +%s (%d for %d turns)", enemy_idx, status_label(status), amount, turns);
    combat_feed_add(cs, "%s: %s", e->def->name, status_label(status));
}

static void apply_status_to_ally(CombatState *cs, int ally_idx, StatusType status, int turns, int amount)
{
    if (ally_idx < 0 || ally_idx >= cs->party.count) return;
    PartyMember *pm = &cs->party.members[ally_idx];
    if (!pm->alive) return;

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
            for (int i = 0; i < cs->party.count; i++)
                apply_status_to_ally(cs, i, effect->status, effect->turns, effect->amount);
            break;

        case CARD_EFFECT_RESET_CASTER_AGGRO:
        {
            int caster = -1;
            find_caster(cs, card->class, &caster);
            if (caster >= 0)
            {
                cs->party.members[caster].aggro = 0;
                ft_spawn(22.0f, 176.0f, "AGGRO RESET", 8, (Color){ 120, 220, 160, 255 });
                LOG_I(CAT_CARD, "  %s: aggro reset", card->name);
                combat_feed_add(cs, "%s reset aggro", cs->party.members[caster].name);
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
                ft_spawn(22.0f, 176.0f, "AGGRO TRANSFER", 8, (Color){ 180, 180, 220, 255 });
                LOG_I(CAT_CARD, "  %s: moved %d aggro to Guardian", card->name, transfer);
                combat_feed_add(cs, "Aggro moved to Guardian");
            }
            else
            {
                cs->party.members[target_ally].aggro = 0;
                LOG_I(CAT_CARD, "  %s: reduced ally aggro by %d", card->name, transfer);
                combat_feed_add(cs, "Aggro cleared");
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

static void apply_relic_combat_start(CombatState *cs)
{
    if (!cs) return;

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_BATTLE_DRUM))
    {
        cs->energy.current += 1;
        if (cs->energy.current > cs->energy.max) cs->energy.current = cs->energy.max;
        combat_feed_add(cs, "Battle Drum: +1 energy");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_SCOUTING_MAP))
    {
        deck_draw(&cs->deck);
        combat_feed_add(cs, "Scouting Map: drew 1");
    }

    if (relic_has(g_state.relics, g_state.relic_count, RELIC_WARD_STONE))
    {
        for (int i = 0; i < cs->party.count; i++)
            if (cs->party.members[i].alive)
                cs->party.members[i].shield += 4;
        combat_feed_add(cs, "Ward Stone: +4 Shield");
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

static void resolve_card_on_target(CombatState *cs, int hand_idx, int target_enemy, int target_ally)
{
    if (hand_idx < 0 || hand_idx >= cs->deck.hand_count) return;

    CardInstance *inst = &cs->deck.cards[cs->deck.hand[hand_idx]];
    const CardDef *card = inst->def;
    if (!card || !card->name) return;
    int played_uid = inst->uid;

    bool ug = inst->upgraded;

    int dmg = card_damage(card, ug);
    int hl  = card_heal(card, ug);
    int sh  = card_shield(card, ug);

    LOG_I(CAT_CARD, "Playing %s (enemy=%d, ally=%d) upgraded=%d channel=%d", card->name, target_enemy, target_ally, ug, card->channel);
    combat_spawn_card_throw(cs, hand_idx, card, ug, target_enemy, target_ally);
    combat_flash_played_card(cs, card, target_enemy, target_ally);
    combat_feed_add(cs, "Played %s", card->name);

    // ── Combo check ─────────────────────────────────────────
    float combo_mult = 1.0f;
    if (card->class == CLASS_NONE)
    {
        // Utility cards don't affect combo
    }
    else if (cs->combo_class == card->class && card->cost > cs->combo_last_cost)
    {
        cs->combo_count++;
        combo_mult = 1.0f + cs->combo_count * 0.10f;
        // Animate combo UI
        cs->combo_scale = 1.0f;
        cs->combo_tween = tween_create(&cs->combo_scale, 1.3f, 0.15f, EASE_OUT_BACK);
        tween_chain(cs->combo_tween, &cs->combo_scale, 1.0f, 0.3f, EASE_OUT_ELASTIC);
        cs->combo_shake = 0;
        tween_create(&cs->combo_shake, 1.0f, 0.08f, EASE_OUT_QUAD);
        LOG_I(CAT_CARD, "  COMBO x%d (+%d%%)", cs->combo_count + 1, (int)((combo_mult - 1.0f) * 100));
    }
    else
    {
        cs->combo_count = 0;
        cs->combo_class = card->class;
    }
    if (card->class != CLASS_NONE)
        cs->combo_last_cost = card->cost;

    // Apply multiplier
    dmg = (int)(dmg * combo_mult);
    hl  = (int)(hl * combo_mult);
    sh  = (int)(sh * combo_mult);

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

    // ── Echo Bell: double effects of first card played (cost 1+) ──
    float echo_mult = 1.0f;
    if (!cs->echo_used && card->cost >= 1 && relic_has(g_state.relics, g_state.relic_count, RELIC_ECHO_BELL))
    {
        echo_mult = 2.0f;
        cs->echo_used = true;
        combat_feed_add(cs, "Echo Bell: doubled");
    }

    // Apply multiplier with echo
    dmg = (int)(dmg * echo_mult);
    hl  = (int)(hl * echo_mult);
    sh  = (int)(sh * echo_mult);

    // ── Utility card effects ────────────────────────────────
    // Channel cards don't resolve immediately — they start a channel instead
    if (!card->channel)
    {
        if (card->target == TARGET_ALL_ENEMIES)
        {
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].def && cs->enemies[i].hp > 0)
                    apply_damage_to_enemy(cs, i, dmg);
        }
        else if (dmg > 0 && target_enemy >= 0)
        {
            int repeat_hits = card_repeat_hits(card);

            for (int hit = 0; hit < repeat_hits; hit++)
                apply_damage_to_enemy(cs, target_enemy, dmg);
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
    } // end if(!card->channel)

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
}

// ── Check win/loss ──────────────────────────────────────────

static void check_victory(CombatState *cs)
{
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].def && cs->enemies[i].hp > 0) return;
    cs->phase = COMBAT_VICTORY;
    strcpy(cs->result_message, "VICTORY! Click to continue.");
    combat_feed_add(cs, "Victory");

    // Gold popup immediately on victory
    if (!cs->gold_spawned)
    {
        int gold_gain = g_state.encounter_is_boss ? 50 : (g_state.encounter_is_elite ? 25 : 10);
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_GILDED_CHARM))
            gold_gain += 8;
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_RABBIT_FOOT) && (rand() % 10) == 0)
            gold_gain *= 2;
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

static void enemy_action(EnemyState *e, CombatState *cs)
{
    if (!e->def) return;
    int ab_idx = e->intent.ability_idx;
    if (ab_idx < 0 || ab_idx >= e->def->ability_count) return;

    const EnemyAbility *ab = &e->def->abilities[ab_idx];
    int damage = (int)(ab->base_damage * cs->floor_scale);

    // Snare Trap reduces damage
    int trap_idx = status_find(e->statuses, e->status_count, STATUS_TRAP);
    if (trap_idx >= 0)
    {
        damage -= e->statuses[trap_idx].value;
        if (damage < 0) damage = 0;
        LOG_I(CAT_CARD, "  Snare Trap reduces damage by %d", e->statuses[trap_idx].value);
    }

    if (ab->intent == INTENT_HEAL)
    {
        int target = enemy_lowest_hp(cs);
        if (target >= 0) apply_heal_to_enemy(cs, target, ab->heal_amount);
        return;
    }

    if (ab->intent == INTENT_SHIELD || ab->intent == INTENT_BUFF)
    {
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), ab->shield_amount);
        return;
    }

    if (ab->intent == INTENT_AOE || ab->intent == INTENT_WIPE)
    {
        for (int i = 0; i < cs->party.count; i++)
            apply_damage_to_ally(cs, i, damage, e->def->name);
        if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET))
            apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);
        check_defeat(cs);
        return;
    }

    int repeats = 1;
    if (strcmp(ab->name, "Dual Strike") == 0 || strcmp(ab->name, "Rapid Strikes") == 0)
        repeats = 2;

    for (int hit = 0; hit < repeats; hit++)
    {
        int target = party_highest_aggro(&cs->party);
        if (target < 0) break;
        apply_damage_to_ally(cs, target, damage, e->def->name);
    }
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET) && damage > 0)
        apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);

    if (ab->shield_amount > 0 && e->hp > 0)
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), ab->shield_amount);

    if (ab->heal_amount > 0 && e->hp > 0)
        apply_heal_to_enemy(cs, (int)(e - cs->enemies), ab->heal_amount);

    check_defeat(cs);
}

static int choose_enemy_intent(EnemyState *e)
{
    if (!e->def || e->def->ability_count == 0) return -1;
    return e->current_ability % e->def->ability_count;
}

// ── Turn progression ────────────────────────────────────────

static void advance_turn(CombatState *cs)
{
    cs->turn++;
    LOG_I(CAT_COMBAT, "=== Turn %d ===", cs->turn);

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
                        apply_damage_to_enemy(cs, ei, card_damage(cc, false));
            if (cc->heal > 0)
                for (int i = 0; i < cs->party.count; i++)
                    if (cs->party.members[i].alive)
                        apply_heal_to_ally(cs, i, card_heal(cc, false));
            if (cc->shield > 0)
                for (int i = 0; i < cs->party.count; i++)
                    if (cs->party.members[i].alive)
                        apply_shield_to_ally(cs, i, card_shield(cc, false));
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
            ft_spawn((float)(cs->enemies[ei].pos_x + 4), (float)(cs->enemies[ei].pos_y - 18), bbuf, 7, (Color){ 255, 150, 50, 255 });
            vfx_spawn_burst((float)cs->enemies[ei].pos_x, (float)(cs->enemies[ei].pos_y - 5), (Color){ 255, 135, 45, 255 }, 4);
            LOG_I(CAT_CARD, "  enemy[%d] Burning: %d damage", ei, burn_dmg);
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

        if (e->intent.ability_idx >= 0)
        {
            e->intent.remaining_turns--;
            if (e->intent.remaining_turns <= 0)
            {
                enemy_action(e, cs);
                e->intent.ability_idx = -1;
            }
        }

        if (e->intent.ability_idx < 0)
        {
            int idx = choose_enemy_intent(e);
            if (idx >= 0)
            {
                e->intent.ability_idx = idx;
                e->intent.remaining_turns = e->def->abilities[idx].cast_time;
                e->current_ability++;
            }
        }
    }

    check_defeat(cs);
    if (cs->phase == COMBAT_DEFEAT) return;
    check_victory(cs);
    if (cs->phase == COMBAT_VICTORY) return;

    energy_refresh(&cs->energy);
    deck_discard_hand(&cs->deck);
    for (int i = 0; i < 5; i++)
        if (deck_draw(&cs->deck) < 0) break;

    cs->target_mode = TGT_NONE;
    cs->phase = COMBAT_PLAYER_TURN;
    combat_set_turn_banner(cs, "PLAYER TURN");
}

static void combat_end_turn_internal(CombatState *cs)
{
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
    LOG_T("  party created: %d members, run_deck has %d cards", cs->party.count, g_state.run_deck.card_count);

    memcpy(&cs->deck, &g_state.run_deck, sizeof(Deck));
    deck_prepare_for_combat(&cs->deck);
    for (int i = 0; i < cs->party.count; i++)
        if (!cs->party.members[i].alive)
            deck_remove_class_cards(&cs->deck, cs->party.members[i].class);
    LOG_T("  deck copied: card_count=%d draw_count=%d", cs->deck.card_count, cs->deck.draw_count);

    deal_opening_hand(&cs->deck);
    LOG_T("  hand dealt: hand_count=%d draw_count=%d", cs->deck.hand_count, cs->deck.draw_count);

    LOG_T("  calling energy_init(%d, %d, %d)", 4, MAX_ENERGY, BASE_REGEN);
    energy_init(&cs->energy, 4, MAX_ENERGY, BASE_REGEN);
    LOG_T("  energy: %d/%d regen=%d", cs->energy.current, cs->energy.max, cs->energy.regen);

    cs->turn = 0;
    cs->hovered_card = -1;
    cs->target_mode = TGT_NONE;
    cs->target_hand_idx = -1;
    cs->hovered_enemy = -1;
    cs->hovered_ally = -1;
    cs->gold_spawned = false;
    cs->floor_scale = area_difficulty_scale(g_state.current_area) * (1.0f + 0.12f * (float)g_state.map.floor);
    cs->phoenix_used = false;
    cs->echo_used = false;
    cs->mana_gem_bonus = relic_has(g_state.relics, g_state.relic_count, RELIC_MANA_GEM) ? 1 : 0;
    cs->channel_card = NULL;
    cs->channel_remaining = 0;
    cs->channel_class = CLASS_NONE;
    cs->target_offset = 0;
    cs->target_offset_tween = -1;
    cs->combo_class = CLASS_NONE;
    cs->combo_last_cost = -1;
    cs->combo_count = 0;
    cs->combo_scale = 1.0f;
    cs->combo_tween = -1;
    cs->combo_shake = 0;
    cs->turn_banner_timer = 0.0f;
    cs->turn_banner_text[0] = '\0';
    cs->enemy_banner_timer = 0.0f;
    cs->end_turn_flash = 0.0f;
    cs->play_flash_timer = 0.0f;
    cs->play_flash_text[0] = '\0';
    for (int i = 0; i < 5; i++)
    {
        cs->action_feed[i][0] = '\0';
        cs->action_feed_timer[i] = 0.0f;
    }

    apply_relic_combat_start(cs);

    LOG_T("  setting up enemies");

    cs->enemy_count = (encounter && encounter->count > 0) ? encounter->count : 1;
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
        cs->enemies[i].intent.ability_idx = -1;
        cs->enemies[i].intent.remaining_turns = 0;
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
        cs->target_mode = TGT_NONE;
        cs->target_hand_idx = -1;
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
            resolve_card_on_target(cs, hand_idx, -1, caster >= 0 ? caster : 0);
            cs->target_mode = TGT_NONE;
            cs->target_hand_idx = -1;
            check_victory(cs);
            if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);
            if (cs->phase == COMBAT_DEFEAT) return;
            if (cs->energy.current <= 0 || cs->deck.hand_count == 0)
                combat_end_turn_internal(cs);
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

    if (cs->energy.current < card->cost) return;

    energy_spend(&cs->energy, card->cost);

    LOG_D(CAT_CARD, "handle_card_click: card=%s target=%d energy=%d", card->name, (int)card->target, cs->energy.current);

    if (card->target == TARGET_ENEMY || card->target == TARGET_ALLY)
    {
        LOG_D(CAT_CARD, "  -> entering targeting mode");
        start_targeting(cs, hand_idx, card->target);
    }
    else
    {
        LOG_D(CAT_CARD, "  -> resolving immediately (target=%d)", (int)card->target);
        resolve_card_on_target(cs, hand_idx, -1, -1);
        check_victory(cs);
        if (cs->phase == COMBAT_VICTORY) return;
        check_defeat(cs);
        if (cs->phase == COMBAT_DEFEAT) return;
        if (cs->energy.current <= 0 || cs->deck.hand_count == 0)
            combat_end_turn_internal(cs);
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

    LOG_T("CU: getting mouse");
    Vector2 mouse = GetMousePosition();
    cs->hovered_card = -1;

    // ── Targeting mode ──────────────────────────────────────
    if (cs->target_mode == TGT_SELECT_ENEMY)
    {
        cs->hovered_enemy = hit_test_enemies(cs, mouse);
        if (cs->hovered_enemy >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            resolve_card_on_target(cs, cs->target_hand_idx, cs->hovered_enemy, -1);
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_offset = 0;
            check_victory(cs); if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);  if (cs->phase == COMBAT_DEFEAT) return;
            if (cs->energy.current <= 0 || cs->deck.hand_count == 0) combat_end_turn_internal(cs);
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            cs->energy.current += cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def->cost;
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_offset = 0;
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
                ft_spawn(244.0f, 222.0f, "INVALID TARGET", 8, (Color){ 230, 90, 90, 255 });
                return;
            }
            resolve_card_on_target(cs, cs->target_hand_idx, -1, cs->hovered_ally);
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_offset = 0;
            check_victory(cs); if (cs->phase == COMBAT_VICTORY) return;
            check_defeat(cs);  if (cs->phase == COMBAT_DEFEAT) return;
            if (cs->energy.current <= 0 || cs->deck.hand_count == 0) combat_end_turn_internal(cs);
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            cs->energy.current += cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def->cost;
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_offset = 0;
        }
        return;
    }

    // ── Normal card interaction ────────────────────────────
    HandLayout hand_layout = layout_hand(cs->deck.hand_count);

    for (int i = cs->deck.hand_count - 1; i >= 0; i--)
    {
        Rectangle r = layout_hand_card_rect(hand_layout, i);
        r.y -= 34.0f;
        r.height += 34.0f;
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




