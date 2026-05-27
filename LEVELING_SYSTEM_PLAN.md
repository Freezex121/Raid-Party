# Run-Local Party Member Leveling Plan

## Summary
- Add a run-local leveling system for individual party members.
- Levels are not persistent across runs.
- Experience is earned from card energy spent, so the party member whose class cards are used most levels fastest.
- Each party member can gain at most 20 XP per combat to prevent farming through draw/energy loops.
- Level-up rewards are choice-based and resolve outside combat.

## Core Rules

### XP Source
- When a card is successfully played, grant XP to the party member matching the card's class.
- XP gained equals the card's effective energy cost.
- A 2-energy Guardian card gives 2 XP to the Guardian.
- Free cards give 0 XP unless explicitly marked otherwise later.
- Cost reductions count the effective paid cost, not printed cost.
- Channel cards give XP when played, not when the channel resolves.
- Consume/one-use cards still give XP when played.

### Utility Cards
- Utility cards have no class owner.
- Proposed rule: utility cards grant XP to the lowest-level living party member.
- Tie-breaker: lowest current XP, then earliest party slot.
- This keeps utility-heavy turns from wasting progression while avoiding utility cards becoming targeted XP abuse.

### XP Cap
- Each party member has a per-combat XP cap of 20.
- XP beyond the cap is ignored for that combat.
- The cap resets at combat start.
- The UI should show capped state clearly, such as `XP capped`.

## Level Curve
- Start each run at level 1 (no XP needed — baseline).
- Cap: level 10.
- Formula (per-level cost, integer arithmetic): `xp_to_next = (10 * level) * (level / 2) + 5`

| Level | XP to Next | Total XP to Reach |
|-------|-----------|-------------------|
| 1 | 5 | 0 |
| 2 | 25 | 5 |
| 3 | 35 | 30 |
| 4 | 85 | 65 |
| 5 | 105 | 150 |
| 6 | 185 | 255 |
| 7 | 215 | 440 |
| 8 | 325 | 655 |
| 9 | 365 | 980 |
| 10 | — | 1345 |

At 20 XP per combat cap and ~25 combats per full run, a character can earn ~500 XP max. This puts the expected range at **level 6–7 by run end**, with level 10 only reachable in exceptionally long/ascension runs with heavy single-class focus. Progression stretches across the whole run without hitting max too early.

Formula implemented as:
```c
int xp_for_level(int level) {
    return (10 * level) * (level / 2) + 5;
}
```

## Data Model

Add run-local fields to `PartyMember` (`src/systems/party.h`):

```c
int level;                         // Current level (1-10)
int xp;                            // Total run XP
int combat_xp;                     // XP earned this combat (resets each fight)
int pending_levels;                // Number of level-ups queued (0 if none)
int perks[MAX_MEMBER_PERKS];       // Chosen perk IDs (-1 = empty)
int perk_count;                    // Number of perks chosen
```

**Constants:**
```c
#define MAX_MEMBER_PERKS 9         // Max 9 levels ups = 9 perk picks
#define MAX_COMBAT_XP 20           // Per-combat XP cap
#define MAX_LEVEL 10               // Level cap
```

**Notes:**
- `xp` is total run XP earned across all combats.
- `combat_xp` resets to 0 in `combat_start()`.
- `pending_levels` is incremented when a level threshold is crossed during combat. It's consumed by the level-up screen after combat.
- `perks[]` stores chosen reward IDs as small integers (enum below). Reset to -1 when party is created in `draft_screen.c`.
- Perks are run-local — they do not persist beyond the run.

**Perk ID enum (defined in `src/systems/party.h`):**
```c
typedef enum {
    PERK_HP_PLUS_4,              // +4 max HP, heal 4
    PERK_STARTING_SHIELD_1,      // +1 starting Shield per combat
    PERK_CARD_DMG_1,             // +1 card damage for this member
    PERK_CARD_HEAL_1,            // +1 card healing for this member
    PERK_CARD_SHIELD_1,          // +1 card Shield for this member
    // Class-specific:
    PERK_GUARDIAN_TAUNT_SHIELD,  // First Taunt each combat grants +4 Shield
    PERK_CLERIC_OVERHEAL_SHIELD, // Overhealing grants 2 Shield
    PERK_MAGE_FIRST_SPELL_DMG,   // First spell each turn deals +1 damage
    PERK_ROGUE_MARK_REFUND,      // First Mark payoff each combat refunds 1 energy
    PERK_SHAMAN_EXTEND_STATUS,   // First Conductive/Totem each combat lasts +1 turn
    PERK_RANGER_MARKED_DMG,      // First hit against Marked each combat +2 damage
    PERK_PALADIN_HEAL_SHIELD,    // Heals also grant 1 Shield
    PERK_WARLOCK_BLIGHT_BOOST,   // First Blight each combat gains +1 value
    PERK_BARD_FIRST_DRAW,        // First song each combat draws 1 card
    PERK_COUNT
} PerkId;
```

