#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <stdbool.h>

typedef int AchievementId;

enum {
    ACH_FIRST_STEPS = 0,
    ACH_CHAMPION = 1,
    ACH_PERFECTIONIST = 2,
    ACH_SOLO_ARTIST = 3,
    ACH_FULL_HOUSE = 4,
    ACH_INTERRUPTED = 5,
    ACH_HOARDER = 6,
    ACH_SPEED_DEMON = 7,
    ACH_COMPLETIONIST = 8,
    ACH_LEGACY_COUNT = 9
};

#define ACH_MAX_COUNT 64
#define ACH_COUNT ACH_MAX_COUNT

typedef struct {
    bool first_run;
    bool won;
    int area_index;
    int floor_reached;
    int bosses_defeated;
    int party_size;
    const int *party_classes;
    int deaths;
    int relic_count;
    int run_interrupts;
    int total_interrupts;
    int best_combat_turns;
    int area_count;
    int runs_completed;
    int wins;
    int total_bosses_defeated;
    int ascension_level;
} AchievementRunStats;

bool achievements_load_json(const char *path);
int achievement_count(void);
AchievementId achievement_at(int order_index);
bool achievement_loaded(AchievementId id);
bool achievement_condition_met(AchievementId id, const AchievementRunStats *stats);
const char *achievement_name(AchievementId id);
const char *achievement_key(AchievementId id);
AchievementId achievement_from_key(const char *key);
const char *achievement_desc(AchievementId id);
int achievement_reward(AchievementId id);

#endif
