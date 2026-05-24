#include "theme.h"
#include "assets.h"
#include "util/math_utils.h"
#include "util/text.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static Color color_fade_alpha(Color c, unsigned char a)
{
    c.a = a;
    return c;
}

static Rectangle snap_rect(Rectangle r)
{
    int x = snap_i(r.x);
    int y = snap_i(r.y);
    int w = snap_i(r.width);
    int h = snap_i(r.height);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return (Rectangle){ (float)x, (float)y, (float)w, (float)h };
}

static Rectangle fit_card_art_rect(Rectangle bounds)
{
    float sx = bounds.width / (float)CARD_ART_SOURCE_W;
    float sy = bounds.height / (float)CARD_ART_SOURCE_H;
    float scale = sx < sy ? sx : sy;

    float w = CARD_ART_SOURCE_W * scale;
    float h = CARD_ART_SOURCE_H * scale;
    return snap_rect((Rectangle){
        bounds.x + (bounds.width - w) * 0.5f,
        bounds.y + (bounds.height - h) * 0.5f,
        w,
        h
    });
}

static int scaled_x(Rectangle r, float sx)
{
    return (int)(r.x + sx * (r.width / (float)CARD_ART_SOURCE_W));
}

static int scaled_y(Rectangle r, float sy)
{
    return (int)(r.y + sy * (r.height / (float)CARD_ART_SOURCE_H));
}

static int scaled_font(Rectangle r, int source_px)
{
    (void)r;
    return source_px >= 5 ? 18 : 10;
}

static int scaled_len(Rectangle r, float source_px)
{
    int len = snap_i(source_px * (r.width / (float)CARD_ART_SOURCE_W));
    return len < 1 ? 1 : len;
}

static void draw_text_fit(const char *text, int x, int y, int max_w, int size, Color color)
{
    if (!text || max_w <= 0) return;
    while (size > 10 && MeasureText(text, size) > max_w)
        size--;
    if (MeasureText(text, size) <= max_w)
    {
        DrawText(text, x, y, size, color);
        return;
    }

    char clipped[96];
    int len = 0;
    int text_len = (int)strlen(text);
    while (len < text_len && len < (int)sizeof(clipped) - 4)
    {
        clipped[len] = text[len];
        clipped[len + 1] = '\0';
        if (MeasureText(TextFormat("%s...", clipped), size) > max_w)
            break;
        len++;
    }
    if (len <= 0) return;
    clipped[len] = '\0';
    DrawText(TextFormat("%s...", clipped), x, y, size, color);
}

static const CardEffect *card_effect(const CardDef *card, CardEffectType type)
{
    if (!card || !card->effects || card->effect_count <= 0) return NULL;
    for (int i = 0; i < card->effect_count; i++)
        if (card->effects[i].type == type)
            return &card->effects[i];
    return NULL;
}

static const char *status_name(StatusType status)
{
    switch (status)
    {
        case STATUS_BURNING:    return "BURNING";
        case STATUS_RENEW:      return "RENEW";
        case STATUS_TRAP:       return "TRAP";
        case STATUS_TOTEM_HEAL: return "TOTEM";
        case STATUS_BLEED:      return "BLEED";
        case STATUS_WEAKNESS:   return "WEAKNESS";
        case STATUS_ENERGY_DRAIN: return "ENERGY DRAIN";
        case STATUS_MARKED:     return "MARKED";
        case STATUS_CONDUCTIVE: return "CONDUCTIVE";
        case STATUS_BLIGHT:     return "BLIGHT";
        default:                return "STATUS";
    }
}

static bool card_id_is(const CardDef *card, const char *id)
{
    return card && card->id && strcmp(card->id, id) == 0;
}

static const char *target_code(const CardDef *card)
{
    if (card_id_is(card, "wlk_dark_harvest")) return "BLIGHT";
    if (card && card->channel && card->target == TARGET_SELF)
    {
        if (card->damage > 0) return "ALL";
        if (card->heal > 0 || card->shield > 0) return "ALL";
    }
    if (card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES)) return "ALL";

    switch (card ? card->target : TARGET_SELF)
    {
        case TARGET_ENEMY:       return "SINGLE";
        case TARGET_ALL_ENEMIES: return "ALL";
        case TARGET_ALLY:        return "SINGLE";
        case TARGET_ALL_ALLIES:  return "ALL";
        case TARGET_SELF:        return "SELF";
    }
    return "SELF";
}

