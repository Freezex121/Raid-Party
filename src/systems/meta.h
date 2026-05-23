#ifndef META_H
#define META_H

#include <stdbool.h>

typedef struct {
    int runs_completed;
    int wins;
    int best_floor;
    int bosses_defeated_total;
    bool slot4_unlocked;
    bool slot5_unlocked;
} MetaProgress;

void meta_set_defaults(MetaProgress *meta);
void meta_load(MetaProgress *meta);
void meta_save(const MetaProgress *meta);
int meta_party_slots(const MetaProgress *meta);
void meta_record_run(MetaProgress *meta, bool won, int floor_reached, int bosses_defeated);

#endif
