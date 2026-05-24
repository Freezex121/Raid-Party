# Raid Paper Legends — Balancing Plan

## Analysis

### Issue 1: Single PM > Full Team

**Root causes identified:**
- **Same resources regardless of party size**: Draw 5 cards, start 4 energy, regen 3 per turn whether solo or 5 members.
- **4-card starter decks cycle perfectly**: A solo rogue draws all 4 cards per turn, always having Kick (interrupt).
- **Interrupt has no limit**: No cooldown, no diminishing returns, no per-turn cap — interrupt every turn forever.
- **No aggro problem solo**: No tank needed; just interrupt the single enemy's cast each turn.
- **No party size scaling on enemies**: Enemy stats are identical regardless of how many members you bring.

### Issue 2: Game Too Easy

**Root causes:**
- Interrupt nullifies most enemy threat.
- Predictable round-robin enemy AI.
- No pressure mechanics (timer, enrage, stacking difficulty).
- No boss phase changes.
- No party debuffs from enemies.

### Issue 3: Not Enough Meta Progression

**Root causes:**
- Only 3 upgrade tracks (slot 4, slot 5, starting gold) = 6 purchases, 30 total renown.
- No ascension/difficulty system.
- No class unlocks.
- No achievements.
- No starting bonuses beyond gold/slots.

---

## Proposed Changes

### 1. Party Size Resource Scaling

Scale draw and energy regen by party count so there's a concrete advantage to bringing more members:

| Party Size | Draw / Turn | Start Energy | Regen / Turn |
|------------|-------------|--------------|--------------|
| 1          | 3           | 2            | 2            |
| 2          | 4           | 3            | 2            |
| 3          | 5           | 4            | 3 (current)  |
| 4          | 5           | 4            | 4            |
| 5          | 6           | 5            | 4            |

Solo/duo parties become resource-starved: you can't interrupt every turn if you only draw 3 cards and have 2 energy.

**Files to change:**
- `src/combat/combat.c` — `deal_opening_hand()` and `advance_turn()`: change hardcoded `5` to a party-size-dependent value.
- `src/combat/combat.c` — `combat_start()`: scale starting energy by party count.
- `src/systems/energy.c` / `src/systems/energy.h` — optionally pass party count to `energy_init()`.

---

### 2. Interrupt Cooldown

Add an interrupt cooldown so interrupted enemies cannot be interrupted again for 2 turns. This keeps interrupts useful as tactical tools for full parties while preventing the solo "infinite stall" strategy.

**Files to change:**
- `src/combat/combat.h` — add `int interrupt_cooldown` field to `EnemyState`.
- `src/combat/combat.c` — `interrupt_enemy()`: set cooldown when interrupted, prevent re-interrupt while > 0.
- `src/combat/combat.c` — `advance_turn()`: tick interrupt cooldowns down.

---

### 3. Improved Enemy AI

**a) Dynamic ability selection**
Instead of pure round-robin, enemies choose abilities based on context:
- If interrupted last turn, 50% chance to repeat the same ability with reduced cast time.
- If multiple party members are alive, prefer AOE.
- If low on HP, prioritize healing/shielding abilities.

**b) Multi-phase bosses**
When boss HP drops below 50%, unlock a new ability or reduce cast times on existing ones.

**c) Status effects on party**
Add enemy abilities that apply status effects (weakness, DOT, energy reduction) to create pressure.

**d) Encounter scaling by party size**
Scale the number of enemies in an encounter based on party size (1 enemy for solo, 1-2 for duo, 2-3 for trio, 3 for full party).

**Files to change:**
- `src/combat/combat.c` — `choose_enemy_intent()`: rewrite to be context-aware.
- `src/combat/combat.c` — `advance_turn()`: add phase-change check for bosses.
- `assets/data/enemies.json` — add new abilities with status effects.
- `src/data/encounter_defs.c` — optionally scale encounters by party size.

---

### 4. Ascension System

After winning a run, unlock Ascension Level 1. Each level adds a stacking difficulty modifier:

| Level | Modifier                    |
|-------|-----------------------------|
| 1     | +10% enemy damage           |
| 2     | -1 starting energy          |
| 3     | +15% enemy HP               |
| 4     | All enemies have +1 cast speed |
| 5     | Start with 1 Doubt in deck  |
| 6     | +15% enemy damage           |
| 7     | Bosses gain a bonus ability |
| 8     | -1 card draw per turn       |
| 9     | Start with 2 Doubts in deck |
| 10    | All enemies deal +25% dmg   |

