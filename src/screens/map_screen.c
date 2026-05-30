#include "screens.h"
#include "game.h"
#include "systems/map.h"
#include "data/area_defs.h"
#include "assets.h"
#include "ui/ui.h"
#include "data/encounter_defs.h"
#include "util/log.h"
#include "util/text.h"
#include "ui/relic_tray.h"
#include "ui/theme.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

static int hovered_node = -1;
static int last_floor = -1;
static float map_scroll_x = 0.0f;
static float map_scroll_y = 0.0f;
static bool show_debug = false;

static float clampf_local(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static Rectangle map_content_bounds(MapState *map)
{
    if (!map || !map->nodes || map->node_count <= 0)
        return (Rectangle){ 0.0f, 52.0f, (float)VIRT_W, (float)(VIRT_H - 52) };

    int min_x = map->nodes[0].x;
    int min_y = map->nodes[0].y;
    int max_x = map->nodes[0].x;
    int max_y = map->nodes[0].y;
    for (int i = 1; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        if (n->x < min_x) min_x = n->x;
        if (n->y < min_y) min_y = n->y;
        if (n->x > max_x) max_x = n->x;
        if (n->y > max_y) max_y = n->y;
    }

    return (Rectangle){
        (float)(min_x - 54),
        (float)(min_y - 54),
        (float)(max_x - min_x + 108),
        (float)(max_y - min_y + 112)
    };
}

static void clamp_map_scroll(void)
{
    Rectangle bounds = map_content_bounds(&g_state.map);
    float max_x = bounds.width > VIRT_W ? bounds.width - VIRT_W : 0.0f;
    float max_y = bounds.height > (VIRT_H - 52) ? bounds.height - (VIRT_H - 52) : 0.0f;
    map_scroll_x = clampf_local(map_scroll_x, 0.0f, max_x);
    map_scroll_y = clampf_local(map_scroll_y, 0.0f, max_y);
}

static float map_center_offset_x(Rectangle bounds)
{
    return bounds.width < VIRT_W ? ((float)VIRT_W - bounds.width) * 0.5f : 0.0f;
}

static Vector2 node_screen_pos(MapNode *node)
{
    Rectangle bounds = map_content_bounds(&g_state.map);
    return (Vector2){
        (float)node->x - bounds.x - map_scroll_x + map_center_offset_x(bounds),
        (float)node->y - bounds.y - map_scroll_y + 52.0f
    };
}

void map_screen_update(void)
{
    if (g_state.map.floor != last_floor || g_state.map.node_count == 0)
    {
        const AreaDef *area = area_def(g_state.current_area);
        map_generate(&g_state.map, g_state.map.floor, area ? area->id : NULL);
        g_state.map.current_index = -1;
        last_floor = g_state.map.floor;
        map_scroll_x = 0.0f;
        map_scroll_y = 0.0f;
        LOG_I(CAT_SCREEN, "Map screen: floor %d, %d nodes", g_state.map.floor, g_state.map.node_count);
    }

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_MAP)
    {
        if (game_tutorial_handle_skip())
            return;
    }

    // DECK button
    Rectangle deck_btn = { 10.0f, 8.0f, (float)BTN_NARROW, (float)BTN_H };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), deck_btn))
    {
        game_change_screen(SCREEN_DECK);
        return;
    }
    Rectangle settings_btn = { 10.0f + BTN_NARROW + 6.0f, 8.0f, (float)BTN_NARROW, (float)BTN_H };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), settings_btn))
    {
        game_open_settings(SCREEN_MAP);
        return;
    }

    // Toggle debug overlay
    if (IsKeyPressed(KEY_F5))
    {
        show_debug = !show_debug;
        LOG_I(CAT_SCREEN, "Map debug overlay: %s", show_debug ? "ON" : "OFF");
    }

    if (g_state.map.current_index >= 0)
    {
        int ci = g_state.map.current_index;
        NodeType t = g_state.map.nodes[ci].type;
        LOG_I(CAT_SCREEN, "Entering node %d type=%d", ci, t);

        g_state.encounter = NULL;
        g_state.encounter_is_elite = false;
        g_state.encounter_is_boss = false;
        const AreaDef *area = area_def(g_state.current_area);
        const char *area_id = area ? area->id : NULL;

        if (t == NODE_COMBAT)
        {
            int idx = 0;
            for (int i = 0; i < g_state.map.node_count && i < ci; i++)
                if (g_state.map.nodes[i].type == NODE_COMBAT) idx++;
            g_state.encounter = encounter_for_area_floor(area_id, g_state.map.floor, idx);
        }
        else if (t == NODE_ELITE)
        {
            g_state.encounter = elite_for_area_floor(area_id, g_state.map.floor);
            g_state.encounter_is_elite = true;
        }
        else if (t == NODE_BOSS)
        {
            g_state.encounter = boss_for_area_floor(area_id, g_state.map.floor);
            g_state.encounter_is_boss = true;
        }
        else if (t == NODE_START)
        {
            g_state.map.nodes[ci].completed = true;
            g_state.map.current_index = -1;
            map_unlock_next(&g_state.map);
            return;
        }

        if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_MAP &&
            (t == NODE_COMBAT || t == NODE_ELITE || t == NODE_BOSS))
        {
            g_state.tutorial_step = TUTORIAL_STEP_COMBAT_PARTY;
        }
        else if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_MAP)
        {
            game_skip_tutorial();
        }

        game_change_screen(t == NODE_REST ? SCREEN_REST :
                           t == NODE_SHOP ? SCREEN_SHOP :
                           t == NODE_EVENT ? SCREEN_EVENT :
                           SCREEN_RUN);
        return;
    }

    Vector2 mouse = GetMousePosition();
    hovered_node = -1;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            map_scroll_x -= wheel * 32.0f;
        else
            map_scroll_y -= wheel * 32.0f;
    }
    if (IsKeyDown(KEY_DOWN)) map_scroll_y += 3.0f;
    if (IsKeyDown(KEY_UP)) map_scroll_y -= 3.0f;
    if (IsKeyDown(KEY_RIGHT)) map_scroll_x += 3.0f;
    if (IsKeyDown(KEY_LEFT)) map_scroll_x -= 3.0f;
    clamp_map_scroll();

    for (int i = g_state.map.node_count - 1; i >= 0; i--)
    {
        MapNode *n = &g_state.map.nodes[i];
        if (!n->available || n->completed) continue;
        Vector2 pos = node_screen_pos(n);
        if (CheckCollisionPointRec(mouse, (Rectangle){ pos.x - 20.0f, pos.y - 20.0f, 40.0f, 40.0f }))
        {
            hovered_node = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                LOG_I(CAT_INPUT, "Clicked map node %d type=%d", i, n->type);
                n->completed = true;
                if (n->type == NODE_START)
                {
                    map_unlock_next(&g_state.map);
                }
                else
                {
                    g_state.map.current_index = i;
                }
            }
            break;
        }
    }
}

