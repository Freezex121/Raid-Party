#include "meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define META_SAVE_PATH "RaidParty_Meta.sav"

void meta_set_defaults(MetaProgress *meta)
{
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
    meta->best_floor = 1;
    meta->highest_area_unlocked = 0;
    meta->max_ascension_unlocked = 0;
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
    if (meta->ascension_level < 0) meta->ascension_level = 0;
    if (meta->ascension_level > META_ASCENSION_MAX) meta->ascension_level = META_ASCENSION_MAX;
    if (meta->max_ascension_unlocked < 0) meta->max_ascension_unlocked = 0;
    if (meta->max_ascension_unlocked > META_ASCENSION_MAX) meta->max_ascension_unlocked = META_ASCENSION_MAX;
    if (meta->ascension_level > meta->max_ascension_unlocked)
        meta->ascension_level = meta->max_ascension_unlocked;
    if (meta->starting_relic_rank < 0) meta->starting_relic_rank = 0;
    if (meta->starting_relic_rank > META_LEGACY_MAX_RANK)
        meta->starting_relic_rank = META_LEGACY_MAX_RANK;
    if (meta->interrupts_total < 0) meta->interrupts_total = 0;
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
        else if (strcmp(key, "ascension_level") == 0) meta->ascension_level = value;
        else if (strcmp(key, "max_ascension_unlocked") == 0) meta->max_ascension_unlocked = value;
        else if (strcmp(key, "starting_relic_rank") == 0) meta->starting_relic_rank = value;
        else if (strcmp(key, "interrupts_total") == 0) meta->interrupts_total = value;
        else if (strcmp(key, "slot4_unlocked") == 0) meta->slot4_unlocked = value != 0;
        else if (strcmp(key, "slot5_unlocked") == 0) meta->slot5_unlocked = value != 0;
        else if (strcmp(key, "paladin_unlocked") == 0) meta->paladin_unlocked = value != 0;
        else if (strcmp(key, "warlock_unlocked") == 0) meta->warlock_unlocked = value != 0;
        else if (strcmp(key, "bard_unlocked") == 0) meta->bard_unlocked = value != 0;
        else if (strncmp(key, "achievement_", 12) == 0)
        {
            int idx = atoi(key + 12);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievements[idx] = value != 0;
        }
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
    fprintf(f, "ascension_level=%d\n", meta->ascension_level);
    fprintf(f, "max_ascension_unlocked=%d\n", meta->max_ascension_unlocked);
    fprintf(f, "starting_relic_rank=%d\n", meta->starting_relic_rank);
    fprintf(f, "interrupts_total=%d\n", meta->interrupts_total);
    fprintf(f, "slot4_unlocked=%d\n", meta->slot4_unlocked ? 1 : 0);
    fprintf(f, "slot5_unlocked=%d\n", meta->slot5_unlocked ? 1 : 0);
    fprintf(f, "paladin_unlocked=%d\n", meta->paladin_unlocked ? 1 : 0);
    fprintf(f, "warlock_unlocked=%d\n", meta->warlock_unlocked ? 1 : 0);
    fprintf(f, "bard_unlocked=%d\n", meta->bard_unlocked ? 1 : 0);
    for (int i = 0; i < ACH_COUNT; i++)
        fprintf(f, "achievement_%d=%d\n", i, meta->achievements[i] ? 1 : 0);
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

int meta_next_legacy_cost(const MetaProgress *meta)
{
    if (!meta || meta->starting_relic_rank >= META_LEGACY_MAX_RANK)
        return 0;
    static const int costs[META_LEGACY_MAX_RANK] = { 5, 10, 15 };
    return costs[meta->starting_relic_rank];
}

bool meta_area_unlocked(const MetaProgress *meta, int area_index)
{
    if (!meta) return area_index <= 0;
    return area_index >= 0 && area_index <= meta->highest_area_unlocked;
}

bool meta_class_unlocked(const MetaProgress *meta, int class_index)
{
    if (class_index < 0) return false;
    if (class_index <= 5) return true;
    if (!meta) return false;
    switch (class_index)
    {
        case 6: return meta->paladin_unlocked;
        case 7: return meta->warlock_unlocked;
        case 8: return meta->bard_unlocked;
        default: return false;
    }
}

bool meta_unlock_class(MetaProgress *meta, int class_index)
{
    if (!meta || class_index < 6 || class_index > 8)
        return false;
    switch (class_index)
    {
        case 6: meta->paladin_unlocked = true; return true;
        case 7: meta->warlock_unlocked = true; return true;
        case 8: meta->bard_unlocked = true; return true;
        default: return false;
    }
}

static int award_achievement(MetaProgress *meta, AchievementId id, char *names, int names_size)
{
    if (!meta || id < 0 || id >= ACH_COUNT || meta->achievements[id])
        return 0;

    meta->achievements[id] = true;
    int reward = achievement_reward(id);
    if (names && names_size > 0)
    {
        int used = (int)strlen(names);
        const char *name = achievement_name(id);
        if (used > 0 && used < names_size - 2)
        {
            names[used++] = ',';
            names[used++] = ' ';
            names[used] = '\0';
        }
        if (used < names_size - 1)
            snprintf(names + used, names_size - used, "%s", name);
    }
    return reward;
}

int meta_record_run(
    MetaProgress *meta,
    bool won,
    int area_index,
    int floor_reached,
    int bosses_defeated,
    int party_size,
    int deaths,
    int relic_count,
    int interrupts,
    int best_combat_turns,
    int area_count,
    int *achievement_renown,
    char *achievement_names,
    int achievement_names_size)
{
    if (!meta) return 0;
    if (area_index < 0) area_index = 0;
    if (achievement_renown) *achievement_renown = 0;
    if (achievement_names && achievement_names_size > 0) achievement_names[0] = '\0';

    bool first_run = meta->runs_completed == 0;
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
    if (won)
        renown_gained += meta->ascension_level;
    if (renown_gained < 1)
        renown_gained = 1;

    int ach = 0;
    if (first_run)
        ach += award_achievement(meta, ACH_FIRST_STEPS, achievement_names, achievement_names_size);
    if (won)
        ach += award_achievement(meta, ACH_CHAMPION, achievement_names, achievement_names_size);
    if (won && deaths <= 0)
        ach += award_achievement(meta, ACH_PERFECTIONIST, achievement_names, achievement_names_size);
    if (won && party_size == 1)
        ach += award_achievement(meta, ACH_SOLO_ARTIST, achievement_names, achievement_names_size);
    if (won && party_size >= 5)
        ach += award_achievement(meta, ACH_FULL_HOUSE, achievement_names, achievement_names_size);
    meta->interrupts_total += interrupts;
    if (meta->interrupts_total >= 20)
        ach += award_achievement(meta, ACH_INTERRUPTED, achievement_names, achievement_names_size);
    if (relic_count >= 10)
        ach += award_achievement(meta, ACH_HOARDER, achievement_names, achievement_names_size);
    if (won && best_combat_turns > 0 && best_combat_turns <= 3)
        ach += award_achievement(meta, ACH_SPEED_DEMON, achievement_names, achievement_names_size);
    if (won && area_count > 0 && area_index >= area_count - 1)
        ach += award_achievement(meta, ACH_COMPLETIONIST, achievement_names, achievement_names_size);

    renown_gained += ach;
    if (achievement_renown) *achievement_renown = ach;
    meta->renown += renown_gained;

    if (won && area_index >= meta->highest_area_unlocked)
        meta->highest_area_unlocked = area_index + 1;
    if (won && meta->max_ascension_unlocked < META_ASCENSION_MAX)
        meta->max_ascension_unlocked++;

    sanitize_meta(meta);
    return renown_gained;
}
