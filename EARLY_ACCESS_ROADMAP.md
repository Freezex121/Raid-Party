# Early Access Roadmap: Raid Paper Legends

This document keeps the long-term vision intact, but organizes it into priority phases that can be followed step by step. The goal is to avoid treating every good idea as an immediate requirement. Steam success depends first on a strong first impression, a readable core loop, and a trustworthy demo/EA build.

## How To Use This Roadmap

- Finish phases in order unless a later task is cheap and unblocks an earlier one.
- Do not expand content before the current loop feels clear, stable, and fun.
- Treat art/audio/UX as launch-critical, not decoration.
- Keep all long-term ideas, but only promote them into the current milestone when they support the next public release.
- Every phase should end with a playable build and a short test pass.

## Current Content Snapshot

| Category | Count | Demo Target | EA Target | v1.0 Target |
|----------|-------|-------------|-----------|-------------|
| Areas | 3 | 3 polished | 4-5 | 7 |
| Cards | 86 | 86-100 balanced | 100-180 | 280+ |
| Enemies | 36 | 36 polished | 60 | 90+ |
| Encounters | 97 | 97 tuned | 160 | 240+ |
| Events | 28 | 28 polished | 45 | 65+ |
| Relics | 51 | 51 polished | 80 | 110+ |
| Classes | 9 | 9 | 9 | 9+ optional future class |
| Texture PNGs | 23 mostly placeholder | key visible assets final | 80+ final | 150+ final |
| Audio files | 0 | 10-15 minimum | 30+ | 60+ |

## Priority Gate Summary

| Gate | Target | Purpose |
|------|--------|---------|
| P0 | Internal Stability | Make the current game reliable enough to evaluate honestly. |
| P1 | First-10-Minute Polish | Make a new player understand and enjoy the first session. |
| P2 | Steam Page / Wishlist Build | Create screenshots, trailer footage, capsule direction, and a public-facing demo candidate. |
| P3 | Public Demo | Ship a limited, polished demo to collect feedback and wishlists. |
| P4 | Early Access Content Expansion | Add areas, enemies, events, relics, and selected cards after the core loop proves itself. |
| P5 | Early Access Launch | Launch v0.1 at $9.99 with clear known issues and roadmap. |
| P6 | EA Updates | Add New Game+, Forge systems, Raid Mode, and community-requested improvements. |
| P7 | v1.0 | Finish final areas, art, localization, achievements, controller support, and $14.99 launch. |

---

## P0: Internal Stability And Build Hygiene

**Priority:** Immediate  
**Target duration:** 1-2 weeks  
**Rule:** No major new content until this phase is complete.

### Goals

- Make the current 3-area game stable from title screen to game over.
- Confirm JSON-only content loading works consistently in dev and standalone builds.
- Reduce obvious UI overflow, unreadable text, bad hitboxes, and map navigation problems.
- Make sure every run can be completed or lost without broken state.

### Required Work

- Run content validation after any data change.
- Build Debug and Release after any systems change.
- Keep `assets/data` and `Builds/Standalone/assets/data` in sync until the build pipeline copies data automatically.
- Audit all screens for text spill, broken buttons, invisible disabled states, and unreadable tooltips.
- Confirm settings work: fullscreen, integer scale 1x-4x, master/music/SFX volume.
- Confirm gold, meta progression, ascension, leveling, relics, and map progression do not corrupt saves or run state.

### Definition Of Done

- `python scripts/validate_content.py` passes.
- `cmake --build build --config Debug --target RaidParty` passes.
- `cmake --build build --config Release --target RaidParty` passes.
- A full run can be played through all current areas without a blocker.
- A new player can identify cards, targets, energy, enemy intent, rewards, and map choices.

---

## P1: First-10-Minute Polish

**Priority:** Highest public-facing priority  
**Target duration:** 2-4 weeks  
**Rule:** This phase matters more than adding cards.

### Goals

- Make the first session feel intentional, readable, and fun.
- Improve the emotional "I get it" moment before asking players to learn deeper systems.
- Prepare the game for screenshots and trailer capture.

### Core UX Tasks