static const char *target_name(TargetType target)
{
    switch (target)
    {
        case TARGET_ENEMY:       return "one enemy";
        case TARGET_ALL_ENEMIES: return "all enemies";
        case TARGET_ALLY:        return "one ally";
        case TARGET_ALL_ALLIES:  return "all allies";
        case TARGET_SELF:        return "self";
    }
    return "self";
}

static const char *effect_target_name(const CardDef *card)
{
    if (card_id_is(card, "wlk_dark_harvest")) return "all BLIGHTED enemies";
    if (card && card->channel && card->target == TARGET_SELF)
    {
        if (card->damage > 0) return "all enemies";
        if (card->heal > 0 || card->shield > 0) return "all allies";
    }
    if (card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES)) return "all allies";
    return target_name(card ? card->target : TARGET_SELF);
}

static const char *card_class_label(const CardDef *card)
{
    if (!card || card->class == CLASS_NONE) return "Utility";
    return class_name(card->class);
}

static Color token_color(const char *token, Color fallback)
{
    if (!token || !token[0]) return fallback;
    if (token[0] == 'D') return (Color){ 235, 95, 85, 255 };
    if (token[0] == 'H' || token[0] == 'R') return (Color){ 105, 235, 130, 255 };
    if (token[0] == 'S') return (Color){ 125, 190, 255, 255 };
    if (token[0] == 'B' || token[0] == 'T') return (Color){ 245, 155, 75, 255 };
    if (token[0] == 'I' || token[0] == 'C') return (Color){ 205, 135, 245, 255 };
    if (token[0] == 'E') return (Color){ 255, 235, 120, 255 };
    if (token[0] == 'A') return (Color){ 185, 200, 240, 255 };
    return fallback;
}

static int add_token(char tokens[][12], int count, int max_tokens, const char *fmt, ...)
{
    if (count >= max_tokens) return count;

    va_list args;
    va_start(args, fmt);
    vsnprintf(tokens[count], 12, fmt, args);
    va_end(args);
    return count + 1;
}

static int build_card_tokens(const CardDef *card, bool upgraded, char tokens[][12], int max_tokens)
{
    if (!card) return 0;

    int count = 0;
    int dmg = card_damage(card, upgraded);
    int heal = card_heal(card, upgraded);
    int shield = card_shield(card, upgraded);
    int hits = card_repeat_hits(card);

    if (dmg > 0 && !card_id_is(card, "wlk_dark_harvest"))
    {
        if (hits > 1) count = add_token(tokens, count, max_tokens, "D%dx%d", dmg, hits);
        else count = add_token(tokens, count, max_tokens, "D%d", dmg);
    }
    if (heal > 0 && card->heal2 > 0)
        count = add_token(tokens, count, max_tokens, "H%d+%d", heal, card->heal2);
    else if (heal > 0)
        count = add_token(tokens, count, max_tokens, "H%d", heal);
    if (shield > 0)
        count = add_token(tokens, count, max_tokens, "S%d", shield);
    if (card->burn_stacks > 0)
        count = add_token(tokens, count, max_tokens, "B%d", card->burn_stacks);
    if (card->interrupt)
        count = add_token(tokens, count, max_tokens, "INT");
    if (card->taunt)
        count = add_token(tokens, count, max_tokens, "T%d", card->taunt_turns);
    if (card->channel)
        count = add_token(tokens, count, max_tokens, "CH%d", card->channel_turns);

    const CardEffect *effect = card_effect(card, CARD_EFFECT_DRAW_CARDS);
    if (effect) count = add_token(tokens, count, max_tokens, "DRAW%d", effect->amount);
    effect = card_effect(card, CARD_EFFECT_GAIN_ENERGY);
    if (effect) count = add_token(tokens, count, max_tokens, "EN+%d", effect->amount);
    if (card_effect(card, CARD_EFFECT_REVIVE_TARGET)) count = add_token(tokens, count, max_tokens, "REV");
    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ALLY);
    if (effect && effect->status == STATUS_RENEW) count = add_token(tokens, count, max_tokens, "REN%d", effect->turns);
    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES);
    if (effect && effect->status == STATUS_TOTEM_HEAL) count = add_token(tokens, count, max_tokens, "TOT%d", effect->turns);
    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY);
    if (effect && effect->status == STATUS_TRAP) count = add_token(tokens, count, max_tokens, "TRAP");
    if (effect && effect->status == STATUS_MARKED) count = add_token(tokens, count, max_tokens, "MARK");
    if (effect && effect->status == STATUS_CONDUCTIVE) count = add_token(tokens, count, max_tokens, "COND");
    if (effect && effect->status == STATUS_BLIGHT) count = add_token(tokens, count, max_tokens, "BLGT");
    if (card_effect(card, CARD_EFFECT_RESET_CASTER_AGGRO)) count = add_token(tokens, count, max_tokens, "AG0");
    if (card_effect(card, CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN)) count = add_token(tokens, count, max_tokens, "AG>GD");

    if (card->exhaust && !card->consume && !card_has_effect(card, CARD_EFFECT_REVIVE_TARGET) && !card->channel)
        count = add_token(tokens, count, max_tokens, "EXH");
    if (card->consume)
        count = add_token(tokens, count, max_tokens, "ONE");

    return count;
}

