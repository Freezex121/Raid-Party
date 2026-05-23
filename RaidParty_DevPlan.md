# Raid Party Dev Plan

Last updated: 2026-05-23

This plan tracks what the current Raylib prototype already has, what is only partially working, and what should be added next to make the game look and feel polished, readable, and professional.

## Current Direction

Raid Party is a party-based deckbuilding roguelite with MMO-style combat language: party frames, aggro, enemy casts, interrupts, healing, shields, class identity, map routing, shops, rests, and card rewards.

The current codebase is a playable prototype slice, not yet a production-quality vertical slice. The next milestone should be "Polished Vertical Slice v0.2": one complete run path that feels great moment-to-moment before adding more systems.

## Project Status Snapshot

### Done

- C99 + Raylib project builds through CMake.
- Main loop, 1280x720 window, fixed 60 FPS target, and game state routing exist.
- The game renders to a 640x360 virtual canvas and upscales 2x to a 1280x720 pixel-perfect window.
- Core screens exist: Title, Draft, Map, Combat/Run, Rest, Shop, Card Reward.
- Title and draft screens have basic tweened entrance/hover motion.
- Shared tween engine exists in `src/util/tween.c` and is called once per frame from `main.c`.
- Logging exists with category files under `logs/`.
- UI helpers exist for buttons, panels, labels, and wrapped text.
- Draft screen supports 6 classes: Guardian, Cleric, Mage, Rogue, Shaman, Ranger.
- Current run setup selects 3-5 party members based on meta-progression unlocks and builds a shared deck from their class cards.
- Party system supports HP, max HP, shields, aggro, alive/downed state, and status slots.
- Deck system supports shuffle, draw, hand, discard, exhaust, card upgrades, adding cards, and removing class cards when a party member goes down.
- Energy system supports max energy, spend, and per-turn refresh.
- Combat supports player turns, enemy turns, targeting enemies/allies, end turn, victory, defeat, card rewards, gold rewards, and multiple enemy slots.
- Run party state now persists across combats, so HP, alive/downed state, and rest healing matter across the run.
- The game starts with 3 party slots and the code supports a 5-member cap for future slot upgrades.
- Combat now receives full encounter compositions instead of only the first enemy.
- Enemy intent execution supports attack, tank buster, AOE, heal, shield, buff/shield, and wipe-style abilities.
- Interrupt cards cancel interruptible enemy casts and show feedback when a cast is interrupted or immune.
- Combat UI renders party frames, HP bars, shields, aggro text, enemy blocks, enemy HP, enemy intent cast bars, hand cards, energy, end turn, floating numbers, and screen shake.
- Enemy shields are displayed.
- Card data exists in C arrays: Guardian, Cleric, Mage, Rogue, Shaman, Ranger, plus utility cards.
- Implemented card effect categories include damage, healing, shield, taunt, revive, burn, renew, healing totem, trap damage reduction, channel cards, exhaust, all-enemy targeting, all-ally targeting, and simple combo scaling.
- Current P0 card audit is complete for known mismatches: interrupt, stealth, smoke bomb, multi-hit cards, Fortify, upgraded rewards, and trap timing now match their text.
- Enemy definitions exist for Training Dummy, Flame Imp, Rage Knight, Cult Healer, Berserker, Living Armor, Venom Stalker, and Arcane Wisp.
- Encounter definitions exist for 3 floors, including normal and elite pools.
- Fixed branching map exists for 3 floors with Start, Combat, Elite, Rest, Shop, and Boss nodes.
- Rest site supports heal or upgrade.
- Shop supports upgrade or remove with gold costs.
- Reward screen generates 3-4 card choices from selected party classes plus utility cards.
- Upgraded reward cards remain upgraded when added to the run deck.
- Game over/victory results screen exists with floor reached, bosses defeated, gold, deck size, and final party state.
- P1 combat feel pass is implemented: target previews, readable intent bars, party-frame smoothing/statuses/target highlight, hand entry/hover motion, disabled-card treatment, turn/action feedback, combo badge, recent action feed, and screen fades.
- P2 visual polish pass is implemented: shared assets/theme layer, textured tactical background, class/card/node color language, cost gems, effect badges, 64x96 full-card pixel-art template scaled 3x, class portraits, illustrated enemy stand-ins, map icon nodes, deck card previews, combat/reward VFX bursts, burn sparks, and boss danger flash.
- The 640x360 UI layout pass is implemented across all screens with shared layout helpers, larger cards, larger party/enemy/cast UI, combat side panels, card inspector panels, and overlap-aware hand hit-testing.
- P3 audio foundation is started: Raylib audio lifecycle is initialized, optional SFX/music slots load from `assets/audio`, and safe play/update/stop helpers exist.
- P4 data/content/balance pass is implemented: runtime content loads from JSON manifests under `assets/data`, card special behavior resolves through effect chains, content validation and balance-report scripts exist, and each floor has a named boss encounter.
- P5 run-depth pass is implemented: relics, event nodes, boss/event relic rewards, meta party-slot unlocks, and meta-progress persistence now exist.
- Area-based meta progression is being expanded: areas are JSON-defined, unlock sequentially, can run 3-5 floors, and feed a Renown-based meta shop.
- Map editing is now intended to be data-only: `tools/map_editor.html` can edit areas, area-specific floors, arbitrary node counts, and arbitrary connection counts for runtime JSON loading.