Each ascension level grants +1 bonus renown at the end of a run. This creates:
- A difficulty slider for players who find the game too easy.
- Long-term meta progression to chase.
- Different build challenges at high ascension.

**Files to change:**
- `src/systems/meta.h` / `src/systems/meta.c` — add `ascension_level` field, save/load.
- `src/screens/title_screen.c` — show and select ascension level.
- `src/combat/combat.c` — apply ascension modifiers in `combat_start()` and `advance_turn()`.
- `src/screens/game_over_screen.c` — show ascension level and bonus renown.

---

### 5. Class Unlocking

Add 2-3 unlockable classes purchasable with renown:

| Class    | Role                  | Cost | Description                                       |
|----------|-----------------------|------|---------------------------------------------------|
| Paladin  | Hybrid Tank / Healer  | 12   | AOE heals, defensive buffs, light damage.         |
| Warlock  | DOT / Curses          | 12   | Continuous damage, enemy debuffs, self-harm.      |
| Bard     | Buffer                | 12   | Party-wide buffs, energy generation, card draw.   |

**Files to change:**
- `CLASS_COUNT` references across multiple files — extend to 8-9.
- `assets/data/cards.json` — add new class cards (8+ each).
- `assets/data/classes.json` — add new class definitions.
- `src/systems/class_colors.h` — add new colors.
- `assets/art/` — add new icons.
- `src/systems/meta.h` / `src/systems/meta.c` — add unlock tracking.
- `src/screens/draft_screen.c` — show/hide locked classes with lock icon.
- `src/screens/title_screen.c` — show unlock progress.

---

### 6. Starting Relic Unlock

Add "Legacy" meta upgrade: unlock the ability to start each run with a relic.

| Upgrade    | Cost | Effect                              |
|------------|------|-------------------------------------|
| Legacy I   | 5    | Start with a random common relic.   |
| Legacy II  | 10   | Start with a random uncommon relic. |
| Legacy III | 15   | Choose from 2 random relics at start. |

**Files to change:**
- `src/systems/meta.h` / `src/systems/meta.c` — add `starting_relic_rank` field.
- `src/screens/draft_screen.c` — after drafting, optionally show relic selection.
- `assets/data/relics.json` — add rarity field to relic definitions.

---

### 7. Achievement Milestones

Add tracked milestones that grant renown rewards:

| Achievement    | Trigger                      | Reward |
|----------------|------------------------------|--------|
| First Steps    | Complete first run           | 2R     |
| Champion       | Win a run                    | 3R     |
| Perfectionist  | Win with no deaths           | 5R     |
| Solo Artist    | Win with 1 party member      | 5R     |
| Full House     | Win with 5 party members     | 5R     |
| Interrupted    | Interrupt 20 enemy casts     | 3R     |
| Hoarder        | Collect 10 relics in a run   | 3R     |
| Speed Demon    | Win a floor in 3 turns       | 3R     |
| Completionist  | Beat all areas               | 10R    |

**Files to add / change:**
- New `src/systems/achievements.c` / `src/systems/achievements.h`.
- `src/systems/meta.h` — add achievement tracking array.
- Check conditions in `src/combat/combat.c`, `src/screens/game_over_screen.c`, etc.
- Display in title screen or new achievements screen.

---

## Effort Summary

| # | Change                    | Difficulty | Impact on Issue(s)     |
|---|---------------------------|------------|------------------------|
| 1 | Party size scaling        | Medium     | 1 (High)               |
| 2 | Interrupt cooldown        | Small      | 1 (High)               |
| 3 | Improved enemy AI         | Large      | 2 (High)               |
| 4 | Ascension system          | Medium     | 2 + 3 (Very High)      |
| 5 | Class unlocking           | Large      | 3 (High)               |
| 6 | Starting relic            | Small      | 3 (Medium)             |
| 7 | Achievements              | Medium     | 3 (Medium)             |

All items can be implemented incrementally and independently. Items 1-2 are the highest priority for the solo interrupt problem. Items 3-4 address the "too easy" complaint. Items 5-7 expand the meta progression layer.
