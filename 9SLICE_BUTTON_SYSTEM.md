# 9-Slice Button System Plan

## Sprite Assets

Two PNG files in `assets/art/`, white/neutral (tinted at runtime):

### `btn_standard.png` — 13×22 pixels

```
┌──────┬─┬──────┐
│      │ │      │
│ 6×22 │1│ 6×22 │
│ cap  │ │ cap  │
└──────┴─┴──────┘
```

- **Left cap** (6×22): rounded left corner, 1px border, dark edge
- **Center** (1×22): solid fill, stretches horizontally
- **Right cap** (6×22): rounded right corner, 1px border, dark edge

### `btn_large.png` — 18×44 pixels

```
┌───────┬─┬───────┐
│       │ │       │
│ 8×44  │2│ 8×44  │
│ cap   │ │ cap   │
└───────┴─┴───────┘
```

- **Left cap** (8×44): taller corners, 2px border, room for title+subtitle
- **Center** (2×44): solid fill, stretches
- **Right cap** (8×44):

---

## 9-Slice Render Function

```c
void draw_9slice(Texture2D tex, int left, int right,
                 Rectangle dest, Color tint)
{
    float cw = dest.width - left - right;
    // Left cap
    DrawTexturePro(tex, (Rectangle){0, 0, left, tex.height},
                        (Rectangle){dest.x, dest.y, left, dest.height},
                        zero, 0, tint);
    // Center stretch
    DrawTexturePro(tex, (Rectangle){left, 0, 1, tex.height},
                        (Rectangle){dest.x+left, dest.y, cw, dest.height},
                        zero, 0, tint);
    // Right cap
    DrawTexturePro(tex, (Rectangle){left+1, 0, right, tex.height},
                        (Rectangle){dest.x+dest.width-right, dest.y, right, dest.height},
                        zero, 0, tint);
}
```

With `TEXTURE_FILTER_POINT` set on the texture (already the pattern in the codebase), the 1px center slice repeats cleanly.

---

## Standard Widths

| Constant | Width | Button Labels |
|----------|-------|--------------|
| `BTN_NARROW` | **80** | DECK, SET, Skip, `<` `>` `+` `-`, Gold overlay, Scale |
| `BTN_MED` | **120** | BACK, META SHOP, COLLECTIVE, SETTINGS, ACHIEVEMENTS, End Turn, Reroll, Extra +1, Confirm, ALLOW, NO THANKS |
| `BTN_WIDE` | **160** | START AREA, BEGIN RUN, HEAL PARTY, UPGRADE A CARD, FULLSCREEN, TELEMETRY toggle, BUY CARD |
| `BTN_FULL` | **200** | Shop services (UPGRADE - 30g + subtitle), REMOVE CARD + subtitle, TRAIN HP + subtitle, ENERGY/DRAW BOON + subtitle |

---

## Color Tinting

Button backgrounds are tinted at runtime via `DrawTexturePro`'s color parameter. The 9-slice texture is white so it renders as any color.

### Per-State Tints

| Role | Normal | Hover | Where |
|------|--------|-------|-------|
| **Primary** | `(46,117,182)` | `(80,160,230)` | START AREA, BEGIN RUN, BUY CARD |
| **Default** | `(42,48,70)` | `(70,78,110)` | META SHOP, BACK, SETTINGS, telemetry |
| **Green** | `(40,120,60)` | `(60,180,80)` | HEAL PARTY, Confirm, TRAIN HP |
| **Red** | `(160,60,60)` | `(220,100,100)` | REMOVE CARD |
| **Gold** | `(180,150,50)` | `(220,190,70)` | Upgrade badges, special actions |
| **Blue** | `(50,80,140)` | `(80,140,220)` | UPGRADE A CARD |
| **Disabled** | Same as normal, alpha 80 | — | Unavailable options |

### Helper Macros

