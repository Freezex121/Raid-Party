#include "assets.h"
#include "systems/map.h"
#include "util/math_utils.h"
#include "raylib.h"
#include <stdio.h>

#define MUSIC_FADE_DURATION 0.5f

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

Texture2D load_art_texture(const char *filename)
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

static Texture2D load_card_layer_texture(const char *filename)
{
    char nested[96];
    snprintf(nested, sizeof(nested), "cards/%s", filename);

    Texture2D tex = load_art_texture(nested);
    if (tex.id != 0)
        return tex;
    return load_art_texture(filename);
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
    g_assets.music_fade_remaining = 0.0f;
    g_assets.music_fade_next = MUSIC_COUNT;
    g_assets.music_volume = 1.0f;
    g_assets.sfx_volume = 1.0f;

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
        [SFX_CARD_DRAW] = "card_draw.wav",
        [SFX_DAMAGE] = "damage.wav",
        [SFX_DAMAGE_HEAVY] = "damage_heavy.wav",
        [SFX_HEAL] = "heal.wav",
        [SFX_SHIELD] = "shield.wav",
        [SFX_TAUNT] = "taunt.wav",
        [SFX_INTERRUPT] = "interrupt.wav",
        [SFX_BURN_TICK] = "burn_tick.wav",
        [SFX_BLEED_TICK] = "bleed_tick.wav",
        [SFX_PARTY_DOWNED] = "party_downed.wav",
        [SFX_PARTY_REVIVED] = "party_revived.wav",
        [SFX_ENEMY_CAST_WARNING] = "enemy_cast_warning.wav",
        [SFX_BOSS_CAST_WARNING] = "boss_cast_warning.wav",
        [SFX_ENEMY_ATTACK] = "enemy_attack.wav",
        [SFX_GOLD_PICKUP] = "gold_pickup.wav",
        [SFX_REWARD_PICKUP] = "reward_pickup.wav",
        [SFX_LEVEL_UP] = "level_up.wav",
        [SFX_VICTORY] = "victory.wav",
        [SFX_DEFEAT] = "defeat.wav",
        [SFX_MAP_SELECT] = "map_select.wav",
        [SFX_SYNERGY_TRIGGER] = "synergy_trigger.wav",
        [SFX_SHOP_PURCHASE] = "shop_purchase.wav",
        [SFX_ERROR] = "error.wav",
    };

    const char *music_files[MUSIC_COUNT] = {
        [MUSIC_TITLE] = "title.ogg",
        [MUSIC_MAP] = "map.ogg",
        [MUSIC_COMBAT_GREENWOOD] = "combat_greenwood.ogg",
        [MUSIC_COMBAT_VENOM] = "combat_venom.ogg",
        [MUSIC_COMBAT_CINDER] = "combat_cinder.ogg",
        [MUSIC_COMBAT_CATACOMBS] = "combat_catacombs.ogg",
        [MUSIC_COMBAT_CITADEL] = "combat_citadel.ogg",
        [MUSIC_BOSS_GREENWOOD] = "boss_greenwood.ogg",
        [MUSIC_BOSS_VENOM] = "boss_venom.ogg",
        [MUSIC_BOSS_CINDER] = "boss_cinder.ogg",
        [MUSIC_BOSS_CATACOMBS] = "boss_catacombs.ogg",
        [MUSIC_BOSS_CITADEL] = "boss_citadel.ogg",
        [MUSIC_SHOP] = "shop.ogg",
        [MUSIC_REST] = "rest.ogg",
        [MUSIC_EVENT] = "event.ogg",
        [MUSIC_VICTORY] = "victory.ogg",
        [MUSIC_DEFEAT] = "defeat.ogg",
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
#ifndef __EMSCRIPTEN__
                make_font_bitmap(&font);
#endif
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
        "assets/art/card.png",
        "../assets/art/card.png",
        "../../assets/art/card.png",
    };

    g_assets.card_template = (Texture2D){0};
    g_assets.card_template_upgraded = (Texture2D){0};
    g_assets.card_template_maxed = (Texture2D){0};
    g_assets.card_background_count = 0;
    for (int i = 0; i < CARD_BACKGROUND_MAX; i++)
        g_assets.card_backgrounds[i] = (Texture2D){0};
    g_assets.card_tint_mask = (Texture2D){0};
    g_assets.card_info = (Texture2D){0};
    g_assets.card_border = (Texture2D){0};
    g_assets.card_border_upgraded = (Texture2D){0};
    g_assets.card_border_maxed = (Texture2D){0};
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
        "assets/art/card_upgraded.png",
        "../assets/art/card_upgraded.png",
        "../../assets/art/card_upgraded.png",
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

    const char *maxed_card_paths[] = {
        "assets/art/card_maxed.png",
        "../assets/art/card_maxed.png",
        "../../assets/art/card_maxed.png",
    };

    for (int i = 0; i < 3; i++)
    {
        if (FileExists(maxed_card_paths[i]))
        {
            g_assets.card_template_maxed = LoadTexture(maxed_card_paths[i]);
            break;
        }
    }

    if (g_assets.card_template_maxed.id != 0)
        SetTextureFilter(g_assets.card_template_maxed, TEXTURE_FILTER_POINT);

    for (int i = 0; i < CARD_BACKGROUND_MAX; i++)
    {
        char filename[64];
        snprintf(filename, sizeof(filename), "card_background_%02d.png", i);
        Texture2D tex = load_card_layer_texture(filename);
        if (tex.id != 0)
            g_assets.card_backgrounds[g_assets.card_background_count++] = tex;
    }
    g_assets.card_tint_mask = load_card_layer_texture("card_tint_mask.png");
    g_assets.card_info = load_card_layer_texture("card_info.png");
    g_assets.card_border = load_card_layer_texture("card_border.png");
    g_assets.card_border_upgraded = load_card_layer_texture("card_border_upgraded.png");
    g_assets.card_border_maxed = load_card_layer_texture("card_border_maxed.png");

    g_assets.relic_template = load_art_texture("relic_template.png");
    g_assets.relic_icon_placeholder = load_art_texture("relic_icon.png");

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

    for (int i = 0; i < RELIC_COUNT; i++)
    {
        g_assets.relic_icons[i] = (Texture2D){0};
        const char *id = relic_id_string((RelicId)i);
        if (id)
        {
            char filename[64];
            snprintf(filename, sizeof(filename), "relic_%s.png", id);
            g_assets.relic_icons[i] = load_art_texture(filename);
        }
        if (g_assets.relic_icons[i].id == 0 && g_assets.relic_icon_placeholder.id != 0)
            g_assets.relic_icons[i] = g_assets.relic_icon_placeholder;
    }

    const char *node_files[] = {
        [NODE_COMBAT] = "node_combat.png",
        [NODE_ELITE]  = "node_elite.png",
        [NODE_REST]   = "node_rest.png",
        [NODE_SHOP]   = "node_shop.png",
        [NODE_EVENT]  = "node_event.png",
        [NODE_BOSS]   = "node_boss.png",
    };
    for (int i = 0; i < 8; i++)
        g_assets.node_sprites[i] = (Texture2D){0};
    for (int i = NODE_COMBAT; i <= NODE_BOSS; i++)
        if (node_files[i])
            g_assets.node_sprites[i] = load_art_texture(node_files[i]);

    g_assets.btn_standard = load_art_texture("btn_standard.png");
    g_assets.btn_large = load_art_texture("btn_large.png");

    // Load keyword art, with simple placeholders for incomplete asset packs.
    {
        static const char *kw_files[KW_COUNT] = {
            [KW_ECHO] = "kw_echo.png",
            [KW_LIFESTEAL] = "kw_lifesteal.png",
            [KW_RETAIN] = "kw_retain.png",
            [KW_INTERRUPT] = "kw_interrupt.png",
            [KW_TAUNT] = "kw_taunt.png",
            [KW_FLEETING] = "kw_fleeting.png",
            [KW_EXHAUST] = "kw_exhaust.png",
        };
        static const Color kw_colors[KW_COUNT] = {
            { 200, 160, 40, 255 },   // KW_ECHO - gold
            { 200, 60, 60, 255 },    // KW_LIFESTEAL - red
            { 60, 160, 200, 255 },   // KW_RETAIN - blue
            { 160, 100, 200, 255 },  // KW_INTERRUPT - purple
            { 200, 120, 40, 255 },   // KW_TAUNT - orange
            { 80, 80, 80, 255 },     // KW_FLEETING - grey
            { 120, 80, 80, 255 },    // KW_EXHAUST - brown
        };
        for (int i = 0; i < KW_COUNT; i++)
        {
            g_assets.kw_icons[i] = load_art_texture(kw_files[i]);
            if (g_assets.kw_icons[i].id != 0)
                continue;

            Image img = GenImageColor(KW_ICON_SIZE, KW_ICON_SIZE, kw_colors[i]);
            ImageDrawRectangle(&img, 0, 0, KW_ICON_SIZE, 2, RAYWHITE);
            ImageDrawRectangle(&img, 0, 0, 2, KW_ICON_SIZE, RAYWHITE);
            ImageDrawRectangle(&img, KW_ICON_SIZE - 2, 0, 2, KW_ICON_SIZE, RAYWHITE);
            ImageDrawRectangle(&img, 0, KW_ICON_SIZE - 2, KW_ICON_SIZE, 2, RAYWHITE);
            g_assets.kw_icons[i] = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }

    // Meta tree icons are optional while the art pack is being filled in.
    {
        static const char *meta_icon_files[META_ICON_COUNT] = {
            [META_ICON_UNLOCK_PROGRESS] = "meta_unlock_progress.png",
            [META_ICON_BLADES_I] = "meta_blades_i.png",
            [META_ICON_BLADES_II] = "meta_blades_ii.png",
            [META_ICON_BLADES_III] = "meta_blades_iii.png",
            [META_ICON_OPENING_STRIKE_I] = "meta_opening_strike_i.png",
            [META_ICON_OPENING_STRIKE_II] = "meta_opening_strike_ii.png",
            [META_ICON_VICTORY_MOMENTUM_I] = "meta_victory_momentum_i.png",
            [META_ICON_VICTORY_MOMENTUM_II] = "meta_victory_momentum_ii.png",
            [META_ICON_EXPLOIT_WEAKNESS_I] = "meta_exploit_weakness_i.png",
            [META_ICON_EXPLOIT_WEAKNESS_II] = "meta_exploit_weakness_ii.png",
            [META_ICON_ELITE_HUNTER] = "meta_elite_hunter.png",
            [META_ICON_BOSS_SLAYER] = "meta_boss_slayer.png",
            [META_ICON_WARLOCK_RUMORS] = "meta_warlock_rumors.png",
            [META_ICON_UNLOCK_WARLOCK] = "meta_unlock_warlock.png",
            [META_ICON_WARLOCK_ADEPT] = "meta_warlock_adept.png",
            [META_ICON_TROPHY_HUNTER_I] = "meta_trophy_hunter_i.png",
            [META_ICON_TROPHY_HUNTER_II] = "meta_trophy_hunter_ii.png",
            [META_ICON_ARMOR_I] = "meta_armor_i.png",
            [META_ICON_ARMOR_II] = "meta_armor_ii.png",
            [META_ICON_ARMOR_III] = "meta_armor_iii.png",
            [META_ICON_WARDENS_OATH] = "meta_wardens_oath.png",
            [META_ICON_LAST_STAND] = "meta_last_stand.png",
            [META_ICON_COMBAT_SHIELD_I] = "meta_combat_shield_i.png",
            [META_ICON_COMBAT_SHIELD_II] = "meta_combat_shield_ii.png",
            [META_ICON_COMBAT_SHIELD_III] = "meta_combat_shield_iii.png",
            [META_ICON_EMERGENCY_BARRIER] = "meta_emergency_barrier.png",
            [META_ICON_THICK_SKIN_I] = "meta_thick_skin_i.png",
            [META_ICON_THICK_SKIN_II] = "meta_thick_skin_ii.png",
            [META_ICON_THICK_SKIN_III] = "meta_thick_skin_iii.png",
            [META_ICON_SHIELD_CAP_I] = "meta_shield_cap_i.png",
            [META_ICON_SHIELD_CAP_II] = "meta_shield_cap_ii.png",
            [META_ICON_SHIELD_CAP_III] = "meta_shield_cap_iii.png",
            [META_ICON_SHIELD_CAP_IV] = "meta_shield_cap_iv.png",
            [META_ICON_SHIELD_CAP_V] = "meta_shield_cap_v.png",
            [META_ICON_SHIELD_CAP_VI] = "meta_shield_cap_vi.png",
            [META_ICON_TRAVELERS_PACK] = "meta_travelers_pack.png",
            [META_ICON_UPGRADED_FORTIFY] = "meta_upgraded_fortify.png",
            [META_ICON_PALADIN_OATH] = "meta_paladin_oath.png",
            [META_ICON_UNLOCK_PALADIN] = "meta_unlock_paladin.png",
            [META_ICON_PALADIN_BULWARK] = "meta_paladin_bulwark.png",
            [META_ICON_MANA_CRYSTAL] = "meta_mana_crystal.png",
            [META_ICON_CHARGED_CRYSTAL] = "meta_charged_crystal.png",
            [META_ICON_STARTING_ENERGY] = "meta_starting_energy.png",
            [META_ICON_PREPARATION] = "meta_preparation.png",
            [META_ICON_UPGRADED_PREPARATION] = "meta_upgraded_preparation.png",
            [META_ICON_CAMP_SUPPLIES] = "meta_camp_supplies.png",
            [META_ICON_CAMP_MASTER] = "meta_camp_master.png",
            [META_ICON_SCOUTS_KIT_I] = "meta_scouts_kit_i.png",
            [META_ICON_SCOUTS_KIT_II] = "meta_scouts_kit_ii.png",
            [META_ICON_SCOUTS_KIT_III] = "meta_scouts_kit_iii.png",
            [META_ICON_FIRST_AID] = "meta_first_aid.png",
            [META_ICON_UPGRADED_REJUVENATION] = "meta_upgraded_rejuvenation.png",
            [META_ICON_HEALING_TOUCH_I] = "meta_healing_touch_i.png",
            [META_ICON_HEALING_TOUCH_II] = "meta_healing_touch_ii.png",
            [META_ICON_BARD_SCHOOL] = "meta_bard_school.png",
            [META_ICON_UNLOCK_BARD] = "meta_unlock_bard.png",
            [META_ICON_BARD_ENCORE] = "meta_bard_encore.png",
            [META_ICON_HARMONY] = "meta_harmony.png",
            [META_ICON_TRAVEL_FUND_I] = "meta_travel_fund_i.png",
            [META_ICON_TRAVEL_FUND_II] = "meta_travel_fund_ii.png",
            [META_ICON_TRAVEL_FUND_III] = "meta_travel_fund_iii.png",
            [META_ICON_GOLD_CONVERSION] = "meta_gold_conversion.png",
            [META_ICON_MASTER_RAIDER] = "meta_master_raider.png",
            [META_ICON_COMPLETIONIST_BANNER] = "meta_completionist_banner.png",
            [META_ICON_MERCHANT_CONTACTS_I] = "meta_merchant_contacts_i.png",
            [META_ICON_MERCHANT_CONTACTS_II] = "meta_merchant_contacts_ii.png",
            [META_ICON_BLACK_MARKET] = "meta_black_market.png",
            [META_ICON_REWARD_REROLL_I] = "meta_reward_reroll_i.png",
            [META_ICON_REWARD_CHOICE_I] = "meta_reward_choice_i.png",
            [META_ICON_REWARD_CHOICE_II] = "meta_reward_choice_ii.png",
            [META_ICON_VETERAN_REWARDS] = "meta_veteran_rewards.png",
            [META_ICON_LEGACY_I] = "meta_legacy_i.png",
            [META_ICON_LEGACY_II] = "meta_legacy_ii.png",
            [META_ICON_LEGACY_III] = "meta_legacy_iii.png",
            [META_ICON_RELIC_CHOICE] = "meta_relic_choice.png",
            [META_ICON_RELIC_ECHO_BELL] = "meta_relic_echo_bell.png",
            [META_ICON_RELIC_SPLIT_PRISM] = "meta_relic_split_prism.png",
            [META_ICON_RELIC_BLOOD_AMBER] = "meta_relic_blood_amber.png",
            [META_ICON_RELIC_TITAN_HEART] = "meta_relic_titan_heart.png",
            [META_ICON_RELIC_FRUGAL_TOME] = "meta_relic_frugal_tome.png",
            [META_ICON_PARTY_SLOT_IV] = "meta_party_slot_iv.png",
            [META_ICON_FORMATION_DRILLS] = "meta_formation_drills.png",
            [META_ICON_PARTY_SLOT_V] = "meta_party_slot_v.png",
        };
        for (int i = 0; i < META_ICON_COUNT; i++)
            g_assets.meta_upgrade_icons[i] = load_art_texture(meta_icon_files[i]);
    }

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
    if (g_assets.card_template_maxed.id != 0)
        UnloadTexture(g_assets.card_template_maxed);
    for (int i = 0; i < g_assets.card_background_count; i++)
        if (g_assets.card_backgrounds[i].id != 0)
            UnloadTexture(g_assets.card_backgrounds[i]);
    if (g_assets.card_tint_mask.id != 0)
        UnloadTexture(g_assets.card_tint_mask);
    if (g_assets.card_info.id != 0)
        UnloadTexture(g_assets.card_info);
    if (g_assets.card_border.id != 0)
        UnloadTexture(g_assets.card_border);
    if (g_assets.card_border_upgraded.id != 0)
        UnloadTexture(g_assets.card_border_upgraded);
    if (g_assets.card_border_maxed.id != 0)
        UnloadTexture(g_assets.card_border_maxed);
    if (g_assets.relic_template.id != 0)
        UnloadTexture(g_assets.relic_template);
    for (int i = 0; i < CLASS_COUNT; i++)
        if (g_assets.class_icons[i].id != 0)
            UnloadTexture(g_assets.class_icons[i]);
    for (int i = 0; i < RELIC_COUNT; i++)
        if (g_assets.relic_icons[i].id != 0 && g_assets.relic_icons[i].id != g_assets.relic_icon_placeholder.id)
            UnloadTexture(g_assets.relic_icons[i]);
    if (g_assets.relic_icon_placeholder.id != 0)
        UnloadTexture(g_assets.relic_icon_placeholder);
    for (int i = 0; i < 8; i++)
        if (g_assets.node_sprites[i].id != 0)
            UnloadTexture(g_assets.node_sprites[i]);
    if (g_assets.btn_standard.id != 0)
        UnloadTexture(g_assets.btn_standard);
    if (g_assets.btn_large.id != 0)
        UnloadTexture(g_assets.btn_large);
    for (int i = 0; i < KW_COUNT; i++)
        if (g_assets.kw_icons[i].id != 0)
            UnloadTexture(g_assets.kw_icons[i]);
    for (int i = 0; i < META_ICON_COUNT; i++)
        if (g_assets.meta_upgrade_icons[i].id != 0)
            UnloadTexture(g_assets.meta_upgrade_icons[i]);
    g_assets.loaded = false;
}