- Add or improve tutorial/onboarding for:
  - Energy and card costs.
  - Enemy intent and cast bars.
  - Targeting allies/enemies.
  - Shield, HP, aggro, marks, passives, statuses, and class synergies.
  - Map movement and no-backtracking rules.
  - Leveling, XP cap, and level-up choices.
- Make the first combat intentionally easy and explanatory.
- Make the first reward moment feel good: card reward, relic reward, gold reward, or level-up.
- Make party member frames readable with hover details instead of cramming every stat into the banner.
- Make shop/rest/event choices clearly show cost, affordability, and result.

### Visual Polish Tasks

- Finalize card templates and upgraded/maxed templates.
- Finalize node sprites enough for the map to look deliberate.
- Finalize class icons enough for portraits and cards to feel readable.
- Add at least one finished combat background for the first area.
- Improve title screen and map presentation for screenshots.

### Audio Minimum For This Phase

Use temporary-but-pleasant audio if needed.

| SFX | Count | Notes |
|-----|-------|-------|
| Button hover/click | 2 | Already referenced in code. |
| Card hover/play/discard | 3 | Core tactile feedback. |
| Damage/heal/shield | 3 | Basic combat readability. |
| Victory/Defeat | 2 | Run feedback. |
| Level up / Gold pickup | 2 | Reward feedback. |
| **Minimum Total** | **12** | Enough for a demo candidate. |

### Definition Of Done

- A new player can finish the first floor without external explanation.
- Screenshots from combat, map, reward, shop, and title screen look intentional.
- No screen in the first 10 minutes looks like debug UI.
- Placeholder art may remain, but it must be coherent and not distracting.

---

## P2: Steam Page And Wishlist Build

**Priority:** Start before EA; does not require EA launch readiness  
**Target duration:** 2-4 weeks after P1

### Goals

- Create a Steam page early enough to collect wishlists.
- Create a stable build suitable for trailer capture and private testing.
- Do not wait for all EA content before making the store page.

### Steamworks / Store Page Tasks

**CAPI/Steam SDK:**
- Link raylib build with Steamworks SDK.
- Initialize Steam on launch, gracefully skip if not running.
- Implement Steam overlay support.
- Implement cloud saves for `RaidParty_Meta.sav`.
- Implement achievements later in this phase or in P3.
- Defer leaderboards until EA or post-EA unless they become easy.

**Store page assets:**
- Capsule art:
  - 616x353
  - 374x448
  - 1920x620
- 5 screenshots showing:
  - Combat
  - Map
  - Shop
  - Rest site
  - Relic reward
- Trailer:
  - 30-60s gameplay loop.
  - Must show card play, enemy intents, map choice, reward, and class identity.
- Short description and feature list.

### Settings And Options

| Feature | Priority | Implementation |
|---------|----------|----------------|
| Windowed/Fullscreen toggle | Required | raylib `ToggleFullscreen()` |
| Resolution / scale options | Required | Integer scale of 640x360; no ratio distortion |
| Volume sliders | Required | Master, SFX, Music |
| Controller support | Later EA | raylib gamepad API, menu navigation, combat card selection |
| Key rebinding | Optional | Low priority |

### Definition Of Done

- Steam page assets exist in correct sizes.
- Trailer footage can be captured without avoiding broken-looking screens.
- Private testers can play the build without a developer standing over them.

---

## P3: Public Demo

**Priority:** Best next public milestone  
**Target duration:** 1-2 months after P2

### Demo Scope

- Keep the demo focused on the best current content.
- Use the current 3 areas only if all 3 are polished enough.
- It is acceptable to cap the demo at 1-2 areas if that produces a stronger experience.
- Keep all 9 classes available if draft flow explains party size and class role well.
- Keep the current card pool unless balance demands temporary cuts.
- Keep meta progression limited or reset-friendly so players understand the full game will expand.

### Demo Content Targets

| Category | Demo Target |
|----------|-------------|
| Areas | 1-3 polished |
| Cards | 86-100 balanced |
| Enemies | Current enemies tuned |
| Events | Current 28 polished |
| Relics | Current 51 polished |
| Audio | 10-15 minimum files |
| Art | Key UI, cards, nodes, class icons, first background |

### Demo Feedback Targets

