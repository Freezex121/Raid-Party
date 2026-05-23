#include "status.h"
#include <string.h>

int status_find(StatusEffect *statuses, int count, StatusType type)
{
    for (int i = 0; i < count; i++)
        if (statuses[i].type == type) return i;
    return -1;
}

void status_apply(StatusEffect *statuses, int *count, StatusType type, int turns, int value)
{
    int idx = status_find(statuses, *count, type);
    if (idx >= 0)
    {
        statuses[idx].turns = turns;
        statuses[idx].value = value;
    }
    else if (*count < MAX_STATUSES)
    {
        statuses[*count].type = type;
        statuses[*count].turns = turns;
        statuses[*count].value = value;
        (*count)++;
    }
}

void status_tick(StatusEffect *statuses, int *count)
{
    for (int i = *count - 1; i >= 0; i--)
    {
        statuses[i].turns--;
        if (statuses[i].turns <= 0)
        {
            for (int j = i; j < *count - 1; j++)
                statuses[j] = statuses[j + 1];
            (*count)--;
        }
    }
}

void status_clear(StatusEffect *statuses, int *count)
{
    memset(statuses, 0, sizeof(StatusEffect) * (*count));
    *count = 0;
}