static void draw_card_tokens(Rectangle dest, const CardDef *card, bool upgraded, Color fallback)
{
    char tokens[12][12];
    int token_count = build_card_tokens(card, upgraded, tokens, 12);
    int size = 10;
    int x0 = scaled_x(dest, 6);
    int x = x0;
    int max_x = scaled_x(dest, 88);
    int y = scaled_y(dest, 94);
    int line_h = scaled_len(dest, 12);
    if (line_h < size + 2) line_h = size + 2;
    int gap = scaled_len(dest, 4);
    if (gap < 2) gap = 2;

    int row = 0;
    for (int i = 0; i < token_count && row < 3; i++)
    {
        int tw = MeasureText(tokens[i], size);
        if (x > x0 && x + tw > max_x)
        {
            row++;
            if (row >= 3) break;
            x = x0;
            y += line_h;
        }

        draw_text_fit(tokens[i], x, y, max_x - x, size, token_color(tokens[i], fallback));
        x += tw + gap;
    }
}

static int card_stat_heal(const CardDef *card, bool upgraded)
{
    int heal = card_heal(card, upgraded);
    if (card && card->heal2 > 0)
        heal += card->heal2;

    if (heal > 0)
        return heal;

    const CardEffect *effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ALLY);
    if (effect && effect->status == STATUS_RENEW)
        return effect->amount;

    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES);
    if (effect && effect->status == STATUS_TOTEM_HEAL)
        return effect->amount;

    return 0;
}

static int card_stat_damage(const CardDef *card, bool upgraded)
{
    return card_damage(card, upgraded) * card_repeat_hits(card);
}

static int card_stat_target_count(const CardDef *card)
{
    if (!card) return 0;
    if (card_id_is(card, "wlk_dark_harvest"))
        return MAX_ENEMIES;

    if (card->channel && card->target == TARGET_SELF)
    {
        if (card->damage > 0)
            return MAX_ENEMIES;
        if (card->heal > 0 || card->shield > 0 || card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES))
            return MAX_PARTY_SIZE;
    }

    if (card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES))
        return MAX_PARTY_SIZE;

    switch (card->target)
    {
        case TARGET_ALL_ENEMIES: return MAX_ENEMIES;
        case TARGET_ALL_ALLIES:  return MAX_PARTY_SIZE;
        case TARGET_ENEMY:
        case TARGET_ALLY:
        case TARGET_SELF:
            return 1;
    }
    return 0;
}

static void draw_card_stat_number(Rectangle dest, int row, int value, Color color)
{
    static const int row_y[5] = { 34, 52, 70, 88, 106 };
    if (row < 0 || row >= 5) return;

    char text[8];
    snprintf(text, sizeof(text), "%d", value);

    int size = 10;
    int x = scaled_x(dest, 13);
    int y = scaled_y(dest, (float)row_y[row]);
    int max_w = scaled_len(dest, 17);
    while (size > 6 && MeasureText(text, size) > max_w)
        size--;

    DrawText(text, x + max_w - MeasureText(text, size), y, size, color);
}

static void add_detail_line(char lines[][80], int *count, int max_lines, const char *fmt, ...)
{
    if (*count >= max_lines) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(lines[*count], 80, fmt, args);
    va_end(args);
    (*count)++;
}

