#ifndef DECK_H
#define DECK_H

#include <stdbool.h>
#include "party.h"

#define MAX_DECK_SIZE 60
#define MAX_HAND_SIZE 10

typedef enum {
    CARD_ATTACK,
    CARD_SKILL,
    CARD_POWER
} CardType;

typedef enum {
    TARGET_ENEMY,
    TARGET_ALL_ENEMIES,
    TARGET_ALLY,
    TARGET_ALL_ALLIES,
    TARGET_SELF,
} TargetType;

typedef struct {
    const char *id;
    const char *name;
    CardType type;
    ClassType class;
    int cost;
    int damage;
    int heal;
    int heal2;
    bool heal_self;
    int shield;
    int burn_stacks;
    bool taunt;
    int taunt_turns;
    bool interrupt;
    int aggro_self;
    bool exhaust;
    bool channel;
    int channel_turns;
    TargetType target;
    const char *description;
} CardDef;

typedef struct {
    const CardDef *def;
    int uid;
    bool upgraded;
} CardInstance;

int  card_damage(const CardDef *def, bool upgraded);
int  card_heal(const CardDef *def, bool upgraded);
int  card_shield(const CardDef *def, bool upgraded);

typedef struct {
    CardInstance cards[MAX_DECK_SIZE];
    int card_count;
    int draw[MAX_DECK_SIZE];
    int draw_count;
    int hand[MAX_HAND_SIZE];
    int hand_count;
    int discard[MAX_DECK_SIZE];
    int discard_count;
    int exhaust[MAX_DECK_SIZE];
    int exhaust_count;
    int next_uid;
} Deck;

void deck_init(Deck *deck);
void deck_init_from_classes(Deck *deck, int *class_indices, int count);
void deck_add_card(Deck *deck, const CardDef *def);
void deck_add_card_upgraded(Deck *deck, const CardDef *def, bool upgraded);
void deck_prepare_for_combat(Deck *deck);
void deck_shuffle(Deck *deck);
int  deck_draw(Deck *deck);
void deck_discard_index(Deck *deck, int hand_idx);
void deck_exhaust_index(Deck *deck, int hand_idx);
void deck_discard_hand(Deck *deck);
void deck_remove_class_cards(Deck *deck, ClassType ct);
int  deck_hand_size(Deck *deck);

#endif
