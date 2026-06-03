# Sound Design Plan — Raid Paper Legends

## Current State

The audio infrastructure is **fully built but completely disconnected**:
- 17 SFX slots, 3 music slots defined in `GameSfx`/`GameMusic` enums (`src/assets.h:19-45`)
- Loading pipeline, playback API, per-frame update, volume control all implemented (`src/assets.c`)
- Settings screen has Master/Music/SFX sliders persisted to `settings.cfg`
- **`assets_play_sfx()` and `assets_play_music()` are never called** from any gameplay code
- **Zero audio files** exist on disk

---

## Expanded SFX Slots (17 → 28)

| # | Enum | Trigger Point | Description |
|---|------|---------------|-------------|
| 1 | `SFX_BUTTON_HOVER` | Any UI button hover | Soft tick, ~80ms. Pencil tap on wood. Subtle. |
| 2 | `SFX_BUTTON_CLICK` | Any UI button press | Deeper click, ~120ms. Firm keypress/latch. |
| 3 | `SFX_CARD_HOVER` | Mouse over hand card | Paper shuffle, ~150ms. One-card fan. Light, crisp. |
| 4 | `SFX_CARD_PLAY` | Card played from hand | Whoosh + impact, ~300ms. Swoosh into thud. |
| 5 | `SFX_CARD_DISCARD` | End-of-turn discard | Paper flutter, ~200ms. Cards dropping onto pile. |
| 6 | `SFX_CARD_DRAW` | Deal hand / draw per turn | Card fanning, ~250ms. Multiple cards sliding together. |
| 7 | `SFX_DAMAGE` | Any damage dealt | Impact hit, ~150ms. Punch/slash/thud. |
| 8 | `SFX_DAMAGE_HEAVY` | Combo/crit damage | Bigger impact, ~200ms. Deeper, with low-end thump. |
| 9 | `SFX_HEAL` | Heal applied | Gentle chime, ~200ms. Warm ascending tone (RPG potion). |
| 10 | `SFX_SHIELD` | Shield applied | Metallic resonance, ~200ms. Protective clang/shimmer. |
| 11 | `SFX_TAUNT` | Guardian taunt triggers | War drum/horn, ~300ms. Aggressive bark/battle cry. |
| 12 | `SFX_INTERRUPT` | Enemy cast interrupted | Sharp cut, ~150ms. Glass shatter or string snap. |
| 13 | `SFX_BURN_TICK` | Burning status ticks | Fire crackle, ~200ms. Embers popping. |
| 14 | `SFX_BLEED_TICK` | Bleed status ticks | Wet drip, ~150ms. Squelch or blood drop. |
| 15 | `SFX_PARTY_DOWNED` | Ally HP hits 0 | Heavy thud + groan, ~400ms. Body hitting ground. |
| 16 | `SFX_PARTY_REVIVED` | Ally revived | Rising chime, ~350ms. Uplifting magical resurrection. |
| 17 | `SFX_ENEMY_CAST_WARNING` | Enemy intent bar appears | Low ominous drone, ~250ms. Warning tone. |
| 18 | `SFX_BOSS_CAST_WARNING` | Boss intent bar appears | Deeper, louder version, ~350ms. More threatening. |
| 19 | `SFX_ENEMY_ATTACK` | Enemy card throw spawns | Whoosh + impact, ~300ms. Aggressive, lower than player. |
| 20 | `SFX_GOLD_PICKUP` | Gold gained | Coin jingle, ~300ms. 2-3 coins clinking. |
| 21 | `SFX_REWARD_PICKUP` | Card/relic reward screen | Fanfare snippet, ~400ms. Triumphant ascending notes. |
| 22 | `SFX_LEVEL_UP` | Party member levels up | Ascending bells, ~500ms. 3-4 note upward scale. |
| 23 | `SFX_VICTORY` | Combat victory | Fanfare, ~800ms. Resolving chord, triumphant. |
| 24 | `SFX_DEFEAT` | Party wipe | Descending tones, ~600ms. Somber minor-key decay. |
| 25 | `SFX_MAP_SELECT` | Map node hover/click | Soft harp pluck, ~120ms. One pleasant note. |
| 26 | `SFX_SYNERGY_TRIGGER` | Class synergy/combo activates | Magical sparkle, ~250ms. Chime with reverb tail. |
| 27 | `SFX_SHOP_PURCHASE` | Buy item in shop | Cash register ding, ~200ms. Satisfying purchase. |
| 28 | `SFX_ERROR` | Can't afford, invalid target | Soft buzzer, ~150ms. Negative but not annoying. |

