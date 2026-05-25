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
    ft_spawn(p.x, p.y, buf, 10, (Color){ 80, 255, 80, 255 });
    vfx_spawn_burst(p.x, p.y - 8.0f, (Color){ 95, 245, 135, 255 }, 4);

    LOG_I(CAT_CARD, "  ally[%d] %s: +%d HP (%d)", ally_idx, pm->name, amount, pm->hp);

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

    const EnemyAbility *ab = &e->def->abilities[e->intent.ability_idx];
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
    if (cs->combo_prime == COMBO_PRIME_SHADOW_DANCE && card_is_heal_card(card))
        return 0;
    if (cs->combo_prime == COMBO_PRIME_ELEMENTAL_FURY && card_is_fire_spell(card))
        return 0;
    return card->cost;
}

static void combo_prime_set(CombatState *cs, ComboPrime prime, const char *title, const char *subtitle)
{
    if (!cs || prime == COMBO_PRIME_NONE) return;
    cs->combo_prime = prime;
    cs->combo_prime_turns_remaining = relic_has(g_state.relics, g_state.relic_count, RELIC_SYNERGY_HOURGLASS) ? 2 : 1;
    snprintf(cs->synergy_banner_title, sizeof(cs->synergy_banner_title), "%s", title);
    snprintf(cs->synergy_banner_subtitle, sizeof(cs->synergy_banner_subtitle), "%s", subtitle);
    cs->synergy_banner_timer = 1.35f;
    cs->synergy_flash_timer = 0.32f;
    cs->combo_scale = 1.0f;
    cs->combo_tween = tween_create(&cs->combo_scale, 1.35f, 0.12f, EASE_OUT_BACK);
    tween_chain(cs->combo_tween, &cs->combo_scale, 1.0f, 0.35f, EASE_OUT_ELASTIC);
    combat_feed_add(cs, "%s primed", title);
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
    cs->combo_prime = COMBO_PRIME_NONE;
    cs->combo_prime_turns_remaining = 0;
}

