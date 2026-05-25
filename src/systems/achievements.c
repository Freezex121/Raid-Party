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

const char *achievement_desc(AchievementId id)
{
    switch (id)
    {
        case ACH_FIRST_STEPS:   return "Complete your first run.";
        case ACH_CHAMPION:      return "Win a run.";
        case ACH_PERFECTIONIST: return "Win a run with no deaths.";
        case ACH_SOLO_ARTIST:   return "Win a run with 1 party member.";
        case ACH_FULL_HOUSE:    return "Win a run with 5 party members.";
        case ACH_INTERRUPTED:   return "Interrupt 20 enemy casts across all runs.";
        case ACH_HOARDER:       return "Collect 10 relics in a single run.";
        case ACH_SPEED_DEMON:   return "Clear a combat floor in 3 turns or fewer.";
        case ACH_COMPLETIONIST: return "Beat all areas.";
        default:                return "";
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