---

## Expanded Music Slots (3 → 17)

| # | Enum | Plays During | Description |
|---|------|-------------|-------------|
| 1 | `MUSIC_TITLE` | Title screen | Orchestral, adventurous. Light strings, celesta. Medium tempo. |
| 2 | `MUSIC_MAP` | Map screen | Calm, contemplative. Acoustic guitar or harp arpeggios. Low intensity. |
| 3 | `MUSIC_COMBAT_GREENWOOD` | Greenwood Breach combats | Upbeat, heroic. Woodwinds, light percussion. Forest feel. |
| 4 | `MUSIC_COMBAT_VENOM` | Venom Mire combats | Dark, swampy. Low strings, muted drums, eerie pads. |
| 5 | `MUSIC_COMBAT_CINDER` | Cinder Spire combats | Intense, volcanic. Heavy percussion, brassy stabs. Fast tempo. |
| 6 | `MUSIC_COMBAT_CATACOMBS` | Sunken Catacombs combats | Gothic, undead. Pipe organ, choir pads, skeletal percussion. Minor key. |
| 7 | `MUSIC_COMBAT_CITADEL` | Sky Citadel combats | Aerial, holy. Choir, bells, harp glissandos. Major key, uplifting. |
| 8 | `MUSIC_BOSS_GREENWOOD` | Greenwood boss fight | Heroic escalation. Bigger orchestration, faster tempo. |
| 9 | `MUSIC_BOSS_VENOM` | Venom boss fight | Dark escalation. Deep brass, pounding drums, swamp ambience. |
| 10 | `MUSIC_BOSS_CINDER` | Cinder boss fight | Volcanic fury. Full orchestra, aggressive percussion, fire ambience. |
| 11 | `MUSIC_BOSS_CATACOMBS` | Catacombs boss fight | Gothic horror peak. Pipe organ full blast, choir chant, bell tolls. |
| 12 | `MUSIC_BOSS_CITADEL` | Citadel boss fight | Epic holy war. Full choir, trumpets, harp cascades, thunderous drums. |
| 13 | `MUSIC_SHOP` | Shop screen | Light, whimsical. Plucked strings, gentle percussion. Cozy mercantile. |
| 14 | `MUSIC_REST` | Rest site screen | Peaceful, restorative. Soft piano or harp, long sustains. |
| 15 | `MUSIC_EVENT` | Event screen | Curious, mysterious. Pizzicato strings, subtle tension. |
| 16 | `MUSIC_VICTORY` | Victory sting | Short fanfare, ~3-5 seconds. Triumphant chord resolution. |
| 17 | `MUSIC_DEFEAT` | Defeat sting | Short somber chord, ~3-5 seconds. Minor resolution. |

### Fallback Logic

If an area-specific music file is not found on disk, fall back to generic tracks:
- Any `MUSIC_COMBAT_*` missing → use `MUSIC_COMBAT_GREENWOOD` (generic combat)
- Any `MUSIC_BOSS_*` missing → use `MUSIC_BOSS_GREENWOOD` (generic boss)

The existing `MUSIC_COMBAT` and `MUSIC_BOSS` enums are removed in favor of this fallback.

---

## File Structure

