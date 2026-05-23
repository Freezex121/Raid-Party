#include "meta.h"
#include <stdio.h>
#include <string.h>

#define META_SAVE_PATH "RaidParty_Meta.sav"

void meta_set_defaults(MetaProgress *meta)
{
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
    meta->best_floor = 1;
    meta->highest_area_unlocked = 0;
}

static void sanitize_meta(MetaProgress *meta)
{
    if (!meta) return;
    if (meta->best_floor < 1) meta->best_floor = 1;
    if (meta->renown < 0) meta->renown = 0;
    if (meta->highest_area_unlocked < 0) meta->highest_area_unlocked = 0;
    if (meta->starting_gold_rank < 0) meta->starting_gold_rank = 0;
    if (meta->starting_gold_rank > META_TRAVEL_FUND_MAX_RANK)
        meta->starting_gold_rank = META_TRAVEL_FUND_MAX_RANK;
    if (meta->slot5_unlocked)
        meta->slot4_unlocked = true;
}

void meta_load(MetaProgress *meta)
{
    meta_set_defaults(meta);
    if (!meta) return;

    FILE *f = fopen(META_SAVE_PATH, "r");
    if (!f)
        return;

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        char key[64];
        int value = 0;
        if (sscanf(line, "%63[^=]=%d", key, &value) != 2)
            continue;
        if (strcmp(key, "runs_completed") == 0) meta->runs_completed = value;
        else if (strcmp(key, "wins") == 0) meta->wins = value;
        else if (strcmp(key, "best_floor") == 0) meta->best_floor = value;
        else if (strcmp(key, "bosses_defeated_total") == 0) meta->bosses_defeated_total = value;
        else if (strcmp(key, "renown") == 0) meta->renown = value;
        else if (strcmp(key, "highest_area_unlocked") == 0) meta->highest_area_unlocked = value;
        else if (strcmp(key, "starting_gold_rank") == 0) meta->starting_gold_rank = value;
        else if (strcmp(key, "slot4_unlocked") == 0) meta->slot4_unlocked = value != 0;
        else if (strcmp(key, "slot5_unlocked") == 0) meta->slot5_unlocked = value != 0;
    }

    fclose(f);
    sanitize_meta(meta);
}

void meta_save(const MetaProgress *meta)
{
    if (!meta) return;

    FILE *f = fopen(META_SAVE_PATH, "w");
    if (!f)
        return;

    fprintf(f, "runs_completed=%d\n", meta->runs_completed);
    fprintf(f, "wins=%d\n", meta->wins);
    fprintf(f, "best_floor=%d\n", meta->best_floor);
    fprintf(f, "bosses_defeated_total=%d\n", meta->bosses_defeated_total);
    fprintf(f, "renown=%d\n", meta->renown);
    fprintf(f, "highest_area_unlocked=%d\n", meta->highest_area_unlocked);
    fprintf(f, "starting_gold_rank=%d\n", meta->starting_gold_rank);
    fprintf(f, "slot4_unlocked=%d\n", meta->slot4_unlocked ? 1 : 0);
    fprintf(f, "slot5_unlocked=%d\n", meta->slot5_unlocked ? 1 : 0);
    fclose(f);
}

int meta_party_slots(const MetaProgress *meta)
{
    if (!meta) return 3;
    if (meta->slot5_unlocked) return 5;
    if (meta->slot4_unlocked) return 4;
    return 3;
}

int meta_starting_gold(const MetaProgress *meta)
{
    if (!meta) return 0;
    int rank = meta->starting_gold_rank;
    if (rank < 0) rank = 0;
    if (rank > META_TRAVEL_FUND_MAX_RANK) rank = META_TRAVEL_FUND_MAX_RANK;
    return rank * META_TRAVEL_FUND_GOLD_PER_RANK;
}

int meta_next_travel_fund_cost(const MetaProgress *meta)
{
    if (!meta || meta->starting_gold_rank >= META_TRAVEL_FUND_MAX_RANK)
        return 0;
    return (meta->starting_gold_rank + 1) * 3;
}

bool meta_area_unlocked(const MetaProgress *meta, int area_index)
{
    if (!meta) return area_index <= 0;
    return area_index >= 0 && area_index <= meta->highest_area_unlocked;
}

int meta_record_run(MetaProgress *meta, bool won, int area_index, int floor_reached, int bosses_defeated)
{
    if (!meta) return 0;
    if (area_index < 0) area_index = 0;
    meta->runs_completed++;
    if (won)
        meta->wins++;
    if (floor_reached > meta->best_floor)
        meta->best_floor = floor_reached;
    meta->bosses_defeated_total += bosses_defeated;

    int renown_gained = bosses_defeated * 2;
    if (floor_reached > 1)
        renown_gained += 1;
    if (won)
        renown_gained += 3 + area_index;
    if (renown_gained < 1)
        renown_gained = 1;

    meta->renown += renown_gained;

    if (won && area_index >= meta->highest_area_unlocked)
        meta->highest_area_unlocked = area_index + 1;

    sanitize_meta(meta);
    return renown_gained;
}
