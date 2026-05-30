# New Card Effects & Passive Synergies Plan

## Part 1: Turn End Changes

### Remove auto-end-turn triggers

Delete all 4 instances of:
```c
if (cs->energy.current <= 0 || cs->deck.hand_count == 0)
    combat_end_turn_internal(cs);
```

These are at `combat.c` lines 2642, 2688, 2761, 2790.

### Pulse the End Turn button when actionable

In `run_screen.c`, the End Turn button currently renders as a static rect. Add a pulsing alpha/border animation when `cs->energy.current <= 0 || cs->deck.hand_count == 0`:

```c
Rectangle end_turn_btn = layout_end_turn_button();
bool should_pulse = (cs->energy.current <= 0 || cs->deck.hand_count == 0);
Color btn_col = { 50, 50, 80, 255 };

if (should_pulse)
{
    float pulse = 0.5f + 0.5f * sinf(GetTime() * 4.0f);
    unsigned char pulse_a = (unsigned char)(180 + 75 * pulse);
    btn_col.a = pulse_a;
}
```

When not pulsing, the button looks normal. When pulsing, the button gains a pulsing subtle alpha shift, signaling "you can end your turn now."

---

## Part 2: New Keyword Effects on Cards

### New bool fields on CardDef (`card_defs.h`)

| Field | Type | JSON Key | Default |
|-------|------|----------|---------|
| `lifesteal` | int | `"lifesteal"` | 0 |
| `splash` | int | `"splash"` | 0 |
| `retain` | bool | `"retain"` | false |
| `fleeting` | bool | `"fleeting"` | false |
| `echo` | bool | `"echo"` | false |

### Effect descriptions

| Keyword | What it does |
|---------|-------------|
| **lifesteal: N** | Heal the caster for `N` HP after dealing damage |
| **splash: N** | Deal `N` splash damage to enemies adjacent to the primary target |
| **retain** | This card is not discarded from hand at end of turn |
| **fleeting** | This card exhausts at end of turn even if not played |
| **echo** | After resolving, play a copy of this card targeting a random valid target at 50% effectiveness |

### Implementation hooks in `combat.c`

**lifesteal:** In `resolve_card_on_target()`, after damage is applied:
```c
if (card->lifesteal > 0 && dmg > 0) {
    int caster = -1;
    find_caster(cs, card->class, &caster);
    if (caster >= 0) {
        apply_heal_to_ally(cs, caster, card->lifesteal);
        combat_feed_add(cs, "%s lifesteal %d", card->name, card->lifesteal);
    }
}
```

**splash:** In the damage target loop, after dealing damage to primary:
```c
if (card->splash > 0 && target_enemy >= 0) {
    int splash = card->splash;
    int left = target_enemy - 1;
    int right = target_enemy + 1;
    if (left >= 0 && cs->enemies[left].def && cs->enemies[left].hp > 0)
        apply_damage_to_enemy(cs, left, splash);
    if (right < cs->enemy_count && cs->enemies[right].def && cs->enemies[right].hp > 0)
        apply_damage_to_enemy(cs, right, splash);
}
```

**retain:** In `advance_turn()`, before the hand discard step, scan the hand for retain cards and skip them:
```c
// In the hand discard section of advance_turn:
int write_idx = 0;
for (int i = 0; i < deck->hand_count; i++) {
    if (deck->cards[deck->hand[i]].def && deck->cards[deck->hand[i]].def->retain)
        deck->hand[write_idx++] = deck->hand[i];  // keep in hand
    else
        deck_discard_index(deck, i);  // discard normally
}
deck->hand_count = write_idx;
```

**fleeting:** Same block as retain, but exhaust fleeting cards instead of discarding:
```c
for (int i = deck->hand_count - 1; i >= 0; i--) {
    if (deck->cards[deck->hand[i]].def && deck->cards[deck->hand[i]].def->fleeting) {
        deck_exhaust_index(deck, i);
    }
}
```

**echo:** After the card resolves in `resolve_card_on_target()`, if the card has echo and hasn't already been echoed this turn:
```c
if (card->echo && !cs->echo_flag) {
    cs->echo_flag = true;  // once per card play, not once per turn
    // Resolve again on a random valid target
    resolve_card_on_target(cs, hand_idx, -1, -1, 0);
}
```