static int build_card_detail_lines(const CardDef *card, bool upgraded, char lines[][80], int max_lines)
{
    if (!card) return 0;

    int count = 0;
    int dmg = card_damage(card, upgraded);
    int heal = card_heal(card, upgraded);
    int shield = card_shield(card, upgraded);
    int hits = card_repeat_hits(card);

    if (card->channel)
        add_detail_line(lines, &count, max_lines, "Channel: %d turns, then resolves", card->channel_turns);

    if (dmg > 0)
    {
        if (hits > 1)
            add_detail_line(lines, &count, max_lines, "Damage: %d x%d to %s", dmg, hits, effect_target_name(card));
        else
            add_detail_line(lines, &count, max_lines, "Damage: %d to %s", dmg, effect_target_name(card));
    }

    if (heal > 0)
    {
        if (card->target == TARGET_ENEMY && !card->heal_self)
            add_detail_line(lines, &count, max_lines, "Heal: %d to lowest HP ally", heal);
        else if (card->heal_self)
            add_detail_line(lines, &count, max_lines, "Heal: %d to self", heal);
        else
            add_detail_line(lines, &count, max_lines, "Heal: %d to %s", heal, effect_target_name(card));
    }

    if (card->heal2 > 0)
        add_detail_line(lines, &count, max_lines, "Bonus heal: %d to another ally", card->heal2);

    if (shield > 0)
    {
        if (card->target == TARGET_ENEMY)
            add_detail_line(lines, &count, max_lines, "Shield: %d to caster", shield);
        else
            add_detail_line(lines, &count, max_lines, "Shield: %d to %s", shield, effect_target_name(card));
    }

    if (card->burn_stacks > 0)
        add_detail_line(lines, &count, max_lines, "Burning: %d stacks for 3 turns", card->burn_stacks);
    if (card->interrupt)
        add_detail_line(lines, &count, max_lines, "Interrupts the target's cast");
    if (card->taunt)
        add_detail_line(lines, &count, max_lines, "Taunt: pull enemy attacks for %d turn%s", card->taunt_turns, card->taunt_turns == 1 ? "" : "s");

    const CardEffect *effect = card_effect(card, CARD_EFFECT_DRAW_CARDS);
    if (effect)
        add_detail_line(lines, &count, max_lines, "Draw: %d card%s", effect->amount, effect->amount == 1 ? "" : "s");
    effect = card_effect(card, CARD_EFFECT_GAIN_ENERGY);
    if (effect)
        add_detail_line(lines, &count, max_lines, "Energy: gain %d", effect->amount);
    if (card_effect(card, CARD_EFFECT_REVIVE_TARGET))
        add_detail_line(lines, &count, max_lines, "Revive a downed ally; exhaust");
    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ALLY);
    if (effect && effect->status == STATUS_RENEW)
        add_detail_line(lines, &count, max_lines, "Renew: +%d HP for %d turns", effect->amount, effect->turns);
    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_ALL_ALLIES);
    if (effect && effect->status == STATUS_TOTEM_HEAL)
        add_detail_line(lines, &count, max_lines, "All allies gain +%d HP for %d turns", effect->amount, effect->turns);
    effect = card_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY);
    if (effect && effect->status == STATUS_TRAP)
        add_detail_line(lines, &count, max_lines, "Trap: target deals -%d damage for %d turns", effect->amount, effect->turns);
    else if (effect && effect->status == STATUS_MARKED)
        add_detail_line(lines, &count, max_lines, "Applies MARKED for %d turns", effect->turns);
    else if (effect && effect->status == STATUS_CONDUCTIVE)
        add_detail_line(lines, &count, max_lines, "Applies CONDUCTIVE for %d turns", effect->turns);
    else if (effect && effect->status == STATUS_BLIGHT)
        add_detail_line(lines, &count, max_lines, "Applies BLIGHT for %d turns", effect->turns);
    else if (effect && effect->status != STATUS_NONE)
        add_detail_line(lines, &count, max_lines, "Applies %s for %d turns", status_name(effect->status), effect->turns);
    if (card_id_is(card, "rog_backstab"))
        add_detail_line(lines, &count, max_lines, "vs MARKED: refund 1 energy");
    if (card_id_is(card, "rog_evis"))
        add_detail_line(lines, &count, max_lines, "vs MARKED: +8 damage, consumes MARKED");
    if (card_id_is(card, "mag_missiles"))
        add_detail_line(lines, &count, max_lines, "vs MARKED: draw 1 card");
    if (card_id_is(card, "clr_smite"))
        add_detail_line(lines, &count, max_lines, "vs MARKED: +8 lowest ally heal, consumes");
    if (card_id_is(card, "pal_holy_strike"))
        add_detail_line(lines, &count, max_lines, "vs MARKED: +6 lowest ally heal, consumes");
    if (card_id_is(card, "wlk_drain_life"))
        add_detail_line(lines, &count, max_lines, "vs MARKED: +4 lowest ally heal");
    if (card_id_is(card, "mag_fireball"))
        add_detail_line(lines, &count, max_lines, "vs CONDUCTIVE: arcs 50%% to adjacent");
    if (card_id_is(card, "mag_meteor"))
        add_detail_line(lines, &count, max_lines, "Consumes CONDUCTIVE: +10 damage each");
    if (card_id_is(card, "rog_shadow"))
        add_detail_line(lines, &count, max_lines, "vs CONDUCTIVE: hits all charged enemies");
    if (card_id_is(card, "wlk_shadow_bolt"))
        add_detail_line(lines, &count, max_lines, "vs CONDUCTIVE: applies BLIGHT");
    if (card_id_is(card, "grd_shield_slam"))
        add_detail_line(lines, &count, max_lines, "vs BLIGHT: gain +6 Shield, consumes");
    if (card_id_is(card, "clr_holy_fire"))
        add_detail_line(lines, &count, max_lines, "vs BLIGHT: heal caster 10, consumes");
    if (card_id_is(card, "pal_judgment"))
        add_detail_line(lines, &count, max_lines, "vs BLIGHT: apply Trap, consumes");
    if (card_id_is(card, "pal_aegis_aura"))
        add_detail_line(lines, &count, max_lines, "BLIGHT: +3 Shield per blighted enemy");
    if (card_id_is(card, "wlk_hellfire"))
        add_detail_line(lines, &count, max_lines, "vs BLIGHT: +4 damage to blighted targets");
    if (card_id_is(card, "wlk_dark_harvest"))
        add_detail_line(lines, &count, max_lines, "BLIGHT: hit all blighted, heal party");
    if (card_id_is(card, "brd_dissonance"))
        add_detail_line(lines, &count, max_lines, "Applies CONDUCTIVE to all enemies hit");
    if (card_id_is(card, "brd_battle_hymn"))
        add_detail_line(lines, &count, max_lines, "Extends MARKED/CONDUCTIVE/BLIGHT by 1");
    if (card_id_is(card, "brd_finale"))
        add_detail_line(lines, &count, max_lines, "+2 Heal/Shield per active enemy synergy");
    if (card_effect(card, CARD_EFFECT_RESET_CASTER_AGGRO))
        add_detail_line(lines, &count, max_lines, "Reset caster aggro to 0");
    if (card_effect(card, CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN))
        add_detail_line(lines, &count, max_lines, "Move target ally aggro to Guardian");
    if (card->aggro_self > 0)
        add_detail_line(lines, &count, max_lines, "Aggro: +%d to caster", card->aggro_self);
    if (card->exhaust && !card->consume && !card_has_effect(card, CARD_EFFECT_REVIVE_TARGET) && !card->channel)
        add_detail_line(lines, &count, max_lines, "Exhausts after use");
    if (card->consume)
        add_detail_line(lines, &count, max_lines, "One-use: removed from deck after play");

    if (count == 0 && card->description)
        add_detail_line(lines, &count, max_lines, "%s", card->description);

    return count;
}

