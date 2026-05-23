#ifndef ASSETS_H
#define ASSETS_H

#include "raylib.h"
#include "systems/party.h"
#include "constants.h"
#include <stdbool.h>

#define CARD_ART_SOURCE_W CARD_SOURCE_W
#define CARD_ART_SOURCE_H CARD_SOURCE_H

typedef struct {
    Font ui_font;
    bool ui_font_loaded;
    Texture2D paper_texture;
    Texture2D card_template;
    Texture2D class_icons[CLASS_COUNT];
    bool loaded;
} GameAssets;

extern GameAssets g_assets;

void assets_load(void);
void assets_unload(void);

#endif