```
assets/audio/
├── sfx/
│   ├── button_hover.wav
│   ├── button_click.wav DONE
│   ├── card_hover.wav
│   ├── card_play.wav
│   ├── card_discard.wav
│   ├── card_draw.wav
│   ├── damage.wav
│   ├── damage_heavy.wav
│   ├── heal.wav
│   ├── shield.wav
│   ├── taunt.wav
│   ├── interrupt.wav
│   ├── burn_tick.wav
│   ├── bleed_tick.wav
│   ├── party_downed.wav
│   ├── party_revived.wav
│   ├── enemy_cast_warning.wav
│   ├── boss_cast_warning.wav
│   ├── enemy_attack.wav
│   ├── gold_pickup.wav DONE
│   ├── reward_pickup.wav
│   ├── level_up.wav DONE
│   ├── victory.wav
│   ├── defeat.wav
│   ├── map_select.wav
│   ├── synergy_trigger.wav
│   ├── shop_purchase.wav
│   └── error.wav
└── music/
    ├── title.ogg DONE
    ├── map.ogg DONE
    ├── combat_greenwood.ogg DONE
    ├── combat_venom.ogg DONE
    ├── combat_cinder.ogg DONE
    ├── combat_catacombs.ogg
    ├── combat_citadel.ogg
    ├── boss_greenwood.ogg DONE
    ├── boss_venom.ogg DONE
    ├── boss_cinder.ogg DONE
    ├── boss_catacombs.ogg
    ├── boss_citadel.ogg
    ├── shop.ogg DONE
    ├── rest.ogg DONE
    ├── event.ogg DONE
    ├── victory.ogg DONE
    └── defeat.ogg DONE
```

---

## Wiring Plan — Every Trigger Point

### Music Triggers

| Trigger | File | Context |
|---------|------|---------|
| Title screen enters | `title_screen.c` | `assets_play_music(MUSIC_TITLE)` on init |
| Map screen enters | `map_screen.c` | `assets_play_music(MUSIC_MAP)` on init |
| Combat starts (normal) | `combat.c:combat_start()` | `assets_play_combat_music(area, false)` |
| Combat starts (boss) | `combat.c:combat_start()` | `assets_play_combat_music(area, true)` |
| Combat victory | `combat.c:check_victory()` | Stop combat → play `MUSIC_VICTORY` sting |
| Combat defeat | `combat.c:check_defeat()` | Stop combat → play `MUSIC_DEFEAT` sting |
| Shop opens | `shop_screen.c` | `assets_play_music(MUSIC_SHOP)` on init |
| Rest site opens | `rest_screen.c` | `assets_play_music(MUSIC_REST)` on init |
| Event opens | `event_screen.c` | `assets_play_music(MUSIC_EVENT)` on init |
| Return to map | `map_screen.c` | `assets_play_music(MUSIC_MAP)` on init |

Helper function in `assets.c`:
```c
void assets_play_combat_music(int area_id, bool is_boss) {
    GameMusic music;
    switch (area_id) {
        case 0: music = is_boss ? MUSIC_BOSS_GREENWOOD : MUSIC_COMBAT_GREENWOOD; break;
        case 1: music = is_boss ? MUSIC_BOSS_VENOM    : MUSIC_COMBAT_VENOM;    break;
        case 2: music = is_boss ? MUSIC_BOSS_CINDER   : MUSIC_COMBAT_CINDER;   break;
        case 3: music = is_boss ? MUSIC_BOSS_CATACOMBS: MUSIC_COMBAT_CATACOMBS; break;
        case 4: music = is_boss ? MUSIC_BOSS_CITADEL  : MUSIC_COMBAT_CITADEL;  break;
        default: music = is_boss ? MUSIC_BOSS_GREENWOOD : MUSIC_COMBAT_GREENWOOD;
    }
    assets_play_music(music);
}
```

### SFX Triggers — UI (`ui.c`)

| Sound | Trigger |
|-------|---------|
| `SFX_BUTTON_HOVER` | First frame `draw_btn_standard()` or `draw_btn_large()` detects hover on a new button |
| `SFX_BUTTON_CLICK` | `IsMouseButtonPressed(MOUSE_LEFT)` on a button |

