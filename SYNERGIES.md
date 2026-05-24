# Raid Paper Legends — Cross-Class Synergies Design Doc

## Current State

Each class is self-contained. Cards only care about what their own class does. The only cross-class mechanic is the combo system (same-class cost escalation for 10% damage per stack). Paladin, Warlock, and Bard already have card data in `cards.json` but aren't integrated as playable classes.

| Class | Role | Core Mechanic |
|-------|------|---------------|
| Guardian | Tank | Shields, taunt, aggro generation |
| Cleric | Healer | Single/group healing, renew, revive |
| Mage | Burst DPS | High single-target, burning, channeled AoE |
| Rogue | Interrupt DPS | Interrupt, aggro drop, backstab damage |
| Shaman | Support | Healing totems, interrupt, group shield, chain heal |
| Ranger | Sustained DPS | Consistent damage, multi-target, snare trap |

**Existing infrastructure:**
- 8 card effect types (draw_cards, gain_energy, revive_target, apply_status_X, reset_caster_aggro, transfer_aggro_to_guardian)
- 7 status effects (BURNING, RENEW, TRAP, TOTEM_HEAL, BLEED, WEAKNESS, ENERGY_DRAIN)
- Status effect attributes: `type`, `turns`, `value`
- Enemies and party members both maintain status effect arrays (max 8 each)

---

## Design Philosophy Shift

**Before:** "MMO flavored deckbuilding."

**After:** "Emergent party dynamics."

That's the difference between pretty good and potentially exceptional.

Raids are not about "my class does damage." They're about "our classes create moments together." That's the emotional space this game should live in.

The core insight: classes should be SETTING UP OTHER CLASSES. Real multiplayer raid psychology translated into solo gameplay. Examples:

- Ranger marks a target → Rogue cashes it out → Cleric benefits unexpectedly
- Shaman makes an enemy conductive → Mage's fireball chains across the entire screen
- Rogue poisons a target → Guardian's shield slam generates bonus shields

That creates "party synergy stories" — the kind of moments players remember and share.

### Design Tenets

1. **Party first, classes second.** A class's value is measured by what it enables for the rest of the party, not just its own output.
2. **Create decisions, not damage.** Synergies should ask "do I spend this now or save it?" not "which number goes up?"
3. **Disaster management over perfection.** The soul of MMO raiding is plans breaking and recovery mattering. Lean into instability.
4. **Visual clarity is non-negotiable.** Every effect must be readable at a glance from screenshots and streams.
5. **Status discipline.** Every new status must justify its existence. Too many statuses kills strategy games.

---

## Core: Cross-Class Status Interactions

The primary direction. Add new status effects that one class applies and a different class consumes for bonus effects. This reuses the existing status effect system and adds the most strategic depth per line of code.

### Status Discipline — IMPORTANT

Existing statuses: BURNING, RENEW, TRAP, TOTEM_HEAL, BLEED, WEAKNESS, ENERGY_DRAIN.

We add only **three** new statuses. Each must earn its place:

| Status | Must Justify |
|--------|--------------|
| MARKED | Visual clarity, consume-vs-maintain tension, universal readability |
| CONDUCTIVE | Changes battlefield behavior, creates insane build moments |
| BLIGHT | Fixes passive support problem, incentivizes aggressive tank/healer play |

No more without revisiting this doc. Every status must have visual clarity, unique gameplay identity, and emotional impact.

---

### MARKED — Consume vs Maintain Tension

**Theme:** Ranger spots a weakness. Allies exploit it — but consuming MARKED removes it.

Ranger's Snare Trap is reworked to apply MARKED (stacks: 2 turns) instead of TRAP. MARKED sits on the enemy and is visible via an icon above their health bar. Cards that interact with MARKED:

