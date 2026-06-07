#include "meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static int current_unix_timestamp(void)
{
    time_t now = time(NULL);
    if (now <= 0) return 0;
    if (now > 2147483647) return 2147483647;
    return (int)now;
}

typedef struct {
    MetaContentUnlock id;
    const char *key;
    const char *name;
    const char *secret_name;
    AchievementId required_achievement;
} MetaContentDef;

static const MetaContentDef META_CONTENT_DEFS[META_CONTENT_COUNT] = {
    [META_CONTENT_PARTY_SLOT_IV] = { META_CONTENT_PARTY_SLOT_IV, "party_slot_iv", "Party Slot IV", "Party Slot IV", ACH_FIRST_STEPS },
    [META_CONTENT_FORMATION_DRILLS] = { META_CONTENT_FORMATION_DRILLS, "formation_drills", "Formation Drills", "Formation Drills", ACH_CHAMPION },
    [META_CONTENT_PARTY_SLOT_V] = { META_CONTENT_PARTY_SLOT_V, "party_slot_v", "Party Slot V", "Party Slot V", ACH_CHAMPION },
    [META_CONTENT_CLASS_PALADIN] = { META_CONTENT_CLASS_PALADIN, "class_paladin", "Unlock Paladin", "Unlock Paladin", ACH_PERFECTIONIST },
    [META_CONTENT_CLASS_WARLOCK] = { META_CONTENT_CLASS_WARLOCK, "class_warlock", "Unlock Warlock", "Unlock Warlock", ACH_INTERRUPTED },
    [META_CONTENT_CLASS_BARD] = { META_CONTENT_CLASS_BARD, "class_bard", "Unlock Bard", "Unlock Bard", ACH_SPEED_DEMON },
    [META_CONTENT_RELIC_ECHO_BELL] = { META_CONTENT_RELIC_ECHO_BELL, "relic_echo_bell", "Echo Bell", "Echo Bell", ACH_HOARDER },
    [META_CONTENT_RELIC_SPLIT_PRISM] = { META_CONTENT_RELIC_SPLIT_PRISM, "relic_split_prism", "Split Prism", "Split Prism", ACH_SPEED_DEMON },
    [META_CONTENT_RELIC_BLOOD_AMBER] = { META_CONTENT_RELIC_BLOOD_AMBER, "relic_blood_amber", "Blood Amber", "Blood Amber", ACH_SOLO_ARTIST },
    [META_CONTENT_RELIC_TITAN_HEART] = { META_CONTENT_RELIC_TITAN_HEART, "relic_titan_heart", "Titan Heart", "Titan Heart", ACH_PERFECTIONIST },
    [META_CONTENT_RELIC_FRUGAL_TOME] = { META_CONTENT_RELIC_FRUGAL_TOME, "relic_frugal_tome", "Frugal Tome", "Frugal Tome", ACH_CHAMPION },
    [META_CONTENT_CARD_TACTICAL_SHIFT] = { META_CONTENT_CARD_TACTICAL_SHIFT, "card_tactical_shift", "Tactical Shift", "Mystery Card", ACH_FIRST_STEPS },
    [META_CONTENT_CARD_IRON_INTERCEPT] = { META_CONTENT_CARD_IRON_INTERCEPT, "card_iron_intercept", "Iron Intercept", "Mystery Card", ACH_INTERRUPTED },
    [META_CONTENT_CARD_SANCTUARY] = { META_CONTENT_CARD_SANCTUARY, "card_sanctuary", "Sanctuary", "Mystery Card", ACH_PERFECTIONIST },
    [META_CONTENT_CARD_PRISMATIC_BURST] = { META_CONTENT_CARD_PRISMATIC_BURST, "card_prismatic_burst", "Prismatic Burst", "Mystery Card", ACH_SPEED_DEMON },
    [META_CONTENT_RELIC_DUELIST_SIGIL] = { META_CONTENT_RELIC_DUELIST_SIGIL, "relic_duelist_sigil", "Duelist Sigil", "Mystery Relic", ACH_SOLO_ARTIST },
    [META_CONTENT_RELIC_FELLOWSHIP_STANDARD] = { META_CONTENT_RELIC_FELLOWSHIP_STANDARD, "relic_fellowship_standard", "Fellowship Standard", "Mystery Relic", ACH_FULL_HOUSE },
    [META_CONTENT_RELIC_CHRONICLE_QUILL] = { META_CONTENT_RELIC_CHRONICLE_QUILL, "relic_chronicle_quill", "Chronicle Quill", "Mystery Relic", ACH_COMPLETIONIST },
    [META_CONTENT_EVENT_CHAMPIONS_FEAST] = { META_CONTENT_EVENT_CHAMPIONS_FEAST, "event_champions_feast", "Champion's Feast", "Mystery Event", ACH_CHAMPION },
    [META_CONTENT_EVENT_CROWDED_STAGE] = { META_CONTENT_EVENT_CROWDED_STAGE, "event_crowded_stage", "Crowded Stage", "Mystery Event", ACH_FULL_HOUSE },
    [META_CONTENT_EVENT_HALL_OF_RECORDS] = { META_CONTENT_EVENT_HALL_OF_RECORDS, "event_hall_of_records", "Hall of Records", "Mystery Event", ACH_COMPLETIONIST },
};

