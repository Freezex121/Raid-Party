#include "energy.h"

void energy_init(Energy *e, int start, int max, int regen)
{
    e->max = max;
    e->regen = regen;
    e->current = start > max ? max : start;
}

bool energy_has(Energy *e, int amount)
{
    return e->current >= amount;
}

void energy_spend(Energy *e, int amount)
{
    e->current -= amount;
    if (e->current < 0) e->current = 0;
}

void energy_refresh(Energy *e)
{
    e->current += e->regen;
    if (e->current > e->max) e->current = e->max;
}