static void combo_check_chain(CombatState *cs, ClassType previous, ClassType current)
{
    if (!cs || previous == CLASS_NONE || current == CLASS_NONE || previous == current) return;

    if (previous == CLASS_GUARDIAN && current == CLASS_CLERIC)
        combo_prime_set(cs, COMBO_PRIME_SHIELD_OF_FAITH, "SHIELD OF FAITH", "Next heal +50%");
    else if (previous == CLASS_MAGE && current == CLASS_ROGUE)
        combo_prime_set(cs, COMBO_PRIME_ARCANE_ASSAULT, "ARCANE ASSAULT", "Next attack applies Burning");
    else if (previous == CLASS_SHAMAN && current == CLASS_RANGER)
        combo_prime_set(cs, COMBO_PRIME_STORM_VOLLEY, "STORM VOLLEY", "Next AoE +50%");
    else if (previous == CLASS_ROGUE && current == CLASS_CLERIC)
        combo_prime_set(cs, COMBO_PRIME_SHADOW_DANCE, "SHADOW DANCE", "Next heal costs 0");
    else if (previous == CLASS_SHAMAN && current == CLASS_MAGE)
        combo_prime_set(cs, COMBO_PRIME_ELEMENTAL_FURY, "ELEMENTAL FURY", "Next Mage damage card costs 0");
    else if (previous == CLASS_ROGUE && current == CLASS_MAGE)
        combo_prime_set(cs, COMBO_PRIME_BACKDRAFT, "BACKDRAFT", "Next damage card +25%");
    else if (previous == CLASS_PALADIN && current == CLASS_BARD)
        combo_prime_set(cs, COMBO_PRIME_SACRED_CHORUS, "SACRED CHORUS", "Next group heal/shield +50%");
    else if (previous == CLASS_BARD && current == CLASS_WARLOCK)
        combo_prime_set(cs, COMBO_PRIME_DARK_REFRAIN, "DARK REFRAIN", "Next Warlock damage applies BLIGHT");
    else if (previous == CLASS_WARLOCK && current == CLASS_PALADIN)
        combo_prime_set(cs, COMBO_PRIME_ABSOLUTION, "ABSOLUTION", "Next Paladin card consumes BLIGHT to heal");
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

static void resolve_card_on_target(CombatState *cs, int hand_idx, int target_enemy, int target_ally)
{
    if (hand_idx < 0 || hand_idx >= cs->deck.hand_count) return;

    CardInstance *inst = &cs->deck.cards[cs->deck.hand[hand_idx]];
    const CardDef *card = inst->def;
    if (!card || !card->name) return;
    int played_uid = inst->uid;

    bool ug = inst->upgraded;

    int dmg = card_damage(card, ug) + meta_dmg_bonus(&g_state.meta);
    int hl  = card_heal(card, ug);
    int sh  = card_shield(card, ug) + meta_shield_bonus(&g_state.meta);

    LOG_I(CAT_CARD, "Playing %s (enemy=%d, ally=%d) upgraded=%d channel=%d", card->name, target_enemy, target_ally, ug, card->channel);
    combat_spawn_card_throw(cs, hand_idx, card, ug, target_enemy, target_ally);
    combat_flash_played_card(cs, card, target_enemy, target_ally);
    combat_feed_add(cs, "Played %s", card->name);

    // ── Combo check ─────────────────────────────────────────
    if (dmg > 0 && card_is_attack_card(card) && relic_has(g_state.relics, g_state.relic_count, RELIC_WHETSTONE))
        dmg += 1;
    if (dmg > 0 && target_enemy >= 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_MARK_OF_THE_HUNT) &&
        enemy_has_status(cs, target_enemy, STATUS_MARKED))
        dmg += 2;
    if (hl > 0 && card_is_heal_card(card) && relic_has(g_state.relics, g_state.relic_count, RELIC_PRAYER_BEADS))
        hl += 2;
    if (sh > 0 && relic_has(g_state.relics, g_state.relic_count, RELIC_BALLAST_RING))
        sh += 2;

    ClassType previous_class = cs->last_played_class;
    ComboPrime spent_prime = cs->combo_prime;

    if (spent_prime == COMBO_PRIME_SHIELD_OF_FAITH && card_is_heal_card(card))
    {
        hl = (hl * 3 + 1) / 2;
        combat_feed_add(cs, "Shield of Faith: heal +50%%");
        combo_prime_clear(cs);
    }
    else if (spent_prime == COMBO_PRIME_SHADOW_DANCE && card_is_heal_card(card))
    {
        combat_feed_add(cs, "Shadow Dance: heal was free");
        combo_prime_clear(cs);
    }
    else if (spent_prime == COMBO_PRIME_ELEMENTAL_FURY && card_is_fire_spell(card))
    {
        combat_feed_add(cs, "Elemental Fury: spell was free");
        combo_prime_clear(cs);
    }
    else if (spent_prime == COMBO_PRIME_STORM_VOLLEY && card_is_aoe_card(card))
    {
        dmg = (dmg * 3 + 1) / 2;
        combat_feed_add(cs, "Storm Volley: AoE +50%%");
        combo_prime_clear(cs);
    }
    else if (spent_prime == COMBO_PRIME_BACKDRAFT && dmg > 0)
    {
        dmg = (dmg * 5 + 2) / 4;
        combat_feed_add(cs, "Backdraft: damage +25%%");
        combo_prime_clear(cs);
    }
    else if (spent_prime == COMBO_PRIME_SACRED_CHORUS && card->target == TARGET_ALL_ALLIES)
    {
        hl = (hl * 3 + 1) / 2;
        sh = (sh * 3 + 1) / 2;
        combat_feed_add(cs, "Sacred Chorus: group support +50%%");
        combo_prime_clear(cs);
    }

    if (!cs->ambush_used && dmg > 0 && party_has_pair(cs, CLASS_ROGUE, CLASS_RANGER))
    {
        dmg *= 2;
        cs->ambush_used = true;
        combat_feed_add(cs, "Ambush: first strike doubled");
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
    bool arcane_assault = spent_prime == COMBO_PRIME_ARCANE_ASSAULT && card_is_attack_card(card);
    bool dark_refrain = spent_prime == COMBO_PRIME_DARK_REFRAIN && card->class == CLASS_WARLOCK && dmg > 0;
    bool absolution = spent_prime == COMBO_PRIME_ABSOLUTION && card->class == CLASS_PALADIN;
    int extra_lowest_heal = 0;
    int extra_caster_heal = 0;
    int blight_consumed_count = 0;

    if (card_is(card, "rng_pounce") && marked_target)
    {
        dmg = ug ? 18 : 12;
        combat_feed_add(cs, "[MARKED] Pounce found the opening");
    }
    if (card_is(card, "rog_evis") && marked_target)
    {
        dmg += 8;
        consume_marked = true;
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
        combat_feed_add(cs, "[MARKED] Backstab refunded 1 energy");
    }
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
    int damage = (int)(ab->base_damage * cs->floor_scale * cs->enemy_damage_scale);

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
        {
            apply_damage_to_ally(cs, i, damage, e->def->name);
            if (ab->status != STATUS_NONE && ab->status_turns > 0)
                apply_status_to_ally(cs, i, ab->status, ab->status_turns, ab->status_amount);
        }
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
        int target;
        if (cs->party.count > 1 && (rand() % 4) == 0)
            target = party_random_alive(&cs->party);
        else
            target = party_highest_aggro(&cs->party);
        if (target < 0) break;
        apply_damage_to_ally(cs, target, damage, e->def->name);
        if (hit == 0 && ab->status != STATUS_NONE && ab->status_turns > 0)
            apply_status_to_ally(cs, target, ab->status, ab->status_turns, ab->status_amount);
    }
    if (relic_has(g_state.relics, g_state.relic_count, RELIC_THORNED_AMULET) && damage > 0)
        apply_damage_to_enemy(cs, (int)(e - cs->enemies), 2);

    if (ab->shield_amount > 0 && e->hp > 0)
        apply_shield_to_enemy(cs, (int)(e - cs->enemies), ab->shield_amount);

    if (ab->heal_amount > 0 && e->hp > 0)
        apply_heal_to_enemy(cs, (int)(e - cs->enemies), ab->heal_amount);

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

static int enemy_cast_time(CombatState *cs, EnemyState *e, int ability_idx)
{
    int cast = e->def->abilities[ability_idx].cast_time;
    int asc = active_ascension();
    if (asc >= 4)
        cast--;
    if (g_state.encounter_is_boss && e->phase >= 1)
        cast--;
    if (e->interrupted_recently && e->last_interrupted_ability == ability_idx)
        cast--;
    if (cast < 1) cast = 1;
    return cast;
}

static int choose_enemy_intent(CombatState *cs, int enemy_index)
{
    EnemyState *e = &cs->enemies[enemy_index];
    if (!e->def || e->def->ability_count == 0) return -1;

    if (e->interrupted_recently &&
        e->last_interrupted_ability >= 0 &&
        e->last_interrupted_ability < e->def->ability_count &&
        (rand() % 100) < 50)
    {
        e->interrupted_recently = false;
        return e->last_interrupted_ability;
    }
    e->interrupted_recently = false;

    int alive_party = living_party_count(cs);
    bool low_hp = e->hp * 100 <= e->max_hp * 40;
    int best = -1;
    int best_score = -9999;
    int max_abilities = e->def->ability_count;
    if (g_state.encounter_is_boss && active_ascension() < 7 && max_abilities > 3)
        max_abilities = 3;

    for (int i = 0; i < max_abilities; i++)
    {
        const EnemyAbility *ab = &e->def->abilities[i];
        int score = (rand() % 8) + i;
        if (alive_party > 1 && ab->intent == INTENT_AOE)
            score += 24;
        if (low_hp && (ab->intent == INTENT_HEAL || ab->intent == INTENT_SHIELD || ab->intent == INTENT_BUFF))
            score += 28;
        if (!low_hp && (ab->intent == INTENT_HEAL || ab->intent == INTENT_SHIELD))
            score -= 10;
        if (ab->status != STATUS_NONE && alive_party > 0)
            score += 8;
        if (ab->intent == INTENT_WIPE && e->phase <= 0)
            score -= 12;
        if (score > best_score)
        {
            best_score = score;
            best = i;
        }
    }

    if (best < 0)
        best = e->current_ability % max_abilities;
    return best;
}

// ── Turn progression ────────────────────────────────────────

static void advance_turn(CombatState *cs)
{
    cs->turn++;
    LOG_I(CAT_COMBAT, "=== Turn %d ===", cs->turn);
    cs->last_played_class = CLASS_NONE;
    if (cs->combo_prime != COMBO_PRIME_NONE && cs->combo_prime_turns_remaining > 0)
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
            int idx = choose_enemy_intent(cs, ei);
            if (idx >= 0)
            {
                e->intent.ability_idx = idx;
                e->intent.remaining_turns = enemy_cast_time(cs, e, idx);
                e->current_ability++;
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
    deck_discard_hand(&cs->deck);
    deal_cards(&cs->deck, cs->turn_draw_count);

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
    cs->target_offset = 0;
    cs->target_offset_tween = -1;
    cs->combo_class = CLASS_NONE;
    cs->combo_last_cost = -1;
    cs->combo_count = 0;
    cs->last_played_class = CLASS_NONE;
    cs->combo_prime = COMBO_PRIME_NONE;
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
    cs->vengeful_ally = -1;
    for (int i = 0; i < 5; i++)
    {
        cs->action_feed[i][0] = '\0';
        cs->action_feed_timer[i] = 0.0f;
    }

    apply_relic_combat_start(cs);

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
        cs->enemies[i].phase = 0;
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

    int effective_cost = combat_effective_card_cost(cs, card);
    if (cs->energy.current < effective_cost) return;

    energy_spend(&cs->energy, effective_cost);

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
            const CardDef *card = cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def;
            cs->energy.current += combat_effective_card_cost(cs, card);
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
                ft_spawn(244.0f, 222.0f, "INVALID TARGET", 10, (Color){ 230, 90, 90, 255 });
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
            const CardDef *card = cs->deck.cards[cs->deck.hand[cs->target_hand_idx]].def;
            cs->energy.current += combat_effective_card_cost(cs, card);
            cs->target_mode = TGT_NONE; cs->target_hand_idx = -1; cs->target_offset = 0;
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




