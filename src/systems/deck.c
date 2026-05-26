#include "deck.h"
#include "data/card_defs.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

int card_upgrade_value(int base_value, int upgrade_level)
{
    int value = base_value;
    if (upgrade_level < 0) upgrade_level = 0;
    if (upgrade_level > 2) upgrade_level = 2;
    for (int i = 0; i < upgrade_level; i++)
        value = (value * 3 + 1) / 2;
    return value;
}

int card_damage(const CardDef *def, int upgrade_level)
{
    if (!def) return 0;
    return card_upgrade_value(def->damage, upgrade_level);
}
int card_heal(const CardDef *def, int upgrade_level)
{
    if (!def) return 0;
    return card_upgrade_value(def->heal, upgrade_level);
}
int card_shield(const CardDef *def, int upgrade_level)
{
    if (!def) return 0;
    return card_upgrade_value(def->shield, upgrade_level);
}

int card_repeat_hits(const CardDef *def)
{
    if (!def || def->repeat_hits < 1) return 1;
    return def->repeat_hits;
}

bool card_has_effect(const CardDef *def, CardEffectType type)
{
    if (!def || !def->effects || def->effect_count <= 0) return false;
    for (int i = 0; i < def->effect_count; i++)
        if (def->effects[i].type == type)
            return true;
    return false;
}

bool card_upgrade_changes_values(const CardDef *def)
{
    return card_upgrade_changes_values_at(def, 0);
}

bool card_upgrade_changes_values_at(const CardDef *def, int upgrade_level)
{
    if (!def || upgrade_level < 0 || upgrade_level >= 2) return false;
    return card_damage(def, upgrade_level + 1) != card_damage(def, upgrade_level) ||
           card_heal(def, upgrade_level + 1) != card_heal(def, upgrade_level) ||
           card_shield(def, upgrade_level + 1) != card_shield(def, upgrade_level);
}

int card_clamp_upgrade_level(const CardDef *def, int upgrade_level)
{
    if (upgrade_level < 0) upgrade_level = 0;
    if (upgrade_level > 2) upgrade_level = 2;
    while (upgrade_level > 0 && !card_upgrade_changes_values_at(def, upgrade_level - 1))
        upgrade_level--;
    return upgrade_level;
}

void deck_init(Deck *deck)
{
    memset(deck, 0, sizeof(Deck));
}

void deck_add_card(Deck *deck, const CardDef *def)
{
    deck_add_card_with_level(deck, def, 0);
}

void deck_add_card_upgraded(Deck *deck, const CardDef *def, bool upgraded)
{
    deck_add_card_with_level(deck, def, upgraded ? 1 : 0);
}

void deck_add_card_with_level(Deck *deck, const CardDef *def, int upgrade_level)
{
    if (!def || !def->id || !def->name) return;
    if (deck->card_count >= MAX_DECK_SIZE) return;
    upgrade_level = card_clamp_upgrade_level(def, upgrade_level);
    int idx = deck->card_count++;
    deck->cards[idx].def = def;
    deck->cards[idx].uid = deck->next_uid++;
    deck->cards[idx].upgrade_level = upgrade_level;
    deck->draw[deck->draw_count++] = idx;
}

void deck_prepare_for_combat(Deck *deck)
{
    deck->draw_count = 0;
    deck->hand_count = 0;
    deck->discard_count = 0;
    deck->exhaust_count = 0;

    for (int i = 0; i < deck->card_count; i++)
    {
        if (deck->cards[i].def && deck->cards[i].def->name)
            deck->draw[deck->draw_count++] = i;
    }

    deck_shuffle(deck);
}

void deck_init_from_classes(Deck *deck, int *class_indices, int count)
{
    deck_init(deck);
    const int starter_cards_per_member = 4;
    for (int i = 0; i < count; i++)
    {
        ClassType ct = (ClassType)class_indices[i];
        if (ct < 0 || ct >= CLASS_COUNT || !class_card_sets[ct]) continue;

        const CardDef *set = class_card_sets[ct];
        int n = class_card_counts[ct];
        if (n > starter_cards_per_member)
            n = starter_cards_per_member;
        for (int c = 0; c < n; c++)
            deck_add_card(deck, &set[c]);
    }
    deck_shuffle(deck);
}

void deck_shuffle(Deck *deck)
{
    for (int i = deck->draw_count - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int tmp = deck->draw[i];
        deck->draw[i] = deck->draw[j];
        deck->draw[j] = tmp;
    }
}

