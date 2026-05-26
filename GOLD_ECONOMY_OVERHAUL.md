# Gold Economy Overhaul Plan

## Problem
Gold is plentiful (500–900g per run) but has almost nothing to spend it on (30–280g spent per run). Leftover gold is lost. Renown is the only persistent currency but all sources are run-end only.

## 0. Persistent Gold Display

Show gold on screen **at all times** — combat, map, rest, shop, events. No longer an afterthought.

**Implementation:** Add `game_draw_gold_overlay()` — a small gold-text pill in the top-right corner of every screen. Called in `main.c`'s render loop after `ft_draw()`.

**Files:** `src/main.c`, `src/game.c`, `src/game.h`

---

## 1. Gold → Renown at Run End

Leftover gold (current `g_state.gold` balance) converts to renown at the end of each run.

**Rate:** 50g → 1 renown (integer division).

In `game_over_screen_update()` after `meta_record_run()`:
```c
int gold_renown = g_state.gold / 50;
g_state.meta.renown += gold_renown;
```
Display `"Gold converted: +%dR"` on the game-over screen.

**Files:** `src/screens/game_over_screen.c`, `src/game.h`, `src/systems/meta.c`

---

## 2. Meta-Shop Repricing

All meta-shop costs roughly doubled to absorb the new renown influx from gold conversion.

| Item | Current | Proposed |
|------|---------|----------|
| Party Slot IV | 4R | **20R** |
| Party Slot V | 8R | **40R** |
| Travel Fund (rank 1/2/3) | 3/6/9R | **6/12/18R** |
| Legacy (rank 1/2/3) | 5/10/15R | **20/30/45R** |
| Class unlocks | 12R | **20R** |
| Scout's Kit | 5R | **10R** |
| Mana Crystal | 6R | **10R** |
| Traveler's Pack | 8R | **15R** |
| First Aid | 8R | **15R** |
| Sharpened Blades (per rank) | 5R | **10R** |
| Reinforced Armor (per rank) | 5R | **10R** |
| Seasoned Adventurer | 8R | **15R** |
| Master Raider | 12R | **25R** |

**Files:** `src/screens/meta_shop_screen.c`, `src/systems/meta.h`, `src/systems/meta.c`

---

## 3. Extra Shop Options

### Card for Sale (left panel, 25g)
One random card from party class + utility pools, displayed like a reward card. Buy button to add to deck. New card each shop visit. Reroll single card for 5g

### HP Boost (15g)
Pick a party member → +5 max HP + 5hp heal.

### Boon (20g)
Choose +1 energy or +2 draw for the next combat. Stored as temp state flags, consumed at next combat start.

**Layout:** Keep existing UPGRADE (30g) and REMOVE (20g) buttons. Add a second row of buttons below.

**Files:** `src/screens/shop_screen.c`, `src/data/card_defs.h`, `src/game.h`, `src/combat/combat.c`

---

## 4. Reroll Rework

### Remove old token system
- Delete `reroll_tokens` from `GameState`
- Remove boss/elite reroll drops in `run_screen.c`
- Remove `EVENT_EFFECT_GAIN_REROLL_TOKEN` from event system
- Events that gave tokens now give 15–18g instead

### Add gold-based options to card reward screen
- **Reroll (10g)** — same effect as before, costs gold
- **Extra Choice (15g)** — adds +1 card to the reward pool and regenerates

**Files:** `src/screens/card_reward_screen.c`, `src/screens/run_screen.c`, `src/screens/event_screen.c`, `src/screens/draft_screen.c`, `src/game.h`, `src/data/event_defs.h`, `src/data/event_defs.c`, `assets/data/events.json`

---

## 5. Gold Relics (4 new, positive only)

| Relic | Rarity | Effect | Hook |
|-------|--------|--------|------|
| **Gilded Blade** | 2 | "+1 damage per 50g held (max +6)" | Damage calc in `resolve_card_on_target()` |
| **Prosperity Charm** | 1 | "Combat start: gain 1 shield per 20g held to all allies" | `combat_start()` |
| **Golden Idol** | 3 | "Combat start: gain +1 max HP per 25g held" | `combat_start()` |
| **Hoarder's Scales** | 2 | "Combat end: gain 1 gold per 20g held" | Post-gold calc in `check_victory()` |
| **Fate's Interest** | 2 | "Boons last 3 turns but cost 30g" | idk |

