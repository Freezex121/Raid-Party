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
- Start each run at level 1.
- Suggested cap: level 5.
- Suggested thresholds:

| Level | Total XP Required |
|-------|-------------------|
| 1 | 0 |
| 2 | 8 |
| 3 | 20 |
| 4 | 48 |
| 5 | 75 |

Rationale: energy-based XP grows quickly in longer fights, so thresholds should be higher than card-count XP.

## Data Model

Add run-local fields to `PartyMember`:

```c
int level;
int xp;
int combat_xp;
int pending_level_choices;
int level_perks[MAX_MEMBER_LEVEL_PERKS];
int level_perk_count;
```

Notes:
- `xp` is total run XP.
- `combat_xp` resets in `combat_start()`.
- `pending_level_choices` queues level-up rewards for after combat.
- Perks are run-local and reset when the party is created.

## Combat Integration

### Awarding XP
Hook into `resolve_card_on_target()` after energy is spent and before the card leaves hand.

Flow:
1. Determine effective cost with `combat_effective_card_cost()`.
2. Determine XP recipient:
   - card class member if `card->class != CLASS_NONE`
   - lowest-level living member for utility cards
3. Clamp XP by `20 - combat_xp`.
4. Add XP.
5. Check level thresholds.
6. Queue level choices for any gained levels.

### No Farming Edge Cases
- Do not award XP if combat phase is not `COMBAT_PLAYER_TURN`.
- Do not award XP if energy spend failed.
- Do not award XP from channel resolution.
- Do not award XP from relic-triggered free copies unless they are actual played cards.

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

### Party Frames
- Show `LV X` on each party frame.
- Show XP progress as a tiny bar or text: `XP 4/16`.
- If combat XP is capped, show a small `CAP` tag.
- If a member has pending level choices, show a small `!`.

### Combat Feedback
- Add action feed lines:
  - `Guardian +2 XP`
  - `Guardian reached Level 3`
- Avoid floating text spam unless it is very small and rare.

### Level-Up Screen
- New screen: `SCREEN_LEVEL_UP`.
- One party member at a time.
- Show portrait, current level, and two reward choices.
- After picking, move to next pending member.
- Continue to card reward, relic reward, discard, or map depending on queued post-combat destination.

## Reward Flow Integration

Current combat victory flow routes through:
- card reward
- discard after elite/boss
- relic reward after boss
- map
- game over after final boss

Add a post-combat pending destination:

```c
GameScreen post_level_screen;
```

Before changing screens after combat, call a helper:

```c
game_go_to_level_up_or(destination);
```

If there are pending level choices, it stores `destination` and opens `SCREEN_LEVEL_UP`.
Otherwise it goes directly to `destination`.

## Balance Notes
- Energy XP rewards deliberate use, but high-cost cards may level faster than intended.
- The 20 XP per fight cap is mandatory.
- Free-card synergy builds should not generate XP unless they actually pay energy.
- Underfilled parties will level faster because they play more of their own cards. This is good and supports solo/duo runs.
- Level rewards should be useful but smaller than relics.

## Implementation Steps

1. Add party member XP/level fields and initialize them in party creation.
2. Add XP threshold helpers and `party_member_gain_xp()`.
3. Track per-combat `combat_xp` and reset it in `combat_start()`.
4. Award energy-based XP in card resolution.
5. Add pending level-up queue to `GameState`.
6. Add `SCREEN_LEVEL_UP` and route post-combat destinations through it.
7. Draw level/XP/cap/pending indicators on party frames.
8. Add basic generic rewards.
9. Add class-specific rewards.
10. Build and run combat smoke tests.

## Test Plan
- Build with `cmake --build build --config Debug --target RaidParty`.
- Start a run and verify all party members begin at level 1 with 0 XP.
- Play 1, 2, and 3 energy cards and verify matching XP gains.
- Verify cost-reduced/free cards award effective paid cost.
- Verify utility cards award XP to the lowest-level living party member.
- Verify each party member caps at 20 XP per combat.
- Verify level-up choices appear after combat, not during combat.
- Verify selected rewards affect only that party member and reset next run.
- Verify solo and duo parties level faster but cannot exceed the per-combat cap.