| Card | Change |
|------|--------|
| Rogue "Backstab" | If target is MARKED, refund 1 energy. Does not consume MARKED. |
| Rogue "Eviscerate" | If target is MARKED, deal +8 damage and CONSUME MARKED. |
| Mage "Arcane Missiles" | Each missile that hits a MARKED enemy draws 1 card (max 1 per cast). Does not consume. |
| Cleric "Smite" | If target is MARKED, also heal the lowest HP ally for 8. CONSUMES MARKED. |
| Ranger new card "Pounce" | Apply MARKED and deal 8 damage. If enemy was already MARKED, deal 12 instead (does not consume). |

**Why this works:** Consume vs maintain is premium deckbuilder design. Players constantly ask "Do I spend this synergy now or optimize around it?" Rogue wants to consume MARKED for big damage, but Ranger wants to keep it alive for consistent value. That tension creates interesting turns.

**Visual feedback:** MARKED icon pulses on the enemy. When consumed, a brief particle burst and floating text ("MARKED!") confirms the payoff.

---

### CONDUCTIVE — Battlefield-Changing AoE Synergy

**Theme:** Shaman charges the battlefield with elemental energy. Mage detonates it.

Shaman's Lightning Bolt and Windfury apply CONDUCTIVE (2 turns). While CONDUCTIVE, the enemy hums with electrical particles. Mage and Rogue abilities trigger arcs:

| Card | Change |
|------|--------|
| Shaman "Lightning Bolt" | Now also applies CONDUCTIVE (2 turns) to target. |
| Mage "Fireball" | If target is CONDUCTIVE, 50% of damage arcs to adjacent enemy. Does not consume. |
| Mage "Meteor" | Consumes CONDUCTIVE on all targets, deals +10 damage per stack consumed. |
| Rogue "Shadow Strike" | If target is CONDUCTIVE, also hits all CONDUCTIVE enemies for 50% damage. |
| New Shaman card "Chain Lightning" | 2 cost. Deal 10 damage. Applies CONDUCTIVE. Jumps to 2 random enemies. |

**Why this works:** Party composition changes battlefield behavior. "My fireball chains across the entire screen because my Shaman set it up" is the kind of synergy players remember. It's not +2 damage — it's a moment.

**Visual feedback:** CONDUCTIVE enemies have lightning particles. When arc triggers, a visible lightning bolt connects the targets. Screen shake on multi-target arcs.

---

### BLIGHT — Aggressive Support Incentive

**Theme:** Rogue and Warlock poison the enemy. Guardian and Cleric gain defensive benefits by striking those poisoned wounds.

Rogue's Poison Blade and Warlock's Corruption apply BLIGHT (3 turns). BLIGHTED enemies have a visible purple aura. Defensive classes consume it:

| Card | Change |
|------|--------|
| Rogue "Poison Blade" | Now also applies BLIGHT (3 turns, 2 value). |
| Guardian "Shield Slam" | If target is BLIGHTED, gain +6 shield. CONSUMES BLIGHT. |
| Cleric "Holy Fire" | If target is BLIGHTED, heal the caster for 10. CONSUMES BLIGHT. |
| Paladin "Judgment" | If target is BLIGHTED, apply a trap too. CONSUMES BLIGHT. |
| New Warlock card "Dark Harvest" | 2 cost. Deal 6 damage to all BLIGHTED enemies. Heal party for 3 per enemy hit. Does not consume. |

**Why this works:** Tank/healer gameplay in card games often becomes passive and boring. BLIGHT incentivizes aggressive support play — the tank wants to attack, the healer wants to throw damage. That creates more dynamic turns and more interesting decisions for classes that would otherwise just turtle.

**Visual feedback:** BLIGHTED enemies have a dripping purple icon. On consume, the effect pops with a brief shield/heal visual on the caster.

---

## Cross-Class Combo System (Supplemental)

Replace the current same-class combo system (boring +10% per stack) with named cross-class chains. When you play a card of class A followed by a card of class B, a unique synergy fires.