**Files:** `src/systems/relic.h`, `assets/data/relics.json`, `src/combat/combat.c`

---

## 6. Tiered Card Upgrades

### Data model change
```c
// Before:  bool upgraded;
// After:   int upgrade_level;   // 0 = base, 1 = upgraded, 2 = maxed
```

### Formula
Apply the same `(x * 3 + 1) / 2` for each level:
```c
int card_damage(const CardDef *def, int upgrade_level) {
    int val = def->damage;
    if (upgrade_level >= 1) val = (val * 3 + 1) / 2;
    if (upgrade_level >= 2) val = (val * 3 + 1) / 2;
    return val;
}
```
Same for `card_heal` and `card_shield`.

### Cost & availability
| Level | Shop | Rest site | Events | Boss rewards |
|-------|------|-----------|--------|-------------|
| 0 → 1 | 30g | Free | Can happen | Can happen |
| 1 → 2 | **60g** | **No** | **No** | **No** |

Level 1→2 is **shop-only**.

### Visuals
- Level 1: `card_template_upgraded.png`
- Level 2: `card_template_maxed.png` (new asset to create)

`theme_draw_card_art()` picks the template based on `upgrade_level`.

### Upgrade preview
In the shop upgrade-pick mode:
- If upgrading level 0→1: show the current stat delta preview (DMG 2>3, etc.)
- If upgrading level 1→2: show the next tier's stat delta

### `card_upgrade_changes_values()`
Now checks whether level 2 changes anything from level 1. A card is upgradeable if either tier changes stats from the current level.

### Shop UI
The upgrade button changes depending on available cards:
- If any level-0 cards exist: show "UPGRADE (30g)"
- If any level-1 cards exist: show "SUPER UPGRADE (60g)"
- If both exist: two buttons

Deck browser in upgrade mode labels upgrades accordingly.

**Files:** `src/systems/deck.h`, `src/systems/deck.c`, `src/ui/deck_browser.c`, `src/ui/theme.c`, `src/screens/shop_screen.c`, `src/screens/rest_screen.c`, `src/screens/event_screen.c`, `src/screens/card_reward_screen.c`, `src/combat/combat.c`, `src/ui/hand_render.c`, `src/ui/party_frames.c`, `src/screens/codex_screen.c`, `src/screens/draft_screen.c`, `src/assets.c`, `src/assets.h`

---

## Summary of all files

| # | File | Phase |
|---|------|-------|
| 1 | `src/main.c` | 0 |
| 2 | `src/game.h` | 0, 1, 3, 4, 6 |
| 3 | `src/game.c` | 0 |
| 4 | `src/systems/meta.h` | 1, 2 |
| 5 | `src/systems/meta.c` | 1, 2 |
| 6 | `src/systems/deck.h` | 6 |
| 7 | `src/systems/deck.c` | 6 |
| 8 | `src/systems/relic.h` | 5 |
| 9 | `assets/data/relics.json` | 5 |
| 10 | `src/combat/combat.c` | 5, 6 |
| 11 | `src/screens/game_over_screen.c` | 1 |
| 12 | `src/screens/meta_shop_screen.c` | 2 |
| 13 | `src/screens/shop_screen.c` | 3, 6 |
| 14 | `src/screens/card_reward_screen.c` | 4, 6 |
| 15 | `src/screens/run_screen.c` | 4 |
| 16 | `src/screens/event_screen.c` | 4, 6 |
| 17 | `src/screens/draft_screen.c` | 4, 6 |
| 18 | `src/screens/rest_screen.c` | 6 |
| 19 | `src/screens/codex_screen.c` | 6 |
| 20 | `src/ui/theme.c` | 6 |
| 21 | `src/ui/deck_browser.c` | 6 |
| 22 | `src/ui/hand_render.c` | 6 |
| 23 | `src/ui/party_frames.c` | 6 |
| 24 | `src/data/event_defs.h` | 4 |
| 25 | `src/data/event_defs.c` | 4 |
| 26 | `assets/data/events.json` | 4 |
| 27 | `src/assets.h` | 6 |
| 28 | `src/assets.c` | 6 |

## 7. Metric Tracking

Create a new log for metrics. This log will be in the main log folder and persist between sessions. The goal of this log will be to track gold income and usage for balancing purposes.