Collect feedback on:
- First-run comprehension.
- Combat readability.
- Card clarity.
- Enemy cast bar clarity.
- Shop/rest/event clarity.
- Level-up reward excitement.
- Run length.
- Balance spikes.
- Which classes feel fun or confusing.

### Definition Of Done

- Demo has no known blockers.
- Demo has at least one complete, satisfying run arc.
- Store page is live or ready.
- Feedback channel exists: Discord, Steam forum, itch page, or form.

---

## P4: Early Access Content Expansion

**Priority:** After demo feedback  
**Target duration:** 3-6 months  
**Rule:** Add content where players feel repetition first.

### EA Content Targets

| Category | Current | EA Target |
|----------|---------|-----------|
| Areas | 3 | 4-5 |
| Cards | 86 | 100-180 |
| Enemies | 36 | 60 |
| Encounters | 97 | 160 |
| Events | 28 | 45 |
| Relics | 51 | 80 |
| Classes | 9 | 9 |
| Texture PNGs | 23 mostly placeholder | 80+ final |
| Audio files | 0 | 30+ |

### New Areas

**Area 4: Sunken Catacombs**  
Planned floors: 6  
Difficulty: 165%

- Theme: Undead/crypt, skeletal warriors, necromancers, bone constructs.
- New mechanic: **Darkness**. Every 3 turns all enemies gain +X damage until a light status is applied.
- 10 new enemies: skeleton, wraith, bone golem, lich, etc.
- 5 floor layouts.
- Boss: Lich King, summons adds and uses Darkness aura.

**Area 5: Sky Citadel**  
Planned floors: 6  
Difficulty: 185%

- Theme: Aerial/holy, winged warriors, storm elementals, celestial beings.
- New mechanic: **Aegis**. Enemies gain temporary invulnerability that must be stripped.
- 10 new enemies: storm herald, seraph, aegis guard, tempest, etc.
- 5 floor layouts.
- Boss: Storm Titan, multi-phase shield/storm/berserk fight.

### Cards

The long-term EA target can reach 180 cards, but do not add all of these before balance is stable. Add cards in small batches and test them.

| Class | Current | New | Target Total | Design Direction |
|-------|---------|-----|--------------|------------------|
| Guardian | 9 | +10 | 19 | More reactive tank tools, damage reflection |
| Cleric | 8 | +10 | 18 | AoE heals, cleanse effects |
| Mage | 9 | +10 | 19 | More elemental variety: ice, arcane |
| Rogue | 8 | +10 | 18 | Combo finishers, poisons |
| Shaman | 9 | +10 | 19 | Totem synergies, weather effects |
| Ranger | 9 | +10 | 19 | Beast companions, traps |
| Paladin | 8 | +10 | 18 | Auras, holy damage-over-time |
| Warlock | 9 | +10 | 19 | Demon summoning, self-damage payoff |
| Bard | 8 | +10 | 18 | Song stacking, crescendo effects |
| Utility | 8 | +14 | 22 | Multi-class cards, neutral synergies |
| **Total** | **86** | **+104** | **190** | |

**New card mechanics to introduce:**
- **Combo counters:** cards that get stronger as you play other cards first, especially Rogue and Bard.
- **Auras:** persistent per-class effects that cost energy to maintain, especially Paladin.
- **Pacts:** Warlock cards that give a strong effect now but a debuff later.
- **Dual-class cards:** cards that require two specific classes in the party to activate bonus effects.
- **Reactions:** cards that trigger when enemies do specific actions, especially Guardian and Rogue.

### Enemies

| Area | Current | New | Total |
|------|---------|-----|-------|
| Greenwood Breach | 6 | +2 | 8 |
| Venom Mire | 10 | +2 | 12 |
| Cinder Spire | 11 | +3 | 14 |
| Sunken Catacombs | 0 | +10 | 10 |
| Sky Citadel | 0 | +10 | 10 |
| Cross-area / elites | 9 | +6 | 15 |
| **Total** | **36** | **+33** | **69** |

**New enemy ability types:**
- **Mark/Stack:** applies a stacking marker to a party member; at X stacks, triggers a big hit.
- **Link:** two enemies share HP; damage to one splashes to the other.
- **Phase gate:** boss enters a shielded phase; must kill adds to drop the shield.
- **Tether:** pulls the lowest-HP ally to the front, forcing aggro management.

