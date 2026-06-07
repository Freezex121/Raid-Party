# Boss Design Plan — Raid Paper Legends

## Design Philosophy

Bosses should feel like puzzles to solve, not just bigger health bars. Each boss should:

- Introduce a **unique pressure vector** the player must adapt to (not just bigger damage numbers)
- Have **readable intents** so the player can make informed decisions
- Reward **system knowledge** — knowing when to burst, when to defend, when to cleanse
- Tell a **story through mechanics** — the boss's abilities should reflect who it is
- Create a **memorable arc** — phases should escalate tension, not just add numbers
- Offer **counterplay** — every threatening mechanic should have a way to mitigate or outplay it

---

## Current Boss System (As-Is)

The current boss system is minimal:

| Feature | How It Works |
|---------|--------------|
| Boss flag | `g_state.encounter_is_boss` — set when entering a boss node |
| Phase trigger | At 50% HP: `phase` flips 0→1, gains +10 shield, cast times reduce by 1 |
| Reward | 2 card picks (instead of 1), relic guaranteed, 50 gold (vs 10 normal / 25 elite) |
| Difficulty | Bosses share the same enemy JSON format; no special boss-specific fields |

All current "boss" mechanics are just existing enemy abilities used at higher values. There is no code distinction between a boss card and a normal enemy card.

---

## Generic Boss Mechanics (Reusable Systems)

These are systems that can be built once and applied to any boss. Each mechanic is a tool in the toolbox.

---

### GM-1: Advanced Phase State Machine

**Problem:** Current phases are a boolean flip at 50% HP with a fixed effect.

**Solution:** Replace `int phase` (0/1) with a configurable state machine.

```
EnemyState.phase:
  0          → Phase 1 (default, full HP)
  1, 2, 3... → Later phases, each defined by data

PhaseDef (new struct, per-boss):
  trigger_hp_pct:    HP % that activates this phase (e.g. 75, 50, 25)
  on_enter_effects:  Array of effects to apply on phase entry
    - gain_shield: int
    - gain_energy: int
    - heal_pct: int
    - apply_status_to_party: { type, turns, value }
    - set_invulnerable: bool
    - swap_deck: id (swap to a different card set)
    - spawn_enemies: { id, count }
    - apply_arena_aura: { type, value, duration }
  modify_stats:      Permanent stat changes for this phase
    - energy_per_turn_delta: int
    - hand_size_delta: int
    - damage_scale_delta: float
    - cast_time_reduction: int
  new_cards:         Card indices to add to the deck this phase
  remove_cards:      Card indices to remove from the deck this phase
```

**JSON example:**

```json
"phases": [
  {
    "trigger_hp_pct": 50,
    "on_enter": {
      "gain_shield": 15,
      "gain_energy": 2,
      "spawn_enemies": { "id": "flame_imp", "count": 1 }
    },
    "modify_stats": {
      "energy_per_turn_delta": 1,
      "cast_time_reduction": 1
    }
  },
  {
    "trigger_hp_pct": 25,
    "on_enter": {
      "apply_arena_aura": { "type": "damage_tick", "value": 3, "duration": 0 },
      "swap_deck": "boss_raging_deck"
    },
    "modify_stats": {
      "damage_scale_delta": 0.5
    }
  }
]
```

**Effort:** Medium (3-5 days — new struct, JSON parsing, hook into `advance_turn()`)
**Reuse:** Every boss

---

### GM-2: Arena Auras

**Problem:** All boss effects are single-target or self-target. Nothing affects the whole battlefield persistently.

**Solution:** Add an `arena_effects[]` array to `CombatState` that applies passive global effects until cleared.

```
ArenaEffect types:
  AURA_DAMAGE_TICK        — Deal X damage to all allies at turn start
  AURA_HEAL_TICK          — Heal all enemies for X at turn start
  AURA_SHIELD_TICK        — Shield all enemies for X at turn start
  AURA_COST_UP            — Increase all card costs by X
  AURA_COST_DOWN          — Reduce all card costs by X (enemy side)
  AURA_DAMAGE_REDUCTION   — Reduce all player damage by X%
  AURA_HEAL_REDUCTION     — Reduce all healing by X%
  AURA_DRAW_REDUCTION     — Reduce hand draw by X per turn
  AURA_ENERGY_DRAIN       — Drain X energy per turn from player
  AURA_ENEMY_DAMAGE_UP    — Increase all enemy damage by X%
  AURA_SILENCE_CLASSES    — Silence classes (bitmask)
```

**Clearing auras:** Auras can be cleared by player actions (deal X damage in one turn, play a specific card type, interrupt, etc.) or have a fixed duration.

```json
{ "aura_type": "damage_tick", "value": 3, "duration": 0, "clear_on_damage_threshold": 15 }
```

**Effort:** Medium (3-4 days — new system + UI indicators)
**Reuse:** High — many bosses benefit from arena flavor

---

### GM-3: Accumulator (Generic Rage/Charge System)

**Problem:** No way to track a growing threat that players must manage.

**Solution:** `EnemyState` gains an `accumulator` (int) that can be modified by events and spent by cards.

```
Accumulator triggers (configurable per-card):
  gain_on_player_card_play:  int  // +X when player plays any card
  gain_on_player_attack:     int  // +X when player deals damage
  gain_on_player_skill:      int  // +X when player uses a skill
  gain_on_boss_attacked:     int  // +X per hit the boss takes
  gain_on_turn_start:        int  // +X every turn start
  gain_on_ally_damaged:      int  // +X per ally hit this turn

Accumulator spenders (on enemy card resolution):
  spend:                     int  // consume X accumulator for bonus effect
  spend_all:                 bool // consume all accumulator
  damage_per_spent:          int  // bonus damage per accumulator consumed
  heal_per_spent:            int  // bonus heal per accumulator consumed
  shield_per_spent:          int  // bonus shield per accumulator consumed
  status_per_spent:          int  // bonus status stacks per accumulator consumed

Accumulator effects (passive):
  cap:                       int  // maximum accumulator (default 20)
  decay_per_turn:            int  // automatic decay (default 0)
  decay_on_defend:           bool // decay when boss uses a non-attack card
```

**JSON example:**

```json
{
  "accumulate": true,
  "accumulator_cap": 15,
  "gain_on_boss_attacked": 1,
  "decay_per_turn": 2,
  "cards": [
    {
      "name": "Fury Blast",
      "spend_all": true,
      "damage_per_spent": 4,
      "base_damage": 5,
      "cast_time": 2
    }
  ]
}
```

