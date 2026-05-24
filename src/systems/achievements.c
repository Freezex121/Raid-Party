#include "achievements.h"

const char *achievement_name(AchievementId id)
{
    switch (id)
    {
        case ACH_FIRST_STEPS:   return "First Steps";
        case ACH_CHAMPION:      return "Champion";
        case ACH_PERFECTIONIST: return "Perfectionist";
        case ACH_SOLO_ARTIST:   return "Solo Artist";
        case ACH_FULL_HOUSE:    return "Full House";
        case ACH_INTERRUPTED:   return "Interrupted";
        case ACH_HOARDER:       return "Hoarder";
        case ACH_SPEED_DEMON:   return "Speed Demon";
        case ACH_COMPLETIONIST: return "Completionist";
        default:                return "Achievement";
    }
}

int achievement_reward(AchievementId id)
{
    switch (id)
    {
        case ACH_FIRST_STEPS:   return 2;
        case ACH_CHAMPION:      return 3;
        case ACH_PERFECTIONIST: return 5;
        case ACH_SOLO_ARTIST:   return 5;
        case ACH_FULL_HOUSE:    return 5;
        case ACH_INTERRUPTED:   return 3;
        case ACH_HOARDER:       return 3;
        case ACH_SPEED_DEMON:   return 3;
        case ACH_COMPLETIONIST: return 10;
        default:                return 0;
    }
}