### Partially Done Or Risky

- Party growth starts at 3 slots, with 4th and 5th slots now intended as Renown shop purchases instead of automatic boss-count unlocks.
- Players can begin a run with any party size from 1 up to their current max; the draft UI warns when the party is not full.
- Runtime data now uses JSON as the source of truth for classes, cards, enemies, encounters, relic definitions, events, and map layouts. C still owns behavior enums and effect/relic hooks.
- Map layouts and encounter floor pools are dynamically allocated at runtime instead of capped to the old 5 floors / 14 nodes / 3 connections assumptions.
- Named boss identities and mechanics exist for all 3 floors. True multi-phase boss scripting remains future scope.
- Upgrade support affects damage/heal/shield numbers, but future special-effect upgrades still need a content/balance audit.
- Card text/effect audits should continue whenever new cards or effect types are added.
- Many visuals use immediate scale/position changes instead of the shared tween system.
- UI is now deliberately laid out for 640x360, but it still needs interactive visual QA across all screens after large layout changes.
- Some visuals are still primitive-shape stand-ins. There is no full sprite pipeline, no final custom font art pass beyond the current pixel font foundation, and no audio content pass.
- Meta-progress persistence exists, but full run save/resume is still future scope.
- Simple content validation and balance reporting exist. There are still no in-game automated tests.
- Build portability is limited because CMake references a hardcoded local Raylib path.

## Next Milestone: Polished Vertical Slice v0.2

Goal: One complete short run that feels cohesive, satisfying, and trustworthy. Keep scope small, but make every included interaction read clearly.

Recommended scope:

- Start with 3 party slots; meta-progress and the meta shop unlock slots 4 and 5.
- Multiple sequential areas, each 3-5 floors, using fixed floor templates and area difficulty scaling.
- 1 real boss per floor, even if each boss only has 2-3 polished mechanics.
- Hardcoded C data is acceptable for this milestone, but effects must match card text.
- Relics, events, and meta slot unlocks are now in scope for the vertical slice, while full run save/resume remains future scope.

Exit criteria:

- A player can Draft -> Map -> Combat -> Reward -> Rest/Shop -> Boss -> next floor -> final victory or defeat.
- Every visible card does exactly what its text says.
- Every enemy cast gives the player enough information to make a tactical choice.
- Damage, healing, shields, aggro, interrupts, deaths, and rewards all have clear visual/audio feedback.
- No screen looks like a debug placeholder.
- Text does not overlap or clip at 1280x720.
- All hitboxes match their 640x360 visual layout, especially overlapped hand cards and map/deck selections.
- The game can be played without reading logs.

## Priority Work

### P0 - Make The Current Game Correct (Complete)

