#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

typedef enum {
    ACH_FIRST_STEPS,
    ACH_CHAMPION,
    ACH_PERFECTIONIST,
    ACH_SOLO_ARTIST,
    ACH_FULL_HOUSE,
    ACH_INTERRUPTED,
    ACH_HOARDER,
    ACH_SPEED_DEMON,
    ACH_COMPLETIONIST,
    ACH_COUNT
} AchievementId;

const char *achievement_name(AchievementId id);
const char *achievement_desc(AchievementId id);
int achievement_reward(AchievementId id);

#endif
