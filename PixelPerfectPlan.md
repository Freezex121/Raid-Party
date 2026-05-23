# Pixel-Perfect 3× Upscale Plan

Convert Raid Party from smooth 1280×720 to a crisp pixel-art look at 3× scale (427×240 virtual → 1281×720 display).

---

## Phase 0 — Constants & Virtual Resolution

Centralize the resolution so one value scales everything.

**In `constants.h`:**
```c
#define VIRT_W 427
#define VIRT_H 240
#define SCALE 3
#define SCREEN_W (VIRT_W * SCALE)   // 1281
#define SCREEN_H (VIRT_H * SCALE)   // 720
```

**Replace all hardcoded `1280` / `720` references** with `VIRT_W` / `VIRT_H`:

| File | References |
|---|---|
| `run_screen.c` | ~8 (card positions, enemy positions, end turn button, energy display, cast bar, combo text, floating text) |
| `hand_render.c` | 2 (hand positioning math) |
| `map_screen.c` | 3 (node positioning, text centering) |
| `party_frames.c` | 3 (party frame layout) |
| `cast_bar.c` | 2 (bar positioning) |
| `enemy_render.c` | 2 (enemy sprite positioning) |
| `draft_screen.c` | 2 (card grid layout) |
| `rest_screen.c`, `shop_screen.c` | ~4 each |
| `card_reward_screen.c` | 2 |
| `combat.c` | 2 (calc_enemy_positions) |
| `floating_text.c` | 1 (gold popup position) |

**Effort:** 30 min — mechanical search-and-replace.

---

## Phase 1 — Render Texture Pipeline

Draw everything at virtual resolution, upscale to window with nearest-neighbor.

**In `main.c`:**

```c
RenderTexture2D target = LoadRenderTexture(VIRT_W, VIRT_H);

while (!WindowShouldClose())
{
    // ── Draw to virtual canvas ──
    BeginTextureMode(target);
    ClearBackground((Color){ 14, 14, 26, 255 });

    // ... existing draw logic (screens) ...

    EndTextureMode();

    // ── Upscale to window ──
    BeginDrawing();
    ClearBackground(BLACK);
    rlSetTextureFilter(target.texture.id, RL_TEXTURE_FILTER_POINT);
    DrawTexturePro(
        target.texture,
        (Rectangle){ 0, 0, (float)VIRT_W, (float)-VIRT_H },  // negative height = flip
        (Rectangle){ 0, 0, (float)SCREEN_W, (float)SCREEN_H },
        (Vector2){ 0, 0 }, 0, WHITE
    );
    EndDrawing();
}
```

`RL_TEXTURE_FILTER_POINT` disables bilinear filtering → crisp pixel edges.

**Remove** `BeginMode2D` / `EndMode2D` / `rlPushMatrix` / `rlTranslatef` / `rlPopMatrix` — render texture replaces them.

**Effort:** 1 hour.

**Result at this point:**
- Everything rendered at 427×240
- Shapes are soft/blurry (anti-aliased at low res)
- Text is smooth (default raylib font)

---

## Phase 2 — Replace Rounded Shapes with Pixel Art

No more sub-pixel anti-aliasing. Every shape snaps to pixels.

### Replacements

| Current Call | Replacement |
|---|---|
| `DrawRectangleRounded(...)` | `DrawRectangle(...)` |
| `DrawRectangleRoundedLinesEx(...)` | `DrawRectangleLinesEx(...)` |
| `DrawCircle(...)` | Pre-rendered pixel circle texture or `DrawRectangle` with mask |
| `DrawLine(...)` | `DrawRectangle` 1px tall/wide |

### Card Layout at VIRT_W (427×240)

| Element | Old (1280×720) | New (427×240) |
|---|---|---|
| Card width | 180 | 60 |
| Card height | 220 | 75 |
| Hand gap | 10 | 3 |
| Hand Y | 720-220-15 = 485 | 240-75-5 = 160 |
| Party frame W | 300 | 100 |
| Party frame H | 70 | 24 |
| Enemy size | 110 | 40 |
| Font sizes | 11, 14, 16, 20, 22 | 8, 10, 12, 14 |
| Card description | 11px wrapped | 8px wrapped |
| Stats line start | cy+ch-52 | cy+ch-18 |

### Hit-testing

All `CheckCollisionPointRec` calls use VIRT_W/VIRT_H positions — must match the new pixel-snapped coordinates.

**Effort:** 3-4 hours.

---

## Phase 3 — Pixel Font

Replace raylib's smooth default font with a true pixel bitmap font.

### Options

1. **raylib bitmap font from PNG** — Create a `font.png` with 8×8 or 8×16 glyphs, load via `LoadFont("font.png")`.
2. **Embedded pixel font** — Use a free pixel font TTF (e.g., m3x6, Unscii, PxPlus_IBM_VGA8), `LoadFontEx("font.ttf", 8, 0, 0)`.
3. **Manual glyph atlas** — `GenImageFontAtlas()` with raw pixel data.

