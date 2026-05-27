# Node Sprite Assets Plan

## Overview

Replace the current circle+text-icon node rendering on the map screen with full sprite textures. Each node type gets its own sprite (32×32, 48×48 for boss). Sprites replace `DrawCircle` for the node body — hover, locked, and completed states are drawn as overlays on top.

Also, remove the `NODE_START` concept. Every floor begins with a normal combat node instead.

---

## Sprite Files

All sprites go in `assets/art/`, loaded via `load_art_texture()` like class icons.

| File | Size | Node Type |
|------|------|-----------|
| `node_combat.png` | 32×32 | NODE_COMBAT |
| `node_elite.png` | 32×32 | NODE_ELITE |
| `node_rest.png` | 32×32 | NODE_REST |
| `node_shop.png` | 32×32 | NODE_SHOP |
| `node_event.png` | 32×32 | NODE_EVENT |
| `node_boss.png` | 48×48 | NODE_BOSS |

No `node_start.png` — start nodes are removed.

---

## Start Node Removal

### `src/systems/map_gen.c`

In the generator, replace `NODE_START` assignment with `NODE_COMBAT`:
```c
// Before:  grid[first_node].type = NODE_START;
// After:   grid[first_node].type = NODE_COMBAT;
```
Row 0 column 0 is now a regular combat node. The player clicks it, fights, and continues.

### `src/screens/map_screen.c`

Remove the `NODE_START` branch in the node-entry handler (lines 145–151):
```c
// Delete this entire block:
else if (t == NODE_START)
{
    g_state.map.nodes[ci].completed = true;
    g_state.map.current_index = -1;
    map_unlock_next(&g_state.map);
    return;
}
```
The first node now routes to `SCREEN_RUN` like any other combat node.

The auto-complete-and-unlock behavior for start nodes is no longer needed — the first combat serves the same purpose (beat the fight → node completed → map unlocks).

### `src/systems/map.c`

`map_find_start()` already falls back to returning index 0 when no `NODE_START` is found. No change needed — the first node (row 0 col 0) is automatically set to `available = true`.

### `NodeType` enum (`src/systems/map.h`)

Keep `NODE_START` in the enum to avoid shifting all values. It just won't be generated or handled specially anymore.

---

## Asset Loading

### `src/assets.h`

```c
Texture2D node_sprites[8];   // indexed by NodeType (0-7)
```

### `src/assets.c`

In `assets_load()`:
```c
const char *node_files[] = {
    "node_combat.png", "node_elite.png", "node_rest.png",
    "node_shop.png", "node_event.png", "node_boss.png"
};
int node_enum_start = NODE_COMBAT;  // = 1, skip NODE_START
for (int i = 0; i < 6; i++)
    g_assets.node_sprites[node_enum_start + i] = load_art_texture(node_files[i]);
```

In `assets_unload()`:
```c
for (int i = NODE_COMBAT; i <= NODE_BOSS; i++)
    if (g_assets.node_sprites[i].id != 0)
        UnloadTexture(g_assets.node_sprites[i]);
```

---

## Rendering Changes (`src/screens/map_screen.c`)

### Draw order (unchanged):
1. Connection lines between nodes
2. Node sprites + overlays
3. Node name text below

### Per-node rendering flow:

```c
Texture2D tex = g_assets.node_sprites[n->type];
float sprite_size = (n->type == NODE_BOSS) ? 48.0f : 32.0f;
int half = (int)(sprite_size * 0.5f);
Rectangle dest = { (float)(sx - half), (float)(sy - half), sprite_size, sprite_size };
```

**Locked** (not available, not completed):
```c
if (tex.id != 0)
    DrawTexturePro(tex, src_rect, dest, origin, 0.0f, (Color){ 80, 80, 80, 180 });
else
    DrawCircle(sx, sy, 14, dark_circle);  // fallback
```

**Completed:**
```c
if (tex.id != 0)
{
    DrawTexturePro(tex, src_rect, dest, origin, 0.0f, WHITE);
    DrawCircle(sx, sy, half - 2, (Color){ 70, 200, 115, 130 });  // green tint
}
```

**Hovered (available):**
```c
if (tex.id != 0)
{
    float pulse = 1.0f + 0.07f * sinf((float)GetTime() * 5.5f);
    DrawCircle(sx, sy, (float)(half + 4) * pulse, glow_color);
    DrawRectangleLinesEx(dest, 2.0f, WHITE);
    DrawTexturePro(tex, src_rect, dest, origin, 0.0f, WHITE);
}
```

**Available (not hovered):**
```c
if (tex.id != 0)
    DrawTexturePro(tex, src_rect, dest, origin, 0.0f, WHITE);
else
    DrawCircle(sx, sy, 14, type_color);
```

**Fallback:** If `tex.id == 0`, use the current `DrawCircle` + `DrawText(icon)` code. Keeps the game playable if sprites are missing.

The node name text (`theme_node_name(n->type)`) is still drawn below the sprite in all states.

### Hit-testing (unchanged):
```c
CheckCollisionPointRec(mouse, (Rectangle){ pos.x - 20.0f, pos.y - 20.0f, 40.0f, 40.0f })
```
The 40×40 box accommodates both 32×32 and 48×48 sprites.

---

## Files Changed

| File | Change |
|------|--------|
| `src/assets.h` | Add `Texture2D node_sprites[8]` to `GameAssets` |
| `src/assets.c` | Load `node_*.png` (6 files, skipping start), unload in `assets_unload()` |
| `src/screens/map_screen.c` | Replace circle+text with `DrawTexturePro` for each node. Add overlay states. Remove `NODE_START` entry handler. |
| `src/systems/map_gen.c` | Replace `NODE_START` with `NODE_COMBAT` for row 0 column 0 |

## Not Changed

- `src/systems/map.h` — `NodeType` enum kept as-is
- `src/systems/map.c` — `map_find_start()` fallback already handles no start node
- `src/ui/theme.h` / `src/ui/theme.c` — `theme_node_name()` still used for label text
- Connection drawing, hit-testing, click handling — all unchanged