int deck_draw(Deck *deck)
{
    if (deck->hand_count >= MAX_HAND_SIZE) return -1;

    for (;;)
    {
        if (deck->draw_count == 0)
        {
            for (int i = 0; i < deck->discard_count; i++)
                deck->draw[deck->draw_count++] = deck->discard[i];
            deck->discard_count = 0;
            if (deck->draw_count > 0) deck_shuffle(deck);
        }

        if (deck->draw_count == 0) return -1;

        int idx = deck->draw[--deck->draw_count];
        if (idx < 0 || idx >= deck->card_count) continue;
        if (!deck->cards[idx].def || !deck->cards[idx].def->name) continue;

        deck->hand[deck->hand_count++] = idx;
        return idx;
    }
}

void deck_discard_index(Deck *deck, int hand_idx)
{
    if (hand_idx < 0 || hand_idx >= deck->hand_count) return;
    int card_idx = deck->hand[hand_idx];
    if (card_idx >= 0 && card_idx < deck->card_count && deck->cards[card_idx].def)
        deck->discard[deck->discard_count++] = card_idx;
    for (int i = hand_idx; i < deck->hand_count - 1; i++)
        deck->hand[i] = deck->hand[i + 1];
    deck->hand_count--;
}

void deck_exhaust_index(Deck *deck, int hand_idx)
{
    if (hand_idx < 0 || hand_idx >= deck->hand_count) return;
    int card_idx = deck->hand[hand_idx];
    if (card_idx >= 0 && card_idx < deck->card_count && deck->cards[card_idx].def)
        deck->exhaust[deck->exhaust_count++] = card_idx;
    for (int i = hand_idx; i < deck->hand_count - 1; i++)
        deck->hand[i] = deck->hand[i + 1];
    deck->hand_count--;
}

void deck_discard_hand(Deck *deck)
{
    for (int i = 0; i < deck->hand_count; i++)
    {
        int card_idx = deck->hand[i];
        if (card_idx >= 0 && card_idx < deck->card_count && deck->cards[card_idx].def)
            deck->discard[deck->discard_count++] = card_idx;
    }
    deck->hand_count = 0;
}

void deck_remove_card_by_uid(Deck *deck, int uid)
{
    for (int i = 0; i < deck->card_count; i++)
    {
        if (!deck->cards[i].def || deck->cards[i].uid != uid) continue;

        for (int j = 0; j < deck->draw_count; j++)
            if (deck->cards[deck->draw[j]].uid == uid)
                { deck->draw[j] = deck->draw[--deck->draw_count]; break; }
        for (int j = 0; j < deck->hand_count; j++)
            if (deck->cards[deck->hand[j]].uid == uid)
                { for (int k = j; k < deck->hand_count - 1; k++) deck->hand[k] = deck->hand[k + 1]; deck->hand_count--; break; }
        for (int j = 0; j < deck->discard_count; j++)
            if (deck->cards[deck->discard[j]].uid == uid)
                { deck->discard[j] = deck->discard[--deck->discard_count]; break; }
        for (int j = 0; j < deck->exhaust_count; j++)
            if (deck->cards[deck->exhaust[j]].uid == uid)
                { deck->exhaust[j] = deck->exhaust[--deck->exhaust_count]; break; }

        deck->cards[i].def = NULL;
        return;
    }
}

void deck_remove_class_cards(Deck *deck, ClassType ct)
{
    int removed = 0;
    for (int i = deck->card_count - 1; i >= 0; i--)
    {
        if (!deck->cards[i].def || deck->cards[i].def->class != ct) continue;

        int uid = deck->cards[i].uid;
        for (int j = 0; j < deck->draw_count; j++)
            if (deck->cards[deck->draw[j]].uid == uid)
                { deck->draw[j] = deck->draw[--deck->draw_count]; break; }
        for (int j = 0; j < deck->hand_count; j++)
            if (deck->cards[deck->hand[j]].uid == uid)
                { for (int k = j; k < deck->hand_count - 1; k++) deck->hand[k] = deck->hand[k + 1]; deck->hand_count--; break; }
        for (int j = 0; j < deck->discard_count; j++)
            if (deck->cards[deck->discard[j]].uid == uid)
                { deck->discard[j] = deck->discard[--deck->discard_count]; break; }
        for (int j = 0; j < deck->exhaust_count; j++)
            if (deck->cards[deck->exhaust[j]].uid == uid)
                { deck->exhaust[j] = deck->exhaust[--deck->exhaust_count]; break; }

        // Null the def pointer instead of compacting (keeps hand indices valid)
        deck->cards[i].def = NULL;
        removed++;
    }
}

int deck_hand_size(Deck *deck)
{
    return deck->hand_count;
}

