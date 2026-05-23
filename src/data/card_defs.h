#ifndef CARD_DEFS_H
#define CARD_DEFS_H

#include "systems/deck.h"

#define CLASS_CARD_COUNT 8
#define GUARDIAN_CARD_COUNT 7
#define CLERIC_CARD_COUNT 8
#define MAGE_CARD_COUNT 8
#define ROGUE_CARD_COUNT 7
#define SHAMAN_CARD_COUNT 7
#define RANGER_CARD_COUNT 7

extern const CardDef guardian_cards[CLASS_CARD_COUNT];
extern const CardDef cleric_cards[CLASS_CARD_COUNT];
extern const CardDef mage_cards[CLASS_CARD_COUNT];
extern const CardDef rogue_cards[CLASS_CARD_COUNT];
extern const CardDef shaman_cards[CLASS_CARD_COUNT];
extern const CardDef ranger_cards[CLASS_CARD_COUNT];

extern const CardDef *class_card_sets[CLASS_COUNT];
extern const int class_card_counts[CLASS_COUNT];

#define UTILITY_CARD_COUNT 4
extern const CardDef utility_cards[UTILITY_CARD_COUNT];

#endif
