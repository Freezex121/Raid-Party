#include "meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define META_SAVE_PATH "RaidParty_Meta.sav"

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

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
    meta->dmg_bonus = clamp_int(meta->dmg_bonus, 0, 3);
    meta->shield_bonus = clamp_int(meta->shield_bonus, 0, 3);
    meta->first_draw_bonus = clamp_int(meta->first_draw_bonus, 0, 3);
    meta->opening_damage_bonus = clamp_int(meta->opening_damage_bonus, 0, 2);
    meta->execute_draw_rank = clamp_int(meta->execute_draw_rank, 0, 2);
    meta->weak_enemy_damage_bonus = clamp_int(meta->weak_enemy_damage_bonus, 0, 2);
    meta->elite_damage_bonus = clamp_int(meta->elite_damage_bonus, 0, 1);
    meta->boss_damage_bonus = clamp_int(meta->boss_damage_bonus, 0, 2);
    meta->warlock_damage_bonus = clamp_int(meta->warlock_damage_bonus, 0, 2);
    meta->combat_start_shield = clamp_int(meta->combat_start_shield, 0, 4);
    meta->max_hp_bonus_rank = clamp_int(meta->max_hp_bonus_rank, 0, 3);
    meta->shield_cap_rank = clamp_int(meta->shield_cap_rank, 0, META_SHIELD_CAP_MAX_RANK);
    meta->paladin_shield_bonus = clamp_int(meta->paladin_shield_bonus, 0, 2);
    meta->starting_energy_bonus = clamp_int(meta->starting_energy_bonus, 0, 2);
    meta->heal_bonus = clamp_int(meta->heal_bonus, 0, 2);
    meta->camp_bonus_rank = clamp_int(meta->camp_bonus_rank, 0, 2);
    meta->bard_draw_bonus = clamp_int(meta->bard_draw_bonus, 0, 3);
    meta->shop_discount_rank = clamp_int(meta->shop_discount_rank, 0, 3);
    meta->reward_reroll_discount_rank = clamp_int(meta->reward_reroll_discount_rank, 0, 1);
    meta->reward_choice_bonus = clamp_int(meta->reward_choice_bonus, 0, 2);
    meta->reward_upgrade_chance_rank = clamp_int(meta->reward_upgrade_chance_rank, 0, 2);
    meta->gold_conversion_rank = clamp_int(meta->gold_conversion_rank, 0, 2);
    meta->relic_choice_bonus = clamp_int(meta->relic_choice_bonus, 0, 1);
    meta->relic_unlock_flags &= META_RELIC_UNLOCK_ALL;
    if (meta->warlock_damage_bonus > 0)
        meta->warlock_unlocked = true;
    if (meta->paladin_shield_bonus > 0)
        meta->paladin_unlocked = true;
    if (meta->bard_draw_bonus > 0)
        meta->bard_unlocked = true;
    if (meta->starting_gold_rank > 0 ||
        meta->starting_relic_rank > 0 ||
        meta->slot4_unlocked ||
        meta->slot5_unlocked ||
        meta->paladin_unlocked ||
        meta->warlock_unlocked ||
        meta->bard_unlocked ||
        meta->start_prep ||
        meta->start_energize ||
        meta->start_fortify ||
        meta->start_rejuv ||
        meta->dmg_bonus > 0 ||
        meta->shield_bonus > 0 ||
        meta->first_draw_bonus > 0 ||
        meta->seasoned_adventurer ||
        meta->master_raider ||
        meta->opening_damage_bonus > 0 ||
        meta->execute_draw_rank > 0 ||
        meta->weak_enemy_damage_bonus > 0 ||
        meta->elite_damage_bonus > 0 ||
        meta->boss_damage_bonus > 0 ||
        meta->warlock_damage_bonus > 0 ||
        meta->combat_start_shield > 0 ||
        meta->max_hp_bonus_rank > 0 ||
        meta->shield_cap_rank > 0 ||
        meta->fortify_upgraded ||
        meta->paladin_shield_bonus > 0 ||
        meta->emergency_barrier_unlocked ||
        meta->last_stand_unlocked ||
        meta->starting_energy_bonus > 0 ||
        meta->prep_upgraded ||
        meta->rejuv_upgraded ||
        meta->heal_bonus > 0 ||
        meta->camp_bonus_rank > 0 ||
        meta->bard_draw_bonus > 0 ||
        meta->shop_discount_rank > 0 ||
        meta->reward_reroll_discount_rank > 0 ||
        meta->reward_choice_bonus > 0 ||
        meta->reward_upgrade_chance_rank > 0 ||
        meta->gold_conversion_rank > 0 ||
        meta->relic_choice_bonus > 0 ||
        meta->relic_unlock_flags != 0 ||
        meta->formation_drills)
        meta->meta_progress_unlocked = true;
}

