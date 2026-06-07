# Collection Screen Plan

## Overview

A new screen that lets the player browse **all cards** in the game — both player (class) cards and enemy cards. The screen is accessible from the Codex.

---

## Layout (640×360 virtual resolution)

### Both Tabs

```
┌──────────────────────────────────────────────────────┐
│  COLLECTION                           [Back]         │  y=0..18
│──────────────────────────────────────────────────────│
│  [Player Cards]  [Enemy Cards]                       │  y=20..38  (mode tabs)
│                                                      │
│  ┌────────────── main content ──────────────────┐   │  y=42..∞
│  │                                                │   │
│  │  (varies by tab — see below)                   │   │
│  │                                                │   │
│  └────────────────────────────────────────────────┘   │
│                                                      │
│  ┌──── Inspector (95% height) ──────────────────┐   │  y=42, h≈300
│  │                                                │   │
│  │  (card tooltip + unlock info)                  │   │
│  │                                                │   │
│  └────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────┘
```

**Inspector panel**: Right side, x=448, w=172, y=42, h≈300 (stretches ~95% from tabs to bottom, leaving ~18px for back button area). Used for hovered card details in both tabs.

---

## Tab 1: Player Cards

### Class Row (y=42..62)

A horizontal row of compact class buttons:

```
[G] [C] [M] [R] [S] [R] [P*] [W*] [B*] [U]
```

- `[G]` = Guardian, `[C]` = Cleric, `[M]` = Mage, `[R]` = Rogue, `[S]` = Shaman
- `[R]` = Ranger, `[P*]` = Paladin, `[W*]` = Warlock, `[B*]` = Bard, `[U]` = Utility
- Locked classes (Paladin, Warlock, Bard) shown with `*` and dimmed style
- Each button: ~52w × 20h, drawn with class color accent + class abbreviation

### Card Grid (x=10..442, y=66..hand_y-8)

Scrollable viewport showing the selected class's cards sorted by energy cost (ascending, then alphabetically).

**Dimensions**: ~432w × ~240h

**Card size**: 96w × 120h, gap=3

**Grid**: 4 columns × 2 rows visible (8 cards). Classes have 8-9 cards, so scrolling may not be needed but is supported.

**For each card**:
- Rendered with `theme_draw_card_art_seeded()` (existing function)
- **Locked cards** (`reward_only && !meta_content_active(meta, unlock_key)`):
  - After drawing card art, overlay a dark semi-transparent rect
  - Draw "LOCKED" text centered on the card
- **Locked class cards** (class not yet unlocked via meta):
  - Same locked overlay style
  - Inspector explains: "Class Locked — Purchase in the Meta Shop"

### Inspector (Right Panel)

When a card is hovered:
- Use `theme_draw_card_tooltip_limited()` for the normal tooltip content
- **If locked**: append extra lines after the normal tooltip:
  - > **LOCKED** — Complete "[unlock_event]" or purchase in the Meta Shop
  - > *(text derived from `unlock_key` / `unlock_event` on CardDef)*

When a locked class button is hovered:
- Draw a small tooltip: "Purchase [Class Name] in the Meta Progression Shop to unlock cards."

---

## Tab 2: Enemy Cards

### Enemy List (x=10..125, y=42..340)

Scrollable sidebar list of all enemies, organized by area.

**Built once** on screen entry by iterating the encounter API:

```
for each area (area_def_by_index)
  for each floor of that area
    for each normal encounter (encounter_for_area_floor)
      collect unique EnemyDef pointers → mark as NORMAL
    collect elite encounter enemies → mark as ELITE
    collect boss encounter enemies → mark as BOSS
```

Display format:

```
Greenwood Breach
  Thorn Sprite         [Normal]
  Briar Archer         [Normal]
  Giant Spider         [Normal]
  ...
  --- ELITE ---
  Mossback Guard       [Elite]
  --- BOSS ---
  Bramble Matron       [Boss]

Venom Marsh
  Mire Leech           [Normal]
  ...
  --- ELITE ---
  Rot Basilisk         [Elite]
  --- BOSS ---
  Greater Manticore    [Boss]
```

