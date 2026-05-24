#include "assets.h"
#include "raylib.h"
#include <stdio.h>

GameAssets g_assets;

static void make_font_bitmap(Font *font)
{
    if (font->texture.id == 0) return;
    Image img = LoadImageFromTexture(font->texture);
    if (img.data == NULL || img.width == 0 || img.height == 0) return;

    int count = img.width * img.height;

    if (img.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA)
    {
        unsigned char *pixels = (unsigned char *)img.data;
        for (int i = 0; i < count; i++)
        {
            int a = pixels[i * 2 + 1];
            if (a > 127)
            {
                pixels[i * 2] = 255;
                pixels[i * 2 + 1] = 255;
            }
            else
            {
                pixels[i * 2] = 0;
                pixels[i * 2 + 1] = 0;
            }
        }
    }
    else
    {
        Color *colors = LoadImageColors(img);
        if (colors != NULL)
        {
            for (int i = 0; i < count; i++)
            {
                colors[i].a = colors[i].a > 127 ? 255 : 0;
                if (colors[i].a == 0)
                    colors[i] = (Color){ 0, 0, 0, 0 };
                else
                    colors[i] = (Color){ 255, 255, 255, 255 };
            }
            for (int y = 0; y < img.height; y++)
                for (int x = 0; x < img.width; x++)
                    ImageDrawPixel(&img, x, y, colors[y * img.width + x]);
            UnloadImageColors(colors);
        }
    }

    UnloadTexture(font->texture);
    font->texture = LoadTextureFromImage(img);
    SetTextureFilter(font->texture, TEXTURE_FILTER_POINT);
    UnloadImage(img);
}