void map_screen_draw(void)
{
    theme_draw_background();

    const AreaDef *area = area_def(g_state.current_area);
    int floor_count = area_floor_count(g_state.current_area);

    // Deck button
    Vector2 mouse = GetMousePosition();
    Rectangle deck_btn = { 10.0f, 8.0f, (float)BTN_NARROW, (float)BTN_H };
    draw_btn_standard(deck_btn, (Color){ 50, 50, 80, 255 }, (Color){ 80, 80, 120, 255 }, "DECK");

    Rectangle settings_btn = { 10.0f + BTN_NARROW + 6.0f, 8.0f, (float)BTN_NARROW, (float)BTN_H };
    draw_btn_standard(settings_btn, (Color){ 50, 50, 80, 255 }, (Color){ 80, 80, 120, 255 }, "SET");

    char title[96];
    snprintf(title, sizeof(title), "%s", area ? area->name : "Area");
    draw_text_box((Rectangle){ 184.0f, 14.0f, 288.0f, 22.0f }, title, 18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "Area %d  Floor %d/%d  Choose your route",
        g_state.current_area + 1,
        g_state.map.floor + 1,
        floor_count);
    draw_text_box((Rectangle){ 184.0f, 36.0f, 288.0f, 14.0f }, subtitle, 10, 0, (Color){ 150, 155, 180, 210 }, TEXT_ALIGN_CENTER);

    MapState *map = &g_state.map;
    clamp_map_scroll();

    BeginScissorMode(0, 52, VIRT_W, VIRT_H - 52);

    // Draw map background — load from area def per floor
    {
        static Texture2D cached_bg = {0};
        static int cached_area = -2;
        static int cached_floor = -2;

        Texture2D bg_tex = {0};
        const AreaDef *area_def_ptr = area_def(g_state.current_area);
        int load_area = g_state.current_area;
        int load_floor = g_state.map.floor;

        // Check cache
        if (cached_bg.id != 0 && cached_area == load_area && cached_floor == load_floor)
        {
            bg_tex = cached_bg;
        }
        else
        {
            // Unload old cached texture
            if (cached_bg.id != 0)
            {
                UnloadTexture(cached_bg);
                cached_bg = (Texture2D){0};
            }

            if (area_def_ptr && area_def_ptr->map_bg_files && area_def_ptr->map_bg_count > 0)
            {
                int idx = load_floor < area_def_ptr->map_bg_count ? load_floor : area_def_ptr->map_bg_count - 1;
                if (idx < 0) idx = 0;
                cached_bg = load_art_texture(area_def_ptr->map_bg_files[idx]);
                if (cached_bg.id != 0)
                    SetTextureFilter(cached_bg, TEXTURE_FILTER_POINT);
                bg_tex = cached_bg;
                cached_area = load_area;
                cached_floor = load_floor;
            }
        }
        if (bg_tex.id != 0)
        {
            Rectangle bounds = map_content_bounds(map);
            float view_h = (float)(VIRT_H - 52);
            float max_scroll = bounds.height - view_h;
            if (max_scroll < 0.0f) max_scroll = 0.0f;
            float scroll_ratio = max_scroll > 0.0f ? map_scroll_y / max_scroll : 0.0f;
            if (scroll_ratio < 0.0f) scroll_ratio = 0.0f;
            if (scroll_ratio > 1.0f) scroll_ratio = 1.0f;
            // Parallax: move at 30% speed so nodes feel like they're in the world
            float parallax_ratio = scroll_ratio * 0.3f;
            float img_h = (float)bg_tex.height;
            float y_offset = (img_h - view_h) * parallax_ratio;
            if (y_offset < 0.0f) y_offset = 0.0f;
            if (y_offset > img_h - view_h) y_offset = img_h - view_h;
            DrawTexturePro(bg_tex,
                (Rectangle){ 0.0f, y_offset, (float)bg_tex.width, view_h },
                (Rectangle){ 0.0f, 52.0f, (float)VIRT_W, view_h },
                (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
    }

    // Draw connection lines with black outline + thicker
    for (int i = 0; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        Vector2 from = node_screen_pos(n);
        Color conn_col = n->completed ? (Color){ 125, 140, 165, 170 } : (Color){ 60, 64, 86, 120 };
        float line_w = n->completed ? 3.0f : 2.0f;
        for (int c = 0; c < n->conn_count; c++)
        {
            int ni = n->conns[c];
            if (ni < 0 || ni >= map->node_count) continue;
            MapNode *next = &map->nodes[ni];
            Vector2 to = node_screen_pos(next);
            // Black outline
            DrawLineEx(from, to, line_w + 2.0f, (Color){ 0, 0, 0, 180 });
            // Colored line on top
            DrawLineEx(from, to, line_w, conn_col);
        }
    }

    for (int i = 0; i < map->node_count; i++)
    {
        MapNode *n = &map->nodes[i];
        Vector2 pos = node_screen_pos(n);
        int sx = (int)pos.x;
        int sy = (int)pos.y;
        const char *name = theme_node_name(n->type);

        Texture2D tex = g_assets.node_sprites[n->type];
        float sprite_size = (n->type == NODE_BOSS) ? 48.0f : 32.0f;
        int half = (int)(sprite_size * 0.5f);
        Rectangle dest_rect = { (float)(sx - half), (float)(sy - half), sprite_size, sprite_size };
        Rectangle src_rect = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
        Color sprite_tint = { 255, 255, 255, 255 };
        bool has_tex = (tex.id != 0);

        if (!n->available && !n->completed)
        {
            if (has_tex)
                DrawTexturePro(tex, src_rect, dest_rect, (Vector2){0, 0}, 0.0f, (Color){ 150, 150, 150, 200 });
            else
            {
                int r = (n->type == NODE_BOSS) ? 18 : 14;
                DrawCircle(sx, sy, (float)r, (Color){ 28, 29, 42, 255 });
                DrawCircleLines(sx, sy, (float)r, (Color){ 58, 58, 78, 120 });
                int locked_size = n->type == NODE_BOSS ? 18 : 10;
                Color dim = theme_node_color(n->type);
                dim.a = 128;
                DrawText(theme_node_icon(n->type), sx - MeasureText(theme_node_icon(n->type), locked_size) / 2, sy - locked_size / 2, locked_size, dim);
            }
            int name_w = MeasureText(name, 10);
            DrawRectangle(sx - name_w / 2 - 3, sy + half + 2, name_w + 6, 12, (Color){ 0, 0, 0, 140 });
            DrawText(name, sx - name_w / 2, sy + half + 4, 10, (Color){ 255, 255, 255, 180 });
            continue;
        }

        // Completed: full sprite tinted green
        if (n->completed)
        {
            if (has_tex)
            {
                DrawTexturePro(tex, src_rect, dest_rect, (Vector2){0, 0}, 0.0f, (Color){ 70, 230, 130, 230 });
            }
            else
            {
                int r = (n->type == NODE_BOSS) ? 18 : 14;
                DrawCircle(sx, sy, (float)r, (Color){ 48, 52, 68, 255 });
                DrawCircle(sx, sy, r - 3, (Color){ 70, 200, 115, 150 });
            }
        }
        else if (i == hovered_node)
        {
            // Pulse tint between dark grey and white
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 5.5f);
            unsigned char grey = (unsigned char)(100 + 155 * pulse);
            Color hover_tint = { grey, grey, grey, 230 };
            if (has_tex)
            {
                DrawTexturePro(tex, src_rect, dest_rect, (Vector2){0, 0}, 0.0f, hover_tint);
            }
            else
            {
                Color c = theme_node_color(n->type);
                DrawCircle(sx, sy, half + 2, RAYWHITE);
                DrawCircle(sx, sy, (float)half, c);
                int icon_size = n->type == NODE_BOSS ? 18 : 10;
                DrawText(theme_node_icon(n->type), sx - MeasureText(theme_node_icon(n->type), icon_size) / 2, sy - icon_size / 2, icon_size, RAYWHITE);
            }
        }
        else
        {
            if (has_tex)
                DrawTexturePro(tex, src_rect, dest_rect, (Vector2){0, 0}, 0.0f, sprite_tint);
            else
            {
                int r = (n->type == NODE_BOSS) ? 18 : 14;
                Color c = theme_node_color(n->type);
                DrawCircle(sx, sy, r + 3, (Color){ c.r, c.g, c.b, 35 });
                DrawCircle(sx, sy, (float)r, c);
                DrawCircle(sx, sy, r - 3, (Color){ 0, 0, 0, 60 });
                int icon_size = n->type == NODE_BOSS ? 18 : 10;
                DrawText(theme_node_icon(n->type), sx - MeasureText(theme_node_icon(n->type), icon_size) / 2, sy - icon_size / 2, icon_size, RAYWHITE);
            }
        }

        int name_w = MeasureText(name, 10);
        DrawRectangle(sx - name_w / 2 - 3, sy + half + 2, name_w + 6, 12, (Color){ 0, 0, 0, 140 });
        DrawText(name, sx - name_w / 2, sy + half + 4, 10, RAYWHITE);
    }

    // Debug overlay — shows cumulative path values per lane
    if (show_debug)
    {
        static const Color path_palette[] = {
            { 255, 100, 100, 230 },  // red
            { 100, 220, 100, 230 },  // green
            { 100, 150, 255, 230 },  // blue
            { 255, 220, 50, 230 },   // gold
            { 200, 100, 255, 230 },  // purple
        };
        static const int palette_count = sizeof(path_palette) / sizeof(path_palette[0]);

        // Per-node path state
        int path_val[64][5];
        int path_lane[64][5];
        int path_count[64];
        memset(path_val, 0, sizeof(path_val));
        memset(path_lane, -1, sizeof(path_lane));
        memset(path_count, 0, sizeof(path_count));

        // Helper: node value
        int node_val[64];
        for (int i = 0; i < map->node_count; i++)
        {
            switch (map->nodes[i].type)
            {
                case NODE_SHOP:   node_val[i] = 12; break;
                case NODE_EVENT:  node_val[i] = 10; break;
                case NODE_REST:   node_val[i] = 3;  break;
                case NODE_ELITE:  node_val[i] = -5; break;
                default:          node_val[i] = 1;  break;
            }
        }

        // Forward propagation: process nodes in order
        // Start node gets its own value as path 0
        if (map->node_count > 0)
        {
            path_count[0] = 1;
            path_val[0][0] = node_val[0];
            path_lane[0][0] = 0;
        }

        for (int i = 0; i < map->node_count; i++)
        {
            if (path_count[i] == 0 && i > 0) continue;

            MapNode *n = &map->nodes[i];

            for (int c = 0; c < n->conn_count; c++)
            {
                int child = n->conns[c];
                if (child < 0 || child >= map->node_count) continue;

                for (int p = 0; p < path_count[i]; p++)
                {
                    int lane = path_lane[i][p];
                    int val = path_val[i][p] + node_val[child];

                    // Add to child if this lane isn't already tracked
                    bool found = false;
                    for (int cp = 0; cp < path_count[child]; cp++)
                    {
                        if (path_lane[child][cp] == lane)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found && path_count[child] < 5)
                    {
                        path_lane[child][path_count[child]] = lane;
                        path_val[child][path_count[child]] = val;
                        path_count[child]++;
                    }
                }
            }
        }

        // Draw path values at each node
        for (int i = 0; i < map->node_count; i++)
        {
            Vector2 pos = node_screen_pos(&map->nodes[i]);
            int sx = (int)pos.x;
            int sy = (int)pos.y;

            for (int p = 0; p < path_count[i]; p++)
            {
                char vt[8];
                snprintf(vt, sizeof(vt), "%+d", path_val[i][p]);
                Color pc = path_palette[path_lane[i][p] % palette_count];
                DrawText(vt, sx + 20 + p * 30, sy - 6, 10, pc);
            }
        }
    }

    EndScissorMode();

    relic_tray_draw(g_state.relics, g_state.relic_count, (Rectangle){ 482.0f, 10.0f, 146.0f, 42.0f });

    Rectangle bounds = map_content_bounds(map);
    if (bounds.height > VIRT_H - 52 || bounds.width > VIRT_W)
    {
        const char *hint = "Wheel scrolls map  |  Shift+wheel pans";
        draw_text_box((Rectangle){ 110.0f, (float)(VIRT_H - 13), 420.0f, 12.0f }, hint, 10, 0, (Color){ 150, 155, 180, 185 }, TEXT_ALIGN_CENTER);
    }

    if (hovered_node >= 0 && map->nodes[hovered_node].available && !map->nodes[hovered_node].completed)
    {
        MapNode *n = &map->nodes[hovered_node];
        Rectangle tip = { (float)((VIRT_W / 2) - 70), (float)(VIRT_H - 38), 140.0f, 26.0f };
        DrawRectangleRec(tip, (Color){ 20, 21, 32, 230 });
        DrawRectangleLinesEx(tip, 1.0f, (Color){ 95, 100, 130, 220 });
        draw_text_box((Rectangle){ tip.x + 4.0f, tip.y + 4.0f, tip.width - 8.0f, 12.0f },
            theme_node_name(n->type), 10, 0, theme_node_color(n->type), TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ tip.x + 4.0f, tip.y + 16.0f, tip.width - 8.0f, 10.0f },
            "Click to travel", 10, 0, (Color){ 175, 178, 205, 220 }, TEXT_ALIGN_CENTER);
    }

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_MAP)
    {
        Rectangle hl = { 54.0f, 52.0f, 532.0f, 268.0f };
        game_draw_tutorial_overlay_ex(hl,
            "Choose A Route",
            "Lit nodes are available. You can only move through connected paths and cannot backtrack to higher floors once you commit.",
            "Click a lit node to travel  |  Right-click/Esc: skip",
            0,
            0);
    }
}