### New CardEffectType entries (`deck.h`)

| Enum | JSON Key | What it does |
|------|----------|-------------|
| `CARD_EFFECT_DEAL_DAMAGE_TO_ALL` | `"damage_all_enemies"` | Deal `amount` damage to all enemies |
| `CARD_EFFECT_SHIELD_ALL` | `"shield_all_allies"` | Grant `amount` shield to all allies |
| `CARD_EFFECT_DEBUFF_CASTER` | `"debuff_caster"` | Apply `status` to the caster for `turns` |
| `CARD_EFFECT_SHUFFLE_CARD` | `"shuffle_into_deck"` | Shuffle a copy of this card into the draw pile |

---

## Part 3: New Passive Synergies

### Design

Every class has exactly **2 pair passives** (no solo passives). Each pair covers 2 classes, so 9 pairs total cover all 9 classes with 2 synergies each.

| Class | Synergy 1 | With | Synergy 2 | With |
|-------|-----------|------|-----------|------|
| Guardian | Molten Armor | Mage | Sheltered Bulwark | Paladin |
| Cleric | Nature's Nurture | Shaman | Musical Mend | Bard |
| Mage | Molten Armor | Guardian | Magical Might | Warlock |
| Rogue | Ambush | Ranger | Sneaky Steal | Paladin |
| Shaman | Nature's Nurture | Cleric | Call of Nature | Bard |
| Ranger | Ambush | Rogue | Deadly Poison | Warlock |
| Paladin | Sheltered Bulwark | Guardian | Sneaky Steal | Rogue |
| Warlock | Magical Might | Mage | Deadly Poison | Ranger |
| Bard | Musical Mend | Cleric | Call of Nature | Shaman |

### Already implemented (2)

| Synergy | Classes | Effect | File |
|---------|---------|--------|------|
| **Molten Armor** | Guardian+Mage | Shield gained → 2 damage to random enemy | `combat.c` — existing |
| **Ambush** | Rogue+Ranger | First damaging card each combat * 1.5 damage | `combat.c` — existing |

### New synergies to add (7)

| Synergy | Classes | Effect | Implementation |
|---------|---------|--------|---------------|
| **Sheltered Bulwark** | Guardian+Paladin | When the Guardian gains Shield, the Paladin gains 2 Shield | In `apply_shield_to_ally()`, if target is Guardian and pair is alive |
| **Nature's Nurture** | Cleric+Shaman | Healing Totem heals +2 per tick | In `advance_turn()` totem tick section |
| **Musical Mend** | Cleric+Bard | First heal each turn also draws 1 card | In `resolve_card_on_target()` when hl > 0, once per turn flag |
| **Magical Might** | Mage+Warlock | First spell each turn deals +3 damage | In `resolve_card_on_target()` when dmg > 0 and (mage or warlock), once per turn |
| **Sneaky Steal** | Rogue+Paladin | All damage dealt has 10% lifesteal | In `apply_aggro_reset()` or the reset aggro card effect handler |
| **Call of Nature** | Shaman+Bard | CONDUCTIVE applications also apply MARKED for 1 turn | In `apply_status_to_enemy()` when applying CONDUCTIVE |
| **Deadly Poison** | Ranger+Warlock | BLIGHT also deals {blight turns} immediate damage | In `apply_status_to_enemy()` when applying BLIGHT, once per combat flag |

### JSON entries in `synergies.json`

All 9 pairs defined in the `pair_passives` array with their trigger and effect. Example:

```json
{
  "id": "sheltered_bulwark",
  "classes": ["guardian", "paladin"],
  "trigger": "shield_gained",
  "on_class": "guardian",
  "effect": "shield_other",
  "value": 2,
  "name": "Sheltered Bulwark",
  "desc": "Guardian+Paladin: when the Guardian gains Shield, the Paladin gains 2 Shield."
}
```

### Per-combat flags needed

Add to `CombatState` for the once-per-turn and once-per-combat passives:

```c
bool musical_mend_used;     // per turn
bool magical_might_used;    // per turn
bool deadly_poison_used;    // per combat
```

Reset turn flags in `advance_turn()`. Reset combat flags in `combat_start()`.

## Part 4: Extras

Echo bell relic should give new echo status to the first card played, rather than how it's currently set up.