Implementation: `ui.c` tracks `last_hovered_btn_id` statically. When hover changes to a new button, play hover SFX. The draw functions already receive hover state via color parameters — extend them to accept an optional `int btn_id` for hover tracking.

### SFX Triggers — Combat Cards (`combat.c`)

| Sound | Location |
|-------|----------|
| `SFX_CARD_HOVER` | `combat_update()` when `cs->hovered_card` changes to valid index |
| `SFX_CARD_PLAY` | `resolve_card_on_target()` at start of resolution |
| `SFX_CARD_DISCARD` | `advance_turn()` inside `deck_discard_hand(&cs->deck)` |
| `SFX_CARD_DRAW` | `advance_turn()` inside `deal_cards()` and in `combat_start()` inside `deal_opening_hand()` |
| `SFX_ENEMY_ATTACK` | `combat_spawn_enemy_card_throw()` when a throw is spawned |

### SFX Triggers — Damage/Heal/Shield (`combat.c`)

| Sound | Location |
|-------|----------|
| `SFX_DAMAGE` | `apply_damage_to_ally()` and `apply_damage_to_enemy()` — every hit |
| `SFX_DAMAGE_HEAVY` | Same functions when `cs->combo_scale > 1.1f` or crit flag |
| `SFX_HEAL` | `apply_heal_to_ally()` and `apply_heal_to_enemy()` — every heal |
| `SFX_SHIELD` | `apply_shield_to_ally()` and `apply_shield_to_enemy()` — every shield |

### SFX Triggers — Status/Class Effects (`combat.c`)

| Sound | Location |
|-------|----------|
| `SFX_BURN_TICK` | `advance_turn()` burn damage loop |
| `SFX_BLEED_TICK` | `advance_turn()` bleed damage loop |
| `SFX_TAUNT` | Where taunt status is applied |
| `SFX_INTERRUPT` | Where enemy `remaining_turns` is cleared by interrupt |
| `SFX_PARTY_DOWNED` | `apply_damage_to_ally()` when HP reaches 0 |
| `SFX_PARTY_REVIVED` | Where revive logic runs |
| `SFX_SYNERGY_TRIGGER` | When `cs->synergy_banner_timer` is first set (synergy/combo activation) |

### SFX Triggers — Enemy/Awareness (`combat.c`)

| Sound | Location |
|-------|----------|
| `SFX_ENEMY_CAST_WARNING` | `advance_turn()` block 3 — after picking new intent for non-boss enemy |
| `SFX_BOSS_CAST_WARNING` | `advance_turn()` block 3 — after picking new intent for boss enemy |
| `SFX_ENEMY_ATTACK` | `combat_spawn_enemy_card_throw()` — already listed above |

### SFX Triggers — Rewards/Progression

| Sound | Location | File |
|-------|----------|------|
| `SFX_GOLD_PICKUP` | `game_gain_gold()` or `check_victory()` | `game.c` / `combat.c` |
| `SFX_REWARD_PICKUP` | Reward screen opens | `reward_screen.c` |
| `SFX_LEVEL_UP` | Member levels up | `combat.c` or `game.c` at XP threshold |
| `SFX_SHOP_PURCHASE` | Shop item bought | `shop_screen.c` |
| `SFX_VICTORY` | `phase = COMBAT_VICTORY` | `combat.c:check_victory()` |
| `SFX_DEFEAT` | `phase = COMBAT_DEFEAT` | `combat.c:check_defeat()` |
| `SFX_MAP_SELECT` | Map node clicked | `map_screen.c` |
| `SFX_ERROR` | Can't afford / invalid target | `combat.c:handle_card_click()` / targeting |

---

## Button Hover One-Shot Logic (`ui.c`)

Since `draw_btn_standard()` runs every frame, hover SFX must only play once when hover begins:

```c
static int last_hovered_btn_id = -1;

// Inside draw_btn_standard/draw_btn_large, after hit-test:
bool hover = CheckCollisionPointRec(mouse, rect);
if (hover && btn_id != last_hovered_btn_id) {
    assets_play_sfx(SFX_BUTTON_HOVER);
    last_hovered_btn_id = btn_id;
}
if (!hover && btn_id == last_hovered_btn_id) {
    last_hovered_btn_id = -1;
}
```

Each button gets a unique `int btn_id` passed by callers (constant per button per screen). The draw functions' signatures are extended:
```c
void draw_btn_standard(Rectangle rect, Color base, Color hover, const char *text, int btn_id);
void draw_btn_large(Rectangle rect, Color base, Color hover, const char *text, int btn_id);
```

---

## Music Fade/Transition

For EA: simple stop + start. For v1.0: crossfade.

```c
void assets_play_music(GameMusic music) {
    // If same music already playing, do nothing
    if (audio_loaded && music_playing && current_music == music) return;

    // Stop current
    assets_stop_music();

    // Start new
    if (!music_loaded[music]) {
        // Fallback logic here
        if (music >= MUSIC_BOSS_GREENWOOD && music <= MUSIC_BOSS_CITADEL)
            music = MUSIC_BOSS_GREENWOOD;
        else if (music >= MUSIC_COMBAT_GREENWOOD && music <= MUSIC_COMBAT_CITADEL)
            music = MUSIC_COMBAT_GREENWOOD;
        if (!music_loaded[music]) return;
    }

    current_music = music;
    SetMusicVolume(music_stream, music_volume);
    PlayMusicStream(music_stream);
    music_playing = true;
}
```

---

## Implementation Order

| Step | What | Files |
|------|------|-------|
| 1 | Expand `GameSfx` and `GameMusic` enums with all new slots | `src/assets.h` |
| 2 | Update `sfx_files[]` and `music_files[]` with all new filenames | `src/assets.c` |
| 3 | Add `assets_play_combat_music(area, is_boss)` helper with area→track mapping | `src/assets.c` |
| 4 | Add fallback logic in `assets_play_music()` — area-specific missing → generic | `src/assets.c` |
| 5 | Extend `draw_btn_standard()` / `draw_btn_large()` with `int btn_id` param + hover SFX | `src/ui/ui.c`, `src/ui/ui.h` |
| 6 | Update all button call sites to pass unique btn_ids | All screen files |
| 7 | Wire music triggers: title, map, combat, shop, rest, event | `title_screen.c`, `map_screen.c`, `combat.c`, `shop_screen.c`, `rest_screen.c`, `event_screen.c` |
| 8 | Wire combat SFX: card play/draw/discard/hover, damage/heal/shield, enemy attack | `combat.c` |
| 9 | Wire status SFX: burn tick, bleed tick, taunt, interrupt | `combat.c` |
| 10 | Wire party SFX: downed, revived, level up | `combat.c` |
| 11 | Wire enemy SFX: cast warning, boss cast warning | `combat.c` |
| 12 | Wire reward SFX: gold, reward, victory, defeat, shop purchase, map select, error | `combat.c`, `shop_screen.c`, `reward_screen.c`, `map_screen.c`, `game.c` |
| 13 | Wire synergy SFX: synergy trigger | `combat.c` |
| 14 | Test with dummy/placeholder audio files (silence .wav / .ogg) | All |

---

## Estimated Audio File Count

| Category | Count |
|----------|-------|
| SFX (.wav) | 28 |
| Music (.ogg) | 17 |
| **Total** | **45 files** |

Roadmap P6 target: 20 SFX + 16 music = 36. This plan adds +8 SFX and +1 music beyond the roadmap for full coverage (map screen music, synergy trigger, map select, error, shop purchase, level up, bleed tick, heavy damage).

---

## Estimated Contractor Budget

| Item | Estimate |
|------|----------|
| 28 SFX (avg 3-5 sec each, ~2 min total) | $200-$500 |
| 17 music tracks (1-3 min each, ~35 min total) | $2,000-$5,000 |
| **Total** | **$2,200-$5,500** |

Matches roadmap estimate ($2,000-$5,000 for 60 min audio).
