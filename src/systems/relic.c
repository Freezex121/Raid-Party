#include "relic.h"
#include <stdlib.h>

static const RelicDef relic_defs[RELIC_COUNT] = {
    [RELIC_WARD_STONE] = {
        RELIC_WARD_STONE,
        "Ward Stone",
        "WD",
        "At combat start, all allies gain 4 Shield."
    },
    [RELIC_MENDING_BEAD] = {
        RELIC_MENDING_BEAD,
        "Mending Bead",
        "MB",
        "At combat start, heal all allies for 4."
    },
    [RELIC_BATTLE_DRUM] = {
        RELIC_BATTLE_DRUM,
        "Battle Drum",
        "DR",
        "Start each combat with +1 energy."
    },
    [RELIC_SCOUTING_MAP] = {
        RELIC_SCOUTING_MAP,
        "Scouting Map",
        "MP",
        "Draw 1 extra card at combat start."
    },
    [RELIC_GILDED_CHARM] = {
        RELIC_GILDED_CHARM,
        "Gilded Charm",
        "GC",
        "Gain +8 gold after each combat."
    },
    [RELIC_VETERAN_SIGIL] = {
        RELIC_VETERAN_SIGIL,
        "Veteran Sigil",
        "VS",
        "Boss rewards always include an upgraded card."
    },
};

const RelicDef *relic_def(RelicId id)
{
    if (id < 0 || id >= RELIC_COUNT) return NULL;
    return &relic_defs[id];
}

bool relic_has(const RelicId *owned, int count, RelicId id)
{
    if (!owned || id < 0 || id >= RELIC_COUNT) return false;
    for (int i = 0; i < count; i++)
        if (owned[i] == id)
            return true;
    return false;
}

bool relic_add_unique(RelicId *owned, int *count, RelicId id)
{
    if (!owned || !count || id < 0 || id >= RELIC_COUNT) return false;
    if (*count >= MAX_RUN_RELICS) return false;
    if (relic_has(owned, *count, id)) return false;
    owned[*count] = id;
    (*count)++;
    return true;
}

RelicId relic_random_unowned(const RelicId *owned, int count)
{
    RelicId pool[RELIC_COUNT];
    int pool_count = 0;
    for (int i = 0; i < RELIC_COUNT; i++)
    {
        RelicId id = (RelicId)i;
        if (!relic_has(owned, count, id))
            pool[pool_count++] = id;
    }

    if (pool_count <= 0) return RELIC_NONE;
    return pool[rand() % pool_count];
}

int relic_generate_choices(const RelicId *owned, int count, RelicId *out, int max_choices)
{
    if (!out || max_choices <= 0) return 0;

    RelicId pool[RELIC_COUNT];
    int pool_count = 0;
    for (int i = 0; i < RELIC_COUNT; i++)
    {
        RelicId id = (RelicId)i;
        if (!relic_has(owned, count, id))
            pool[pool_count++] = id;
    }

    int choice_count = pool_count < max_choices ? pool_count : max_choices;
    for (int i = 0; i < choice_count; i++)
    {
        int pick = i + rand() % (pool_count - i);
        RelicId tmp = pool[i];
        pool[i] = pool[pick];
        pool[pick] = tmp;
        out[i] = pool[i];
    }

    return choice_count;
}
