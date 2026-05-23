#include "meta.h"
#include <stdio.h>
#include <string.h>

#define META_SAVE_PATH "RaidParty_Meta.sav"

void meta_set_defaults(MetaProgress *meta)
{
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
    meta->best_floor = 1;
}

static void apply_unlock_rules(MetaProgress *meta)
{
    if (!meta) return;
    if (meta->bosses_defeated_total >= 2)
        meta->slot4_unlocked = true;
    if (meta->wins > 0 || meta->bosses_defeated_total >= 6)
        meta->slot5_unlocked = true;
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
        else if (strcmp(key, "slot4_unlocked") == 0) meta->slot4_unlocked = value != 0;
        else if (strcmp(key, "slot5_unlocked") == 0) meta->slot5_unlocked = value != 0;
    }

    fclose(f);
    apply_unlock_rules(meta);
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

void meta_record_run(MetaProgress *meta, bool won, int floor_reached, int bosses_defeated)
{
    if (!meta) return;
    meta->runs_completed++;
    if (won)
        meta->wins++;
    if (floor_reached > meta->best_floor)
        meta->best_floor = floor_reached;
    meta->bosses_defeated_total += bosses_defeated;
    apply_unlock_rules(meta);
}
