#include "meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define META_SAVE_PATH "RaidParty_Meta.sav"

void meta_set_defaults(MetaProgress *meta)
{
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
    meta->best_floor = 1;
    meta->highest_area_unlocked = 0;
    meta->max_ascension_unlocked = 0;
    meta->ascension_beaten = 0;
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

#ifdef __EMSCRIPTEN__
    const char *data = (const char*)EM_ASM_PTR({
        var str = localStorage.getItem('raidparty_save');
        if (!str) return 0;
        var len = lengthBytesUTF8(str) + 1;
        var ptr = _malloc(len);
        stringToUTF8(str, ptr, len);
        return ptr;
    });
    if (data)
    {
        char key[64];
        int value;
        const char *p = data;
        while (*p)
        {
            if (sscanf(p, "%63[^=]=%d", key, &value) == 2)
            {
                if (strcmp(key, "runs_completed") == 0) meta->runs_completed = value;
                else if (strcmp(key, "wins") == 0) meta->wins = value;
                else if (strcmp(key, "best_floor") == 0) meta->best_floor = value;
                else if (strcmp(key, "bosses_defeated_total") == 0) meta->bosses_defeated_total = value;
                else if (strcmp(key, "renown") == 0) meta->renown = value;
                else if (strcmp(key, "highest_area_unlocked") == 0) meta->highest_area_unlocked = value;
                else if (strcmp(key, "starting_gold_rank") == 0) meta->starting_gold_rank = value;
                else if (strcmp(key, "ascension_level") == 0) meta->ascension_level = value;
                else if (strcmp(key, "max_ascension_unlocked") == 0) meta->max_ascension_unlocked = value;
                else if (strcmp(key, "ascension_beaten") == 0) meta->ascension_beaten = value;
                else if (strcmp(key, "starting_relic_rank") == 0) meta->starting_relic_rank = value;
                else if (strcmp(key, "interrupts_total") == 0) meta->interrupts_total = value;
                else if (strcmp(key, "slot4_unlocked") == 0) meta->slot4_unlocked = value != 0;
                else if (strcmp(key, "slot5_unlocked") == 0) meta->slot5_unlocked = value != 0;
                else if (strcmp(key, "paladin_unlocked") == 0) meta->paladin_unlocked = value != 0;
                else if (strcmp(key, "warlock_unlocked") == 0) meta->warlock_unlocked = value != 0;
                else if (strcmp(key, "bard_unlocked") == 0) meta->bard_unlocked = value != 0;
                else if (strcmp(key, "start_prep") == 0) meta->start_prep = value != 0;
                else if (strcmp(key, "start_energize") == 0) meta->start_energize = value != 0;
                else if (strcmp(key, "start_fortify") == 0) meta->start_fortify = value != 0;
                else if (strcmp(key, "start_rejuv") == 0) meta->start_rejuv = value != 0;
                else if (strcmp(key, "dmg_bonus") == 0) meta->dmg_bonus = value;
                else if (strcmp(key, "shield_bonus") == 0) meta->shield_bonus = value;
                else if (strcmp(key, "first_draw_bonus") == 0) meta->first_draw_bonus = value;
                else if (strcmp(key, "seasoned_adventurer") == 0) meta->seasoned_adventurer = value != 0;
                else if (strcmp(key, "master_raider") == 0) meta->master_raider = value != 0;
                else if (strcmp(key, "slot4_unlocked") == 0) meta->slot4_unlocked = value != 0;
                else if (strcmp(key, "slot5_unlocked") == 0) meta->slot5_unlocked = value != 0;
                else if (strcmp(key, "paladin_unlocked") == 0) meta->paladin_unlocked = value != 0;
                else if (strcmp(key, "warlock_unlocked") == 0) meta->warlock_unlocked = value != 0;
                else if (strcmp(key, "bard_unlocked") == 0) meta->bard_unlocked = value != 0;
                else if (strcmp(key, "start_prep") == 0) meta->start_prep = value != 0;
                else if (strcmp(key, "start_energize") == 0) meta->start_energize = value != 0;
                else if (strcmp(key, "start_fortify") == 0) meta->start_fortify = value != 0;
                else if (strcmp(key, "start_rejuv") == 0) meta->start_rejuv = value != 0;
                else if (strcmp(key, "dmg_bonus") == 0) meta->dmg_bonus = value;
                else if (strcmp(key, "shield_bonus") == 0) meta->shield_bonus = value;
                else if (strcmp(key, "first_draw_bonus") == 0) meta->first_draw_bonus = value;
                else if (strcmp(key, "seasoned_adventurer") == 0) meta->seasoned_adventurer = value != 0;
                else if (strcmp(key, "master_raider") == 0) meta->master_raider = value != 0;
                else if (strcmp(key, "tutorial_seen_elite") == 0) meta->tutorial_seen_elite = value != 0;
                else if (strcmp(key, "tutorial_seen_boss") == 0) meta->tutorial_seen_boss = value != 0;
                else if (strcmp(key, "tutorial_seen_shop") == 0) meta->tutorial_seen_shop = value != 0;
                else if (strcmp(key, "tutorial_seen_event") == 0) meta->tutorial_seen_event = value != 0;
                else if (strcmp(key, "tutorial_seen_rest") == 0) meta->tutorial_seen_rest = value != 0;
                else if (strcmp(key, "tutorial_seen_level_up") == 0) meta->tutorial_seen_level_up = value != 0;
                else if (strcmp(key, "tutorial_seen_discard") == 0) meta->tutorial_seen_discard = value != 0;
                else if (strcmp(key, "tutorial_seen_game_over") == 0) meta->tutorial_seen_game_over = value != 0;
                else if (strcmp(key, "tutorial_seen_meta_shop") == 0) meta->tutorial_seen_meta_shop = value != 0;
                else if (strncmp(key, "achievement_", 12) == 0)
                {
                    int idx = atoi(key + 12);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievements[idx] = value != 0;
                }
                else if (strncmp(key, "achievement_time_", 17) == 0)
                {
                    int idx = atoi(key + 17);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievement_times[idx] = value;
                }
                else if (strncmp(key, "achievement_party_", 18) == 0)
                {
                    int idx = atoi(key + 18);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievement_party[idx] = value;
                }
            }
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
        free((void*)data);
    }
    sanitize_meta(meta);
    return;
#endif

    FILE *f = fopen(META_SAVE_PATH, "r");
    if (!f)
        return;

    char line[192];
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
        else if (strcmp(key, "ascension_beaten") == 0) meta->ascension_beaten = value;
        else if (strcmp(key, "starting_relic_rank") == 0) meta->starting_relic_rank = value;
        else if (strcmp(key, "interrupts_total") == 0) meta->interrupts_total = value;
        else if (strcmp(key, "slot4_unlocked") == 0) meta->slot4_unlocked = value != 0;
        else if (strcmp(key, "slot5_unlocked") == 0) meta->slot5_unlocked = value != 0;
        else if (strcmp(key, "paladin_unlocked") == 0) meta->paladin_unlocked = value != 0;
        else if (strcmp(key, "warlock_unlocked") == 0) meta->warlock_unlocked = value != 0;
        else if (strcmp(key, "bard_unlocked") == 0) meta->bard_unlocked = value != 0;
        else if (strcmp(key, "start_prep") == 0) meta->start_prep = value != 0;
        else if (strcmp(key, "start_energize") == 0) meta->start_energize = value != 0;
        else if (strcmp(key, "start_fortify") == 0) meta->start_fortify = value != 0;
        else if (strcmp(key, "start_rejuv") == 0) meta->start_rejuv = value != 0;
        else if (strcmp(key, "dmg_bonus") == 0) meta->dmg_bonus = value;
        else if (strcmp(key, "shield_bonus") == 0) meta->shield_bonus = value;
        else if (strcmp(key, "first_draw_bonus") == 0) meta->first_draw_bonus = value;
        else if (strcmp(key, "seasoned_adventurer") == 0) meta->seasoned_adventurer = value != 0;
        else if (strcmp(key, "master_raider") == 0) meta->master_raider = value != 0;
        else if (strcmp(key, "tutorial_seen_elite") == 0) meta->tutorial_seen_elite = value != 0;
        else if (strcmp(key, "tutorial_seen_boss") == 0) meta->tutorial_seen_boss = value != 0;
        else if (strcmp(key, "tutorial_seen_shop") == 0) meta->tutorial_seen_shop = value != 0;
        else if (strcmp(key, "tutorial_seen_event") == 0) meta->tutorial_seen_event = value != 0;
        else if (strcmp(key, "tutorial_seen_rest") == 0) meta->tutorial_seen_rest = value != 0;
        else if (strcmp(key, "tutorial_seen_level_up") == 0) meta->tutorial_seen_level_up = value != 0;
        else if (strcmp(key, "tutorial_seen_discard") == 0) meta->tutorial_seen_discard = value != 0;
        else if (strcmp(key, "tutorial_seen_game_over") == 0) meta->tutorial_seen_game_over = value != 0;
        else if (strcmp(key, "tutorial_seen_meta_shop") == 0) meta->tutorial_seen_meta_shop = value != 0;
        else if (strncmp(key, "achievement_", 12) == 0)
        {
            int idx = atoi(key + 12);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievements[idx] = value != 0;
        }
        else if (strncmp(key, "achievement_time_", 17) == 0)
        {
            int idx = atoi(key + 17);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievement_times[idx] = value;
        }
        else if (strncmp(key, "achievement_party_", 18) == 0)
        {
            int idx = atoi(key + 18);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievement_party[idx] = value;
        }
    }

    fclose(f);
    sanitize_meta(meta);
}