### Relics

| Rarity | Current | New | Total |
|--------|---------|-----|-------|
| Common, rarity 1 | 16 | +10 | 26 |
| Uncommon, rarity 2 | 21 | +10 | 31 |
| Rare, rarity 3 | 14 | +9 | 23 |
| **Total** | **51** | **+29** | **80** |

**Design space for new relics:**
- Card-type synergies: "Each time you play an Attack, gain 1 Shield."
- Class-specific relics: "Guardian cards cost 1 less energy the first time each turn."
- Event-triggered relics: "When you enter a Rest Site, heal 3 additional HP."
- Curse mitigation: "Curse cards in your deck have -1 energy cost."
- Risk-reward: "At combat start, lose 5 HP. Enemy drops +10 gold."

### Events

| Type | Current | New | Total |
|------|---------|-----|-------|
| Gold/Curse tradeoffs | 7 | +5 | 12 |
| Card rewards/purchases | 10 | +5 | 15 |
| Relic trades | 7 | +5 | 12 |
| HP/stat tradeoffs | 4 | +2 | 6 |
| **Total** | **28** | **+17** | **45** |

### Definition Of Done

- Each new area has unique enemies, boss identity, music/background direction, and at least one mechanic players can name.
- New cards are added in small tested batches.
- Events and relics create memorable decisions, not just bland numeric trades.
- Normal enemies do not overuse devastating effects like energy drain.

---

## P5: Early Access Launch Hardening

**Priority:** Final gate before paid EA  
**Target duration:** 4-6 weeks

### Required Checklist

- Steam page live with screenshots and trailer.
- Demo feedback reviewed and major issues addressed.
- Known bugs documented.
- Roadmap visible in-game or linked clearly from Steam.
- Discord/community channel set up.
- Save compatibility policy decided.
- Crash/blocker test pass complete.
- Controller support either implemented or clearly not promised yet.
- Audio has enough coverage that the game does not feel silent.
- Placeholder art is either replaced or visually coherent enough to look intentional.

### Steamworks For EA

- Achievements: 10 existing + 10 new.
- Cloud saves: `RaidParty_Meta.sav`.
- Steam overlay.
- Leaderboards:
  - Fastest clear per area.
  - Lowest deaths.
  - Highest score.

Leaderboards can be moved to P6 if they delay launch.

### EA Pricing

- EA price: **$9.99 USD**.
- First week discount: **-15%**.
- Planned v1.0 price: **$14.99 USD**.

### Definition Of Done

- A paying player gets a complete, honest, replayable experience.
- Missing content is framed as future expansion, not missing basics.
- The game has enough polish that negative reviews are not mainly about readability, silence, broken UI, or confusion.

---

## P6: Art And Audio Production Track

**Priority:** Runs alongside P1-P5  
**Target duration:** Month 1-8 and beyond

This phase is separated because art/audio can be worked on in parallel. It should not block all coding, but enough of it must be complete before public milestones.

### Audio Target

**Minimum viable audio: 20 SFX + 16 music files**

| SFX | Count | Notes |
|-----|-------|-------|
| Button hover/click | 2 | Already referenced in code |
| Card hover/play/discard | 3 | Already referenced |
| Damage/heal/shield | 3 | Generic hits and heals |
| Enemy attack/cast | 2 | Different from player damage |
| Burn tick / Bleed tick | 2 | Status effect ticks |
| Victory/Defeat | 2 | Run end jingles |
| Level up / Gold pickup | 2 | Reward feedback |
| Taunt / Interrupt | 2 | Class-specific feedback |
| Boss warning | 2 | Already referenced |
| **Total** | **20** | |

| Music | Count | Notes |
|-------|-------|-------|
| Title theme | 1 | Already referenced as `title.ogg` |
| Combat generic | 1 | Already referenced as `combat.ogg` |
| Boss theme | 1 | Already referenced as `boss.ogg` |
| Greenwood Breach | 2 | Per-area ambient combat |
| Venom Mire | 2 | Swamp/dark theme |
| Cinder Spire | 2 | Volcanic intense theme |
| New area 1 theme | 2 | |
| New area 2 theme | 2 | |
| Shop / Rest / Event | 3 | Calm interlude tracks |
| **Total** | **16** | |