- Each enemy shown as a clickable text row (~115w × 14h)
- Highlight the selected enemy
- If an enemy appears in multiple areas, show under the first area it appears in
- If an enemy appears in both normal AND elite/boss encounters, mark with the highest tier (Boss > Elite > Normal)

### Card Grid (x=130..442, y=42..340)

When an enemy is selected from the sidebar, show that enemy's cards in a grid (same dimensions as Player Cards grid).

**Enemy card rendering**: New function `theme_draw_enemy_card()` (in `collection_screen.c` or `theme.c`) that draws a 96×120 card face with:

```
┌──────────────┐
│ EnemyCard    │  ← name band (dark red)
│              │
│  [INTENT]    │  ← intent badge (AOE/ATK/HEAL/etc)
│   Cost: 1    │  ← energy cost
│   T: 2       │  ← cast time
│   Dmg: 12    │  ← damage (red)
│   Shld: 8    │  ← shield (blue)
│   Summon     │  ← summon icon if applicable
│   Bleed      │  ← status icon if applicable
└──────────────┘
```

The function takes an `EnemyCardDef` and draws in the same card-template style as `theme_draw_card_art_seeded()` but with enemy-card-specific data.

### Inspector (Right Panel)

When an enemy card is hovered:
- Custom tooltip showing enemy card details:
  - **Name** + Intent icon + Intent description
  - Cost | Cast Time | Target
  - Damage (with repeats and targets)
  - Heal amount
  - Shield amount
  - Applied status (type, turns, value)
  - Summons (enemy ID + count)
  - Lifesteal / buff values
  - Full description text
- No "locked" concept for enemy cards — always visible.

---

## Files to Create

| File | Contents |
|------|----------|
| `src/screens/collection_screen.c` | ~650 lines — full screen logic |
| `src/screens/collection_screen.h` | ~5 lines — prototypes |

## Files to Modify

| File | Change |
|------|--------|
| `src/game.h` | Add `SCREEN_COLLECTION` to `GameScreen` enum |
| `src/screens/screens.h` | Add `collection_screen_update()` / `collection_screen_draw()` |
| `src/main.c` | Add `SCREEN_COLLECTION` cases in both update and draw switch statements |
| `src/screens/codex_screen.c` | Add "Collection" button to the codex screen's navigation |

## Key Data Access Patterns

**Player cards per class:**
```c
const CardDef **cards = class_card_sets[class];
int count = class_card_counts[class];
```

**Sort cards by cost:**
```c
qsort(cards, count, sizeof(CardDef*), compare_by_cost_then_name);
```

**Lock check:**
```c
bool locked = card->reward_only && !meta_content_active(&g_state.meta, card->unlock_key);
```

**Locked class check:**
```c
bool class_locked = !meta_class_unlocked(&g_state.meta, class);
```

**Iterate all enemies:**
```c
for (int i = 0; i < enemy_defs_loaded_count(); i++) {
    const EnemyDef *e = enemy_def_by_index(i);
}
```

**Enemy cards:**
```c
for (int c = 0; c < ed->card_count; c++) {
    const EnemyCardDef *card = &ed->cards[c];
}
```

**Build enemy→area map:**
```c
for (int a = 0; a < area_defs_loaded_count(); a++) {
    const AreaDef *area = area_def_by_index(a);
    for (int f = 0; f < area->floor_count; f++) {
        // normal
        int nc = encounter_count_for_area_floor(area->id, f);
        for (int e = 0; e < nc; e++)
            collect(encounter_for_area_floor(area->id, f, e), NORMAL);
        // elite
        collect(elite_for_area_floor(area->id, f), ELITE);
        // boss
        collect(boss_for_area_floor(area->id, f), BOSS);
    }
}
```

## States / Edge Cases

| State | Handling |
|-------|----------|
| No area data loaded | Enemy tab shows "No encounter data available" |
| No enemies in an area | Skip that area in the sidebar |
| Class with no cards | Show "This class has no cards yet" in the grid area |
| All classes locked | Class row still shows all classes; locked ones have dimmed style; grid shows "Select a class" |
| Enemy with no cards defined | Skip card grid, show "This enemy has no card data" |
| Tooltip overflow | Clamp inspector content to panel bounds with scrolling if needed |
| Back navigation | Right-click or back button → return to codex |
| Enemy appears in normal+elite | Mark as Elite (higher priority) |
