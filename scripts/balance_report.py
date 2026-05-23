#!/usr/bin/env python3
"""Print a compact balance report from Raid Party JSON manifests."""

from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "assets" / "data"
PARTY_SIZE = 3
ENERGY_PER_TURN = 4


def load(name: str) -> dict:
    return json.loads((DATA_DIR / name).read_text(encoding="utf-8"))


def mean(values: list[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def card_output(card: dict, field: str) -> int:
    value = int(card.get(field, 0))
    if field == "damage":
        value *= int(card.get("repeat_hits", 1))
    return value


def per_energy(cards: list[dict], field: str) -> list[float]:
    values: list[float] = []
    for card in cards:
        cost = int(card.get("cost", 0))
        output = card_output(card, field)
        if cost > 0 and output > 0:
            values.append(output / cost)
    return values


def ability_threat(ability: dict) -> float:
    intent = ability.get("intent")
    damage = float(ability.get("base_damage", 0))
    if intent in {"aoe", "wipe"}:
        damage *= PARTY_SIZE
    if ability.get("name") in {"Dual Strike", "Rapid Strikes"}:
        damage *= 2
    return damage / max(1, int(ability.get("cast_time", 1)))


def encounter_hp(encounter: dict, enemy_by_id: dict[str, dict]) -> int:
    return sum(enemy_by_id[eid]["max_hp"] for eid in encounter["enemies"])


def encounter_threat(encounter: dict, enemy_by_id: dict[str, dict]) -> float:
    total = 0.0
    for enemy_id in encounter["enemies"]:
        enemy = enemy_by_id[enemy_id]
        total += mean([ability_threat(ability) for ability in enemy["abilities"]])
    return total


def main() -> int:
    cards = load("cards.json")["cards"]
    enemies = load("enemies.json")["enemies"]
    floors = load("encounters.json")["floors"]
    relics = load("relics.json")["relics"]
    events = load("events.json")["events"]
    enemy_by_id = {enemy["id"]: enemy for enemy in enemies}

    damage_dpe = per_energy(cards, "damage")
    heal_hpe = per_energy(cards, "heal")
    shield_spe = per_energy(cards, "shield")
    zero_cost_damage = [card for card in cards if card.get("cost") == 0 and card_output(card, "damage") > 0]

    print("Raid Party Balance Report")
    print("=========================")
    print(f"Cards: {len(cards)}")
    print(f"Average damage per energy: {mean(damage_dpe):.2f}")
    print(f"Average healing per energy: {mean(heal_hpe):.2f}")
    print(f"Average shield per energy: {mean(shield_spe):.2f}")
    print(f"Zero-cost damage cards: {', '.join(card['id'] for card in zero_cost_damage) or 'none'}")

    print()
    print("By Class")
    by_class: dict[str, list[dict]] = defaultdict(list)
    for card in cards:
        by_class[card["class"]].append(card)
    for class_name in sorted(by_class):
        class_cards = by_class[class_name]
        print(
            f"- {class_name:8s} cards={len(class_cards):2d} "
            f"dpe={mean(per_energy(class_cards, 'damage')):5.2f} "
            f"hpe={mean(per_energy(class_cards, 'heal')):5.2f} "
            f"spe={mean(per_energy(class_cards, 'shield')):5.2f}"
        )

    print()
    print(f"Enemies: {len(enemies)}")
    print(f"Average enemy HP: {mean([enemy['max_hp'] for enemy in enemies]):.1f}")
    print(f"Relics: {len(relics)}")
    print(f"Events: {len(events)}")

    expected_damage_per_turn = mean(damage_dpe) * ENERGY_PER_TURN if damage_dpe else 1.0
    print()
    print("Encounter Pressure")
    for floor in floors:
        normal = floor["normal"]
        normal_threat = mean([encounter_threat(enc, enemy_by_id) for enc in normal])
        normal_hp = mean([encounter_hp(enc, enemy_by_id) for enc in normal])
        boss = floor["boss"]
        boss_hp = encounter_hp(boss, enemy_by_id)
        boss_threat = encounter_threat(boss, enemy_by_id)
        ttk = boss_hp / expected_damage_per_turn if expected_damage_per_turn > 0 else 0.0
        print(
            f"- Floor {floor['floor']}: normal_hp={normal_hp:5.1f} "
            f"incoming/turn={normal_threat:5.1f} "
            f"boss_hp={boss_hp:3d} boss_incoming/turn={boss_threat:5.1f} "
            f"boss_ttk~{ttk:4.1f} turns"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
