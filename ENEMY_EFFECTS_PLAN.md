# Enemy Effects Expansion Plan

## Current Enemy Capabilities

| Mechanic | How it works | Limitation |
|----------|-------------|------------|
| Single-target damage | Hits highest aggro or random | Only uses `base_damage` |
| AOE/Wipe | Hits all party members | Wipe is uninterruptible |
| Heal | Heals lowest-HP enemy | Can't target specific enemy |
| Shield/Buff | Self-shield only | Can't shield allies |
| Status application | Any status on hit | Only on direct damage hits |
| Multi-hit | Name-matched ("Dual Strike", "Rapid Strikes") | Hardcoded, not data-driven |
| Enrage | Phase 1 at 50% HP, +10 shield, -1 cast time | Runs every boss |

---

## Phase A: Zero Code — JSON-Only Effects

These use existing `EnemyCardDef` fields. Just add/change `enemies.json` data.

| Effect | How | Example card |
|--------|-----|-------------|
| **Energy drain** | Set `"status": "energy_drain"` | "Mana Siphon" — hit for 8, drain 1 energy for 2 turns |
| **Apply weakness** | Set `"status": "weakness"` | "Weakening Blow" — reduces damage by 25% for 2 turns |
| **Self-heal attack** | `heal_amount` + `base_damage` on same card | "Vampiric Strike" — 10 damage + heal self 8 |
| **Apply bleed** | Use existing `"status": "bleed"` | "Deep Cut" — 6 damage + 2 bleed for 3 turns |
| **Apply burning AoE** | AOE card with `"status": "burning"` | "Cinder Breath" — 5 AOE + 2 burn for 2 turns |
| **AOE weakness** | AOE card with `"status": "weakness"` | "Terrifying Roar" — 4 AOE + 10% weakness for 2 turns |

Where to apply: 10-15 new card variations across existing enemies (normals, elites, bosses).

---

## Phase B: Small Code Changes

### B1. Lifesteal (`lifesteal_pct`)

**Field**: `int lifesteal_pct` (default 0) on `EnemyCardDef`

**enemy_action change**: After every `apply_damage_to_ally()` call in both single-target and AOE paths, heal the enemy for `damage * lifesteal_pct / 100`.

```json
{ "name": "Vampiric Bite", "lifesteal_pct": 50, "base_damage": 12, "cast_time": 2 }
```

### B2. Data-driven repeats (`repeats`)

Replace the hardcoded name-match with `int repeats` (default 1).

```json
{ "name": "Rapid Claws", "repeats": 3, "base_damage": 4, "cast_time": 2 }
```

---

## Phase C: Medium Code Changes (planned, not yet implemented)

### C1. Interrupt player (`interrupt_flag`)

`bool interrupts` on `EnemyCardDef`. On resolve, cancels any player channeling or combo prime.

```json
{ "name": "Silence", "interrupts": true, "cast_time": 3, "base_damage": 0 }
```

### C2. Damage buff (`buff_damage`, `buff_turns`)

New `INTENT_BUFF_ATTACK` that temporarily increases `cs->enemy_damage_scale`.

```json
{ "name": "Battle Cry", "intent": "buff_attack", "buff_damage": 25, "buff_turns": 3 }
```

### C3. Shield allies (reuse `target_enemy` from card throw)

Currently shield/buff always targets self. Use the card throw's pre-selected `target_enemy` to shield a different ally.

```json
{ "name": "Fortify", "intent": "shield", "target": "lowest_hp", "shield_amount": 8 }
```

### C4. AOE shield (`INTENT_AOE_SHIELD`)

Shields all living enemies.

```json
{ "name": "Bulwark", "intent": "aoe_shield", "shield_amount": 5, "cast_time": 3 }
```

### C5. Self-sacrifice (`self_damage`)

Deals damage to the enemy as a cost for a powerful effect.

```json
{ "name": "Life Tap", "self_damage": 5, "base_damage": 20, "cast_time": 1 }
```

### C6. Thorns status (`STATUS_THORNS`)

New status effect. When a Thorns-affected enemy is damaged, reflect damage back to a random party member.

```json
{ "name": "Spiked Carapace", "intent": "buff", "shield_amount": 6, "status": "thorns", "status_amount": 3, "status_turns": 4 }
```

### C7. Enrage allies (`enrage_allies`)

When a card with `enrage_allies` resolves, apply shield/buff to all other living enemies at half potency.

```json
{ "name": "War Cry", "intent": "shield", "shield_amount": 6, "enrage_allies": true }
```

---

## Phase D: Large Features (Future)

| Feature | Complexity | Notes |
|---------|-----------|-------|
| Summon adds | High | Dynamic enemy array expansion |
| Link (shared HP) | High | HP pooling + damage redistribution |
| Mark/Stack | Medium | New status + damage-on-threshold |
| Phase gate (invuln until adds dead) | High | New enemy state machine |
| Tether (pull ally) | Medium | Forced aggro mechanic |
| Stun player (skip turn) | Medium | New combat phase state |
| Steal player cards | High | Deck manipulation mid-combat |

---

## Implementation Order

1. **Phase A** content — 10-15 new enemy card variations, zero code
2. **B1 + B2** — Lifesteal + repeats (foundational)
3. **C1** — Interrupt player (big feel impact for bosses)
4. **C3 + C4** — Shield allies + AOE shield
5. **C5** — Self-damage (risk/reward)
6. **C6** — Thorns (retaliation)
7. **C2** — Buff attack damage
8. **C7** — Enrage allies (pack synergy)