void meta_save(const MetaProgress *meta)
{
    if (!meta) return;

#ifdef __EMSCRIPTEN__
    char buf[6144];
    int pos = 0;
    #define FMT(...) pos += snprintf(buf + pos, sizeof(buf) - pos, __VA_ARGS__)
    FMT("runs_completed=%d\n", meta->runs_completed);
    FMT("wins=%d\n", meta->wins);
    FMT("best_floor=%d\n", meta->best_floor);
    FMT("bosses_defeated_total=%d\n", meta->bosses_defeated_total);
    FMT("renown=%d\n", meta->renown);
    FMT("highest_area_unlocked=%d\n", meta->highest_area_unlocked);
    FMT("starting_gold_rank=%d\n", meta->starting_gold_rank);
    FMT("ascension_level=%d\n", meta->ascension_level);
    FMT("max_ascension_unlocked=%d\n", meta->max_ascension_unlocked);
    FMT("ascension_beaten=%d\n", meta->ascension_beaten);
    FMT("starting_relic_rank=%d\n", meta->starting_relic_rank);
    FMT("interrupts_total=%d\n", meta->interrupts_total);
    FMT("slot4_unlocked=%d\n", meta->slot4_unlocked ? 1 : 0);
    FMT("slot5_unlocked=%d\n", meta->slot5_unlocked ? 1 : 0);
    FMT("paladin_unlocked=%d\n", meta->paladin_unlocked ? 1 : 0);
    FMT("warlock_unlocked=%d\n", meta->warlock_unlocked ? 1 : 0);
    FMT("bard_unlocked=%d\n", meta->bard_unlocked ? 1 : 0);
    FMT("start_prep=%d\n", meta->start_prep ? 1 : 0);
    FMT("start_energize=%d\n", meta->start_energize ? 1 : 0);
    FMT("start_fortify=%d\n", meta->start_fortify ? 1 : 0);
    FMT("start_rejuv=%d\n", meta->start_rejuv ? 1 : 0);
    FMT("dmg_bonus=%d\n", meta->dmg_bonus);
    FMT("shield_bonus=%d\n", meta->shield_bonus);
    FMT("first_draw_bonus=%d\n", meta->first_draw_bonus);
    FMT("seasoned_adventurer=%d\n", meta->seasoned_adventurer ? 1 : 0);
    FMT("master_raider=%d\n", meta->master_raider ? 1 : 0);
    FMT("tutorial_seen_elite=%d\n", meta->tutorial_seen_elite ? 1 : 0);
    FMT("tutorial_seen_boss=%d\n", meta->tutorial_seen_boss ? 1 : 0);
    FMT("tutorial_seen_shop=%d\n", meta->tutorial_seen_shop ? 1 : 0);
    FMT("tutorial_seen_event=%d\n", meta->tutorial_seen_event ? 1 : 0);
    FMT("tutorial_seen_rest=%d\n", meta->tutorial_seen_rest ? 1 : 0);
    FMT("tutorial_seen_level_up=%d\n", meta->tutorial_seen_level_up ? 1 : 0);
    FMT("tutorial_seen_discard=%d\n", meta->tutorial_seen_discard ? 1 : 0);
    FMT("tutorial_seen_game_over=%d\n", meta->tutorial_seen_game_over ? 1 : 0);
    FMT("tutorial_seen_meta_shop=%d\n", meta->tutorial_seen_meta_shop ? 1 : 0);
    for (int i = 0; i < ACH_COUNT; i++)
    {
        FMT("achievement_%d=%d\n", i, meta->achievements[i] ? 1 : 0);
        if (meta->achievements[i])
        {
            FMT("achievement_time_%d=%d\n", i, meta->achievement_times[i]);
            FMT("achievement_party_%d=%d\n", i, meta->achievement_party[i]);
        }
    }
    #undef FMT
    EM_ASM({
        localStorage.setItem('raidparty_save', UTF8ToString($0));
    }, buf);
    return;
#endif

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
    fprintf(f, "ascension_beaten=%d\n", meta->ascension_beaten);
    fprintf(f, "starting_relic_rank=%d\n", meta->starting_relic_rank);
    fprintf(f, "interrupts_total=%d\n", meta->interrupts_total);
    fprintf(f, "slot4_unlocked=%d\n", meta->slot4_unlocked ? 1 : 0);
    fprintf(f, "slot5_unlocked=%d\n", meta->slot5_unlocked ? 1 : 0);
    fprintf(f, "paladin_unlocked=%d\n", meta->paladin_unlocked ? 1 : 0);
    fprintf(f, "warlock_unlocked=%d\n", meta->warlock_unlocked ? 1 : 0);
    fprintf(f, "bard_unlocked=%d\n", meta->bard_unlocked ? 1 : 0);
    fprintf(f, "start_prep=%d\n", meta->start_prep ? 1 : 0);
    fprintf(f, "start_energize=%d\n", meta->start_energize ? 1 : 0);
    fprintf(f, "start_fortify=%d\n", meta->start_fortify ? 1 : 0);
    fprintf(f, "start_rejuv=%d\n", meta->start_rejuv ? 1 : 0);
    fprintf(f, "dmg_bonus=%d\n", meta->dmg_bonus);
    fprintf(f, "shield_bonus=%d\n", meta->shield_bonus);
    fprintf(f, "first_draw_bonus=%d\n", meta->first_draw_bonus);
    fprintf(f, "seasoned_adventurer=%d\n", meta->seasoned_adventurer ? 1 : 0);
    fprintf(f, "master_raider=%d\n", meta->master_raider ? 1 : 0);
    fprintf(f, "tutorial_seen_elite=%d\n", meta->tutorial_seen_elite ? 1 : 0);
    fprintf(f, "tutorial_seen_boss=%d\n", meta->tutorial_seen_boss ? 1 : 0);
    fprintf(f, "tutorial_seen_shop=%d\n", meta->tutorial_seen_shop ? 1 : 0);
    fprintf(f, "tutorial_seen_event=%d\n", meta->tutorial_seen_event ? 1 : 0);
    fprintf(f, "tutorial_seen_rest=%d\n", meta->tutorial_seen_rest ? 1 : 0);
    fprintf(f, "tutorial_seen_level_up=%d\n", meta->tutorial_seen_level_up ? 1 : 0);
    fprintf(f, "tutorial_seen_discard=%d\n", meta->tutorial_seen_discard ? 1 : 0);
    fprintf(f, "tutorial_seen_game_over=%d\n", meta->tutorial_seen_game_over ? 1 : 0);
    fprintf(f, "tutorial_seen_meta_shop=%d\n", meta->tutorial_seen_meta_shop ? 1 : 0);
    for (int i = 0; i < ACH_COUNT; i++)
        fprintf(f, "achievement_%d=%d\n", i, meta->achievements[i] ? 1 : 0);
    for (int i = 0; i < ACH_COUNT; i++)
        if (meta->achievements[i])
        {
            fprintf(f, "achievement_time_%d=%d\n", i, meta->achievement_times[i]);
            fprintf(f, "achievement_party_%d=%d\n", i, meta->achievement_party[i]);
        }
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
    static const int costs[META_TRAVEL_FUND_MAX_RANK] = { 6, 12, 18 };
    return costs[meta->starting_gold_rank];
}