void assets_update_audio(void)
{
    if (!IsAudioDeviceReady()) return;

    // Update current music stream
    if (g_assets.music_playing &&
        g_assets.current_music >= 0 && g_assets.current_music < MUSIC_COUNT &&
        g_assets.music_loaded[g_assets.current_music])
    {
        UpdateMusicStream(g_assets.music[g_assets.current_music]);
    }

    // Handle fade
    if (g_assets.music_fade_remaining > 0.0f)
    {
        g_assets.music_fade_remaining -= GetFrameTime();
        if (g_assets.music_fade_remaining < 0.0f)
            g_assets.music_fade_remaining = 0.0f;

        float fade_factor = g_assets.music_fade_remaining / MUSIC_FADE_DURATION;

        if (g_assets.music_playing &&
            g_assets.current_music >= 0 && g_assets.current_music < MUSIC_COUNT &&
            g_assets.music_loaded[g_assets.current_music])
        {
            SetMusicVolume(g_assets.music[g_assets.current_music], g_assets.music_volume * fade_factor);
        }

        // Fade complete
        if (g_assets.music_fade_remaining <= 0.0f && g_assets.music_playing)
        {
            StopMusicStream(g_assets.music[g_assets.current_music]);
            g_assets.music_playing = false;
            g_assets.current_music = MUSIC_COUNT;

            // Start next track if one was queued
            if (g_assets.music_fade_next != MUSIC_COUNT && g_assets.music_loaded[g_assets.music_fade_next])
            {
                g_assets.current_music = g_assets.music_fade_next;
                g_assets.music_playing = true;
                SetMusicVolume(g_assets.music[g_assets.music_fade_next], g_assets.music_volume);
                PlayMusicStream(g_assets.music[g_assets.music_fade_next]);
                g_assets.music_fade_next = MUSIC_COUNT;
            }
        }
    }
}