**Effort:** Small (1-2 days — add field to `EnemyState`, trigger checks, UI display)
**Reuse:** High — can be used for rage, charge, mana, heat, corruption, etc.

---

### GM-4: Status Detonation

**Problem:** Status effects only tick for small amounts. No way to make them a primary threat vector.

**Solution:** Enemy cards can consume status stacks from targets for bonus effects.

```
Fields on EnemyCardDef:
  detonate_status:       StatusType  // which status to consume
  detonate_mult:         int         // bonus damage per stack consumed
  detonate_target:       "self" | "single" | "all"  // who gets detonated on
  detonate_heal_mult:    int         // heal per stack consumed
  detonate_shield_mult:  int         // shield per stack consumed
  detonate_clear:        bool        // whether to clear the stacks (default true)
```

**JSON example:**

```json
{
  "name": "Toxic Rupture",
  "detonate_status": "blight",
  "detonate_mult": 8,
  "base_damage": 3,
  "cast_time": 2
}
```

**Effort:** Small (1 day — new fields, parse, resolution logic in `enemy_action()`)
**Reuse:** Medium — pairs with bosses that apply statuses

---

### GM-5: Tether / Aggro Forcing

**Problem:** Aggro management has no counterplay from the enemy side. Players can stack aggro on one tank and ignore the mechanic.

**Solution:** Tether forces a specific non-tank ally to take the next hit, bypassing aggro.

```
Fields on EnemyCardDef:
  tether:              bool   // applies tether to target
  tether_duration:     int    // turns tether lasts
  tether_target:       "random" | "lowest_hp" | "lowest_shield" | "last_attacker"
  tether_transferrable: bool  // can be transferred via Taunt/Shield (default true)
```

**Tether behavior:**
- The tethered ally's aggro is overridden to +50 above the current tank
- All enemy single-target abilities target the tethered ally automatically
- Tether is cleared by: duration expiry, Taunt (transfers to caster), death
- UI: Chain icon on the tethered ally's portrait

**Effort:** Medium (2-3 days — new field, aggro override, UI indicator, Taunt interaction)
**Reuse:** Medium — thematic for "jailer" or "stalker" archetype bosses

---

### GM-6: Link System (Shared HP)

**Problem:** No way to make multiple enemies share a health pool or require synchronized kills.

**Solution:** Enemies can share a `linked_group_id`. All damage dealt to any member is subtracted from a shared `linked_hp` pool, and each member shows the same HP bar.

```
Fields on EnemyDef:
  linked_group_id:    char[16]  // all enemies with same ID share HP
  linked_revive:      bool      // if true, revives when partner is alive
  linked_revive_hp:   int       // % of max HP to revive with
  linked_revive_turns: int      // turns to revive
  linked_revive_stagger: int    // deal this much damage to surviving partner to delay revive
```

**Behavior:**
- All linked enemies show the same HP bar (the shared pool)
- Damage formula: `linked_hp -= damage / count` (split evenly, or directly subtract and cap each at 0)
- If one dies and linked_revive is true, starts a revive countdown
- Revive is interrupted if the surviving partner takes enough damage
- "Simultaneous kill" (both die within 2 turns of each other) prevents revive entirely

**Effort:** Medium (3-4 days — linked HP tracking, revive tick, damage routing, UI)
**Reuse:** Low-medium — thematic for twins, dual-aspect, symbiotic pairs

---

### GM-7: Invulnerability Gates

**Problem:** No way to make a boss temporarily immune, forcing players to focus on adds or conditions.

**Solution:** `EnemyState` gains a `bool invulnerable` flag. When set, the boss takes zero damage. All attacks show "immune" or "blocked" floating text.

```
Fields on EnemyCardDef:
  set_invulnerable:    bool  // makes the boss invulnerable on resolve
  invulnerable_until:  "all_adds_dead" | "damage_threshold" | "turns" | "card_played"
  invulnerable_turns:  int   // if "turns", how many turns it lasts
  invulnerable_clear_on_damage: int // clear after dealing this much damage to boss (ignoring invuln status)

Summon card fields (for gate phases):
  gate_summon_id:     char[16]  // enemy to spawn
  gate_summon_count:  int       // how many to spawn
  gate_arena_aura:    {...}     // optional arena effect while gate is active
```

**Effort:** Medium-large (3-5 days with summoning integration)
**Reuse:** Medium — epic set-piece moments, but not every boss needs it

---

### GM-8: Deck Manipulation

**Problem:** The player's deck is never under direct attack. Bosses can't create pressure on card economy.

**Solution:** Enemy cards can interact with the player's deck, hand, and discard.

```
Fields on EnemyCardDef:
  curse_count:            int          // shuffle X curse cards into player's draw pile
  curse_card_id:          char[16]     // which curse card (default "dazed")
  curse_pile:             "draw" | "discard" | "hand"
  steal_card:             bool         // steal a card from player's hand
  steal_return_turns:     int          // turns until stolen card returns
  steal_use_against:      bool         // resolve the stolen card's effects against the party
  exhaust_from_deck:      int          // exhaust X random cards from draw pile
  force_discard:          int          // force player to discard X cards from hand
  increase_costs:         int          // increase all card costs by X for 1 turn
  decrease_hand_size:     int          // reduce max hand size by X for 1 turn
```

**Required:**
- A "Dazed" curse card: cost 1, damage 0, exhaust, fleeting, draw 1 (junk that cycles)
- A "Wound" curse card: cost 2, damage 0, exhaust (dead card)
- A "Frailty" curse card: cost 0, damage 0, apply Weakness to caster, exhaust

**Effort:** Medium-large (3-5 days — interaction with `deck.c`, hand management, UI indicators)
**Reuse:** Low-medium — thematic for mind/arcane bosses

---

### GM-9: Scripted Openers & Patterns

**Problem:** Enemy AI is random. Bosses can't have iconic opening moves or predictable attack patterns.

**Solution:** Allow bosses to define a scripted opening sequence and/or weighted card cycles.

```
Fields on EnemyDef:
  opening_sequence: [int]    // ordered list of card indices for the first X turns
  pattern_cycle:    [int]    // if set, boss cycles through this pattern instead of AI picking
  pattern_loop:     bool     // if true, loop the pattern_cycle; if false, shuffle at end
  ai_override:      "pattern" | "weighted" | "reactive"

Reactive AI modes:
  "weighted" — standard scoring AI (current behavior)
  "pattern" — fixed rotation regardless of state
  "reactive" — picks cards based on player state (e.g., "if player has >5 energy, use drain")
```