### Complete Sprite List

**Node sprites**

| File | Size | Status |
|------|------|--------|
| node_combat.png | 32x32 | Placeholder -> final |
| node_elite.png | 32x32 | Placeholder -> final |
| node_rest.png | 32x32 | Placeholder -> final |
| node_shop.png | 32x32 | Placeholder -> final |
| node_event.png | 32x32 | Placeholder -> final |
| node_boss.png | 48x48 | Placeholder -> final |

**Class icons**

| File | Status |
|------|--------|
| guardian_icon.png | Placeholder -> final shield icon |
| cleric_icon.png | Placeholder -> final cross/holy icon |
| mage_icon.png | Placeholder -> final staff/spark icon |
| rogue_icon.png | Placeholder -> final dagger icon |
| shaman_icon.png | Placeholder -> final totem/lightning icon |
| ranger_icon.png | Placeholder -> final bow icon |
| paladin_icon.png | Placeholder -> final winged sword icon |
| warlock_icon.png | Placeholder -> final skull/eye icon |
| bard_icon.png | Placeholder -> final lyre icon |

**Card templates**

| File | Notes |
|------|-------|
| card_template.png | Base card frame |
| card_template_upgraded.png | Level 1 upgrade frame |
| card_template_maxed.png | Level 2 upgrade frame |

**Card art**

Each card needs a 96x120 pixel art illustration. This is the largest art task.

| Batch | Cards | Timeline |
|-------|-------|----------|
| Guardian, Cleric, Mage | 26 | Month 5 |
| Rogue, Shaman, Ranger | 26 | Month 5-6 |
| Paladin, Warlock, Bard | 25 | Month 6 |
| Utility + Curse | 9 | Month 6 |

**Relic icons**

Each relic needs a 16x16 pixel art icon.

| Batch | Relics | Timeline |
|-------|--------|----------|
| Common, rarity 1 | 16 | Month 6 |
| Uncommon, rarity 2 | 21 | Month 6-7 |
| Rare, rarity 3 | 14 | Month 7 |

**Enemy sprites**

- Each enemy needs a full-body pixel art portrait for combat.
- Size: about 64x64 to 128x128 depending on enemy scale.
- Areas 1-3: 36 enemies, Month 6-7.
- Areas 4-5: 20+ enemies, Month 7-8.
- Elite/boss variants should use larger versions or glow effects.

**Backgrounds**

| File | Notes |
|------|-------|
| bg_title.png | Title screen background |
| bg_combat_greenwood.png | Combat backdrop area 1 |
| bg_combat_venom.png | Combat backdrop area 2 |
| bg_combat_cinder.png | Combat backdrop area 3 |
| bg_combat_catacombs.png | Combat backdrop area 4 |
| bg_combat_citadel.png | Combat backdrop area 5 |
| bg_shop.png | Shop screen background |
| bg_rest.png | Rest site background |
| bg_map.png | Map screen parchment background |
| bg_event.png | Generic event background |

**UI elements**

- Buttons: primary, secondary, disabled, hover, pressed.
- Panel backgrounds: dark, light, gold.
- Icons: energy gem, gold, renown, reroll.
- Status effect icons: Burning, Bleed, Renew, MARKED, CONDUCTIVE, BLIGHT, Trap, Weakness, Energy Drain, Totem.
- Bars: HP bar, Shield bar, XP bar.

### Total Art Asset Estimate

| Category | Count |
|----------|-------|
| Node sprites | 6 |
| Class icons | 9 |
| Card templates | 3 |
| Card illustrations | 86+ |
| Relic icons | 51 |
| Enemy sprites | 60 |
| Backgrounds | 10 |
| UI elements | 25+ |
| **Total** | **About 250** |

---

## P7: Early Access v0.1 Launch

**Priority:** Paid launch  
**Target timing:** After P5 is complete

### Launch Checklist

- Steam page live.
- Trailer and screenshots reflect the current build.
- EA roadmap visible.
- Known bugs documented.
- Community channel active.
- Save files tested.
- Achievements/cloud either active or clearly scheduled.
- First-week discount configured.
- Press kit folder prepared:
  - Capsule art.
  - Screenshots.
  - Trailer link.
  - Short description.
  - Feature list.
  - Contact info.

