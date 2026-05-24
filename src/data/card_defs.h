#ifndef CARD_DEFS_H
#define CARD_DEFS_H

#include <stdbool.h>
#include "systems/deck.h"

#define CLASS_CARD_COUNT 9
#define GUARDIAN_CARD_COUNT 8
#define CLERIC_CARD_COUNT 8
#define MAGE_CARD_COUNT 9
#define ROGUE_CARD_COUNT 8
#define SHAMAN_CARD_COUNT 8
#define RANGER_CARD_COUNT 8
#define PALADIN_CARD_COUNT 8
#define WARLOCK_CARD_COUNT 8
#define BARD_CARD_COUNT 8

extern CardDef guardian_cards[CLASS_CARD_COUNT];
extern CardDef cleric_cards[CLASS_CARD_COUNT];
extern CardDef mage_cards[CLASS_CARD_COUNT];
extern CardDef rogue_cards[CLASS_CARD_COUNT];
extern CardDef shaman_cards[CLASS_CARD_COUNT];
extern CardDef ranger_cards[CLASS_CARD_COUNT];
extern CardDef paladin_cards[CLASS_CARD_COUNT];
extern CardDef warlock_cards[CLASS_CARD_COUNT];
extern CardDef bard_cards[CLASS_CARD_COUNT];

extern const CardDef *class_card_sets[CLASS_COUNT];
extern int class_card_counts[CLASS_COUNT];

#define MAX_UTILITY_CARDS 16
extern CardDef utility_cards[MAX_UTILITY_CARDS];
extern int utility_card_count;

bool card_defs_load_json(const char *path);
const CardDef *card_def_by_id(const char *id);
int card_defs_loaded_count(void);

#endif
