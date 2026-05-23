#include "assets.h"
#include <stdio.h>

GameAssets g_assets;

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

void assets_load(void)
{
    g_assets.ui_font = GetFontDefault();
    g_assets.ui_font_loaded = false;

    const char *font_paths[] = {
        "assets/fonts/5x5_pixel.ttf",
        "../assets/fonts/5x5_pixel.ttf",
        "../../assets/fonts/5x5_pixel.ttf",
    };

    for (int i = 0; i < 3; i++)
    {
        if (FileExists(font_paths[i]))
        {
            g_assets.ui_font = LoadFontEx(font_paths[i], 16, NULL, 0);
            g_assets.ui_font_loaded = g_assets.ui_font.texture.id != 0;
            break;
        }
    }

    if (g_assets.ui_font.texture.id != 0)
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

    const char *icon_files[CLASS_COUNT] = {
        [CLASS_GUARDIAN] = "guardian_icon.png",
        [CLASS_CLERIC] = "cleric_icon.png",
        [CLASS_MAGE] = "mage_icon.png",
        [CLASS_ROGUE] = "rogue_icon.png",
        [CLASS_SHAMAN] = "shaman_icon.png",
        [CLASS_RANGER] = "ranger_icon.png",
    };

    for (int i = 0; i < CLASS_COUNT; i++)
        g_assets.class_icons[i] = load_art_texture(icon_files[i]);

    g_assets.loaded = true;
}

void assets_unload(void)
{
    if (!g_assets.loaded) return;

    if (g_assets.ui_font_loaded)
        UnloadFont(g_assets.ui_font);
    UnloadTexture(g_assets.paper_texture);
    UnloadTexture(g_assets.card_template);
    for (int i = 0; i < CLASS_COUNT; i++)
        if (g_assets.class_icons[i].id != 0)
            UnloadTexture(g_assets.class_icons[i]);
    g_assets.loaded = false;
}
