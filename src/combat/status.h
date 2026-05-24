#ifndef STATUS_H
#define STATUS_H

#define MAX_STATUSES 12

typedef enum {
    STATUS_NONE = -1,
    STATUS_BURNING,
    STATUS_RENEW,
    STATUS_TRAP,
    STATUS_TOTEM_HEAL,
    STATUS_BLEED,
    STATUS_WEAKNESS,
    STATUS_ENERGY_DRAIN,
    STATUS_MARKED,
    STATUS_CONDUCTIVE,
    STATUS_BLIGHT,
} StatusType;

typedef struct {
    StatusType type;
    int turns;
    int value;
} StatusEffect;

int  status_find(StatusEffect *statuses, int count, StatusType type);
void status_apply(StatusEffect *statuses, int *count, StatusType type, int turns, int value);
void status_tick(StatusEffect *statuses, int *count);
void status_clear(StatusEffect *statuses, int *count);

#endif
