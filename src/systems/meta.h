#ifndef META_H
#define META_H

#include <stdbool.h>

#define META_SLOT4_COST 4
#define META_SLOT5_COST 8
#define META_TRAVEL_FUND_MAX_RANK 3
#define META_TRAVEL_FUND_GOLD_PER_RANK 10

typedef struct {
    int runs_completed;
    int wins;
    int best_floor;
    int bosses_defeated_total;
    int renown;
    int highest_area_unlocked;
    int starting_gold_rank;
    bool slot4_unlocked;
    bool slot5_unlocked;
} MetaProgress;

void meta_set_defaults(MetaProgress *meta);
void meta_load(MetaProgress *meta);
void meta_save(const MetaProgress *meta);
int meta_party_slots(const MetaProgress *meta);
int meta_starting_gold(const MetaProgress *meta);
int meta_next_travel_fund_cost(const MetaProgress *meta);
bool meta_area_unlocked(const MetaProgress *meta, int area_index);
int meta_record_run(MetaProgress *meta, bool won, int area_index, int floor_reached, int bosses_defeated);

#endif
