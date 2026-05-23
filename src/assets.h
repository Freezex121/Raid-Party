#ifndef ASSETS_H
#define ASSETS_H

#include "raylib.h"
#include "systems/party.h"
#include "constants.h"
#include <stdbool.h>

#define CARD_ART_SOURCE_W CARD_SOURCE_W
#define CARD_ART_SOURCE_H CARD_SOURCE_H
#define UI_FONT_MIN_SIZE 3
#define UI_FONT_MAX_SIZE 32

typedef enum {
    SFX_BUTTON_HOVER,
    SFX_BUTTON_CLICK,
    SFX_CARD_HOVER,
    SFX_CARD_PLAY,
    SFX_CARD_DISCARD,
    SFX_DAMAGE,
    SFX_HEAL,
    SFX_SHIELD,
    SFX_TAUNT,
    SFX_INTERRUPT,
    SFX_BURN_TICK,
    SFX_PARTY_DOWNED,
    SFX_PARTY_REVIVED,
    SFX_ENEMY_CAST_WARNING,
    SFX_BOSS_CAST_WARNING,
    SFX_GOLD_PICKUP,
    SFX_REWARD_PICKUP,
    SFX_COUNT
} GameSfx;

typedef enum {
    MUSIC_TITLE,
    MUSIC_COMBAT,
    MUSIC_BOSS,
    MUSIC_COUNT
} GameMusic;

typedef struct {
    Font ui_font;
    bool ui_font_loaded;
    Font ui_fonts[UI_FONT_MAX_SIZE + 1];
    bool ui_font_sizes_loaded[UI_FONT_MAX_SIZE + 1];
    Texture2D paper_texture;
    Texture2D card_template;
    Texture2D class_icons[CLASS_COUNT];
    Sound sfx[SFX_COUNT];
    bool sfx_loaded[SFX_COUNT];
    Music music[MUSIC_COUNT];
    bool music_loaded[MUSIC_COUNT];
    GameMusic current_music;
    bool music_playing;
    bool audio_loaded;
    bool loaded;
} GameAssets;

extern GameAssets g_assets;

void assets_load(void);
void assets_unload(void);
void assets_update_audio(void);
void assets_play_sfx(GameSfx sfx);
void assets_play_music(GameMusic music);
void assets_stop_music(void);

#endif
