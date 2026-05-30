# Synergy System Data-Driven Rework

## Current State

All synergies are hardcoded in `combat.c` as if/else chains and `party_has_pair()` calls. Adding or tweaking a synergy requires C code changes and recompilation.

## Proposed Format

New file: `assets/data/synergies.json`

Three sections: `pair_passives`, `combos`, and `card_status_interactions`.

### Pair Passives — Always-on class pair effects

```json
{
  "pair_passives": [
    {
      "id": "molten_armor",
      "classes": ["guardian", "mage"],
      "on": "shield_gained",
      "on_class": "guardian",
      "effect": "damage_random_enemy",
      "value": 2,
      "name": "Molten Armor",
      "desc": "Guardian shield gain → random enemy takes 2 damage"
    },
    {
      "id": "earthen_bulwark",
      "classes": ["guardian", "shaman"],
      "on": "status_applied",
      "on_status": "TOTEM_HEAL",
      "effect": "extend_status",
      "value": 2,
      "name": "Earthen Bulwark",
      "desc": "Healing Totem lasts 2 extra turns"
    },
    {
      "id": "shadow_mend",
      "classes": ["cleric", "rogue"],
      "on": "aggro_self_removed",
      "on_class": "rogue",
      "effect": "heal_self",
      "value": 8,
      "name": "Shadow Mend",
      "desc": "Rogue aggro drops heal the Rogue for 8"
    },
    {
      "id": "ambush",
      "classes": ["rogue", "ranger"],
      "on": "first_damage_combat",
      "effect": "multiply_damage",
      "value": 2.0,
      "name": "Ambush",
      "desc": "First damaging card each combat deals double damage",
      "once_per_combat": true
    },
    {
      "id": "absolve",
      "classes": ["paladin", "warlock"],
      "on": "blight_consumed",
      "effect": "heal_lowest_ally",
      "value": 4,
      "name": "Absolve",
      "desc": "Consuming BLIGHT heals the lowest HP ally for 4"
    }
  ]
}
```

### Combos — Class chain combo system

```json
{
  "combos": [
    {
      "id": "shield_of_faith",
      "prev_class": "guardian",
      "next_class": "cleric",
      "title": "SHIELD OF FAITH",
      "subtitle": "Next heal +50%",
      "consume": {
        "card_type": "heal",
        "multiply_heal": 1.5
      }
    },
    {
      "id": "arcane_assault",
      "prev_class": "mage",
      "next_class": "rogue",
      "title": "ARCANE ASSAULT",
      "subtitle": "Next attack applies Burning",
      "consume": {
        "card_type": "attack",
        "apply_status": "BURNING",
        "status_turns": 3,
        "status_value": 2
      }
    },
    {
      "id": "storm_volley",
      "prev_class": "shaman",
      "next_class": "ranger",
      "title": "STORM VOLLEY",
      "subtitle": "Next AoE +50%",
      "consume": {
        "card_type": "aoe",
        "multiply_damage": 1.5
      }
    },
    {
      "id": "shadow_dance",
      "prev_class": "rogue",
      "next_class": "cleric",
      "title": "SHADOW DANCE",
      "subtitle": "Next heal costs 0",
      "consume": {
        "card_type": "heal",
        "free_cost": true
      }
    },
    {
      "id": "elemental_fury",
      "prev_class": "shaman",
      "next_class": "mage",
      "title": "ELEMENTAL FURY",
      "subtitle": "Next Mage damage card costs 0",
      "consume": {
        "card_type": "mage_fire_spell",
        "free_cost": true
      }
    },
    {
      "id": "backdraft",
      "prev_class": "rogue",
      "next_class": "mage",
      "title": "BACKDRAFT",
      "subtitle": "Next damage card +25%",
      "consume": {
        "card_type": "damage",
        "multiply_damage": 1.25
      }
    },
    {
      "id": "sacred_chorus",
      "prev_class": "paladin",
      "next_class": "bard",
      "title": "SACRED CHORUS",
      "subtitle": "Next group heal/shield +50%",
      "consume": {
        "card_type": "group_heal_or_shield",
        "multiply_heal": 1.5,
        "multiply_shield": 1.5
      }
    },
    {
      "id": "dark_refrain",
      "prev_class": "bard",
      "next_class": "warlock",
      "title": "DARK REFRAIN",
      "subtitle": "Next Warlock damage applies BLIGHT",
      "consume": {
        "card_type": "warlock_damage",
        "apply_status": "BLIGHT",
        "status_turns": 3,
        "status_value": 2
      }
    },
    {
      "id": "absolution",
      "prev_class": "warlock",
      "next_class": "paladin",
      "title": "ABSOLUTION",
      "subtitle": "Consumes BLIGHT to heal party",
      "consume": {
        "card_type": "paladin",
        "effect": "consume_blight_heal_party"
      }
    }
  ]
}
```

### Card-Status Interactions

