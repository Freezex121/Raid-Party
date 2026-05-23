#ifndef CAST_BAR_H
#define CAST_BAR_H

#include <stdbool.h>
#include "data/enemy_defs.h"
#include "raylib.h"
#include "game_text.h"

void cast_bar_draw_ex(const char *ability_name, int remaining_turns, int total_turns, bool is_wipe, int x, int y);
void cast_bar_draw_ability(const EnemyAbility *ability, int remaining_turns, int total_turns, int x, int y);
void cast_bar_draw_ability_tooltip(const EnemyAbility *ability, Rectangle bounds);

#endif