void assets_play_sfx(GameSfx sfx)
{
    if (!IsAudioDeviceReady()) return;
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!g_assets.sfx_loaded[sfx]) return;

    SetSoundVolume(g_assets.sfx[sfx], g_assets.sfx_volume);
    SetSoundPitch(g_assets.sfx[sfx], random_range(0.95f, 1.05f));
    PlaySound(g_assets.sfx[sfx]);
}

void assets_play_music(GameMusic music)
{
    if (!IsAudioDeviceReady()) return;
    if (music < 0 || music >= MUSIC_COUNT) return;

    // Same track already playing (and not about to fade away)?
    if (g_assets.music_playing && g_assets.current_music == music && g_assets.music_fade_remaining <= 0.0f)
        return;

    // Resolve fallback: area-specific combat track missing -> generic
    GameMusic attempt = music;
    if (!g_assets.music_loaded[attempt])
    {
        GameMusic fallback = MUSIC_COUNT;
        if (attempt >= MUSIC_COMBAT_GREENWOOD && attempt <= MUSIC_COMBAT_CITADEL)
            fallback = MUSIC_COMBAT_GREENWOOD;
        else if (attempt >= MUSIC_BOSS_GREENWOOD && attempt <= MUSIC_BOSS_CITADEL)
            fallback = MUSIC_BOSS_GREENWOOD;
        else if (attempt >= MUSIC_SHOP && attempt <= MUSIC_DEFEAT && !g_assets.music_loaded[attempt])
            fallback = MUSIC_COUNT;

        if (fallback != MUSIC_COUNT && g_assets.music_loaded[fallback])
            attempt = fallback;
    }

    if (!g_assets.music_loaded[attempt])
        return;

    // If music is currently playing, fade out first
    if (g_assets.music_playing)
    {
        g_assets.music_fade_next = attempt;
        if (g_assets.music_fade_remaining <= 0.0f)
            g_assets.music_fade_remaining = MUSIC_FADE_DURATION;
        return;
    }

    // No music playing — start immediately
    g_assets.current_music = attempt;
    g_assets.music_playing = true;
    g_assets.music_fade_remaining = 0.0f;
    g_assets.music_fade_next = MUSIC_COUNT;
    SetMusicVolume(g_assets.music[attempt], g_assets.music_volume);
    PlayMusicStream(g_assets.music[attempt]);
}

