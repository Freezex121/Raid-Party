#ifndef META_H
#define META_H

#include <stdbool.h>
#include "systems/achievements.h"

#define META_SLOT4_COST 4
#define META_SLOT5_COST 8
#define META_TRAVEL_FUND_MAX_RANK 3
#define META_TRAVEL_FUND_GOLD_PER_RANK 10
#define META_ASCENSION_MAX 10
#define META_LEGACY_MAX_RANK 3
#define META_CLASS_UNLOCK_COST 12

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
    int achievement_names_size);

#endif