**Reward stacking:** All generic perks (`PERK_HP_PLUS_4`, `PERK_CARD_DMG_1`, etc.) can be chosen multiple times and stack. The class-specific perks are single-pick — once chosen they won't appear again for that member. This is enforced in choice generation by filtering out already-chosen class perks from the pool.

## Combat Integration

### Awarding XP

Hook into card resolution in `resolve_card_on_target()` (`src/combat/combat.c`) — after energy is deducted but before the card is discarded/exhausted.

**Effective card cost** uses the actual energy paid (not printed cost). The energy system already records this — the existing `combat_spend_energy()` returns whether it succeeded and by how much. The XP hook reads the amount actually deducted.

If the effective cost is 0, no XP is awarded.

**Flow:**
```
combat_xp_gain(cs, card, effective_cost):
1. if cs->phase != COMBAT_PLAYER_TURN → return
2. if effective_cost <= 0 → return
3. Determine recipient:
   a. if card->class != CLASS_NONE → that party member
   b. if card->class == CLASS_NONE (utility) → lowest-level living member
   c. tie-breaker: lowest total XP, then earliest party slot
   d. if no living member → return
4. Clamp: gain = min(effective_cost, MAX_COMBAT_XP - member.combat_xp)
5. if gain <= 0 → return
6. member.xp += gain
7. member.combat_xp += gain
8. Check level thresholds: while level < MAX_LEVEL && member.xp >= xp_for_level(member.level)
   → member.level++
   → member.pending_levels++
```

### No-Farming Edge Cases
- Do not award XP if combat phase is not `COMBAT_PLAYER_TURN`.
- Do not award XP if energy spend failed (e.g., insufficient energy, already resolved).
- Do not award XP from channel resolution — channel cards award XP once when first played.
- Do not award XP from relic-triggered free copies (Echo Bell, Split Prism splash) — they are not paid plays.
- Do not award XP for cards that cost 0 after reductions.
- If all party members are downed, utility card XP is silently lost.

## Level-Up Rewards

Level-up choices should be compact and run-local.

### Reward Timing
- Do not interrupt combat.
- Queue choices during combat.
- After combat rewards are settled, if anyone has pending level choices, go to a `SCREEN_LEVEL_UP`.
- Resolve all pending choices, then continue to the normal reward/map flow.

### Reward Types
- Small stat rewards:
  - `+4 max HP and heal 4`
  - `+1 starting Shield`
  - `+1 card damage for this member`
  - `+1 card healing for this member`
  - `+1 card Shield for this member`
- Class-flavored rewards:
  - Guardian: first Taunt each combat grants +4 Shield.
  - Cleric: overhealing grants 2 Shield.
  - Mage: first spell each turn deals +1 damage.
  - Rogue: first Mark payoff each combat refunds 1 energy.
  - Shaman: first Conductive/Totem application each combat lasts +1 turn.
  - Ranger: first hit against Marked each combat deals +2 damage.
  - Paladin: heals also grant 1 Shield.
  - Warlock: first Blight application each combat gains +1 value.
  - Bard: first song each combat draws 1 card.

### Choice Count
- Offer 2 choices per level.
- Choices should be generated from:
  - one generic option
  - one class-specific option when available

## UI Changes

### Party Frames (`src/ui/party_frames.c`)
- Add `LV X` text below the member's name, right-aligned (small font, themed color).
- Add XP progress text: `XP 14/35`, right side of the frame, replacing or adjacent to the aggro display.
- If `combat_xp >= MAX_COMBAT_XP`, append `CAP` in orange text.
- If `pending_levels > 0`, append a `!` in green after the level text.

### Combat Feedback (`src/combat/combat.c` — action feed)
- On XP gain: `combat_feed_add(cs, "Guardian +2 XP")`
- On level-up: `combat_feed_add(cs, "Guardian reached Level 3!")`
- No floating text for XP gains — action feed only. Consider a small floating "LV UP!" text on level-up only.

### Level-Up Screen (`src/screens/level_up_screen.c`)
- New screen, registered in `src/main.c`'s update/draw switch and `src/screens/screens.h`.
- Display:
  - Background: theme_draw_background()
  - Large class portrait (96px) centered top
  - Member name + new level: e.g., "Thalia — Level 3"
  - Two reward choice cards side by side, styled like relic reward cards
  - Each card shows the perk name and description
  - Hover highlight + click to pick
- After pick: animate transition, show next member if any, or advance to post-combat destination.

## Reward Flow Integration

### Post-Combat Destination Chain

Current combat victory flow (in `run_screen.c`) routes to one of:
- `SCREEN_REWARD` (card reward)
- `SCREEN_RELIC_REWARD` (after boss)
- `SCREEN_GAME_OVER` (final boss cleared)
- `SCREEN_MAP` (otherwise)

Add a new field to `GameState` (`src/game.h`):