static int meta_content_flag(MetaContentUnlock id)
{
    if (id < 0 || id >= META_CONTENT_COUNT) return 0;
    return 1 << id;
}

#define META_UNLOCK_EVENT_REGISTRY_MAX 96

typedef struct {
    const char *unlock_key;
    const char *unlock_event;
    AchievementId achievement;
    bool registered;
} MetaUnlockEventDef;

static MetaUnlockEventDef meta_unlock_events[META_UNLOCK_EVENT_REGISTRY_MAX];
static int meta_unlock_event_count = 0;

void meta_set_defaults(MetaProgress *meta)
{
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
    meta->best_floor = 1;
    meta->highest_area_unlocked = 0;
    meta->max_ascension_unlocked = 0;
    meta->ascension_beaten = 0;
}

static bool meta_has_content_flag(const MetaProgress *meta, MetaContentUnlock id)
{
    int flag = meta_content_flag(id);
    return meta && flag != 0 && ((meta->content_unlock_flags & flag) != 0);
}

static void meta_set_content_flag(MetaProgress *meta, MetaContentUnlock id)
{
    int flag = meta_content_flag(id);
    if (meta && flag != 0)
        meta->content_unlock_flags |= flag;
}

static void migrate_content_unlocks(MetaProgress *meta)
{
    if (!meta) return;

    if (meta->slot5_unlocked)
        meta->slot4_unlocked = true;
    if (meta->slot4_unlocked)
        meta_set_content_flag(meta, META_CONTENT_PARTY_SLOT_IV);
    if (meta->formation_drills)
        meta_set_content_flag(meta, META_CONTENT_FORMATION_DRILLS);
    if (meta->slot5_unlocked)
        meta_set_content_flag(meta, META_CONTENT_PARTY_SLOT_V);
    if (meta->paladin_unlocked)
        meta_set_content_flag(meta, META_CONTENT_CLASS_PALADIN);
    if (meta->warlock_unlocked)
        meta_set_content_flag(meta, META_CONTENT_CLASS_WARLOCK);
    if (meta->bard_unlocked)
        meta_set_content_flag(meta, META_CONTENT_CLASS_BARD);
    if ((meta->relic_unlock_flags & META_RELIC_UNLOCK_ECHO_BELL) != 0)
        meta_set_content_flag(meta, META_CONTENT_RELIC_ECHO_BELL);
    if ((meta->relic_unlock_flags & META_RELIC_UNLOCK_SPLIT_PRISM) != 0)
        meta_set_content_flag(meta, META_CONTENT_RELIC_SPLIT_PRISM);
    if ((meta->relic_unlock_flags & META_RELIC_UNLOCK_BLOOD_AMBER) != 0)
        meta_set_content_flag(meta, META_CONTENT_RELIC_BLOOD_AMBER);
    if ((meta->relic_unlock_flags & META_RELIC_UNLOCK_TITAN_HEART) != 0)
        meta_set_content_flag(meta, META_CONTENT_RELIC_TITAN_HEART);
    if ((meta->relic_unlock_flags & META_RELIC_UNLOCK_FRUGAL_TOME) != 0)
        meta_set_content_flag(meta, META_CONTENT_RELIC_FRUGAL_TOME);

    meta->content_unlock_flags &= META_CONTENT_UNLOCK_ALL;

    if (meta_has_content_flag(meta, META_CONTENT_PARTY_SLOT_V))
    {
        meta->slot4_unlocked = true;
        meta->slot5_unlocked = true;
        meta_set_content_flag(meta, META_CONTENT_PARTY_SLOT_IV);
    }
    if (meta_has_content_flag(meta, META_CONTENT_PARTY_SLOT_IV))
        meta->slot4_unlocked = true;
    if (meta_has_content_flag(meta, META_CONTENT_FORMATION_DRILLS))
        meta->formation_drills = true;
    if (meta_has_content_flag(meta, META_CONTENT_CLASS_PALADIN))
        meta->paladin_unlocked = true;
    if (meta_has_content_flag(meta, META_CONTENT_CLASS_WARLOCK))
        meta->warlock_unlocked = true;
    if (meta_has_content_flag(meta, META_CONTENT_CLASS_BARD))
        meta->bard_unlocked = true;
    if (meta_has_content_flag(meta, META_CONTENT_RELIC_ECHO_BELL))
        meta->relic_unlock_flags |= META_RELIC_UNLOCK_ECHO_BELL;
    if (meta_has_content_flag(meta, META_CONTENT_RELIC_SPLIT_PRISM))
        meta->relic_unlock_flags |= META_RELIC_UNLOCK_SPLIT_PRISM;
    if (meta_has_content_flag(meta, META_CONTENT_RELIC_BLOOD_AMBER))
        meta->relic_unlock_flags |= META_RELIC_UNLOCK_BLOOD_AMBER;
    if (meta_has_content_flag(meta, META_CONTENT_RELIC_TITAN_HEART))
        meta->relic_unlock_flags |= META_RELIC_UNLOCK_TITAN_HEART;
    if (meta_has_content_flag(meta, META_CONTENT_RELIC_FRUGAL_TOME))
        meta->relic_unlock_flags |= META_RELIC_UNLOCK_FRUGAL_TOME;
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
    migrate_content_unlocks(meta);
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
        meta->content_unlock_flags != 0 ||
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
    else if (strcmp(key, "content_unlock_flags") == 0) meta->content_unlock_flags = value;
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
                else if (strncmp(key, "achievement_time_", 17) == 0)
                {
                    int idx = atoi(key + 17);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievement_times[idx] = value;
                }
                else if (strncmp(key, "achievement_timestamp_", 22) == 0)
                {
                    int idx = atoi(key + 22);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievement_timestamps[idx] = value;
                }
                else if (strncmp(key, "achievement_party_", 18) == 0)
                {
                    int idx = atoi(key + 18);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievement_party[idx] = value;
                }
                else if (strncmp(key, "achievement_", 12) == 0)
                {
                    int idx = atoi(key + 12);
                    if (idx >= 0 && idx < ACH_COUNT)
                        meta->achievements[idx] = value != 0;
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
        else if (strncmp(key, "achievement_time_", 17) == 0)
        {
            int idx = atoi(key + 17);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievement_times[idx] = value;
        }
        else if (strncmp(key, "achievement_timestamp_", 22) == 0)
        {
            int idx = atoi(key + 22);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievement_timestamps[idx] = value;
        }
        else if (strncmp(key, "achievement_party_", 18) == 0)
        {
            int idx = atoi(key + 18);
            if (idx >= 0 && idx < ACH_COUNT)
                meta->achievement_party[idx] = value;
        }
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
    FMT("content_unlock_flags=%d\n", meta->content_unlock_flags);
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
            FMT("achievement_timestamp_%d=%d\n", i, meta->achievement_timestamps[i]);
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
    fprintf(f, "content_unlock_flags=%d\n", meta->content_unlock_flags);
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
            fprintf(f, "achievement_timestamp_%d=%d\n", i, meta->achievement_timestamps[i]);
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
    if (!class_is_loaded((ClassType)class_index)) return false;
    const char *unlock_key = class_unlock_key((ClassType)class_index);
    if (!unlock_key || !unlock_key[0])
        return true;
    if (!meta) return false;
    if (meta_content_active(meta, unlock_key))
        return true;
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
    if (!meta || class_index < 0 || class_index >= CLASS_COUNT || !class_is_loaded((ClassType)class_index))
        return false;
    const char *unlock_key = class_unlock_key((ClassType)class_index);
    if (unlock_key && unlock_key[0])
    {
        meta_content_activate(meta, unlock_key);
        return true;
    }
    switch (class_index)
    {
        case 6: meta_content_activate(meta, "class_paladin"); return true;
        case 7: meta_content_activate(meta, "class_warlock"); return true;
        case 8: meta_content_activate(meta, "class_bard"); return true;
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

const char *meta_content_key(MetaContentUnlock id)
{
    if (id < 0 || id >= META_CONTENT_COUNT) return NULL;
    return META_CONTENT_DEFS[id].key;
}

MetaContentUnlock meta_content_from_key(const char *key)
{
    if (!key || !key[0]) return META_CONTENT_COUNT;
    for (int i = 0; i < META_CONTENT_COUNT; i++)
        if (META_CONTENT_DEFS[i].key && strcmp(META_CONTENT_DEFS[i].key, key) == 0)
            return (MetaContentUnlock)i;
    return META_CONTENT_COUNT;
}

const char *meta_content_display_name(const char *key, bool revealed)
{
    MetaContentUnlock id = meta_content_from_key(key);
    if (id < 0 || id >= META_CONTENT_COUNT) return "";
    const MetaContentDef *def = &META_CONTENT_DEFS[id];
    return revealed ? def->name : def->secret_name;
}

static MetaUnlockEventDef *meta_unlock_event_entry(const char *key)
{
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < meta_unlock_event_count; i++)
        if (meta_unlock_events[i].unlock_key && strcmp(meta_unlock_events[i].unlock_key, key) == 0)
            return &meta_unlock_events[i];
    return NULL;
}

const char *meta_content_unlock_event(const char *key)
{
    MetaUnlockEventDef *entry = meta_unlock_event_entry(key);
    if (entry && entry->registered)
        return entry->unlock_event ? entry->unlock_event : "";

    AchievementId req = meta_content_required_achievement(key);
    return (req >= 0 && req < ACH_COUNT) ? achievement_key(req) : "";
}

void meta_content_register_unlock_event(const char *unlock_key, const char *unlock_event)
{
    if (!unlock_key || !unlock_key[0]) return;

    MetaUnlockEventDef *entry = meta_unlock_event_entry(unlock_key);
    if (!entry)
    {
        if (meta_unlock_event_count >= META_UNLOCK_EVENT_REGISTRY_MAX)
            return;
        entry = &meta_unlock_events[meta_unlock_event_count++];
    }

    entry->unlock_key = unlock_key;
    entry->unlock_event = unlock_event ? unlock_event : "";
    entry->achievement = achievement_from_key(entry->unlock_event);
    entry->registered = true;
}

AchievementId meta_content_required_achievement(const char *key)
{
    MetaUnlockEventDef *entry = meta_unlock_event_entry(key);
    if (entry && entry->registered)
        return entry->achievement;

    MetaContentUnlock id = meta_content_from_key(key);
    if (id < 0 || id >= META_CONTENT_COUNT) return ACH_COUNT;
    return META_CONTENT_DEFS[id].required_achievement;
}

bool meta_content_active(const MetaProgress *meta, const char *key)
{
    if (!key || !key[0]) return true;
    MetaContentUnlock id = meta_content_from_key(key);
    if (id < 0 || id >= META_CONTENT_COUNT) return false;
    if (!meta) return false;
    if (meta_has_content_flag(meta, id))
        return true;

    switch (id)
    {
        case META_CONTENT_PARTY_SLOT_IV: return meta->slot4_unlocked;
        case META_CONTENT_FORMATION_DRILLS: return meta->formation_drills;
        case META_CONTENT_PARTY_SLOT_V: return meta->slot5_unlocked;
        case META_CONTENT_CLASS_PALADIN: return meta->paladin_unlocked;
        case META_CONTENT_CLASS_WARLOCK: return meta->warlock_unlocked;
        case META_CONTENT_CLASS_BARD: return meta->bard_unlocked;
        case META_CONTENT_RELIC_ECHO_BELL: return (meta->relic_unlock_flags & META_RELIC_UNLOCK_ECHO_BELL) != 0;
        case META_CONTENT_RELIC_SPLIT_PRISM: return (meta->relic_unlock_flags & META_RELIC_UNLOCK_SPLIT_PRISM) != 0;
        case META_CONTENT_RELIC_BLOOD_AMBER: return (meta->relic_unlock_flags & META_RELIC_UNLOCK_BLOOD_AMBER) != 0;
        case META_CONTENT_RELIC_TITAN_HEART: return (meta->relic_unlock_flags & META_RELIC_UNLOCK_TITAN_HEART) != 0;
        case META_CONTENT_RELIC_FRUGAL_TOME: return (meta->relic_unlock_flags & META_RELIC_UNLOCK_FRUGAL_TOME) != 0;
        default: return false;
    }
}

bool meta_content_eligible(const MetaProgress *meta, const char *key)
{
    if (!key || !key[0]) return true;
    if (meta_content_active(meta, key)) return true;
    AchievementId req = meta_content_required_achievement(key);
    if (req < 0 || req >= ACH_COUNT) return false;
    return meta && meta->achievements[req];
}

void meta_content_activate(MetaProgress *meta, const char *key)
{
    if (!meta || !key || !key[0]) return;
    MetaContentUnlock id = meta_content_from_key(key);
    if (id < 0 || id >= META_CONTENT_COUNT) return;

    meta_set_content_flag(meta, id);
    switch (id)
    {
        case META_CONTENT_PARTY_SLOT_IV:
            meta->slot4_unlocked = true;
            break;
        case META_CONTENT_FORMATION_DRILLS:
            meta->formation_drills = true;
            break;
        case META_CONTENT_PARTY_SLOT_V:
            meta->slot4_unlocked = true;
            meta->slot5_unlocked = true;
            meta_set_content_flag(meta, META_CONTENT_PARTY_SLOT_IV);
            break;
        case META_CONTENT_CLASS_PALADIN:
            meta->paladin_unlocked = true;
            break;
        case META_CONTENT_CLASS_WARLOCK:
            meta->warlock_unlocked = true;
            break;
        case META_CONTENT_CLASS_BARD:
            meta->bard_unlocked = true;
            break;
        case META_CONTENT_RELIC_ECHO_BELL:
            meta->relic_unlock_flags |= META_RELIC_UNLOCK_ECHO_BELL;
            break;
        case META_CONTENT_RELIC_SPLIT_PRISM:
            meta->relic_unlock_flags |= META_RELIC_UNLOCK_SPLIT_PRISM;
            break;
        case META_CONTENT_RELIC_BLOOD_AMBER:
            meta->relic_unlock_flags |= META_RELIC_UNLOCK_BLOOD_AMBER;
            break;
        case META_CONTENT_RELIC_TITAN_HEART:
            meta->relic_unlock_flags |= META_RELIC_UNLOCK_TITAN_HEART;
            break;
        case META_CONTENT_RELIC_FRUGAL_TOME:
            meta->relic_unlock_flags |= META_RELIC_UNLOCK_FRUGAL_TOME;
            break;
        default:
            break;
    }
}

const char *meta_relic_unlock_key(RelicId id)
{
    switch (id)
    {
        case RELIC_ECHO_BELL: return "relic_echo_bell";
        case RELIC_SPLIT_PRISM: return "relic_split_prism";
        case RELIC_BLOOD_AMBER: return "relic_blood_amber";
        case RELIC_TITAN_HEART: return "relic_titan_heart";
        case RELIC_FRUGAL_TOME: return "relic_frugal_tome";
        case RELIC_DUELIST_SIGIL: return "relic_duelist_sigil";
        case RELIC_FELLOWSHIP_STANDARD: return "relic_fellowship_standard";
        case RELIC_CHRONICLE_QUILL: return "relic_chronicle_quill";
        default: return NULL;
    }
}

void meta_content_achievement_rewards(AchievementId id, bool achieved, char *out, int out_size)
{
    if (!out || out_size <= 0) return;
    out[0] = '\0';
    for (int i = 0; i < META_CONTENT_COUNT; i++)
    {
        const MetaContentDef *def = &META_CONTENT_DEFS[i];
        if (meta_content_required_achievement(def->key) != id) continue;
        const char *name = achieved ? def->name : def->secret_name;
        if (!name || !name[0]) continue;
        if (out[0])
            strncat(out, ", ", out_size - strlen(out) - 1);
        strncat(out, name, out_size - strlen(out) - 1);
    }
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
    const char *key = meta_relic_unlock_key(id);
    if (!key) return true;
    return meta_content_active(meta, key);
}

void meta_unlock_relic(MetaProgress *meta, RelicId id)
{
    if (!meta) return;
    const char *key = meta_relic_unlock_key(id);
    if (key)
        meta_content_activate(meta, key);
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
    meta->achievement_timestamps[id] = current_unix_timestamp();
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

    meta->interrupts_total += interrupts;

    AchievementRunStats stats = {
        .first_run = first_run,
        .won = won,
        .area_index = area_index,
        .floor_reached = floor_reached,
        .bosses_defeated = bosses_defeated,
        .party_size = party_size,
        .party_classes = party_classes,
        .deaths = deaths,
        .relic_count = relic_count,
        .run_interrupts = interrupts,
        .total_interrupts = meta->interrupts_total,
        .best_combat_turns = best_combat_turns,
        .area_count = area_count,
        .runs_completed = meta->runs_completed,
        .wins = meta->wins,
        .total_bosses_defeated = meta->bosses_defeated_total,
        .ascension_level = meta->ascension_level,
    };

    int ach = 0;
    for (int i = 0; i < achievement_count(); i++)
    {
        AchievementId id = achievement_at(i);
        if (achievement_condition_met(id, &stats))
            ach += award_achievement(meta, id, achievement_names, achievement_names_size, run_number, party_classes, party_size);
    }

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
