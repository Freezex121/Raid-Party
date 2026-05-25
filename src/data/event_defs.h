#ifndef EVENT_DEFS_H
#define EVENT_DEFS_H

#include <stdbool.h>

#define EVENT_CHOICE_COUNT 3

typedef enum {
    EVENT_EFFECT_NONE,
    EVENT_EFFECT_HEAL_PARTY,
    EVENT_EFFECT_GAIN_GOLD_ADD_CURSE,
    EVENT_EFFECT_REMOVE_CARD,
    EVENT_EFFECT_UPGRADE_RANDOM_CARD_HURT_PARTY,
    EVENT_EFFECT_PAY_GOLD_GAIN_RELIC,
    EVENT_EFFECT_PAY_GOLD_ADD_CARD,
    EVENT_EFFECT_GAIN_GOLD,
    EVENT_EFFECT_GAIN_REROLL_TOKEN,
    EVENT_EFFECT_PAY_GOLD_UPGRADE_RANDOM_CARD,
    EVENT_EFFECT_GAIN_GOLD_HURT_PARTY,
    EVENT_EFFECT_ADD_CARD_ADD_CURSE,
    EVENT_EFFECT_GAIN_RELIC_ADD_CURSE,
    EVENT_EFFECT_DUPLICATE_RANDOM_CARD_HURT_PARTY,
    EVENT_EFFECT_TRANSFORM_RANDOM_CARD,
    EVENT_EFFECT_GAIN_MAX_HP,
} EventEffectType;

typedef struct {
    const char *label;
    const char *description;
    EventEffectType effect;
    int amount;
    int gold;
    int hp_loss;
    const char *curse;
} EventChoiceDef;

typedef struct {
    const char *id;
    const char *name;
    const char *body;
    EventChoiceDef choices[EVENT_CHOICE_COUNT];
    int choice_count;
} EventDef;

bool event_defs_load_json(const char *path);
int event_defs_count(void);
const EventDef *event_def_by_index(int index);

#endif