```c
// Standard button: 22px tall
#define btn_std(dest, tint_normal, tint_hover, label) \
    btn_9slice_draw(g_assets.btn_standard, (dest), \
        (tint_normal), (tint_hover), (label), TEXT_NORMAL)

// Large button: 44px tall
#define btn_large(dest, tint_normal, tint_hover, title, subtitle) \
    btn_9slice_draw_large(g_assets.btn_large, (dest), \
        (tint_normal), (tint_hover), (title), (subtitle))
```

---

## Files Changed

| File | Change |
|------|--------|
| `src/assets.h` | Add `Texture2D btn_standard`, `btn_large` to `GameAssets` |
| `src/assets.c` | Load `btn_standard.png`, `btn_large.png` in `assets_load()`, unload in `assets_unload()` |
| `src/ui/ui.h` | Add `BTN_NARROW` (80), `BTN_MED` (120), `BTN_WIDE` (160), `BTN_FULL` (200). Add `btn_9slice_draw()`, `btn_9slice_draw_large()` declarations. |
| `src/ui/ui.c` | Implement `btn_9slice_draw()` — 9-slice render with hover tinting and centered label text. Implement `btn_9slice_draw_large()` — same but with title + subtitle. Both fall back to solid color rect if texture is `{0}`. |
| `src/ui/button.c` (optional) | Migrate `Button` struct to use 9-slice internally, or leave it and replace call sites individually. |
| `src/screens/title_screen.c` | Replace all 11 buttons with `btn_std()` calls. Snap widths to constants. Make area arrows (24→80) and asc +/- (24→80). |
| `src/screens/map_screen.c` | DECK (66→80), SET (66→80). Both 16px tall → 22px. |
| `src/screens/run_screen.c` | DECK (76→80), End Turn (76→120). |
| `src/screens/shop_screen.c` | 6 service buttons (190→200). Sale BUY (132→160), NEW CARD (132→120). LEAVE (92→80). Train HP member selectors (300→200). |
| `src/screens/rest_screen.c` | HEAL PARTY (170→160), UPGRADE (170→160). Both 44px tall. |
| `src/screens/card_reward_screen.c` | Skip (84→80), Reroll (84→80), Extra (96→120). |
| `src/screens/discard_screen.c` | Skip (80→80), Confirm (100→120). |
| `src/screens/settings_screen.c` | Scale +/- (28→80), FULLSCREEN (panel-48→160), BACK (104→120), TELEMETRY (140→160). |
| `src/screens/draft_screen.c` | BEGIN RUN (180→160). |
| `src/screens/meta_shop_screen.c` | BACK (128→120). |
| `src/screens/codex_screen.c` | BACK (128→120). |
| `src/screens/achievements_screen.c` | BACK (80→80, no change). |
| `src/screens/game.c` | Gold overlay (66→80), +6px height. |

---

## Button Text Drawing

### Standard (22px)
```c
draw_text_box((Rectangle){ dest.x + 4, dest.y + 3, dest.width - 8, dest.height - 6 },
    label, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
```

### Large (44px, with subtitle)
```c
draw_text_box((Rectangle){ dest.x + 4, dest.y + 4, dest.width - 8, 14 },
    title, 10, 0, RAYWHITE, TEXT_ALIGN_CENTER);
draw_text_box((Rectangle){ dest.x + 4, dest.y + 22, dest.width - 8, 14 },
    subtitle, 10, 0, subtitle_color, TEXT_ALIGN_CENTER);
```

---

## What Makes This Easier

- **One sprite per height** — `btn_standard.png` covers ~90% of all buttons in the game
- **Widths don't matter** — the center slice stretches, so 1 sprite handles 11 different widths
- **Colors don't matter** — runtime tinting means no separate sprite for blue/green/red
- **Hover is automatic** — the draw function lerps between normal and hover tint when the mouse is over the rect
- **Height standardization** — DECK/SET buttons become 22px instead of 16px (the game has room for this)
- **Fallback built in** — if the texture isn't loaded, falls back to solid `DrawRectangleRec` with the same color

## Total Sprite Work

Create two small PNG files:
- `btn_standard.png`: 13×22 pixels
- `btn_large.png`: 18×44 pixels

That's it. The entire button system runs on these two images.