| Chain | Classes | Bonus |
|-------|---------|-------|
| **Shield of Faith** | Guardian → Cleric | Next heal this turn is +50% stronger |
| **Arcane Assault** | Mage → Rogue | Next attack this turn applies 2 Burning |
| **Storm Volley** | Shaman → Ranger | Next AoE this turn is +50% damage |
| **Shadow Dance** | Rogue → Cleric | Next heal is free (0 cost) |
| **Elemental Fury** | Shaman → Mage | Next fire spell is free (0 cost) |
| **Backdraft** | Rogue → Mage | Next spell this turn is +25% damage |

**Critical requirement — Visual feedback:** This mechanic DIES if combos are subtle. Every cross-class combo needs:

- **Sound effect** — distinct audio cue for each chain
- **Screen flash** — brief border flash or color tint
- **Combo name** — "ARCANE ASSAULT" slams onto the screen as floating text
- **UI indicator** — next card slot glows to show the bonus is primed

If combos are subtle, players won't emotionally register them. The feedback must be GIANT.

**Implementation:** Modify the combo logic in `resolve_card_on_target()` (combat.c). Add a lookup table of valid class→class chains alongside the existing same-class cost check.

---

## Class Pair Passives (Pruned)

Some class combos get a passive bonus just for existing in the same party. **These must be "OH SHIT" effects, not spreadsheet efficiency.** The following are kept because they change gameplay behavior, not just tick numbers:

| Composition | Passive | Why |
|-------------|---------|-----|
| Guardian + Mage | **Molten Armor**: When Guardian gains shield, deal 2 damage to a random enemy | Creates a reason to shield aggressively |
| Rogue + Ranger | **Ambush**: First card played each combat deals double damage | Changes turn 1 decision-making |
| Cleric + Rogue | **Shadow Mend**: When Rogue drops aggro (Stealth/Smoke Bomb), heal caster for 8 | Incentivizes aggro management |
| Guardian + Shaman | **Earthen Bulwark**: Guardian's shield grants totems +2 duration | Long-term planning synergy |

**Rejected:** Flat +1 burn damage, 1 HP/turn regen, generic efficiency boosts. These drift toward spreadsheet optimization. We want moments, not mathematics.

---

## Raid Mechanics Affecting Deck State (Unexplored Gold)

The strongest unexplored space: raid mechanics that mutate the deck itself. This is where the game becomes genuinely unique — raid mechanics literally change how your deck plays.

Examples:
- **Boss ability "Silence the Healer"**: All Cleric cards in hand become unplayable for 2 turns. The party must improvise without its primary healer.
- **Boss ability "Stun Tank"**: Guardian cards cost +1 until the Guardian recovers. The party needs to survive without the tank.
- **Boss ability "Corruption Pulse"**: Shuffles Curses (Doubt copies) into the draw pile. Party must clear the junk.
- **Boss ability "Mana Burn"**: Reduces max energy by 2 for 3 turns. Forces low-cost card play.
- **Boss ability "Mind Control"**: For the next turn, an ally's class cards are unplayable and deal damage to the party instead.

**Why this works:** Plans break. Synergies fail. Recovery matters. Players improvise. That's the soul of MMO raiding, and that's what makes runs memorable — not the perfect run, but the run where everything went wrong and you still pulled through.

---

## Reaction Triggers (One, Maybe)

Reaction effects are seductive but extremely dangerous. They can create nested trigger chains that turn the game into Path of Exile (incomprehensible). **At most one reaction card, kept brutally simple.**

| Card | Effect |
|------|--------|
| Guardian "Vengeful Retribution" (new) | 1 cost. Apply to self: when healed this turn, explode dealing 8 damage to all enemies. Single use. |