int meta_next_legacy_cost(const MetaProgress *meta)
{
    if (!meta || meta->starting_relic_rank >= META_LEGACY_MAX_RANK)
        return 0;
    static const int costs[META_LEGACY_MAX_RANK] = { 20, 30, 45 };
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

int meta_next_upgrade_cost(int rank)
{
    static const int costs[] = { 5, 6, 8, 8 };
    if (rank < 0 || rank >= 4) return 0;
    return costs[rank];
}

int meta_starting_deck_bonuses(const MetaProgress *meta)
{
    if (!meta) return 0;
    int count = 0;
    if (meta->start_prep) count++;
    if (meta->start_energize) count++;
    if (meta->start_fortify) count++;
    if (meta->start_rejuv) count++;
    return count;
}

int meta_dmg_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->dmg_bonus;
}

int meta_shield_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->shield_bonus;
}

int meta_first_draw_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->first_draw_bonus;
}

int meta_renown_bonus_per_boss(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->seasoned_adventurer ? 1 : 0;
}

int meta_renown_bonus_per_clear(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->master_raider ? 2 : 0;
}

static int award_achievement(MetaProgress *meta, AchievementId id, char *names, int names_size, int run_number, const int *party_classes, int party_size)
{
    if (!meta || id < 0 || id >= ACH_COUNT || meta->achievements[id])
        return 0;

    meta->achievements[id] = true;
    meta->achievement_times[id] = run_number;
    int party_mask = 0;
    for (int i = 0; i < party_size && i < 16; i++)
        if (party_classes[i] >= 0 && party_classes[i] < 24)
            party_mask |= (1 << party_classes[i]);
    meta->achievement_party[id] = party_mask;
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
    const int *party_classes,
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
    int run_number = meta->runs_completed + 1;
    meta->runs_completed++;
    if (won)
        meta->wins++;
    if (floor_reached > meta->best_floor)
        meta->best_floor = floor_reached;
    meta->bosses_defeated_total += bosses_defeated;

    int renown_gained = bosses_defeated * (2 + meta_renown_bonus_per_boss(meta));
    if (floor_reached > 1)
        renown_gained += 1;
    if (won)
        renown_gained += 3 + area_index + meta_renown_bonus_per_clear(meta);
    if (won)
        renown_gained += meta->ascension_level;
    if (renown_gained < 1)
        renown_gained = 1;

    int ach = 0;
    if (first_run)
        ach += award_achievement(meta, ACH_FIRST_STEPS, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (won)
        ach += award_achievement(meta, ACH_CHAMPION, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (won && deaths <= 0)
        ach += award_achievement(meta, ACH_PERFECTIONIST, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (won && party_size == 1)
        ach += award_achievement(meta, ACH_SOLO_ARTIST, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (won && party_size >= 5)
        ach += award_achievement(meta, ACH_FULL_HOUSE, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    meta->interrupts_total += interrupts;
    if (meta->interrupts_total >= 20)
        ach += award_achievement(meta, ACH_INTERRUPTED, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (relic_count >= 10)
        ach += award_achievement(meta, ACH_HOARDER, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (won && best_combat_turns > 0 && best_combat_turns <= 3)
        ach += award_achievement(meta, ACH_SPEED_DEMON, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    if (won && area_count > 0 && area_index >= area_count - 1)
        ach += award_achievement(meta, ACH_COMPLETIONIST, achievement_names, achievement_names_size, run_number, party_classes, party_size);

    renown_gained += ach;
    if (achievement_renown) *achievement_renown = ach;
    meta->renown += renown_gained;

    if (won && area_index >= meta->highest_area_unlocked)
        meta->highest_area_unlocked = area_index + 1;
    // Ascension: only unlock next level if beating at current max
    if (won && meta->max_ascension_unlocked < META_ASCENSION_MAX &&
        meta->ascension_level == meta->max_ascension_unlocked)
        meta->max_ascension_unlocked++;

    // Ascension beat reward: one-time renown bonus per level
    if (won && meta->ascension_level > 0)
    {
        int bit = meta->ascension_level - 1;
        if (!(meta->ascension_beaten & (1 << bit)))
        {
            meta->ascension_beaten |= (1 << bit);
            int asc_reward = (meta->ascension_level >= META_ASCENSION_MAX) ? 30 : 10;
            renown_gained += asc_reward;
            meta->renown += asc_reward;
        }
    }

    sanitize_meta(meta);
    return renown_gained;
}
