#ifndef GAME_TEXT_H
#define GAME_TEXT_H

#include "raylib.h"

void game_draw_text(const char *text, int pos_x, int pos_y, int font_size, Color color);
int  game_measure_text(const char *text, int font_size);

#ifndef GAME_TEXT_NO_REDIRECT
#define DrawText(text, posX, posY, fontSize, ...) game_draw_text((text), (posX), (posY), (fontSize), __VA_ARGS__)
#define MeasureText(text, fontSize) game_measure_text((text), (fontSize))
#endif

#endif