- [x] Choose party size direction: start with 3 party slots and support growth to 5 through future upgrades.
- [x] Implement interrupt resolution for Rogue Kick and Shaman Silence.
- [x] Make uninterruptible/wipe casts reject interrupts with feedback.
- [x] Complete current enemy ability execution: attack, tank buster, AOE, heal, shield, buff/shield, and wipe-style actions.
- [x] Wire full encounter compositions through map -> run -> combat.
- [x] Make party HP/alive state persistent through a run.
- [x] Make rest healing affect the persistent run party.
- [x] Preserve upgraded reward cards when they are added to the deck.
- [x] Audit and fix known current card text/behavior mismatches.
- [x] Add proper victory/defeat result screen.

### P1 - Make Combat Feel Great (Complete)

- [x] Add target preview before committing a card:
  - [x] Show projected damage/heal/shield on hovered target.
  - [x] Show aggro/effect preview where relevant.
  - [x] Reject and label invalid downed-ally targets unless the card is Revive.
- [x] Improve enemy intent readability:
  - [x] Use compact labels for attack, AOE, heal, shield, tank buster, buff, interruptible, and locked casts.
  - [x] Color-code danger and support intents.
  - [x] Add hover detail with exact damage/heal/shield and interruptibility.
- [x] Improve party frames:
  - [x] Smooth HP/shield changes instead of snapping.
  - [x] Highlight the current aggro target.
  - [x] Show statuses as compact labels with turn counts.
  - [x] Make downed state more dramatic and readable.
- [x] Improve hand feel:
  - [x] Cards deal into hand with staggered motion.
  - [x] Hover lift/scale eases instead of snapping.
  - [x] Played cards create a short target flash.
  - [x] Unplayable cards are visually disabled when energy is too low.
- [x] Improve turn feedback:
  - [x] Add player-turn banner, enemy-action banner, and end-turn flash.
  - [x] Show Combo as a proper badge with readable scaling.
  - [x] Add a short recent-action feed.
- [x] Add screen transition manager:
  - [x] Fade out current screen.
  - [x] Switch state while hidden.
  - [x] Fade in next screen.
  - [x] Use it for every `game_change_screen` transition.

### P2 - Make The Game Look Professional (Complete)

- [x] Define an art direction pass:
  - [x] Dark tactical fantasy table, crisp readable UI, colorful class accents.
  - [x] Replace the flattest placeholder areas with texture, depth, icons, and consistent spacing.
- [x] Create a UI style guide in code:
  - [x] Centralized class, card type, node, and effect colors in `src/ui/theme.c`.
  - [x] Shared helpers for class portraits, cost gems, effect badges, card art panels, and background treatment.
- [x] Add asset management:
  - [x] `src/assets.c/.h` loads/unloads shared visual resources.
  - [x] Generated texture fallback avoids missing-file failures.
  - [x] Centralized unload path from `main.c`.
- [x] Add font foundation:
  - [x] Shared default font is routed through the assets layer.
  - [x] Custom font files remain a future art-content upgrade.
- [x] Add card art/icon treatment:
  - [x] Class-colored frames.
  - [x] Card type badges.
  - [x] Cost gems.
  - [x] 64x96 full-card source template with point-filtered 3x upscaling.
  - [x] Effect badges for primary card behavior.
  - [x] Upgrade star/border treatment.
- [x] Add enemy and party visuals:
  - [x] Replace rectangle enemies with illustrated primitive stand-ins.
  - [x] Add class portrait icons to party frames and draft cards.
  - [x] Add enemy idle/breathing animation.
- [x] Add VFX:
  - [x] Damage impact bursts.
  - [x] Heal glow bursts.
  - [x] Shield shimmer bursts.
  - [x] Burn tick sparks.
  - [x] Interrupt snap burst.
  - [x] Boss/tank-buster danger flash.
  - [x] Reward reveal sparkle.
- [x] Improve map presentation:
  - [x] Use node icons instead of text-first circles.
  - [x] Animate hovered available nodes.
  - [x] Clearly distinguish available, completed, locked, elite, shop, rest, and boss nodes.
  - [x] Show floor title and route prompt.
- [x] Rework the UI for the 640x360 virtual canvas:
  - [x] Add shared layout helpers for combat, deck browser, and reward geometry.
  - [x] Resize party frames, enemies, cast bars, cards, buttons, and deck/reward cards.
  - [x] Add combat side panels for action feed and persistent card inspection.
  - [x] Make the bottom hand lane overlap-aware for large hands.
  - [x] Resize title, draft, map, reward, rest, shop, and game-over screens.
  - [x] Keep compact card tokens on-card and move full explanations into inspector panels.

