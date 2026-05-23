#ifndef RELIC_H
#define RELIC_H

#include <stdbool.h>

#define MAX_RUN_RELICS 12
#define RELIC_REWARD_CHOICES 3

typedef enum {
    RELIC_NONE = -1,
    RELIC_WARD_STONE,
    RELIC_MENDING_BEAD,
    RELIC_BATTLE_DRUM,
    RELIC_SCOUTING_MAP,
    RELIC_GILDED_CHARM,
    RELIC_VETERAN_SIGIL,
    RELIC_COUNT
} RelicId;

typedef struct {
    RelicId id;
    const char *name;
    const char *icon;
    const char *description;
} RelicDef;

const RelicDef *relic_def(RelicId id);
bool relic_has(const RelicId *owned, int count, RelicId id);
bool relic_add_unique(RelicId *owned, int *count, RelicId id);
RelicId relic_random_unowned(const RelicId *owned, int count);
int relic_generate_choices(const RelicId *owned, int count, RelicId *out, int max_choices);

#endif