static bool meta_load_expanded_key(MetaProgress *meta, const char *key, int value)
{
    if (strcmp(key, "opening_damage_bonus") == 0) meta->opening_damage_bonus = value;
    else if (strcmp(key, "execute_draw_rank") == 0) meta->execute_draw_rank = value;
    else if (strcmp(key, "weak_enemy_damage_bonus") == 0) meta->weak_enemy_damage_bonus = value;
    else if (strcmp(key, "elite_damage_bonus") == 0) meta->elite_damage_bonus = value;
    else if (strcmp(key, "boss_damage_bonus") == 0) meta->boss_damage_bonus = value;
    else if (strcmp(key, "warlock_damage_bonus") == 0) meta->warlock_damage_bonus = value;
    else if (strcmp(key, "combat_start_shield") == 0) meta->combat_start_shield = value;
    else if (strcmp(key, "max_hp_bonus_rank") == 0) meta->max_hp_bonus_rank = value;
    else if (strcmp(key, "shield_cap_rank") == 0) meta->shield_cap_rank = value;
    else if (strcmp(key, "fortify_upgraded") == 0) meta->fortify_upgraded = value != 0;
    else if (strcmp(key, "paladin_shield_bonus") == 0) meta->paladin_shield_bonus = value;
    else if (strcmp(key, "emergency_barrier_unlocked") == 0) meta->emergency_barrier_unlocked = value != 0;
    else if (strcmp(key, "last_stand_unlocked") == 0) meta->last_stand_unlocked = value != 0;
    else if (strcmp(key, "starting_energy_bonus") == 0) meta->starting_energy_bonus = value;
    else if (strcmp(key, "prep_upgraded") == 0) meta->prep_upgraded = value != 0;
    else if (strcmp(key, "rejuv_upgraded") == 0) meta->rejuv_upgraded = value != 0;
    else if (strcmp(key, "heal_bonus") == 0) meta->heal_bonus = value;
    else if (strcmp(key, "camp_bonus_rank") == 0) meta->camp_bonus_rank = value;
    else if (strcmp(key, "bard_draw_bonus") == 0) meta->bard_draw_bonus = value;
    else if (strcmp(key, "shop_discount_rank") == 0) meta->shop_discount_rank = value;
    else if (strcmp(key, "reward_reroll_discount_rank") == 0) meta->reward_reroll_discount_rank = value;
    else if (strcmp(key, "reward_choice_bonus") == 0) meta->reward_choice_bonus = value;
    else if (strcmp(key, "reward_upgrade_chance_rank") == 0) meta->reward_upgrade_chance_rank = value;
    else if (strcmp(key, "gold_conversion_rank") == 0) meta->gold_conversion_rank = value;
    else if (strcmp(key, "relic_choice_bonus") == 0) meta->relic_choice_bonus = value;
    else if (strcmp(key, "relic_unlock_flags") == 0) meta->relic_unlock_flags = value;
    else if (strcmp(key, "formation_drills") == 0) meta->formation_drills = value != 0;
    else return false;
    return true;
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
                else if (strcmp(key, "meta_progress_unlocked") == 0) meta->meta_progress_unlocked = value != 0;
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
                else if (meta_load_expanded_key(meta, key, value)) {}
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
                else if (meta_load_expanded_key(meta, key, value)) {}
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
        else if (strcmp(key, "meta_progress_unlocked") == 0) meta->meta_progress_unlocked = value != 0;
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
        else if (meta_load_expanded_key(meta, key, value)) {}
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
    char buf[12288];
    int pos = 0;
    #define FMT(...) pos += snprintf(buf + pos, sizeof(buf) - pos, __VA_ARGS__)
    FMT("runs_completed=%d\n", meta->runs_completed);
    FMT("wins=%d\n", meta->wins);
    FMT("best_floor=%d\n", meta->best_floor);
    FMT("bosses_defeated_total=%d\n", meta->bosses_defeated_total);
    FMT("renown=%d\n", meta->renown);
    FMT("meta_progress_unlocked=%d\n", meta->meta_progress_unlocked ? 1 : 0);
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
    FMT("opening_damage_bonus=%d\n", meta->opening_damage_bonus);
    FMT("execute_draw_rank=%d\n", meta->execute_draw_rank);
    FMT("weak_enemy_damage_bonus=%d\n", meta->weak_enemy_damage_bonus);
    FMT("elite_damage_bonus=%d\n", meta->elite_damage_bonus);
    FMT("boss_damage_bonus=%d\n", meta->boss_damage_bonus);
    FMT("warlock_damage_bonus=%d\n", meta->warlock_damage_bonus);
    FMT("combat_start_shield=%d\n", meta->combat_start_shield);
    FMT("max_hp_bonus_rank=%d\n", meta->max_hp_bonus_rank);
    FMT("shield_cap_rank=%d\n", meta->shield_cap_rank);
    FMT("fortify_upgraded=%d\n", meta->fortify_upgraded ? 1 : 0);
    FMT("paladin_shield_bonus=%d\n", meta->paladin_shield_bonus);
    FMT("emergency_barrier_unlocked=%d\n", meta->emergency_barrier_unlocked ? 1 : 0);
    FMT("last_stand_unlocked=%d\n", meta->last_stand_unlocked ? 1 : 0);
    FMT("starting_energy_bonus=%d\n", meta->starting_energy_bonus);
    FMT("prep_upgraded=%d\n", meta->prep_upgraded ? 1 : 0);
    FMT("rejuv_upgraded=%d\n", meta->rejuv_upgraded ? 1 : 0);
    FMT("heal_bonus=%d\n", meta->heal_bonus);
    FMT("camp_bonus_rank=%d\n", meta->camp_bonus_rank);
    FMT("bard_draw_bonus=%d\n", meta->bard_draw_bonus);
    FMT("shop_discount_rank=%d\n", meta->shop_discount_rank);
    FMT("reward_reroll_discount_rank=%d\n", meta->reward_reroll_discount_rank);
    FMT("reward_choice_bonus=%d\n", meta->reward_choice_bonus);
    FMT("reward_upgrade_chance_rank=%d\n", meta->reward_upgrade_chance_rank);
    FMT("gold_conversion_rank=%d\n", meta->gold_conversion_rank);
    FMT("relic_choice_bonus=%d\n", meta->relic_choice_bonus);
    FMT("relic_unlock_flags=%d\n", meta->relic_unlock_flags);
    FMT("formation_drills=%d\n", meta->formation_drills ? 1 : 0);
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
    fprintf(f, "meta_progress_unlocked=%d\n", meta->meta_progress_unlocked ? 1 : 0);
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
    fprintf(f, "opening_damage_bonus=%d\n", meta->opening_damage_bonus);
    fprintf(f, "execute_draw_rank=%d\n", meta->execute_draw_rank);
    fprintf(f, "weak_enemy_damage_bonus=%d\n", meta->weak_enemy_damage_bonus);
    fprintf(f, "elite_damage_bonus=%d\n", meta->elite_damage_bonus);
    fprintf(f, "boss_damage_bonus=%d\n", meta->boss_damage_bonus);
    fprintf(f, "warlock_damage_bonus=%d\n", meta->warlock_damage_bonus);
    fprintf(f, "combat_start_shield=%d\n", meta->combat_start_shield);
    fprintf(f, "max_hp_bonus_rank=%d\n", meta->max_hp_bonus_rank);
    fprintf(f, "shield_cap_rank=%d\n", meta->shield_cap_rank);
    fprintf(f, "fortify_upgraded=%d\n", meta->fortify_upgraded ? 1 : 0);
    fprintf(f, "paladin_shield_bonus=%d\n", meta->paladin_shield_bonus);
    fprintf(f, "emergency_barrier_unlocked=%d\n", meta->emergency_barrier_unlocked ? 1 : 0);
    fprintf(f, "last_stand_unlocked=%d\n", meta->last_stand_unlocked ? 1 : 0);
    fprintf(f, "starting_energy_bonus=%d\n", meta->starting_energy_bonus);
    fprintf(f, "prep_upgraded=%d\n", meta->prep_upgraded ? 1 : 0);
    fprintf(f, "rejuv_upgraded=%d\n", meta->rejuv_upgraded ? 1 : 0);
    fprintf(f, "heal_bonus=%d\n", meta->heal_bonus);
    fprintf(f, "camp_bonus_rank=%d\n", meta->camp_bonus_rank);
    fprintf(f, "bard_draw_bonus=%d\n", meta->bard_draw_bonus);
    fprintf(f, "shop_discount_rank=%d\n", meta->shop_discount_rank);
    fprintf(f, "reward_reroll_discount_rank=%d\n", meta->reward_reroll_discount_rank);
    fprintf(f, "reward_choice_bonus=%d\n", meta->reward_choice_bonus);
    fprintf(f, "reward_upgrade_chance_rank=%d\n", meta->reward_upgrade_chance_rank);
    fprintf(f, "gold_conversion_rank=%d\n", meta->gold_conversion_rank);
    fprintf(f, "relic_choice_bonus=%d\n", meta->relic_choice_bonus);
    fprintf(f, "relic_unlock_flags=%d\n", meta->relic_unlock_flags);
    fprintf(f, "formation_drills=%d\n", meta->formation_drills ? 1 : 0);
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

int meta_opening_damage_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->opening_damage_bonus;
}

