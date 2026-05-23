#ifndef ENERGY_H
#define ENERGY_H

#include <stdbool.h>

#define MAX_ENERGY 6
#define BASE_REGEN 3

typedef struct {
    int current;
    int max;
    int regen;
} Energy;

void energy_init(Energy *e, int start, int max, int regen);
bool energy_has(Energy *e, int amount);
void energy_spend(Energy *e, int amount);
void energy_refresh(Energy *e);

#endif
