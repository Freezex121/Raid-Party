#ifndef HAND_RENDER_H
#define HAND_RENDER_H

#include "systems/deck.h"
#include "systems/energy.h"
#include "combat/combat.h"

void hand_render_draw(Deck *deck, Energy *energy, int hovered_card, ClassType channel_class, int target_idx, float target_offset, ComboPrime combo_prime);

#endif
