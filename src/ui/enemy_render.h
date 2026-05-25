#ifndef ENEMY_RENDER_H
#define ENEMY_RENDER_H

#include "combat/combat.h"

void enemy_render_draw(EnemyState *enemy, bool highlighted, bool targeting);
void enemy_render_draw_status_tooltip(EnemyState *enemy);

#endif