### EA Promise

At launch, the game should promise:
- A complete run structure.
- Meaningful class/party experimentation.
- Replayable map/combat/reward loop.
- Active development toward more areas, enemies, art, audio, and long-term modes.

It should not promise:
- Final art everywhere.
- Perfect balance.
- Localization.
- Trading cards.
- Every future mode.

---

## P8: Post-Launch Early Access Updates

These updates stay in the roadmap, but they should not block EA launch.

### Update 1: New Game+

**Original target:** Month 10

- Ascension 11-20 with new modifiers.
- Golden frames for completing ASC 20.
- New achievement set.

### Update 2: The Forge

**Original target:** Month 12

- Card crafting system: sacrifice cards to upgrade others.
- New relic type: Forge relics, upgradeable mid-run.
- Mini-boss encounter type: appears mid-floor and drops crafting material.

### Update 3: Raid Mode

**Original target:** Month 14

- Endless mode: procedural floors with increasing difficulty.
- Leaderboard integration.
- Weekly challenges with seeded runs.

---

## P9: v1.0 Launch

**Priority:** Full release  
**Price:** $14.99  
**Rule:** v1.0 should feel complete even if updates continue afterward.

### v1.0 Content Targets

| Category | v1.0 Target |
|----------|-------------|
| Areas | 7 |
| Cards | 280+ |
| Enemies | 90+ |
| Encounters | 240+ |
| Events | 65+ |
| Relics | 110+ |
| Texture PNGs | 150+ final |
| Audio files | 60+ |

### v1.0 Additions

- Area 6: The Void, 7 floors, final challenge.
- Area 7: Primordial Forge, secret unlockable area.
- Optional future class note: Mystic, time/space manipulation. Current game already has 9 classes, so decide later whether Mystic is an extra class, a replacement idea, or cut.
- Full controller support verified.
- Localization:
  - DE
  - FR
  - JA
  - KO
  - ZH
  - PT-BR
  - ES
- 100% achievements: 25 total.
- Steam Trading Cards.

---

## Development Budget Estimate

| Phase | Duration | Focus | Team |
|-------|----------|-------|------|
| P0-P1 | 1-2 months | Stability, onboarding, UX, first impression | Solo dev |
| P2-P3 | 1-2 months | Steam page, trailer, demo, first public feedback | Solo dev + possible art/audio help |
| P4-P5 | 3-6 months | EA content, launch hardening, Steam features | Solo dev |
| P6 | 4+ months parallel | Art/audio production | Solo dev + contractors if possible |
| P7-P9 | Ongoing | EA updates, v1.0, localization, final polish | Solo dev + contractors |

### Estimated Contractor Costs

| Item | Estimated Cost |
|------|----------------|
| 60 min audio, SFX + music | $2,000-$5,000 |
| 250 pixel art assets | $8,000-$15,000 |
| Steamworks SDK integration | $500-$1,000 |
| Localization, 7 languages | $3,000-$6,000 |
| **Total contractor budget** | **$13,500-$27,000** |

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Art takes too long | High | Release demo/EA with coherent limited final art, replace iteratively. |
| Content feels thin | Medium | Gate areas behind meta-progression; add ascension; expand where players report repetition. |
| Adding too many cards too early | High | Balance current cards first; add small tested batches. |
| Audio budget too high | Low | Use royalty-free sources first; commission only key music if needed. |
| Burnout on solo dev | Medium | Follow gates; avoid turning EA into 1.0 before launch. |
| Low wishlist / sales | Medium | Start wishlist campaign early; release a demo before EA. |
| Bug density in EA | Medium | Monthly patches, public bug tracker, beta branch. |
| Scope creep | High | Keep P8/P9 ideas out of P0-P5 unless they directly improve demo/EA launch quality. |

---

## Immediate Next Steps

1. Finish P0 stability pass and keep current systems build-clean.
2. Pick the first 10-minute experience and tune it by hand.
3. Finalize the minimum art/audio needed for screenshots.
4. Build the Steam page asset checklist.
5. Prepare a demo candidate before adding large new card batches.