**Reactive conditions:**
```
reactive_cards: [
  { card_index: 0, condition: "player_energy >= 5" },
  { card_index: 1, condition: "party_hp_pct < 50" },
  { card_index: 2, condition: "status_count_on_party > 3" },
  { card_index: 3, condition: "always" }  // fallback
]
```

**Effort:** Medium (2-3 days — opening queue, pattern cycling, condition evaluator)
**Reuse:** High — learning a boss's pattern is core to the mastery loop

---

### GM-10: Reactive Intents (Player-Triggered Responses)

**Problem:** Enemies don't react to the player's turn. All behavior is turn-based.

**Solution:** Certain boss cards can trigger as a **reaction** to player actions, playing between player cards (not just on enemy turns).

```
Fields on EnemyCardDef:
  reaction_type:      "on_player_attack" | "on_player_skill" | "on_player_shield"
                      | "on_player_heal" | "on_damage_taken" | "on_card_played"
                      | "on_turn_end_energy_unspent"
  reaction_trigger:   int    // how many times player must do the action before reaction fires
  reaction_once:      bool   // can only trigger once per turn (default true)
  reaction_priority:  int    // determines order if multiple reactions could fire
```

**Behavior:**
- When the player performs the trigger action, the reaction card is queued
- The reaction resolves **immediately** after the current player card finishes (or at end of turn)
- Reaction cards have **cast_time 0** (instant) by default
- The player sees a warning: "The boss stirs..." when a reaction is partially charged

**Examples:**
- "Every time you play an Attack, the boss gains 1 Rage"
- "When you Shield an ally, the boss Heals 3"
- "If you end a turn with unspent energy, the boss gains +1 energy next turn"

**Effort:** Large (1-2 weeks — interrupt/resolve pipeline, trigger tracking, UI)
**Reuse:** Medium — creates dynamic, interactive fights

---

### GM-11: Minion Buff Aura

**Problem:** Boss + minion encounters have no synergy. Minions don't benefit from the boss being alive.

**Solution:** Bosses can passively buff all other living enemies.

```
Fields on EnemyDef:
  minion_buffs: [
    { effect: "damage_boost", value: 25, range: "all_allies" },
    { effect: "shield_per_turn", value: 3, range: "all_allies" }
  ]
  minion_buff_range: "all_allies" | "same_room" | "adjacent"
  minion_buff_tooltip: "string"  // shown when hovering enemy
```

**Effect:**
- While the boss is alive, all other enemies get the buff
- Buff applies immediately on spawning and persists until boss death
- UI: a buff icon on minions showing they're empowered

**Effort:** Small (1 day — passive effect checks in `advance_turn()`, UI)
**Reuse:** High — creates "kill the healer first" dynamics

---

### GM-12: Damage Threshold Triggers

**Problem:** No reward or punishment for how much damage the player deals in one turn.

**Solution:** Boss can react when the player deals X damage in a single turn.

```
Fields on EnemyDef:
  damage_thresholds: [
    { threshold: 15, response: "gain_shield:10" },
    { threshold: 25, response: "retaliate:8" },
    { threshold: 40, response: "apply_weakness:2" }
  ]
  threshold_reset: "end_of_turn" | "per_combat" | "per_phase"
```

**Behavior:**
- Tracks `turn_damage_received` count per enemy
- When a threshold is crossed, the response applies immediately
- Responses can be: shield, heal, retaliate damage, apply status, gain energy, clear debuffs

**Effort:** Small (1 day — track damage per turn, threshold check in `apply_damage_to_enemy()`)
**Reuse:** Medium — rewards controlled burst, punishes overextending

---

### GM-13: Damage Scaling (HP-Based)

**Problem:** Boss damage output is flat. No incentive to finish them quickly or play defensively at low HP.

**Solution:** Boss damage scales inversely with current HP percentage.

```
Fields on EnemyDef:
  rage_scale:         float  // damage multiplier when below HP%
  rage_hp_threshold:  int    // HP% where scaling kicks in
  rage_max_mult:      float  // maximum damage multiplier at 1 HP
  rage_formula:       "linear" | "exponential" | "step"
```

**Behavior:**
- Below the threshold, all boss damage is multiplied by `1 + rage_scale * (1 - hp_ratio)`
- At 50% HP: small boost
- At 10% HP: dramatic boost
- Creates a "desperation" phase that rewards finishing the boss fast

**Effort:** Trivial (1-2 hours — multiply damage by rage factor in `enemy_action()`)
**Reuse:** High — can be applied to any boss as a "Phase 2 enrage"

---

### GM-14: Damage Reflection / Retribution

**Problem:** No punishment for attacking a boss relentlessly. Nothing forces players to pause.

**Solution:** Boss can gain a temporary reflect buff that damages attackers.

```
Fields on EnemyCardDef:
  reflect_pct:        int    // % of incoming damage reflected to attacker
  reflect_type:       "damage" | "status" | "shield_on_boss"
  reflect_turns:      int    // duration of reflect
  reflect_cap:        int    // max reflected per hit (0 = no cap)
```

**Behavior:**
- When the boss has a reflect buff, any damage dealt to it triggers the reflection
- Reflection applies to the **attacking party member**
- UI: A spiky barrier icon on the boss when reflect is active

**Effort:** Small (1 day — hook into `apply_damage_to_enemy()`, check reflect status)
**Reuse:** Medium — thematic for shielded/armored bosses

---

### GM-15: Channeled Rituals (Extended Cast Abilities)

**Problem:** Currently all casts are 1-3 turns. No multi-phase wind-up abilities.

**Solution:** Boss cards with extended cast times that have previewable effects at each stage.

```
Fields on EnemyCardDef:
  cast_stages: int   // number of stages in the cast (default 1)
  stage_labels: ["Charging...", "Readying...", "FIRING!"]
  stage_effects: [
    { turn: 1, effect: "damage_tick:3" },
    { turn: 2, effect: "spawn_enemy:1" },
    { turn: 3, effect: "fire" }
  ]
  interruptible_stages: [1, 2]  // which stages can be interrupted
  stage_warning: bool  // show a larger warning as stages progress
```

**Behavior:**
- On cast start, the boss shows Stage 1
- Each turn, the stage advances
- At each stage, a minor effect may trigger
- At the final stage, the main ability fires
- Some stages are interruptible; if you time your interrupt at the right stage, you cancel the entire ritual
- Missing the interrupt window means the boss fires the ability