static Texture2D class_icon_texture(ClassType ct)
{
    if (ct < 0 || ct >= CLASS_COUNT) return (Texture2D){0};
    return g_assets.class_icons[ct];
}

Color theme_class_color(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return (Color){ 0, 0, 255, 255 };
        case CLASS_CLERIC:   return (Color){ 255, 255, 255, 255 };
        case CLASS_MAGE:     return (Color){ 255, 0, 0, 255 };
        case CLASS_ROGUE:    return (Color){ 0, 255, 0, 255 };
        case CLASS_SHAMAN:   return (Color){ 0, 0, 157, 255 };
        case CLASS_RANGER:   return (Color){ 0, 157, 0, 255 };
        case CLASS_PALADIN:  return (Color){ 240, 210, 95, 255 };
        case CLASS_WARLOCK:  return (Color){ 125, 70, 185, 255 };
        case CLASS_BARD:     return (Color){ 235, 95, 155, 255 };
        default:             return (Color){ 130, 135, 160, 255 };
    }
}

Color theme_class_dark(ClassType ct)
{
    Color c = theme_class_color(ct);
    c.r = (unsigned char)(c.r * 0.20f);
    c.g = (unsigned char)(c.g * 0.20f);
    c.b = (unsigned char)(c.b * 0.24f);
    return c;
}