### P3 - Add Audio Polish

- [x] Add audio asset loader:
  - [x] Initialize/shutdown Raylib audio from the main lifecycle.
  - [x] Define stable SFX/music slots and asset path conventions.
  - [x] Load optional files without failing when placeholders are missing.
  - [x] Add safe play/update/stop helpers for future SFX/music wiring.
- Add SFX for:
  - Button hover/click.
  - Card hover/play/discard/exhaust.
  - Damage, heal, shield, taunt, interrupt, burn tick.
  - Party member downed/revived.
  - Enemy cast warning and boss cast warning.
  - Gold/reward pickup.
- Add music:
  - Title/menu loop.
  - Normal combat loop.
  - Boss combat loop.
  - Victory/defeat stingers.
- Add volume settings for master/music/SFX.

### P4 - Data, Content, And Balance

- [x] Move content into JSON manifests for runtime loading, validation, and balancing:
  - [x] `assets/data/classes.json`
  - [x] `assets/data/cards.json`
  - [x] `assets/data/enemies.json`
  - [x] `assets/data/encounters.json`
  - [x] `assets/data/relics.json`
  - [x] `assets/data/events.json`
  - [x] `assets/data/maps.json`
- [x] Create a card effect resolver that supports effect chains instead of one-off card-ID checks.
- [x] Add a simple content validation step:
  - [x] No missing names/descriptions.
  - [x] All effects are supported.
  - [x] Costs and target types are valid.
  - [x] Card effect metadata is checked for obvious mismatches.
- [x] Build a balance report script:
  - [x] Average damage per energy.
  - [x] Average healing per energy.
  - [x] Expected incoming damage per floor.
  - [x] Boss time-to-kill estimates.
- [x] Add real bosses:
  - [x] Floor 1 boss: Ember Overseer teaches interrupt or AOE mitigation.
  - [x] Floor 2 boss: Iron Bulwark tests aggro and healing pressure.
  - [x] Floor 3 boss: Void Conductor combines channel, adds, and enrage.

### P5 - Run Depth Systems

- [x] Add relic system after the combat loop is satisfying:
  - [x] Passive modifiers at combat start.
  - [x] Clear relic tray UI.
  - [x] Relic reward sources from bosses and events.
- [x] Add event nodes:
  - [x] Short choices with HP, gold, upgrade, remove, curse, or card outcomes.
  - [x] Keep event UI stylish but compact.
- [x] Add meta-progression:
  - [x] Unlock party slot 4 and party slot 5 through meta shop purchases.
- [x] Add sequential area unlocks with area difficulty scaling.
- [x] Add Renown rewards from runs.
- [x] Add a meta shop for party slots and starting-run support.
- [x] Allow under-full party drafts, including solo attempts.
- [x] Add data-driven map editing support:
  - [x] Runtime maps support arbitrary node counts and connection counts.
  - [x] Runtime maps support optional area-specific floors with shared floor fallbacks.
  - [x] The map screen scrolls/pans for large map layouts.
  - [x] `tools/map_editor.html` edits areas, floors, nodes, positions, and connections without code changes.
  - [x] Avoid raw stat inflation until balance is proven.
  - [ ] Future: class unlocks, alternate starting cards, cosmetics, and difficulty modifiers.
- [x] Add persistence:
  - [x] Save meta-progress.
  - [ ] Optional run save/resume.

### P6 - Quality, Testing, And Release

- Add lightweight tests for pure systems:
  - Deck shuffle/draw/discard/exhaust.
  - Card effect resolution.
  - Enemy ability execution.
  - Map unlock rules.
  - Reward generation.
- Add debug tools:
  - Toggle hitboxes.
  - Force win/lose combat.
  - Spawn specific encounter.
  - Grant gold.
  - Show deck/discard/exhaust piles.
- Add release polish:
  - Portable CMake Raylib discovery instead of hardcoded `C:/raylib/raylib`.
  - App icon.
  - Version display.
  - Crash/log folder cleanup.
  - Release build instructions.

## Professional Feel Checklist