**Effort:** Medium-large (3-5 days — stage tracking, multi-part cast bar UI, per-stage effects)
**Reuse:** Low-medium — epic boss-only mechanic

---

### Summary: Generic Mechanics

| # | Mechanic | Effort | Reuse | Core Fields |
|---|----------|--------|-------|-------------|
| GM-1 | Phase State Machine | 3-5 days | ★★★★★ | `phases[]`, `trigger_hp_pct`, `on_enter` |
| GM-2 | Arena Auras | 3-4 days | ★★★★☆ | `aura_type`, `value`, `clear_condition` |
| GM-3 | Accumulator | 1-2 days | ★★★★☆ | `accumulator`, `gain_*`, `spend_*` |
| GM-4 | Status Detonation | 1 day | ★★★☆☆ | `detonate_status`, `detonate_mult` |
| GM-5 | Tether | 2-3 days | ★★★☆☆ | `tether`, `tether_target` |
| GM-6 | Link | 3-4 days | ★★☆☆☆ | `linked_group_id`, `linked_revive` |
| GM-7 | Invulnerability Gates | 3-5 days | ★★★☆☆ | `invulnerable`, `gate_summon_*` |
| GM-8 | Deck Manipulation | 3-5 days | ★★☆☆☆ | `curse_count`, `steal_card`, `force_discard` |
| GM-9 | Scripted Openers & Patterns | 2-3 days | ★★★★★ | `opening_sequence`, `pattern_cycle` |
| GM-10 | Reactive Intents | 1-2 weeks | ★★★☆☆ | `reaction_type`, `reaction_trigger` |
| GM-11 | Minion Buff Aura | 1 day | ★★★★☆ | `minion_buffs[]` |
| GM-12 | Damage Threshold Triggers | 1 day | ★★★☆☆ | `damage_thresholds[]` |
| GM-13 | HP-Based Damage Scaling | 2 hours | ★★★★★ | `rage_scale`, `rage_hp_threshold` |
| GM-14 | Damage Reflection | 1 day | ★★★☆☆ | `reflect_pct`, `reflect_turns` |
| GM-15 | Channeled Rituals | 3-5 days | ★★☆☆☆ | `cast_stages`, `stage_effects[]` |

---

## Full Boss Designs

---

### Boss 1: Rageforged Colossus

**Tier:** 2 (medium code — GM-3 Accumulator)
**Thematic identity:** A living forge that grows stronger as it's struck. The angrier it gets, the harder it hits.
**Location:** Cinder Spire (final boss variant)
**Fight structure:** Solo boss (no adds)

#### Unique Mechanic — Rage Accumulator

The Colossus uses the **Accumulator** system (GM-3). Every time a player card deals damage to it, the Colossus gains **+1 Rage** (max 20). Rage increases its damage output by `rage * 5%`.

- Rage **decays by 2 per turn** when the Colossus uses a non-attack card (shield/heal)
- Rage is displayed as a visible counter below the HP bar with a fiery color gradient
- At max Rage, the Colossus glows red-hot

#### Card Deck

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Slam | attack | tank | 1 | 1 | 10 + (rage*2) | — | 2 |
| Double Strike | attack | tank | 1 | 2 | 8 x2 + (rage*1.5) | `repeats: 2` | 1 |
| Heat Wave | aoe | all | 1 | 2 | 7 + rage | — | 1 |
| Cool Down | shield | self | 1 | 2 | — | shield:15, **rage decays 2** | 2 |
| Forge Reset | buff | self | 2 | 3 | — | **reset rage to 0**, shield:20 | 1 |
| Overheat | wipe | all | 2 | 3 | 15 + rage | `is_wipe: true` | 1 (phase 2) |

#### Phase Transitions

- **Phase 1** (100% → 50%): Standard deck. Rage maxes at 10. Forge Reset is in the deck.
- **Phase 2** (50% → 25%): Rage cap increases to **20**. Gains +1 energy/turn. Overheat enters the deck. Forge Reset removed.
- **Phase 3** (25% → 0%): **Double actions** — draws 2 cards per turn, can have 2 active intents simultaneously. Cool Down re-enters deck at 2 copies.

#### Player Strategy

- **Control your burst** — don't unload everything if Rage is already high and a big attack is incoming
- **Recognize Cool Down** — when you see the shield intent, that's your safe window to go all-in (Rage decays after its turn)
- **Save burst for Phase 2** — the Overheat wipe has a 3-turn cast; you have 3 turns to finish before it goes off
- **Guardian + shields** are critical for Phase 3 — double actions means double pressure

#### New EnemyCardDef Fields

```
int accumulate;            // per-boss accumulator value
int accumulate_max;        // cap (default 20)
int accumulate_per_hit;    // gain per player hit (default 1)
bool accumulate_decay;     // decay when using non-attack cards
int rage_damage_per;       // bonus damage per accumulator (percent * 5)
```

---

### Boss 2: Twin Oracles of Living Steel

**Tier:** 2 (medium code — GM-6 Link System)
**Thematic identity:** Two ancient constructs forged together. Damage one and the other feels it. They must be destroyed in unison.
**Location:** Sky Citadel
**Fight structure:** 2 linked enemies (Oculus + Praetor)

#### Unique Mechanic — Linked Soul

The two bosses share a **combined HP pool**. Any damage dealt to either subtracts from the shared pool. Both show the same HP bar.

- If one Oracle dies and the other is still alive, the dead one **revives** with 25% shared HP after 2 turns
- Revival can be **staggered** by dealing 10+ damage to the surviving Oracle in one turn (resets the revive countdown)
- **Simultaneous kill** (both die within 2 turns of each other) prevents revive entirely and triggers an **Overloaded Core** (bonus gold/relic reward)

#### Card Decks

**Oculus** — Ranged caster

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Arcane Lance | attack | random | 1 | 1 | 12 | — | 2 |
| Prism Spread | aoe | all | 1 | 2 | 8 | — | 1 |
| Recalibrate | shield | self | 1 | 2 | — | shield:14 | 1 |
| Linked Overcharge | buff | self | 2 | 2 | — | `buff_damage: 20`, `buff_turns: 3` | 1 |

**Praetor** — Melee shield

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Shield Bash | attack | tank | 0 | 1 | 8 + shield:8 | — | 2 |
| Fortify | shield | self | 1 | 2 | — | shield:20 | 2 |
| Ground Slam | aoe | all | 1 | 2 | 10 | — | 1 |
| Intervene | shield | self | 1 | 1 | — | shield:10, **AOE shield** to Oculus:10 | 1 |