int meta_execute_draw_rank(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->execute_draw_rank;
}

int meta_weak_enemy_damage_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->weak_enemy_damage_bonus;
}

int meta_elite_damage_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->elite_damage_bonus;
}

int meta_boss_damage_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->boss_damage_bonus;
}

int meta_warlock_damage_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->warlock_damage_bonus;
}

int meta_combat_start_shield(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->combat_start_shield;
}

int meta_max_hp_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->max_hp_bonus_rank * 2;
}

int meta_shield_cap_percent(const MetaProgress *meta)
{
    int rank = meta ? meta->shield_cap_rank : 0;
    rank = clamp_int(rank, 0, META_SHIELD_CAP_MAX_RANK);
    return META_SHIELD_CAP_BASE_PERCENT + rank * META_SHIELD_CAP_PERCENT_PER_RANK;
}

int meta_paladin_shield_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->paladin_shield_bonus;
}

bool meta_has_emergency_barrier(const MetaProgress *meta)
{
    return meta && meta->emergency_barrier_unlocked;
}

bool meta_has_last_stand(const MetaProgress *meta)
{
    return meta && meta->last_stand_unlocked;
}

int meta_starting_energy_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->starting_energy_bonus;
}

int meta_heal_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->heal_bonus;
}

int meta_camp_bonus_gold(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->camp_bonus_rank * 5;
}