Use this as a pass before calling any milestone "done."

- First screen has a strong identity: logo/title, readable subtitle, responsive Start button, music/SFX, no placeholder text.
- Draft screen makes class roles obvious through color, icons, short tooltips, and deck preview.
- Combat has clear visual hierarchy: party state, enemy intent, hand, energy, and end-turn action are instantly findable.
- Every important action has anticipation, impact, and recovery:
  - Anticipation: hover/preview/target highlight.
  - Impact: animation, VFX, number popup, sound.
  - Recovery: card leaves hand, state settles, next choice is clear.
- Enemy casts create tension without confusion.
- UI uses consistent spacing, typography, colors, and icon language.
- No important text relies on tiny default font.
- Buttons and cards have disabled, hover, pressed, selected, and unavailable states.
- Screen transitions hide loading/state changes.
- Rewards feel celebratory.
- Defeat feels fair and informative.
- Victory feels complete, not like a return to debug state.
- Logs are available for development but never required to understand gameplay.

## Current Architecture Notes

### Actual Core Files

```text
src/main.c                 - Raylib setup, loop, screen update/draw, tween update
src/game.c/.h              - Global game state and screen changes
src/assets.c/.h            - Shared visual/audio asset loading, playback helpers, and unload path
src/util/json.c/.h         - Lightweight runtime JSON parser
src/data/content_loader.c/.h - Loads runtime JSON data before game init
src/combat/combat.c/.h     - Combat state, card resolution, turns, enemies
src/combat/status.c/.h     - Status apply/tick/clear
src/systems/deck.c/.h      - Shared deck and card instances
src/systems/party.c/.h     - Party classes, HP, aggro helpers
src/systems/energy.c/.h    - Energy state
src/systems/map.c/.h       - Fixed map layouts and node unlocks
src/systems/relic.c/.h     - Run relic definitions and reward choice helpers
src/systems/meta.c/.h      - Meta-progress load/save, Renown, area unlocks, and shop upgrades
src/data/area_defs.c/.h    - Runtime-loaded area definitions, floor counts, and difficulty scaling
src/data/card_defs.c/.h    - Runtime-loaded card definitions and card lookup helpers
src/data/enemy_defs.c/.h   - Runtime-loaded enemy definitions
src/data/encounter_defs.c/.h - Runtime-loaded encounter pools
src/data/event_defs.c/.h   - Runtime-loaded event definitions
assets/data/*.json         - Runtime content manifests plus validation/balance inputs
scripts/validate_content.py - JSON content validation
scripts/balance_report.py  - Lightweight balance report
src/ui/*                   - Buttons, cards, party frames, enemies, cast bars, floating text
src/ui/layout.c/.h         - Shared 640x360 UI geometry and hitbox helpers
src/screens/*              - Title, draft, map, run, rest, shop, event, reward, relic reward
```

### Important Design Decision

The original plan targeted 4 party members. The current implementation starts at 3 selected party members, then unlocks 4th/5th slots through meta-progress.

Decision: keep 3 party members as the default first-run experience because it lowers UI density and balance complexity. Treat 4-5 member runs as meta-progression rewards and keep testing the 640x360 combat layout when those slots unlock.

## MVP Scope Update

The old MVP said "no shop," but the current prototype already has shop and rest screens. The new MVP should not remove them; it should polish the existing loop.

MVP v0.2 includes:

- 1-3 person starting party draft from 6 available classes, with meta shop unlocks up to 5 party slots.
- Sequential 3/4/5-floor areas with fixed floor templates.
- Normal, elite, rest, shop, event, boss, and reward nodes.
- Fully functioning combat card text/effects.
- Fully functioning enemy intents.
- Run-persistent party HP.
- Card rewards, upgrades, removes, and gold.
- Relics from bosses and events.
- Meta-progress save file for run count, wins, bosses defeated, best floor, and party-slot unlocks.
- Proper title, victory, and defeat screens.
- Cohesive UI art direction, icons, VFX, and SFX for the included systems.

MVP v0.2 excludes:

- Full run save/resume.
- Large card pool expansion.
- Procedural maps.
- Runtime JSON loader/codegen, unless hardcoded data starts slowing iteration.