#### Phase Transitions

At 50% shared HP, **both enter Phase 2** simultaneously:
- Both gain +2 energy/turn
- Oculus gets a second copy of Arcane Lance
- Praetor gets Rampage (tank_buster, cost 2, cast 2, 24 damage)

#### Player Strategy

- **Balance your damage** — you can't focus-fire one down; spreading damage naturally achieves linked kill
- **Use AOE efficiently** — multi-target cards hit both, making them doubly effective
- **Interrupt the revival** — if one dies early, quickly damage the survivor to stagger the 2-turn revive countdown
- **Rogue/Ranger** single-target is still good — just alternate targets

#### New EnemyDef Fields

```
char linked_group_id[16];   // shared HP pool identifier
bool is_revivable;          // can revive if partner alive
int revive_hp_pct;          // % of shared HP to revive with
int revive_cast_time;       // turns to revive
int revive_stagger_threshold; // damage to surviving partner needed to stagger
bool simultaneous_kill_reward; // bonus reward for close-to-simultaneous kill
```

---

### Boss 3: Soul Warden of the Hollow Court

**Tier:** 2 (medium code — GM-5 Tether)
**Thematic identity:** An undead jailer who chains the souls of the living. It rips your squishiest member to the front and punishes them.
**Location:** Hollow Catacombs
**Fight structure:** Solo boss

#### Unique Mechanic — Tether

The Warden marks a **random non-Guardian party member** (or lowest-HP ally if all are Guardians) as **Tethered** for 3 turns.

- Tethered ally's aggro is overridden to +50 above the current tank's aggro
- All enemy single-target abilities automatically target the tethered ally
- Taunt transfers the tether to the caster (Guardian counterplay)
- A heavy shield card on the tethered ally reduces tether duration by 1
- Chain icon on the tethered ally's portrait

#### Card Deck

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Soul Chain | attack | random | 1 | 2 | — | **apply Tether** 3 turns + MARKED 2 turns | 2 |
| Warden's Strike | attack | tank | 0 | 1 | 8 | if target tethered, **+8 bonus damage** | 2 |
| Spirit Drain | attack | tank | 1 | 1 | 12 | `lifesteal_pct: 50` | 1 |
| Chains of Agony | tank_buster | tank | 1 | 2 | 18 | if target tethered, **+6 bonus** + bleed:2x3 | 1 |
| Binding Ward | shield | self | 1 | 2 | — | shield:16 | 1 |
| Soul Harvest | wipe | all | 2 | 3 | 15 | tethered ally takes **double damage** (30) | 1 |

#### Phase Transitions

- **Phase 1** (100% → 50%): Standard deck. Tethers one ally at a time.
- **Phase 2** (50% → 0%): Gains **Soul Chain Tether all** — applies Tether to ALL non-Guardian allies simultaneously. Soul Chain cast time drops to 1. Gains +1 energy/turn.

#### Player Strategy

- **Guardian is essential** — Taunt transfers the tether to the tank where it belongs
- **Shield the squishy** — if no Guardian, pump shields on whoever gets tethered
- **Burst during non-tether windows** — when no one is tethered, the Warden's damage is much lower
- **Rogue's aggro reset** can temporarily save a tethered ally (but tether re-applies it next turn)

#### New EnemyCardDef Fields

```
bool tether;             // applies tether to target on resolve
int tether_duration;     // how many turns tether lasts
bool tether_all;         // phase 2 variant — tethers all non-tanks
int tether_bonus_damage; // bonus damage if target is tethered
```

#### New Status

```
STATUS_TETHER    — display-only status showing the tether debuff
```

---

### Boss 4: Plague Spreader

**Tier:** 2 (medium code — GM-4 Status Detonation)
**Thematic identity:** A bloated, pestilent monstrosity that coats the party in diseases and then triggers them for catastrophic damage.
**Location:** Venom Mire (final boss variant)
**Fight structure:** Solo boss

#### Unique Mechanic — Status Detonation

The Plague Spreader applies Blight (and other statuses) with its basic attacks, then uses **Detonate** cards to consume all stacks for massive bonus damage.

- Basic attacks apply minor statuses that build up over time
- Detonate cards consume all stacks, dealing `detonate_mult * stacks` bonus damage
- Players must cleanse or prevent status buildup to survive detonations
- The boss has a "Fester" card that **doubles all current blight stacks** on the party

#### Card Deck

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Noxious Spit | attack | random | 0 | 1 | 6 | apply BLIGHT:1x3 | 2 |
| Plague Cloud | aoe | all | 1 | 2 | 5 | apply BLIGHT:1x3 to all | 1 |
| Toxic Burst | attack | random | 1 | 2 | 4 | **detonate blight**, mult:8 per stack | 2 |
| Putrid Regurgitate | aoe | all | 2 | 3 | 3 | **detonate blight** on all, mult:6 per stack | 1 |
| Fester | buff | self | 1 | 2 | — | shield:12, **doubles all blight** on party | 1 |
| Mutagen | heal | self | 1 | 2 | — | heal:8 per blight stack on any target | 1 |

#### Phase Transitions

- **Phase 1:** Standard deck. Blight builds slowly. Detonations are single-target. Max blight per char: 3.
- **Phase 2** (50% HP): Gains Putrid Regurgitate (AOE detonate) and Mutagen. Starts with **2 pre-applied Blight** on each party member. Max blight: 5. Gains +1 energy/turn.

#### Player Strategy

- **Cleanse or out-heal** — Warlock's Blight payoffs backfire here; you need Cleric's Renew or high shields to survive detonations
- **Status management is key** — if you let Blight stack to 5+, the detonation will one-shot
- **Burst the boss during detonation cooldowns** — after it uses Toxic Burst, it has no detonate for 2-3 turns
- **Guardian shields** help absorb the detonation spike

#### New EnemyCardDef Fields

```
StatusType detonate_status;   // status type to consume
int detonate_mult;            // damage per stack consumed
bool detonate_aoe;            // detonate on all party members
StatusType apply_on_hit;      // status to apply on basic attacks
int apply_on_hit_amount;
int apply_on_hit_turns;
bool double_status_on_party;  // fester — doubles existing stacks
```

---

### Boss 5: Mindrender (The Archivists)