Color theme_card_type_color(CardType type)
{
    switch (type)
    {
        case CARD_ATTACK: return (Color){ 215, 82, 70, 255 };
        case CARD_SKILL:  return (Color){ 80, 155, 220, 255 };
        case CARD_POWER:  return (Color){ 175, 100, 225, 255 };
    }
    return (Color){ 130, 135, 160, 255 };
}

Color theme_node_color(NodeType type)
{
    switch (type)
    {
        case NODE_START:  return (Color){ 125, 130, 155, 255 };
        case NODE_COMBAT: return (Color){ 205, 92, 74, 255 };
        case NODE_ELITE:  return (Color){ 185, 86, 205, 255 };
        case NODE_REST:   return (Color){ 78, 190, 120, 255 };
        case NODE_SHOP:   return (Color){ 225, 185, 70, 255 };
        case NODE_EVENT:  return (Color){ 85, 185, 205, 255 };
        case NODE_BOSS:   return (Color){ 230, 65, 70, 255 };
    }
    return (Color){ 125, 130, 155, 255 };
}

const char *theme_class_abbrev(ClassType ct)
{
    switch (ct)
    {
        case CLASS_GUARDIAN: return "GD";
        case CLASS_CLERIC:   return "CL";
        case CLASS_MAGE:     return "MG";
        case CLASS_ROGUE:    return "RG";
        case CLASS_SHAMAN:   return "SH";
        case CLASS_RANGER:   return "RA";
        case CLASS_PALADIN:  return "PL";
        case CLASS_WARLOCK:  return "WL";
        case CLASS_BARD:     return "BD";
        default:             return "--";
    }
}

const char *theme_node_icon(NodeType type)
{
    switch (type)
    {
        case NODE_START:  return "S";
        case NODE_COMBAT: return "X";
        case NODE_ELITE:  return "!";
        case NODE_REST:   return "+";
        case NODE_SHOP:   return "$";
        case NODE_EVENT:  return "?";
        case NODE_BOSS:   return "B";
    }
    return "?";
}

const char *theme_node_name(NodeType type)
{
    switch (type)
    {
        case NODE_START:  return "Start";
        case NODE_COMBAT: return "Combat";
        case NODE_ELITE:  return "Elite";
        case NODE_REST:   return "Rest";
        case NODE_SHOP:   return "Shop";
        case NODE_EVENT:  return "Event";
        case NODE_BOSS:   return "Boss";
    }
    return "Unknown";
}

const char *theme_card_type_label(CardType type)
{
    switch (type)
    {
        case CARD_ATTACK: return "ATK";
        case CARD_SKILL:  return "SKL";
        case CARD_POWER:  return "PWR";
    }
    return "---";
}

const char *theme_primary_effect_label(const CardDef *card)
{
    if (!card) return "---";
    if (card->interrupt) return "INT";
    if (card->taunt) return "TAUNT";
    if (card->burn_stacks > 0) return "BURN";
    const CardEffect *status_fx = card_effect(card, CARD_EFFECT_APPLY_STATUS_TARGET_ENEMY);
    if (status_fx && status_fx->status == STATUS_MARKED) return "MARK";
    if (status_fx && status_fx->status == STATUS_CONDUCTIVE) return "COND";
    if (status_fx && status_fx->status == STATUS_BLIGHT) return "BLIGHT";
    if (card_has_effect(card, CARD_EFFECT_REVIVE_TARGET)) return "REV";
    if (card_has_effect(card, CARD_EFFECT_DRAW_CARDS)) return "DRAW";
    if (card_has_effect(card, CARD_EFFECT_GAIN_ENERGY)) return "ENER";
    if (card_has_effect(card, CARD_EFFECT_RESET_CASTER_AGGRO)) return "AGRO";
    if (card_has_effect(card, CARD_EFFECT_TRANSFER_AGGRO_TO_GUARDIAN)) return "AGRO";
    if (card->channel) return "CHAN";
    if (card->consume) return "ONE";
    if (card->damage > 0 && card->heal > 0) return "DRAIN";
    if (card->damage > 0) return "DMG";
    if (card->heal > 0) return "HEAL";
    if (card->shield > 0) return "SHLD";
    if (card->exhaust) return "EXH";
    return "UTIL";
}

