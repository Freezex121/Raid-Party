#ifndef PARTY_FRAMES_H
#define PARTY_FRAMES_H

#include "raylib.h"
#include "game_text.h"
#include "systems/party.h"

void party_frames_draw(Party *party);
void party_frames_draw_tooltips(Party *party);
int  party_frame_hit_test(Party *party, Vector2 mouse);

#endif