**Tier:** 3 (medium-large code — GM-8 Deck Manipulation)
**Thematic identity:** An undead archivist that steals knowledge — literally. It reaches into your deck and messes with your hand.
**Location:** Hollow Catacombs
**Fight structure:** Solo boss

#### Unique Mechanic — Deck Curse & Card Theft

The Mindrender has two ways to attack the player's deck:

- **Curse Shuffle:** Shuffles Dazed cards into the player's draw pile, polluting draws
- **Card Theft:** Steals a random card from the player's hand. The stolen card appears in the boss's cast bar. After 2 turns, it returns — OR the boss uses "Echo of the Past" to fire the stolen card's effects against the party
- **Rite of Oblivion:** Permanently exhausts cards from the player's draw pile (devastating for thin decks)

#### Card Deck

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Memory Lash | attack | random | 0 | 1 | 7 | — | 2 |
| Forget | attack | random | 1 | 2 | 5 | shuffle **2 Dazed** into player's draw pile | 2 |
| Erase | buff | self | 1 | 2 | — | **steal 1 card** from player's hand (held 2 turns) | 1 |
| Echo of the Past | attack | all | 2 | 3 | 7 | **use stolen card's effects** against party | 1 |
| Knowledge Ward | shield | self | 1 | 2 | — | shield:18 | 1 |
| Rite of Oblivion | wipe | all | 2 | 3 | 12 | also **exhaust 2 random cards** from draw pile | 1 (phase 2) |

#### Phase Transitions

- **Phase 1:** Curse focus — shuffles 2 Dazed per Forget. One copy of Erase.
- **Phase 2** (50% HP): Gains Echo of the Past, Rite of Oblivion. Steals **2 cards** at once. Curse count increases to 3. Gains +1 energy/turn.

#### Player Strategy

- **Thin deck helps** — curses are less punishing when you draw through your deck fast
- **Exhaust synergy classes** (Warlock) can turn curses into fuel
- **Priority target the stolen card** — if it steals your best card, you want to kill/burst before Echo of the Past resolves
- **Card draw classes** (Bard, Rogue) help cycle through curses

#### New EnemyCardDef Fields

```
int curse_count;              // copies of curse to shuffle
const char *curse_card_id;    // which curse card (default: "dazed")
bool steal_card;              // steals a card from player hand
int steal_return_turns;       // turns until card returns (default: 2)
bool steal_use_against;       // use stolen card's effects against player
bool exhaust_player_cards;    // exhausts cards from player's draw pile
int exhaust_count;            // how many cards to exhaust
```

#### Required Curse Cards (add to `cards.json`)

| ID | Name | Cost | Damage | Traits |
|----|------|------|--------|--------|
| dazed | Dazed | 1 | 0 | exhaust, fleeting, draw:1 |
| wound | Wound | 2 | 0 | exhaust |
| frailty | Frailty | 0 | 0 | apply WEAKNESS 15% 1 turn to self, exhaust |

---

### Boss 6: Swarm Mother

**Tier:** 4 (large code — GM-7 Invulnerability Gates + Dynamic Summoning)
**Thematic identity:** A grotesque insectoid queen that never directly attacks. She only spawns Swarmlings that do the real damage. Vulnerable only when all Swarmlings are dead.
**Location:** Venom Mire (secret/elite boss variant)
**Fight structure:** Boss + dynamically summoned Swarmlings

#### Unique Mechanic — Dynamic Summoning & Nest

- `MAX_ENEMIES` increased from 3 to **5** to accommodate adds
- The Swarm Mother has an **Invulnerability shield** that only drops when 0 Swarmlings are alive
- Swarmlings are tiny enemies with 8-15 HP that deal small damage but build up over time
- The Swarm Mother **never attacks directly** — all damage comes from Swarmlings

#### Card Deck (Swarm Mother)

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Lay Eggs | buff | self | 0 | 1 | — | summon **1 Swarmling** | 3 |
| Brood Burst | buff | self | 1 | 2 | — | summon **2 Swarmlings** | 2 |
| Hive Pulse | aoe | all | 1 | 2 | — | all Swarmlings gain **+3 damage** buff for 2 turns | 1 |
| Royal Jelly | heal | self | 1 | 2 | — | heal:15 | 1 |
| Harden Shell | shield | self | 1 | 1 | — | shield:20, **invulnerable while alive** | 1 |
| Desperate Swarm | buff | self | 2 | 3 | — | summon **4 Swarmlings**, shield:15 | 1 (phase 2) |

**Swarmling** (separate enemy definition):

| HP | Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|----|------|--------|--------|------|------|--------|---------|-------|
| 12 | Sting | attack | random | 0 | 1 | 4 | — | 2 |
| 12 | Poison Sting | attack | random | 1 | 1 | 3 | apply BLEED:1x2 | 1 |
| 12 | Burst | attack | all | 1 | 2 | 3 | apply BLEED:1x2 to all | 1 |

#### Phase Transitions

- **Phase 1:** Summons 1-2 Swarmlings per turn. All damage comes from adds.
- **Phase 2** (50% HP): Gains Desperate Swarm. Swarmlings spawn with +5 HP. Brood Burst cast time drops to 1.
- **Vulnerability window:** When 0 Swarmlings alive, Swarm Mother's invulnerability drops for 1 turn (until she uses Lay Eggs again).

#### Player Strategy

- **AOE is king** — Mage, Shaman, and AOE cards wipe out Swarmlings quickly
- **Burst windows are short** — you have ~1 turn between "all Swarmlings dead" and "new ones spawn" to damage the mother
- **Bleed builds up** — Swarmling bleeds will kill you over time if you ignore them
- **Rogue focus** — if you can kill the mother fast enough, you don't need to manage Swarmlings as carefully

#### New Systems Needed

- `MAX_ENEMIES` increased from 3 to 5 (affects UI layout, encounter system, `CombatState`)
- New: `combat_spawn_enemy(cs, const char *enemy_id, int count)` function
- New: `bool invulnerable` field on `EnemyState`, checked in `apply_damage_to_enemy()`
- UI changes to accommodate 5 enemy slots (smaller sprites, adjusted `layout.c`)

---

### Boss 7: The Nullifier (Void Archon Variant)

**Tier:** 3 (medium code — GM-2 Arena Auras)
**Thematic identity:** A being of pure anti-magic that suppresses the party's energy, increases card costs, and punishes expensive plays.
**Location:** Starfall Citadel
**Fight structure:** Solo boss

#### Unique Mechanic — Null Field & Arena Aura

The Nullifier maintains a **Null Field** aura that affects the entire party:

- **AURA_COST_UP**: All player card costs increased by +1 (cannot reduce below 0-cost)
- **AURA_ENERGY_DRAIN**: Drains 1 energy per turn from the player
- **AURA_ENEMY_DAMAGE_UP**: When the player plays a card with cost >= 3, boss gains +1 energy and draws a card

**Disrupting the aura:**
- Dealing 10+ damage in one turn: disrupts for 1 turn
- Landing an interrupt: disrupts for 2 turns
- Playing a 0-cost card: reduces aura intensity by 50% for 1 turn

#### Card Deck

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Null Bolt | attack | random | 0 | 1 | 6 | — | 2 |
| Energy Siphon | attack | tank | 1 | 2 | 8 | apply ENERGY_DRAIN:1x2 | 2 |
| Suppression Wave | aoe | all | 1 | 2 | 5 | **silence all class cards** for 1 turn | 1 |
| Void Collapse | tank_buster | tank | 2 | 2 | 18 | **+4 per player energy drained** this turn | 1 |
| Aegis of the Void | shield | self | 1 | 2 | — | shield:16, apply THORNS:3x3 | 1 |
| True Null | wipe | all | 2 | 3 | 14 | **set player energy to 0** on hit | 1 (phase 2) |

#### Phase Transitions

- **Phase 1:** Standard aura (cost +1, drain 1/turn). Standard deck.
- **Phase 2** (50% HP): Aura intensifies (cost +2, drain 2/turn). Gains True Null. Energy Siphon now applies 2 energy drain. Gains +1 energy/turn.

#### Player Strategy

- **0-cost cards are premium** — they bypass the cost increase entirely
- **Play low to the ground** — don't hoard energy; spend it before it gets drained
- **Rogue's cheap cards** and Paladin's cost-reduction perks shine
- **Interrupts are valuable** — they disrupt the aura for 2 turns, giving you a burst window
- **Bard's card draw** helps find your cheap cards

#### New Systems Needed

```
// In CombatState:
ArenaEffect arena_effects[MAX_ARENA_EFFECTS];
int arena_effect_count;

typedef struct {
    int type;        // AURA_COST_UP, AURA_ENERGY_DRAIN, AURA_ENEMY_DAMAGE_UP
    int value;       // intensity
    int turns_remaining;  // 0 = permanent (until cleared)
    int disrupt_turns;    // > 0 = aura is suppressed
} ArenaEffect;
```

---

### Boss 8: Storm Titan

**Tier:** 4 (large code — GM-1 Phase State Machine + GM-7 Invulnerability Gates + GM-2 Arena Auras)
**Thematic identity:** A primordial sky giant. Phase-gated fight where the Titan becomes invulnerable and summons storm elementals. Players must clear the room to advance.
**Location:** Sky Citadel (final boss)
**Fight structure:** Solo boss with 2 summoned Storm Elementals per gate

#### Unique Mechanic — Invulnerable Gate

The Storm Titan cycles through 3 distinct phase gates at HP thresholds (75%, 50%, 25%). At each gate:

1. Titan becomes **invulnerable** (cannot be damaged)
2. Titan summons **2 Storm Elementals**
3. An **Arena Aura** activates based on the phase
4. Player must kill all Elementals to drop the invulnerability
5. Once shield drops, Titan is vulnerable until next threshold

**Arena Auras per Gate:**
- **Gate 1 (75%):** "Winds of the Storm" — all player card costs +1 while elementals are alive
- **Gate 2 (50%):** "Lightning Field" — all allies take 3 damage at turn start while elementals are alive
- **Gate 3 (25%):** "Eye of the Storm" — all enemy damage doubled while elementals are alive

#### Card Deck (Pre-Gate Phase — Vulnerable)

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Lightning Bolt | attack | random | 1 | 1 | 10 | — | 2 |
| Storm Sweep | aoe | all | 1 | 2 | 8 | — | 1 |
| Thunder Crash | tank_buster | tank | 2 | 2 | 20 | — | 1 |
| Cloud Shield | shield | self | 1 | 2 | — | shield:20 | 1 |

**Card Deck (Gate Phase — Invulnerable, uses these instead):**

| Name | Intent | Target | Cost | Cast | Damage | Special | Count |
|------|--------|--------|------|------|--------|---------|-------|
| Summon Elementals | buff | self | 0 | 1 | — | triggers the gate phase | 1 |
| Static Discharge | aoe | all | 1 | 2 | 6 | — | 2 |
| Wind Wall | shield | self | 1 | 1 | — | shield:12 (even while invuln) | 2 |

**Storm Elemental** (separate enemy definition):

| HP | Name | Intent | Target | Cost | Cast | Damage | Count |
|----|------|--------|--------|------|------|--------|-------|
| 25 | Lightning Strike | attack | random | 0 | 1 | 6 | 2 |
| 25 | Arcane Surge | aoe | all | 1 | 2 | 4 | 1 |

#### Phase Transition Flow

1. Phase 1 Titan fights with pre-gate deck
2. At 75% HP → plays **Summon Elementals** → becomes invulnerable → 2 Elementals appear → Gate 1 arena aura activates
3. Player kills Elementals → invulnerability drops → return to pre-gate deck (with scaling)
4. Repeat at 50% (Gate 2) and 25% (Gate 3)
5. Each successive gate: Elementals spawn with +5 HP, cast times reduced by 1

#### Player Strategy

- **AOE is critical** — you need to clear adds fast to minimize exposure to arena auras
- **Phase economy** — try to time your burst so you enter the gate phase at high HP
- **Guardian taunt** helps keep Elementals off squishies
- **Save big cards** for after the gate drops — that's your damage window

#### New EnemyCardDef Fields

```
int phase_gate_hp_pct;           // HP % that triggers the gate (0 = no gate)
const char *gate_summon_id;      // enemy to spawn at gate
int gate_summon_count;           // how many to spawn
bool set_invulnerable;           // makes boss invulnerable on resolve
const char *gate_aura_type;      // arena aura type during gate
int gate_aura_value;             // aura intensity
const char *gate_deck_override;  // deck to use during gate phase
```

---

## Implementation Roadmap

### Phase 0: Foundation — Shared Infrastructure

