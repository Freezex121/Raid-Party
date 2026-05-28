#ifndef META_H
#define META_H

#include <stdbool.h>
#include "systems/achievements.h"

#define META_SLOT4_COST 20
#define META_SLOT5_COST 40
#define META_TRAVEL_FUND_MAX_RANK 3
#define META_TRAVEL_FUND_GOLD_PER_RANK 10
#define META_ASCENSION_MAX 10
#define META_LEGACY_MAX_RANK 3
#define META_CLASS_UNLOCK_COST 20

typedef struct {
    int runs_completed;
    int wins;
    int best_floor;
    int bosses_defeated_total;
    int renown;
    int highest_area_unlocked;
    int starting_gold_rank;
    int ascension_level;
    int max_ascension_unlocked;
    int starting_relic_rank;
    int interrupts_total;
    bool slot4_unlocked;
    bool slot5_unlocked;
    bool paladin_unlocked;
    bool warlock_unlocked;
    bool bard_unlocked;
    bool achievements[ACH_COUNT];
    int achievement_times[ACH_COUNT];
    int achievement_party[ACH_COUNT];
    bool start_prep;
    bool start_energize;
    bool start_fortify;
    bool start_rejuv;
    int dmg_bonus;
    int shield_bonus;
    int first_draw_bonus;
    bool seasoned_adventurer;
    bool master_raider;
    bool tutorial_seen_elite;
    bool tutorial_seen_boss;
    bool tutorial_seen_shop;
    bool tutorial_seen_event;
    bool tutorial_seen_rest;
    bool tutorial_seen_level_up;
    bool tutorial_seen_discard;
    bool tutorial_seen_game_over;
    bool tutorial_seen_meta_shop;
} MetaProgress;

void meta_set_defaults(MetaProgress *meta);
void meta_load(MetaProgress *meta);
void meta_save(const MetaProgress *meta);
int meta_party_slots(const MetaProgress *meta);
int meta_starting_gold(const MetaProgress *meta);
int meta_next_travel_fund_cost(const MetaProgress *meta);
int meta_next_legacy_cost(const MetaProgress *meta);
bool meta_area_unlocked(const MetaProgress *meta, int area_index);
bool meta_class_unlocked(const MetaProgress *meta, int class_index);
bool meta_unlock_class(MetaProgress *meta, int class_index);
int meta_next_upgrade_cost(int rank);
int meta_starting_deck_bonuses(const MetaProgress *meta);
int meta_dmg_bonus(const MetaProgress *meta);
int meta_shield_bonus(const MetaProgress *meta);
int meta_first_draw_bonus(const MetaProgress *meta);
int meta_renown_bonus_per_boss(const MetaProgress *meta);
int meta_renown_bonus_per_clear(const MetaProgress *meta);
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
    int achievement_names_size);

#endif