int meta_bard_draw_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->bard_draw_bonus;
}

int meta_shop_discount(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->shop_discount_rank * 2;
}

int meta_discounted_cost(const MetaProgress *meta, int base_cost)
{
    if (base_cost <= 0) return 0;
    int cost = base_cost - meta_shop_discount(meta);
    return cost < 1 ? 1 : cost;
}

int meta_reward_reroll_cost(const MetaProgress *meta, int base_cost)
{
    if (base_cost <= 0) return 0;
    if (!meta) return base_cost;
    int cost = base_cost - meta->reward_reroll_discount_rank * 2;
    return cost < 1 ? 1 : cost;
}

int meta_reward_choice_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->reward_choice_bonus;
}

int meta_reward_upgrade_chance_percent(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->reward_upgrade_chance_rank * 10;
}

int meta_gold_conversion_divisor(const MetaProgress *meta)
{
    if (!meta) return 50;
    if (meta->gold_conversion_rank >= 2) return 40;
    if (meta->gold_conversion_rank >= 1) return 45;
    return 50;
}

int meta_relic_choice_bonus(const MetaProgress *meta)
{
    if (!meta) return 0;
    return meta->relic_choice_bonus;
}

int meta_relic_unlock_flag(RelicId id)
{
    switch (id)
    {
        case RELIC_ECHO_BELL: return META_RELIC_UNLOCK_ECHO_BELL;
        case RELIC_SPLIT_PRISM: return META_RELIC_UNLOCK_SPLIT_PRISM;
        case RELIC_BLOOD_AMBER: return META_RELIC_UNLOCK_BLOOD_AMBER;
        case RELIC_TITAN_HEART: return META_RELIC_UNLOCK_TITAN_HEART;
        case RELIC_FRUGAL_TOME: return META_RELIC_UNLOCK_FRUGAL_TOME;
        default: return 0;
    }
}

bool meta_relic_available(const MetaProgress *meta, RelicId id)
{
    int flag = meta_relic_unlock_flag(id);
    if (flag == 0) return true;
    return meta && ((meta->relic_unlock_flags & flag) != 0);
}

void meta_unlock_relic(MetaProgress *meta, RelicId id)
{
    if (!meta) return;
    int flag = meta_relic_unlock_flag(id);
    if (flag != 0)
        meta->relic_unlock_flags |= flag;
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