```json
{
  "card_status_interactions": [
    { "card": "rog_evis", "status": "MARKED", "consume": true, "effect": "add_damage", "value": 8 },
    { "card": "rog_backstab", "status": "MARKED", "consume": true, "effect": "refund_energy", "value": 1 },
    { "card": "mag_missiles", "status": "MARKED", "effect": "draw_cards", "value": 1 },
    { "card": "clr_smite", "status": "MARKED", "consume": true, "effect": "add_lowest_heal", "value": 8 },
    { "card": "pal_holy_strike", "status": "MARKED", "consume": true, "effect": "add_lowest_heal", "value": 6 },
    { "card": "wlk_drain_life", "status": "MARKED", "effect": "add_lowest_heal", "value": 4 },
    { "card": "mag_fireball", "status": "CONDUCTIVE", "effect": "splash_adjacent_50" },
    { "card": "mag_meteor", "status": "CONDUCTIVE", "consume": true, "effect": "add_damage_per_stack", "value": 10 },
    { "card": "rog_shadow", "status": "CONDUCTIVE", "effect": "aoe_conductive_50" },
    { "card": "wlk_shadow_bolt", "status": "CONDUCTIVE", "effect": "apply_blight" },
    { "card": "grd_shield_slam", "status": "BLIGHT", "consume": true, "effect": "add_shield", "value": 6 },
    { "card": "clr_holy_fire", "status": "BLIGHT", "consume": true, "effect": "heal_caster", "value": 10 },
    { "card": "pal_judgment", "status": "BLIGHT", "consume": true, "effect": "apply_trap", "value": 4, "turns": 2 }
  ]
}
```

---

## Implementation

### New files

| File | Purpose |
|------|---------|
| `src/data/synergy_defs.h` | Struct definitions for synergy, combo, card_status |
| `src/data/synergy_defs.c` | JSON loading + lookup functions |

### Struct definitions (`synergy_defs.h`)

```c
// Pair passive synergy
typedef struct {
    const char *id;
    const char *classes[2];       // two class IDs
    const char *trigger;          // "shield_gained", "first_damage_combat", etc.
    const char *on_class;         // optional: class that triggers it
    const char *on_status;        // optional: status type for status triggers
    const char *effect_type;      // "damage_random_enemy", "heal_self", "multiply_damage", etc.
    float value;                  // numeric parameter
    bool once_per_combat;         // flag for one-shot effects
    const char *name;
    const char *desc;
} SynergyPairDef;

// Combo chain
typedef struct {
    const char *id;
    ClassType prev_class;
    ClassType next_class;
    const char *title;
    const char *subtitle;
    const char *consume_card_type;  // "heal", "attack", "aoe", "damage", etc.
    bool free_cost;
    float multiply_damage;
    float multiply_heal;
    float multiply_shield;
    const char *apply_status;       // status to apply on hit
    int status_turns;
    int status_value;
    const char *special_effect;     // "consume_blight_heal_party"
} SynergyComboDef;

// Card-status interaction
typedef struct {
    const char *card_id;
    StatusType status;
    bool consume;
    const char *effect;           // "add_damage", "refund_energy", "heal_caster", etc.
    int value;
    int turns;
} SynergyCardStatusDef;
```

### Replaced code in `combat.c`

The following hardcoded sections get replaced with data-driven loops:

| Current lines | Hardcoded logic | Replaced with |
|--------------|----------------|---------------|
| 207-210 | `party_has_pair()` | Same function, stays |
| 453 | Guardian+Mage shield damage | Loop over `synergy_pairs`, check classes, if trigger matches → apply effect |
| 1016 | Guardian+Shaman totem extend | Same |
| 1036, 1064 | Cleric+Rogue aggro heal | Same |
| 1376 | Rogue+Ranger Ambush | Same |
| 1817 | Paladin+Warlock BLIGHT heal | Same |
| 1123-1173 | `combo_check_chain()`, `combo_prime_set()` | Loop over `synergy_combos` by matching prev/next class |
| 1338-1373 | Combo consume effects | Loop over `synergy_combos` by matching `spent_prime` ID |
| 1436-1547 | Card-status interactions | Loop over `card_status_interactions` by matching card ID |

### Changed files

| File | Change |
|------|--------|
| `src/data/synergy_defs.h` | **New** — struct definitions |
| `src/data/synergy_defs.c` | **New** — JSON loading + lookup functions |
| `src/data/content_loader.c` | Add `synergy_defs_load_json()` call |
| `src/combat/combat.c` | Replace 7 hardcoded pair checks + 9 combo chain checks + ~30 card-status checks with data-driven loops |
| `src/combat/combat.h` | Remove `ComboPrime` enum? Or keep it for internal use but derive values from JSON indices |
| `assets/data/synergies.json` | **New** — all pair passives, combos, card-status interactions |

### What stays hardcoded (not worth generalizing)

- Draft screen synergy hint text (stays in draft_screen.c — cosmetic, not gameplay)
- Combo prime visual effects (banner animation, screen flash — rendering code)
- The "is this card a heal/attack/aoe/fire_spell" helpers (`card_is_heal_card()`, etc.) — these are card-specific logic that belongs in card_defs or stays as helper functions

---

## Benefits

1. **Edit values without recompiling** — tweak Ambush from 2.0x to 1.5x in JSON
2. **Add new class pairs** — add a new entry to `pair_passives` array
3. **Add new combo chains** — add a new entry to `combos` array
4. **See all synergies in one place** — single file documents the entire system
5. **No C code changes for balance** — content team (you) doesn't need to touch combat.c

## Risks

1. **Performance** — trivial, these are tiny arrays (< 50 entries) checked once per card play
2. **Bug migration** — rewriting 7+ hardcoded checks to data-driven loops could introduce edge-case bugs. Mitigation: keep old code alongside new with a compile flag, test both paths produce identical results
3. **JSON loading failure** — if synergies.json is missing, all synergies silently fail. Mitigation: log a warning but don't crash