| # | Task | Files | Effort |
|---|------|-------|--------|
| 0.1 | Add all new `EnemyCardDef` fields with 0/false defaults | `enemy_defs.h`, `enemy_defs.c` | 1 day |
| 0.2 | Add `accumulator`, `invulnerable`, `tethered_ally` to `EnemyState` | `combat.h`, `combat.c` | 1 day |
| 0.3 | Implement `ArenaEffect` array in `CombatState` | `combat.h`, `combat.c` | 2 days |
| 0.4 | Add "Dazed", "Wound", "Frailty" curse cards | `cards.json` | 1 hour |

### Phase 1: GM-3 (Accumulator) + Boss 1

| # | Task | Effort |
|---|------|--------|
| 1.1 | Implement accumulator gain triggers (on damage taken, on turn start, etc.) | 1 day |
| 1.2 | Implement accumulator spend/reset in `enemy_action()` | 1 day |
| 1.3 | UI: accumulator display on enemy HP bar | 1 day |
| 1.4 | Create Rageforged Colossus JSON data | 2 hours |

### Phase 2: GM-6 (Link) + Boss 2

| # | Task | Effort |
|---|------|--------|
| 2.1 | Implement `linked_hp` pool tracking in `CombatState` | 1 day |
| 2.2 | Implement revive timer + stagger mechanic | 1 day |
| 2.3 | UI: shared HP bar, revive countdown indicator | 1 day |
| 2.4 | Create Twin Oracles JSON data | 2 hours |

### Phase 3: GM-5 (Tether) + Boss 3

| # | Task | Effort |
|---|------|--------|
| 3.1 | Implement tether targeting (random non-Guardian, lowest-HP) | 1 day |
| 3.2 | Implement aggro override for tethered ally | 1 day |
| 3.3 | Taunt transfer interaction + tether duration tracking | 1 day |
| 3.4 | UI: chain icon, tether tooltip | 0.5 day |
| 3.5 | Create Soul Warden JSON data | 2 hours |

### Phase 4: GM-4 (Detonation) + Boss 4

| # | Task | Effort |
|---|------|--------|
| 4.1 | Implement status detonation in `enemy_action()` | 1 day |
| 4.2 | Implement "double status on party" (Fester mechanic) | 0.5 day |
| 4.3 | UI: detonation preview in cast bar | 0.5 day |
| 4.4 | Create Plague Spreader JSON data | 2 hours |

### Phase 5: GM-8 (Deck Manipulation) + Boss 5

| # | Task | Effort |
|---|------|--------|
| 5.1 | Implement curse shuffle into draw/discard piles | 1 day |
| 5.2 | Implement card steal from hand + return timer | 1 day |
| 5.3 | Implement "use stolen card effects against party" | 1 day |
| 5.4 | Implement exhaust from draw pile (Rite of Oblivion) | 1 day |
| 5.5 | UI: stolen card display in boss cast bar, curse icons | 1 day |
| 5.6 | Create Mindrender JSON data | 2 hours |

### Phase 6: GM-7 + Summoning + Boss 6

| # | Task | Effort |
|---|------|--------|
| 6.1 | Increase `MAX_ENEMIES` 3 → 5 | 1 day |
| 6.2 | Implement `combat_spawn_enemy()` function | 2 days |
| 6.3 | Implement `invulnerable` flag on `EnemyState` | 1 day |
| 6.4 | Update `layout.c` / `enemy_render.c` for 5 enemies | 2 days |
| 6.5 | Create Swarm Mother + Swarmling JSON data | 1 day |

### Phase 7: GM-2 + GM-1 + Boss 7 & 8

| # | Task | Effort |
|---|------|--------|
| 7.1 | Implement Arena Aura system (apply, tick, clear, disrupt) | 3 days |
| 7.2 | UI: aura indicators on screen | 1 day |
| 7.3 | Implement Phase State Machine (PhaseDef JSON) | 3 days |
| 7.4 | Create Nullifier JSON data | 2 hours |
| 7.5 | Create Storm Titan JSON data (including elementals) | 1 day |

### Phase 8: Remaining GM Systems

| # | Task | Effort |
|---|------|--------|
| 8.1 | GM-9: Scripted Openers & Pattern Cycling | 2-3 days |
| 8.2 | GM-10: Reactive Intents (player-triggered responses) | 1-2 weeks |
| 8.3 | GM-11: Minion Buff Aura | 1 day |
| 8.4 | GM-12: Damage Threshold Triggers | 1 day |
| 8.5 | GM-13: HP-Based Damage Scaling (Rage) | 2 hours |
| 8.6 | GM-14: Damage Reflection | 1 day |
| 8.7 | GM-15: Channeled Rituals (multi-stage casts) | 3-5 days |

---

## Boss Design Summary Matrix

| # | Boss Name | Tier | Effort | Primary GM(s) | Area | New Fields |
|---|-----------|------|--------|---------------|------|------------|
| 1 | Rageforged Colossus | 2 | Medium | GM-3 Accumulator | Cinder Spire | `accumulate_*` |
| 2 | Twin Oracles | 2 | Medium | GM-6 Link | Sky Citadel | `linked_group_id`, `revive_*` |
| 3 | Soul Warden | 2 | Medium | GM-5 Tether | Hollow Catacombs | `tether_*` |
| 4 | Plague Spreader | 2 | Medium | GM-4 Detonation | Venom Mire | `detonate_*`, `apply_on_hit` |
| 5 | Mindrender | 3 | Med-Large | GM-8 Deck Manipulation | Hollow Catacombs | `curse_*`, `steal_card` |
| 6 | Swarm Mother | 4 | Large | GM-7 Summon + Invuln | Venom Mire | `summon_*`, `invulnerable` |
| 7 | Nullifier | 3 | Medium | GM-2 Arena Auras | Starfall Citadel | `aura_*` |
| 8 | Storm Titan | 4 | Large | GM-1 + GM-2 + GM-7 | Sky Citadel | `phase_gate_*`, `gate_summon_*` |

---

## Recommended First Pass

If building sequentially for maximum replayability impact per day spent:

1. **Rageforged Colossus** (GM-3) — simplest new system, teaches accumulator mechanics
2. **Plague Spreader** (GM-4) — detonation reuses existing status system
3. **Soul Warden** (GM-5) — tether creates a completely new play pattern
4. **Twin Oracles** (GM-6) — link system enables multi-target boss design
5. **Mindrender** (GM-8) — deck manipulation changes how players build their deck
6. **Nullifier** (GM-2) — arena auras affect all fights, not just bosses
7. **Swarm Mother** (GM-7 + summoning) — gate system + dynamic enemies
8. **Storm Titan** (GM-1 + GM-2 + GM-7) — combines everything into a final exam fight