void theme_draw_background(void)
{
    DrawRectangleGradientV(0, 0, VIRT_W, VIRT_H,
        (Color){ 18, 19, 31, 255 },
        (Color){ 10, 11, 18, 255 });

    if (g_assets.loaded)
    {
        DrawTexturePro(g_assets.paper_texture,
            (Rectangle){ 0, 0, VIRT_W, VIRT_H },
            (Rectangle){ 0, 0, VIRT_W, VIRT_H },
            (Vector2){ 0, 0 }, 0.0f,
            (Color){ 255, 255, 255, 20 });
    }

    DrawCircleGradient(60, 37, 86.0f, (Color){ 45, 75, 105, 55 }, (Color){ 45, 75, 105, 0 });
    DrawCircleGradient(347, 203, 100.0f, (Color){ 95, 58, 45, 45 }, (Color){ 95, 58, 45, 0 });
    DrawRectangle(0, 0, VIRT_W, VIRT_H, (Color){ 0, 0, 0, 25 });
}

void theme_draw_cost_gem(int cx, int cy, int cost, bool enabled)
{
    Color fill = enabled ? (Color){ 235, 205, 65, 255 } : (Color){ 110, 95, 52, 255 };
    Color edge = enabled ? (Color){ 255, 235, 120, 255 } : (Color){ 80, 75, 68, 255 };

    DrawPoly((Vector2){ (float)cx, (float)cy }, 6, 15.0f, 30.0f, fill);
    DrawPolyLinesEx((Vector2){ (float)cx, (float)cy }, 6, 15.0f, 30.0f, 2.0f, edge);

    char text[4];
    snprintf(text, sizeof(text), "%d", cost);
    DrawText(text, cx - MeasureText(text, 18) / 2, cy - 7, 18, RAYWHITE);
}

void theme_draw_effect_badge(Rectangle bounds, const char *label, Color color)
{
    DrawRectangleRec(bounds, color_fade_alpha(color, 58));
    DrawRectangleLinesEx(bounds, 1.0f, color_fade_alpha(color, 185));
    DrawText(label, snap_i(bounds.x + bounds.width / 2 - MeasureText(label, 10) / 2), snap_i(bounds.y + 5), 10, RAYWHITE);
}

void theme_draw_class_portrait(ClassType ct, int cx, int cy, int radius, bool alive)
{
    Color c = theme_class_color(ct);
    Texture2D icon = class_icon_texture(ct);
    int size = CLASS_PORTRAIT_SIZE;
    if (size < radius * 2) size = radius * 2;

    if (!alive)
        c = (Color){ 95, 75, 78, 230 };

    Rectangle dest = {
        (float)(cx - size / 2),
        (float)(cy - size / 2),
        (float)size,
        (float)size
    };

    if (icon.id != 0)
    {
        Color tint = alive ? WHITE : (Color){ 135, 130, 140, 185 };
        DrawTexturePro(icon,
            (Rectangle){ 0.0f, 0.0f, (float)icon.width, (float)icon.height },
            dest,
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            tint);
    }
    else
    {
        const char *label = theme_class_abbrev(ct);
        int font_size = 10;
        DrawText(label,
            cx - MeasureText(label, font_size) / 2,
            cy - font_size / 2,
            font_size,
            color_fade_alpha(c, alive ? 245 : 155));
    }
}