**Rules enforced:**
- No chain reactions (reactions cannot trigger other reactions)
- One reaction active at a time
- Duration measured in turns, not permanent
- Visual must be obvious ("VENGEFUL" pulsing on the Guardian's frame)

---

## Player Communication

All of these systems mean nothing if the player doesn't know they exist. This section covers every surface where synergy information reaches the player.

### Draft Screen — Prevention First

The draft screen is where parties are assembled. This is the player's first chance to understand class identity and potential synergies.

**Per-class synergy hints:**
When hovering a class card in the draft, show a small box listing which other classes it pairs with and a one-line summary:

```
Rogue
"Interrupt DPS — sets up BLIGHT for tanks, consumes MARKED for burst"
Pairs with: Guardian (Blight)  Ranger (Marked)  Mage (Backdraft combo)
```

**Draft panel layout:** Each class card has a thin color-coded border on the bottom indicating what party role they feed into:

| Border Color | Role |
|-------------|------|
| Red | Damage setup |
| Blue | Defense setup |
| Yellow | Hybrid/Support |
| Purple | Synergy enabler |

This way a player can glance at their party and see: "I have two red borders and one blue — I might lack defensive synergy." No reading required.

### Card Tooltips — Status Preview

Every card that applies a synergy status must state it clearly in the description.

Before:
```
"Deal damage to an enemy."
```

After:
```
"Deal 10 damage. Applies CONDUCTIVE (2 turns).
  CONDUCTIVE: Mage spells arc to adjacent enemies."
```

**Formatting rules for consistency:**
- Status names are ALL CAPS and colored (MARKED = yellow, CONDUCTIVE = blue, BLIGHT = purple)
- Apply/Consume verbs are bold: "**Applies** MARKED" / "**Consumes** MARKED"
- If a card consumes a status, the bonus value is highlighted: "gain **+6** Shield"

Every card that CONSUMES a synergy status must also STATE what happens when the status isn't present (the baseline effect) so the player can compare:

```
Shield Slam: Deal 8 damage to an enemy.
  Base: Gain 4 Shield.
  vs BLIGHTED: Gain 10 Shield instead. Consumes BLIGHT.
```

### Status Icons — In-Combat Readability

Every status effect on enemies and allies must be instantly identifiable. Rules:

- **Shape encodes category:** Circles for offensive (MARKED, BURNING, CONDUCTIVE), diamonds for defensive (RENEW, BLIGHT), squares for control (TRAP, WEAKNESS)
- **Color encodes class origin:** Yellow for Rogue/Ranger (physical), Blue for Mage/Shaman (elemental), Green for Cleric/Druid (nature), Red for Warlock (dark)
- **Duration shown as a number** overlaid on the icon
- **Stack count** shown as smaller number below (or repeated icons for low count)

Statuses with active synergy potential pulse. A MARKED enemy has a pulsing yellow target icon. This tells the player "this enemy has a synergy waiting to be triggered."

### Card Glow — Opportunity Indicators

When a card in hand would trigger a synergy against a valid target, the card glows. This is the most important UI element because it teaches players about synergies through play.

- Card border glows the color of the synergy status it would interact with
- A brief tooltip appears on hover: "Backstab vs MARKED: refunds 1 energy"
- If no valid target exists for the synergy, card does not glow (avoids false signals)

Example flow:
1. Shaman plays Lightning Bolt → enemy gains CONDUCTIVE (blue icon appears)
2. Mage draws Fireball → Fireball card border glows blue
3. Player hovers Fireball → tooltip shows: "CONDUCTIVE: 50% damage arcs to adjacent enemies"
4. Player plays Fireball → arc triggers, lightning connects enemies

The glow teaches the mechanic through gameplay. No reading required.

### Combat Log — Confirmation Text

When a synergy triggers, the combat feed (top-left log) shows a colored line confirming what happened:

```
[CONDUCTIVE] Fireball arcs to Venom Priest for 8 damage!
[MARKED] Eviscerate consumes MARKED! +8 bonus damage!
[BLIGHT] Shield Slam consumes BLIGHT! +6 bonus Shield!
```

These lines use the same color coding as the status icons, creating visual consistency.

### Combo Name Overlay — Cross-Class Combos

When a cross-class combo fires, a name overlay slams onto the screen (center, large text, 1.5 second fade):

```
⚡ ARCANE ASSAULT ⚡
Mage → Rogue: Next attack applies Burning
```

Format:
- Icon (lightning bolt, shield, etc.)
- Name in large bold text
- Subtitle with the effect description

This is the only popup in the game that covers the center of the screen. It must feel BIG because it's the game celebrating your synergy.

### Class Pair Passives — Party Screen

When the party is assembled (visible on map screen and combat screen), a small icon row below the party frames shows active pair passives:

```
[Guardian][Shield][Mage]  →  MOLTEN ARMOR icon
[Rogue][Crossed Daggers][Ranger]  →  AMBUSH icon
```

Hovering the icon shows the effect text. The icon itself depicts the mechanic visually (a shield with flames = Molten Armor). No abstract symbols.

### Permanent Reference — Collective

A new Codex/Collection screen accessible from the title screen. Organizes all game knowledge in one place:

| Tab | Content |
|-----|---------|
| Statuses | Every status effect, what it does, which classes apply/consume it |
| Synergies | Every cross-class interaction listed with examples |
| Combos | Every class→class chain with effect description |
| Classes | Class overview with their role, cards, and synergy partners |
| Pair Passives | Each composition and its passive bonus |

The Statuses page shows a preview of each status icon with its name, effect, and a one-line example:

```
MARKED
Ranger applies. Rogue/Mage/Cleric consume.
Example: "Mark an enemy with Snare Trap, then Eviscerate for +8 damage."
```

This turns the synergy system into a discoverable collection rather than an opaque mechanic.

### Tutorial — Progressive Discovery

No tutorial dumps. Teach through natural gameplay:

1. **First run with Ranger:** The Snare Trap card has a small "i" badge on it. When played, a brief tooltip says: "MARKED — other classes can exploit this."
2. **First time consuming MARKED:** The card glow teaches the player to look for it. When they play the glowing card, the synergy triggers and the combat log confirms it.
3. **After first synergy trigger:** A small notification appears: "Synergy discovered! Check your Codex for more."
4. **First time assembling specific class pair:** A subtle icon appears on the party screen with a pulse animation: "New pair passive unlocked!"

No wall of text. The game shows a mechanic, lets the player trigger it, celebrates it, then points to the Codex for deeper reference.

### Summary of Communication Layers

| Layer | What it conveys | When player sees it |
|-------|----------------|---------------------|
| Draft hints | Class identity and synergy partners | Party building |
| Card tooltips | Which status a card applies/consumes | Reading cards |
| Status icons | What's on an enemy and how long | During combat |
| Card glow | "This card has a synergy ready" | Playing cards |
| Combat log | What just happened | After action |
| Combo overlay | "You just did something cool" | Cross-class combo |
| Party passive icons | "Your composition has bonus effects" | Between combats |
| Codex | Full reference for all systems | At title screen |
| Tutorial nudges | "There's something to discover" | First playthrough |

Every layer reinforces the others. A player can discover synergies through the card glow alone, look up details in tooltips, verify results in the combat log, and find the full list in the Codex.

---

## Effort Estimates

| Proposal | Effort | New Code |
|----------|--------|----------|
| Cross-Class Status Interactions | Medium | ~200 lines across 8 files |
| Cross-Class Combo System | Small | ~80 lines in combat.c |
| Class Pair Passives (pruned) | Small | ~60 lines in combat.c |
| Raid Mechanics Affecting Deck | Medium | ~150 lines per mechanic |
| Reaction Triggers (one card) | Small | ~50 lines |

All can be implemented incrementally. The status interactions are the highest priority — they deliver the most "party synergy story" moments per line of code.

---

## Summary: The Emotional Experience

A run should produce stories like:

- "I MARKED the boss and my Rogue Eviscerated for 42 damage — game-winning play."
- "My Shaman made everything CONDUCTIVE and my Mage's Fireball chained through all three enemies. Full clear in one turn."
- "The boss silenced my Cleric on turn 2 and I had to keep my Guardian alive through shields alone for three turns. Barely survived."

That's the difference between a strategy game and a memorable experience.