### Font size mapping

| Current | Pixel font | Usage |
|---|---|---|
| 22 | 8 | Card name |
| 20 | 8 | Button text |
| 16 | 7 | Card name, headings |
| 14 | 7 | Subheadings, stats |
| 12 | 6 | Labels, energy, descriptions |
| 11 | 6 | Card descriptions |
| 10 | 5 | Stats, class labels, turn counter |

### Files to update

Every `DrawText` call needs font size adjusted — ~15 files, ~60 calls.

**Effort:** 4-5 hours.

---

## Phase 4 — Polish & Tune

Make everything look intentional.

- Adjust enemy HP bar widths, cast bar widths, party frame widths for 427×240.
- Verify hit detection rectangles match new pixel-aligned visuals.
- Card art placeholder → replace colored rectangles with actual pixel art frames (optional).
- Test at SCALE=2 (854×480) and SCALE=3 (1281×720) — both should look crisp.
- Add `#pragma pack(push, 1)` to structs if MSVC alignment causes issues at low res (unlikely but worth checking).

**Effort:** 2-3 hours.

---

---

## Art Size Reference (Virtual Resolution: 427×240, Scale: 3×)

Every element should be designed or rendered at these pixel dimensions to look crisp at 3× upscale.

### UI Elements

| Element | Width | Height | Notes |
|---|---|---|---|
| **Card (hand)** | 60 | 75 | 3px gap between cards |
| **Card (reward)** | 66 | 90 | Larger in reward picker |
| **Card (deck browser)** | 46 | 65 | For upgrade/remove picker |
| **Party frame** | 100 | 24 | Each party member, top of screen |
| **Party HP bar** | 74 | 5 | Inside party frame |
| **Enemy sprite** | 36 | 36 | Rendered as rectangle |
| **Enemy HP bar** | 40 | 5 | Below enemy sprite |
| **Cast bar** | 100 | 10 | Below each enemy |
| **Button** | variable | 16 | Standard button height |
| **Map node radius** | — | — | 11-15px radius (22-30px diameter) |
| **Screen center X** | — | — | 213 (VIRT_W/2) |
| **Screen center Y** | — | — | 120 (VIRT_H/2) |

### Font Sizes (pixel font recommended)

| Context | Size | Old size (1280×720) |
|---|---|---|
| Card name | 8 | 22 |
| Button text | 7 | 20 |
| Section headings | 8 | 16-18 |
| Card description | 6 | 11 |
| Stats (damage/heal) | 6 | 11-12 |
| Labels (class name) | 5 | 10 |
| Energy counter | 6 | 14-16 |
| Turn counter | 5 | 12 |

### Positions (at VIRT_W=427, VIRT_H=240)

| Element | X | Y | Notes |
|---|---|---|---|
| Party frames top | center | 3 | Spread across top |
| Enemy row | spread | 80 | 1-3 enemies, even spacing |
| Enemy HP bar | center below enemy | enemy_y + 26 | — |
| Cast bar | below enemy | enemy_y - 10 | Above enemy sprite |
| Hand row | center | 160 | Cards spread across bottom |
| Energy text | 5 | 3 | Top-left |
| End Turn button | 390 | 220 | Bottom-right |
| "Select a target" text | 5 | 30 | Top-left during targeting |
| Turn counter | 5 | 230 | Bottom-left |
| Combo indicator | center | 28 | Below party frames |
| Channel indicator | center | 34 | Below combo indicator |

### Card Layout (inside 60×75 card)

| Element | Offset X | Offset Y | Size |
|---|---|---|---|
| Card name | 3 | 3 | 8px font |
| Cost circle | right-12 | 4 | 8px radius |
| Class label | 3 | 13 | 5px font |
| Separator line | 3 | 18 | 54px wide |
| Description | 3 | 21 | 54px wide, 6px font |
| Stats (damage/heal) | 3 | bottom-14 | 6px font |

### Map Layout

| Element | Size | Notes |
|---|---|---|
| Node circle radius | 12-16px | Larger for boss (20px) |
| Node spacing X | 50-100px | Depends on row layout |
| Node spacing Y | 70-90px | Between rows |
| Checkmark | 10px | Completed node indicator |

---

## Summary

| Phase | What | Time |
|---|---|---|
| 0 | Virtual resolution constants + replace hardcoded 1280/720 | 30 min |
| 1 | Render texture + nearest-neighbor upscale | 1 hr |
| 2 | Replace rounded shapes, re-layout at 427×240 | 3-4 hr |
| 3 | Pixel bitmap font + re-size all text | 4-5 hr |
| 4 | Polish, hit-testing, visual tuning | 2-3 hr |
| **Total** | | **~11-14 hours** |

### Quick Win (Phases 0-1 only)

Stop after Phase 1 to see the low-res look with smooth text/shapes, then decide if the remaining 7-9 hours is worth it. Most of the "pixel feel" comes from the render texture upscale alone.