static Texture2D make_card_template_fallback(void)
{
    Image img = GenImageColor(CARD_ART_SOURCE_W, CARD_ART_SOURCE_H, (Color){ 8, 8, 10, 255 });
    ImageDrawRectangleLines(&img, (Rectangle){ 1, 1, CARD_ART_SOURCE_W - 2, CARD_ART_SOURCE_H - 2 }, 1, (Color){ 210, 210, 220, 255 });
    ImageDrawRectangleLines(&img, (Rectangle){ 4, 13, CARD_ART_SOURCE_W - 8, 24 }, 1, (Color){ 80, 80, 90, 255 });
    ImageDrawRectangleLines(&img, (Rectangle){ 4, 45, CARD_ART_SOURCE_W - 8, 25 }, 1, (Color){ 80, 80, 90, 255 });
    ImageDrawCircle(&img, CARD_ART_SOURCE_W - 8, 4, 7, (Color){ 245, 230, 40, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

static Texture2D load_art_texture(const char *filename)
{
    const char *roots[] = {
        "assets/art",
        "../assets/art",
        "../../assets/art",
    };

    for (int i = 0; i < 3; i++)
    {
        char path[160];
        snprintf(path, sizeof(path), "%s/%s", roots[i], filename);
        if (FileExists(path))
        {
            Texture2D tex = LoadTexture(path);
            if (tex.id != 0)
                SetTextureFilter(tex, TEXTURE_FILTER_POINT);
            return tex;
        }
    }

    return (Texture2D){0};
}

static bool build_asset_path(char *out, int out_size, const char *folder, const char *filename)
{
    const char *roots[] = {
        "assets",
        "../assets",
        "../../assets",
    };

    for (int i = 0; i < 3; i++)
    {
        snprintf(out, out_size, "%s/%s/%s", roots[i], folder, filename);
        if (FileExists(out))
            return true;
    }

    out[0] = '\0';
    return false;
}

static void load_audio_assets(void)
{
    g_assets.audio_loaded = false;
    g_assets.music_playing = false;
    g_assets.current_music = MUSIC_COUNT;

    for (int i = 0; i < SFX_COUNT; i++)
        g_assets.sfx_loaded[i] = false;
    for (int i = 0; i < MUSIC_COUNT; i++)
        g_assets.music_loaded[i] = false;

    if (!IsAudioDeviceReady())
        return;

    const char *sfx_files[SFX_COUNT] = {
        [SFX_BUTTON_HOVER] = "button_hover.wav",
        [SFX_BUTTON_CLICK] = "button_click.wav",
        [SFX_CARD_HOVER] = "card_hover.wav",
        [SFX_CARD_PLAY] = "card_play.wav",
        [SFX_CARD_DISCARD] = "card_discard.wav",
        [SFX_DAMAGE] = "damage.wav",
        [SFX_HEAL] = "heal.wav",
        [SFX_SHIELD] = "shield.wav",
        [SFX_TAUNT] = "taunt.wav",
        [SFX_INTERRUPT] = "interrupt.wav",
        [SFX_BURN_TICK] = "burn_tick.wav",
        [SFX_PARTY_DOWNED] = "party_downed.wav",
        [SFX_PARTY_REVIVED] = "party_revived.wav",
        [SFX_ENEMY_CAST_WARNING] = "enemy_cast_warning.wav",
        [SFX_BOSS_CAST_WARNING] = "boss_cast_warning.wav",
        [SFX_GOLD_PICKUP] = "gold_pickup.wav",
        [SFX_REWARD_PICKUP] = "reward_pickup.wav",
    };

    const char *music_files[MUSIC_COUNT] = {
        [MUSIC_TITLE] = "title.ogg",
        [MUSIC_COMBAT] = "combat.ogg",
        [MUSIC_BOSS] = "boss.ogg",
    };

    char path[192];
    for (int i = 0; i < SFX_COUNT; i++)
    {
        if (sfx_files[i] && build_asset_path(path, sizeof(path), "audio/sfx", sfx_files[i]))
        {
            g_assets.sfx[i] = LoadSound(path);
            g_assets.sfx_loaded[i] = g_assets.sfx[i].frameCount > 0;
        }
    }

    for (int i = 0; i < MUSIC_COUNT; i++)
    {
        if (music_files[i] && build_asset_path(path, sizeof(path), "audio/music", music_files[i]))
        {
            g_assets.music[i] = LoadMusicStream(path);
            g_assets.music_loaded[i] = g_assets.music[i].frameCount > 0;
            if (g_assets.music_loaded[i])
                g_assets.music[i].looping = true;
        }
    }

    g_assets.audio_loaded = true;
}

void assets_load(void)
{
    g_assets.ui_font = GetFontDefault();
    g_assets.ui_font_loaded = false;
    for (int i = 0; i <= UI_FONT_MAX_SIZE; i++)
    {
        g_assets.ui_fonts[i] = (Font){0};
        g_assets.ui_font_sizes_loaded[i] = false;
        g_assets.ui_font_scales[i] = 1.0f;
    }

    const char *pixel_paths[] = {
        "assets/fonts/5x5_pixel.ttf",
        "../assets/fonts/5x5_pixel.ttf",
        "../../assets/fonts/5x5_pixel.ttf",
    };

    const char *cobble_paths[] = {
        "assets/fonts/Cobblestone.ttf",
        "../assets/fonts/Cobblestone.ttf",
        "../../assets/fonts/Cobblestone.ttf",
    };

    const char *pixel_font = NULL;
    const char *cobble_font = NULL;
    for (int i = 0; i < 3 && (!pixel_font || !cobble_font); i++)
    {
        if (!pixel_font && FileExists(pixel_paths[i])) pixel_font = pixel_paths[i];
        if (!cobble_font && FileExists(cobble_paths[i])) cobble_font = cobble_paths[i];
    }

    if (pixel_font)
    {
        for (int size = UI_FONT_MIN_SIZE; size <= PIXEL_FONT_MAX_SIZE; size++)
        {
            Font font = LoadFontEx(pixel_font, size, NULL, 0);
            if (font.texture.id != 0)
            {
                SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
                g_assets.ui_fonts[size] = font;
                g_assets.ui_font_sizes_loaded[size] = true;
                g_assets.ui_font_scales[size] = PIXEL_FONT_SCALE;
                if (size == 16)
                    g_assets.ui_font = font;
                g_assets.ui_font_loaded = true;
            }
        }
    }

    if (cobble_font)
    {
        for (int size = PIXEL_FONT_MAX_SIZE + 1; size <= UI_FONT_MAX_SIZE; size++)
        {
            Font font = LoadFontEx(cobble_font, size, NULL, 0);
            if (font.texture.id != 0)
            {
                make_font_bitmap(&font);
                SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
                g_assets.ui_fonts[size] = font;
                g_assets.ui_font_sizes_loaded[size] = true;
                g_assets.ui_font_scales[size] = COBBLESTONE_FONT_SCALE;
                if (size == 16)
                    g_assets.ui_font = font;
                g_assets.ui_font_loaded = true;
            }
        }
    }

    if (!g_assets.ui_font_loaded && g_assets.ui_font.texture.id != 0)
        SetTextureFilter(g_assets.ui_font.texture, TEXTURE_FILTER_POINT);

    Image paper = GenImageChecked(128, 128, 16, 16,
        (Color){ 26, 27, 40, 255 },
        (Color){ 20, 21, 32, 255 });
    g_assets.paper_texture = LoadTextureFromImage(paper);
    UnloadImage(paper);

    SetTextureWrap(g_assets.paper_texture, TEXTURE_WRAP_REPEAT);

    const char *card_paths[] = {
        "assets/art/card_template.png",
        "../assets/art/card_template.png",
        "../../assets/art/card_template.png",
    };

    g_assets.card_template = (Texture2D){0};
    g_assets.card_template_upgraded = (Texture2D){0};
    g_assets.relic_template = (Texture2D){0};
    for (int i = 0; i < 3; i++)
    {
        if (FileExists(card_paths[i]))
        {
            g_assets.card_template = LoadTexture(card_paths[i]);
            break;
        }
    }

    if (g_assets.card_template.id == 0)
        g_assets.card_template = make_card_template_fallback();

    SetTextureFilter(g_assets.card_template, TEXTURE_FILTER_POINT);

    const char *upgraded_card_paths[] = {
        "assets/art/card_template_upgraded.png",
        "../assets/art/card_template_upgraded.png",
        "../../assets/art/card_template_upgraded.png",
    };

    for (int i = 0; i < 3; i++)
    {
        if (FileExists(upgraded_card_paths[i]))
        {
            g_assets.card_template_upgraded = LoadTexture(upgraded_card_paths[i]);
            break;
        }
    }

    if (g_assets.card_template_upgraded.id != 0)
        SetTextureFilter(g_assets.card_template_upgraded, TEXTURE_FILTER_POINT);

    g_assets.relic_template = load_art_texture("relic_template.png");

    const char *icon_files[CLASS_COUNT] = {
        [CLASS_GUARDIAN] = "guardian_icon.png",
        [CLASS_CLERIC] = "cleric_icon.png",
        [CLASS_MAGE] = "mage_icon.png",
        [CLASS_ROGUE] = "rogue_icon.png",
        [CLASS_SHAMAN] = "shaman_icon.png",
        [CLASS_RANGER] = "ranger_icon.png",
        [CLASS_PALADIN] = "paladin_icon.png",
        [CLASS_WARLOCK] = "warlock_icon.png",
        [CLASS_BARD] = "bard_icon.png",
    };

    for (int i = 0; i < CLASS_COUNT; i++)
        if (icon_files[i])
            g_assets.class_icons[i] = load_art_texture(icon_files[i]);

    load_audio_assets();

    g_assets.loaded = true;
}

void assets_unload(void)
{
    if (!g_assets.loaded) return;

    assets_stop_music();
    if (IsAudioDeviceReady())
    {
        for (int i = 0; i < SFX_COUNT; i++)
            if (g_assets.sfx_loaded[i])
                UnloadSound(g_assets.sfx[i]);
        for (int i = 0; i < MUSIC_COUNT; i++)
            if (g_assets.music_loaded[i])
                UnloadMusicStream(g_assets.music[i]);
    }

    if (g_assets.ui_font_loaded)
    {
        for (int size = UI_FONT_MIN_SIZE; size <= UI_FONT_MAX_SIZE; size++)
        {
            if (g_assets.ui_font_sizes_loaded[size])
                UnloadFont(g_assets.ui_fonts[size]);
            g_assets.ui_fonts[size] = (Font){0};
            g_assets.ui_font_sizes_loaded[size] = false;
        }
        g_assets.ui_font_loaded = false;
    }
    UnloadTexture(g_assets.paper_texture);
    UnloadTexture(g_assets.card_template);
    if (g_assets.card_template_upgraded.id != 0)
        UnloadTexture(g_assets.card_template_upgraded);
    if (g_assets.relic_template.id != 0)
        UnloadTexture(g_assets.relic_template);
    for (int i = 0; i < CLASS_COUNT; i++)
        if (g_assets.class_icons[i].id != 0)
            UnloadTexture(g_assets.class_icons[i]);
    g_assets.loaded = false;
}

void assets_update_audio(void)
{
    if (!IsAudioDeviceReady()) return;
    if (!g_assets.music_playing) return;
    if (g_assets.current_music < 0 || g_assets.current_music >= MUSIC_COUNT) return;
    if (!g_assets.music_loaded[g_assets.current_music]) return;

    UpdateMusicStream(g_assets.music[g_assets.current_music]);
}

void assets_play_sfx(GameSfx sfx)
{
    if (!IsAudioDeviceReady()) return;
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!g_assets.sfx_loaded[sfx]) return;

    PlaySound(g_assets.sfx[sfx]);
}

void assets_play_music(GameMusic music)
{
    if (!IsAudioDeviceReady()) return;
    if (music < 0 || music >= MUSIC_COUNT) return;
    if (!g_assets.music_loaded[music]) return;

    if (g_assets.music_playing && g_assets.current_music == music)
        return;

    assets_stop_music();
    g_assets.current_music = music;
    g_assets.music_playing = true;
    PlayMusicStream(g_assets.music[music]);
}

void assets_stop_music(void)
{
    if (!IsAudioDeviceReady()) return;
    if (!g_assets.music_playing) return;
    if (g_assets.current_music >= 0 && g_assets.current_music < MUSIC_COUNT && g_assets.music_loaded[g_assets.current_music])
        StopMusicStream(g_assets.music[g_assets.current_music]);

    g_assets.current_music = MUSIC_COUNT;
    g_assets.music_playing = false;
}
