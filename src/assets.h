#ifndef ASSETS_H
#define ASSETS_H

#include "raylib.h"
#include "systems/party.h"
#include "systems/relic.h"
#include "constants.h"
#include <stdbool.h>

#define KW_ICON_SIZE 32
#define META_UPGRADE_ICON_SIZE 32

typedef enum {
    KW_ECHO,
    KW_LIFESTEAL,
    KW_RETAIN,
    KW_INTERRUPT,
    KW_TAUNT,
    KW_FLEETING,
    KW_EXHAUST,
    KW_COUNT
} KeywordIcon;

typedef enum {
    META_ICON_UNLOCK_PROGRESS,
    META_ICON_BLADES_I,
    META_ICON_BLADES_II,
    META_ICON_BLADES_III,
    META_ICON_OPENING_STRIKE_I,
    META_ICON_OPENING_STRIKE_II,
    META_ICON_VICTORY_MOMENTUM_I,
    META_ICON_VICTORY_MOMENTUM_II,
    META_ICON_EXPLOIT_WEAKNESS_I,
    META_ICON_EXPLOIT_WEAKNESS_II,
    META_ICON_ELITE_HUNTER,
    META_ICON_BOSS_SLAYER,
    META_ICON_WARLOCK_RUMORS,
    META_ICON_UNLOCK_WARLOCK,
    META_ICON_WARLOCK_ADEPT,
    META_ICON_TROPHY_HUNTER_I,
    META_ICON_TROPHY_HUNTER_II,
    META_ICON_ARMOR_I,
    META_ICON_ARMOR_II,
    META_ICON_ARMOR_III,
    META_ICON_WARDENS_OATH,
    META_ICON_LAST_STAND,
    META_ICON_COMBAT_SHIELD_I,
    META_ICON_COMBAT_SHIELD_II,
    META_ICON_COMBAT_SHIELD_III,
    META_ICON_EMERGENCY_BARRIER,
    META_ICON_THICK_SKIN_I,
    META_ICON_THICK_SKIN_II,
    META_ICON_THICK_SKIN_III,
    META_ICON_SHIELD_CAP_I,
    META_ICON_SHIELD_CAP_II,
    META_ICON_SHIELD_CAP_III,
    META_ICON_SHIELD_CAP_IV,
    META_ICON_SHIELD_CAP_V,
    META_ICON_SHIELD_CAP_VI,
    META_ICON_TRAVELERS_PACK,
    META_ICON_UPGRADED_FORTIFY,
    META_ICON_PALADIN_OATH,
    META_ICON_UNLOCK_PALADIN,
    META_ICON_PALADIN_BULWARK,
    META_ICON_MANA_CRYSTAL,
    META_ICON_CHARGED_CRYSTAL,
    META_ICON_STARTING_ENERGY,
    META_ICON_PREPARATION,
    META_ICON_UPGRADED_PREPARATION,
    META_ICON_CAMP_SUPPLIES,
    META_ICON_CAMP_MASTER,
    META_ICON_SCOUTS_KIT_I,
    META_ICON_SCOUTS_KIT_II,
    META_ICON_SCOUTS_KIT_III,
    META_ICON_FIRST_AID,
    META_ICON_UPGRADED_REJUVENATION,
    META_ICON_HEALING_TOUCH_I,
    META_ICON_HEALING_TOUCH_II,
    META_ICON_BARD_SCHOOL,
    META_ICON_UNLOCK_BARD,
    META_ICON_BARD_ENCORE,
    META_ICON_HARMONY,
    META_ICON_TRAVEL_FUND_I,
    META_ICON_TRAVEL_FUND_II,
    META_ICON_TRAVEL_FUND_III,
    META_ICON_GOLD_CONVERSION,
    META_ICON_MASTER_RAIDER,
    META_ICON_COMPLETIONIST_BANNER,
    META_ICON_MERCHANT_CONTACTS_I,
    META_ICON_MERCHANT_CONTACTS_II,
    META_ICON_BLACK_MARKET,
    META_ICON_REWARD_REROLL_I,
    META_ICON_REWARD_CHOICE_I,
    META_ICON_REWARD_CHOICE_II,
    META_ICON_VETERAN_REWARDS,
    META_ICON_LEGACY_I,
    META_ICON_LEGACY_II,
    META_ICON_LEGACY_III,
    META_ICON_RELIC_CHOICE,
    META_ICON_RELIC_ECHO_BELL,
    META_ICON_RELIC_SPLIT_PRISM,
    META_ICON_RELIC_BLOOD_AMBER,
    META_ICON_RELIC_TITAN_HEART,
    META_ICON_RELIC_FRUGAL_TOME,
    META_ICON_PARTY_SLOT_IV,
    META_ICON_FORMATION_DRILLS,
    META_ICON_PARTY_SLOT_V,
    META_ICON_COUNT
} MetaUpgradeIcon;