```c
GameScreen post_combat_destination;    // Where to go after level-up screen
```

Replace all `game_change_screen(destination)` calls in the combat victory path with:
```c
game_go_to_level_up_or(destination);
```

This helper function:
```c
void game_go_to_level_up_or(GameScreen destination) {
    // Check if ANY party member has pending_levels > 0
    for (int i = 0; i < g_state.run_party.count; i++)
        if (g_state.run_party.members[i].pending_levels > 0) {
            g_state.post_combat_destination = destination;
            game_change_screen(SCREEN_LEVEL_UP);
            return;
        }
    game_change_screen(destination);
}
```

The level-up screen processes one pending member at a time. After the last member picks, it transitions to `g_state.post_combat_destination`.

### Level-Up Screen (`src/screens/level_up_screen.c`)

**New screen state:**
```c
static int current_member;   // Index into run_party.members being processed
static int choice_a, choice_b;  // PerkId values for the two choices
```

**update():**
- Show two choice buttons. On click, add the chosen perk to the member's `perks[]`, decrement `pending_levels`.
- If that member still has more pending levels, generate new choices for them.
- If no more pending members, advance to `g_state.post_combat_destination`.

**draw():**
- Theme background
- Show member portrait + name + new level number
- Two choice cards/panels side by side with perk name and description
- Progress indicator (e.g., "Member 2/3 — 2 more to choose")

## Balance Notes
- Energy-based XP rewards deliberate use and expensive cards. The per-combat cap prevents farming.
- The 20 XP per-fight cap is mandatory — without it, draw/energy loop builds could reach level 10 in one fight.
- Free-card synergy builds don't generate XP (0 energy = 0 XP).
- Underfilled parties (solo/duo) level faster because they play more of their own class cards. This is a natural buff.
- Generic perks stack (e.g., +1 damage ×3 = +3 damage), providing diminishing returns compared to class-specific perks.
- Class-specific perks are one-pick-only per member, encouraging varied choices across levels.
- Level rewards are smaller than relics — a relic might give +4 shield to everyone, a perk gives +1 shield to one member's cards.
- Meta-progression bonuses (Sharpened Blades, Reinforced Armor) stack additively with level perks. This is intentional — meta is persistent, levels are per-run.

## Implementation Steps

| Step | Files | Detail |
|------|-------|--------|
| 1 | `src/systems/party.h`, `src/systems/party.c` | Add `level`, `xp`, `combat_xp`, `pending_levels`, `perks[MAX_MEMBER_PERKS]`, `perk_count` fields to `PartyMember`. Add `PerkId` enum. Init in party creation (level=1, xp=0, etc.). |
| 2 | `src/systems/party.h`, `src/systems/party.c` | Add `xp_for_level(int level)` helper. Add `party_gain_xp(PartyMember *m, int amount)` — adds XP, handles combat_xp clamping, increments level+pendings when thresholds crossed. |
| 3 | `src/combat/combat.c` | In `combat_start()`, zero `combat_xp` for all party members. |
| 4 | `src/combat/combat.c` | In `resolve_card_on_target()`, after successful energy spend, call XP gain logic. Award to card's class member, or lowest-level living member for utility cards. |
| 5 | `src/game.h`, `src/game.c` | Add `GameScreen post_combat_destination` field. Add `game_go_to_level_up_or()` helper. |
| 6 | `src/screens/level_up_screen.c`, `src/screens/screens.h`, `src/main.c` | Create `SCREEN_LEVEL_UP` screen. Update main loop switch. Wire post-combat routing through `game_go_to_level_up_or()` in `run_screen.c`. |
| 7 | `src/ui/party_frames.c` | Draw `LV X`, XP progress, CAP tag, pending `!` on each party frame. |
| 8 | `src/combat/combat.c` | Add action feed lines for XP gain and level-ups. |
| 9 | `src/screens/level_up_screen.c` | Implement generic reward choices and choice generation logic. |
| 10 | `src/screens/level_up_screen.c`, `src/combat/combat.c` | Implement class-specific reward effects. Class perks need hooks in combat (e.g., Mage's first-spell bonus in damage calc, Guardian's taunt shield in taunt resolution). |

## Test Plan
- Build with `cmake --build build --config Release --target RaidParty`.
- Start a run and verify all party members begin at level 1 with 0 XP.
- Play 1, 2, and 3 energy cards and verify matching XP gains.
- Verify cost-reduced/free cards award effective paid cost (0 for free).
- Verify utility cards award XP to the lowest-level living party member.
- Verify each party member caps at 20 XP per combat.
- Level up mid-combat — verify pending_levels increments, no UI interruption until post-combat.
- Verify level-up choices appear after combat victory (not after wipe).
- Verify chosen perks affect combat (e.g., +1 damage applies).
- Verify members level independently and uneven card distribution creates uneven levels.
- Verify solo/duo parties level faster but still cap at 20 XP per combat per member.