void assets_stop_music(void)
{
    if (!IsAudioDeviceReady()) return;
    if (!g_assets.music_playing) return;

    // Start fade if not already fading
    if (g_assets.music_fade_remaining <= 0.0f)
        g_assets.music_fade_remaining = MUSIC_FADE_DURATION;
    g_assets.music_fade_next = MUSIC_COUNT;
}

void assets_set_music_volume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_assets.music_volume = volume;
    if (!IsAudioDeviceReady()) return;
    for (int i = 0; i < MUSIC_COUNT; i++)
        if (g_assets.music_loaded[i])
            SetMusicVolume(g_assets.music[i], volume);
}

void assets_set_sfx_volume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_assets.sfx_volume = volume;
    if (!IsAudioDeviceReady()) return;
    for (int i = 0; i < SFX_COUNT; i++)
        if (g_assets.sfx_loaded[i])
            SetSoundVolume(g_assets.sfx[i], volume);
}

void assets_play_combat_music(int area_id, bool is_boss)
{
    GameMusic music;
    switch (area_id) {
        case 0:  music = is_boss ? MUSIC_BOSS_GREENWOOD : MUSIC_COMBAT_GREENWOOD; break;
        case 1:  music = is_boss ? MUSIC_BOSS_VENOM    : MUSIC_COMBAT_VENOM;    break;
        case 2:  music = is_boss ? MUSIC_BOSS_CINDER   : MUSIC_COMBAT_CINDER;   break;
        case 3:  music = is_boss ? MUSIC_BOSS_CATACOMBS: MUSIC_COMBAT_CATACOMBS; break;
        case 4:  music = is_boss ? MUSIC_BOSS_CITADEL  : MUSIC_COMBAT_CITADEL;  break;
        default: music = is_boss ? MUSIC_BOSS_GREENWOOD : MUSIC_COMBAT_GREENWOOD;
    }
    assets_play_music(music);
}