#define CARD_ART_SOURCE_W CARD_SOURCE_W
#define CARD_ART_SOURCE_H CARD_SOURCE_H
#define CARD_BACKGROUND_MAX 12
#define UI_FONT_MIN_SIZE 18
#define UI_FONT_MAX_SIZE 72
#define PIXEL_FONT_MAX_SIZE 17
#define PIXEL_FONT_SCALE 0.5f
#define COBBLESTONE_FONT_SCALE 1.0f

typedef enum {
    SFX_BUTTON_HOVER,
    SFX_BUTTON_CLICK,
    SFX_CARD_HOVER,
    SFX_CARD_PLAY,
    SFX_CARD_DISCARD,
    SFX_CARD_DRAW,
    SFX_DAMAGE,
    SFX_DAMAGE_HEAVY,
    SFX_HEAL,
    SFX_SHIELD,
    SFX_TAUNT,
    SFX_INTERRUPT,
    SFX_BURN_TICK,
    SFX_BLEED_TICK,
    SFX_PARTY_DOWNED,
    SFX_PARTY_REVIVED,
    SFX_ENEMY_CAST_WARNING,
    SFX_BOSS_CAST_WARNING,
    SFX_ENEMY_ATTACK,
    SFX_GOLD_PICKUP,
    SFX_REWARD_PICKUP,
    SFX_LEVEL_UP,
    SFX_VICTORY,
    SFX_DEFEAT,
    SFX_MAP_SELECT,
    SFX_SYNERGY_TRIGGER,
    SFX_SHOP_PURCHASE,
    SFX_ERROR,
    SFX_COUNT
} GameSfx;

typedef enum {
    MUSIC_TITLE,
    MUSIC_MAP,
    MUSIC_COMBAT_GREENWOOD,
    MUSIC_COMBAT_VENOM,
    MUSIC_COMBAT_CINDER,
    MUSIC_COMBAT_CATACOMBS,
    MUSIC_COMBAT_CITADEL,
    MUSIC_BOSS_GREENWOOD,
    MUSIC_BOSS_VENOM,
    MUSIC_BOSS_CINDER,
    MUSIC_BOSS_CATACOMBS,
    MUSIC_BOSS_CITADEL,
    MUSIC_SHOP,
    MUSIC_REST,
    MUSIC_EVENT,
    MUSIC_VICTORY,
    MUSIC_DEFEAT,
    MUSIC_COUNT
} GameMusic;

typedef struct {
    Font ui_font;
    bool ui_font_loaded;
    Font ui_fonts[UI_FONT_MAX_SIZE + 1];
    bool ui_font_sizes_loaded[UI_FONT_MAX_SIZE + 1];
    float ui_font_scales[UI_FONT_MAX_SIZE + 1];
    Texture2D paper_texture;
    Texture2D card_template;
    Texture2D card_template_upgraded;
    Texture2D card_template_maxed;
    Texture2D card_backgrounds[CARD_BACKGROUND_MAX];
    int card_background_count;
    Texture2D card_tint_mask;
    Texture2D card_info;
    Texture2D card_border;
    Texture2D card_border_upgraded;
    Texture2D card_border_maxed;
    Texture2D relic_template;
    Texture2D class_icons[CLASS_COUNT];
    Texture2D relic_icons[RELIC_COUNT];
    Texture2D relic_icon_placeholder;
    Texture2D node_sprites[8];
    Texture2D btn_standard;
    Texture2D btn_large;
    Texture2D kw_icons[KW_COUNT];
    Texture2D meta_upgrade_icons[META_ICON_COUNT];
    Sound sfx[SFX_COUNT];
    bool sfx_loaded[SFX_COUNT];
    Music music[MUSIC_COUNT];
    bool music_loaded[MUSIC_COUNT];
    float music_volume;
    float sfx_volume;
    GameMusic current_music;
    bool music_playing;
    float music_fade_remaining;
    GameMusic music_fade_next;
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
void assets_play_combat_music(int area_id, bool is_boss);
Texture2D load_art_texture(const char *filename);
void assets_set_music_volume(float volume);
void assets_set_sfx_volume(float volume);

#endif