void theme_draw_card_art(Rectangle bounds, const CardDef *card, bool upgraded)
{
    bounds = snap_rect(bounds);

    Texture2D template = upgraded && g_assets.card_template_upgraded.id != 0
        ? g_assets.card_template_upgraded
        : g_assets.card_template;

    if (template.id != 0)
    {
        DrawTexturePro(template,
            (Rectangle){ 0.0f, 0.0f, (float)template.width, (float)template.height },
            bounds,
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE);
    }
    else
    {
        DrawRectangleRec(bounds, (Color){ 9, 10, 16, 245 });
    }

    Rectangle dest = fit_card_art_rect(bounds);

    if (!card) return;

    int name_x = scaled_x(dest, 4);
    int name_w = scaled_len(dest, 88);
    int name_size = 10;
    int name_lines = MeasureText(card->name, name_size) > name_w ? 2 : 1;
    int name_h = name_lines * name_size;
    int name_y = scaled_y(dest, 2) + (scaled_len(dest, 26) - name_h) / 2;
    draw_text_wrapped(card->name, name_x, name_y, name_w, name_size, 0, RAYWHITE);

    draw_card_stat_number(dest, 0, card->cost, (Color){ 255, 255, 0, 255 });
    draw_card_stat_number(dest, 1, card_stat_heal(card, upgraded), (Color){ 255, 255, 255, 255 });
    draw_card_stat_number(dest, 2, card_stat_damage(card, upgraded), (Color){ 200, 0, 0, 255 });
    draw_card_stat_number(dest, 3, card_shield(card, upgraded), (Color){ 0, 0, 255, 255 });
    draw_card_stat_number(dest, 4, card_stat_target_count(card), (Color){ 128, 128, 128, 255 });

    Rectangle icon_box = snap_rect((Rectangle){
        (float)scaled_x(dest, 60),
        (float)scaled_y(dest, 30),
        (float)scaled_len(dest, 32),
        (float)scaled_len(dest, 32)
    });
    Texture2D class_icon = class_icon_texture(card->class);
    if (class_icon.id != 0)
    {
        DrawTexturePro(class_icon,
            (Rectangle){ 0.0f, 0.0f, (float)class_icon.width, (float)class_icon.height },
            icon_box,
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE);
    }
    else
    {
        const char *label = theme_class_abbrev(card->class);
        Color label_color = theme_class_color(card->class);
        DrawText(label,
            (int)(icon_box.x + icon_box.width * 0.5f - MeasureText(label, 10) * 0.5f),
            (int)(icon_box.y + icon_box.height * 0.5f - 5.0f),
            10,
            color_fade_alpha(label_color, 240));
    }
}

void theme_draw_card_tooltip(Rectangle bounds, const CardDef *card, bool upgraded)
{
    if (!card) return;

    if (bounds.x < 2.0f) bounds.x = 2.0f;
    if (bounds.y < 2.0f) bounds.y = 2.0f;
    if (bounds.x + bounds.width > VIRT_W - 2)
        bounds.x = (float)(VIRT_W - 2) - bounds.width;
    if (bounds.y + bounds.height > VIRT_H - 2)
        bounds.y = (float)(VIRT_H - 2) - bounds.height;

    Color accent = theme_class_color(card->class);
    Color type = theme_card_type_color(card->type);

    DrawRectangleRec(bounds, (Color){ 8, 9, 15, 244 });
    DrawRectangleRec((Rectangle){ bounds.x, bounds.y, bounds.width, 16.0f }, color_fade_alpha(theme_class_dark(card->class), 235));
    DrawRectangleLinesEx(bounds, 1.0f, color_fade_alpha(accent, 230));

    int x = (int)bounds.x + 5;
    int y = (int)bounds.y + 3;
    int right = (int)(bounds.x + bounds.width) - 5;

    char cost[16];
    snprintf(cost, sizeof(cost), "E%d", card->cost);
    DrawText(cost, right - MeasureText(cost, 10), y, 10, (Color){ 255, 235, 120, 255 });

    int title_w = right - x - MeasureText(cost, 10) - 8;
    draw_text_fit(card->name, x, y, title_w, 10, RAYWHITE);

    char meta[80];
    snprintf(meta, sizeof(meta), "%s  %s  %s",
        card_class_label(card),
        theme_card_type_label(card->type),
        target_code(card));
    DrawText(meta, x, (int)bounds.y + 20, 10, type);

    char lines[12][80];
    int line_count = build_card_detail_lines(card, upgraded, lines, 12);
    int line_y = (int)bounds.y + 32;
    int line_h = 10;
    int max_lines = ((int)(bounds.y + bounds.height) - line_y - 3) / line_h;
    if (max_lines < 0) max_lines = 0;
    if (line_count > max_lines) line_count = max_lines;

    for (int i = 0; i < line_count; i++)
    {
        Color line_color = i == 0 ? RAYWHITE : (Color){ 190, 194, 215, 235 };
        draw_text_fit(lines[i], x, line_y + i * line_h, (int)bounds.width - 10, 10, line_color);
    }
}
