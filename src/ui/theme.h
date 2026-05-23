#ifndef THEME_H
#define THEME_H

#include "raylib.h"
#include "systems/deck.h"
#include "systems/map.h"
#include "data/enemy_defs.h"
#include "assets.h"

#define CARD_ART_ASPECT ((float)CARD_ART_SOURCE_W / (float)CARD_ART_SOURCE_H)

Color theme_class_color(ClassType ct);
Color theme_class_dark(ClassType ct);
Color theme_card_type_color(CardType type);
Color theme_node_color(NodeType type);
const char *theme_class_abbrev(ClassType ct);
const char *theme_node_icon(NodeType type);
const char *theme_node_name(NodeType type);
const char *theme_card_type_label(CardType type);
const char *theme_primary_effect_label(const CardDef *card);

void theme_draw_background(void);
void theme_draw_cost_gem(int cx, int cy, int cost, bool enabled);
void theme_draw_effect_badge(Rectangle bounds, const char *label, Color color);
void theme_draw_class_portrait(ClassType ct, int cx, int cy, int radius, bool alive);
void theme_draw_card_art(Rectangle bounds, const CardDef *card, bool upgraded);
void theme_draw_card_tooltip(Rectangle bounds, const CardDef *card, bool upgraded);

#endif
