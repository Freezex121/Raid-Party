#!/usr/bin/env python3
"""Validate Raid Party JSON content manifests."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "assets" / "data"

CARD_TYPES = {"attack", "skill", "power"}
CLASSES = {"guardian", "cleric", "mage", "rogue", "shaman", "ranger", "utility"}
TARGETS = {"enemy", "all_enemies", "ally", "all_allies", "self"}
EFFECTS = {
    "draw_cards",
    "gain_energy",
    "revive_target",
    "apply_status_target_enemy",
    "apply_status_target_ally",
    "apply_status_all_allies",
    "reset_caster_aggro",
    "transfer_aggro_to_guardian",
}
STATUS_EFFECTS = {"burning", "renew", "trap", "totem_heal"}
ABILITY_INTENTS = {"attack", "tank_buster", "aoe", "wipe", "buff", "heal", "shield"}


def load_json(name: str, errors: list[str]) -> dict:
    path = DATA_DIR / name
    if not path.exists():
        errors.append(f"missing file: {path}")
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        errors.append(f"{name}: invalid JSON at line {exc.lineno}: {exc.msg}")
        return {}


def require_text(obj: dict, field: str, label: str, errors: list[str]) -> None:
    if not isinstance(obj.get(field), str) or not obj[field].strip():
        errors.append(f"{label}: missing {field}")


def require_int(obj: dict, field: str, label: str, errors: list[str], minimum: int = 0) -> None:
    value = obj.get(field)
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum:
        errors.append(f"{label}: {field} must be an integer >= {minimum}")


def validate_cards(data: dict, errors: list[str]) -> set[str]:
    cards = data.get("cards", [])
    if not isinstance(cards, list):
        errors.append("cards.json: cards must be a list")
        return set()

    seen: set[str] = set()
    for card in cards:
        if not isinstance(card, dict):
            errors.append("cards.json: every card must be an object")
            continue

        label = card.get("id", "<missing card id>")
        require_text(card, "id", label, errors)
        require_text(card, "name", label, errors)
        require_text(card, "description", label, errors)

        card_id = card.get("id")
        if card_id in seen:
            errors.append(f"{label}: duplicate card id")
        if isinstance(card_id, str):
            seen.add(card_id)

        if card.get("type") not in CARD_TYPES:
            errors.append(f"{label}: unsupported card type {card.get('type')!r}")
        if card.get("class") not in CLASSES:
            errors.append(f"{label}: unsupported class {card.get('class')!r}")
        if card.get("target") not in TARGETS:
            errors.append(f"{label}: unsupported target {card.get('target')!r}")

        for field in (
            "cost",
            "damage",
            "heal",
            "heal2",
            "shield",
            "burn_stacks",
            "taunt_turns",
            "aggro_self",
            "channel_turns",
        ):
            require_int(card, field, label, errors)
        require_int(card, "repeat_hits", label, errors, minimum=1)

        for field in ("heal_self", "taunt", "interrupt", "exhaust", "channel"):
            if not isinstance(card.get(field), bool):
                errors.append(f"{label}: {field} must be true or false")

        if card.get("channel") and card.get("channel_turns", 0) <= 0:
            errors.append(f"{label}: channel cards need channel_turns > 0")
        if card.get("burn_stacks", 0) > 0:
            has_burn_effect = any(
                fx.get("type") == "apply_status_target_enemy" and fx.get("status") == "burning"
                for fx in card.get("effects", [])
                if isinstance(fx, dict)
            )
            if not has_burn_effect:
                errors.append(f"{label}: burn_stacks should be backed by a burning effect")

        effects = card.get("effects", [])
        if not isinstance(effects, list):
            errors.append(f"{label}: effects must be a list")
            continue
        for effect in effects:
            if not isinstance(effect, dict):
                errors.append(f"{label}: every effect must be an object")
                continue
            effect_type = effect.get("type")
            if effect_type not in EFFECTS:
                errors.append(f"{label}: unsupported effect type {effect_type!r}")
            require_int(effect, "amount", f"{label}.{effect_type}", errors)
            require_int(effect, "turns", f"{label}.{effect_type}", errors)
            if effect_type and effect_type.startswith("apply_status"):
                if effect.get("status") not in STATUS_EFFECTS:
                    errors.append(f"{label}: unsupported status {effect.get('status')!r}")

    return seen


def validate_enemies(data: dict, errors: list[str]) -> set[str]:
    enemies = data.get("enemies", [])
    if not isinstance(enemies, list):
        errors.append("enemies.json: enemies must be a list")
        return set()

    seen: set[str] = set()
    for enemy in enemies:
        if not isinstance(enemy, dict):
            errors.append("enemies.json: every enemy must be an object")
            continue

        label = enemy.get("id", "<missing enemy id>")
        require_text(enemy, "id", label, errors)
        require_text(enemy, "name", label, errors)
        require_int(enemy, "max_hp", label, errors, minimum=1)

        enemy_id = enemy.get("id")
        if enemy_id in seen:
            errors.append(f"{label}: duplicate enemy id")
        if isinstance(enemy_id, str):
            seen.add(enemy_id)

        abilities = enemy.get("abilities", [])
        if not isinstance(abilities, list) or not 1 <= len(abilities) <= 4:
            errors.append(f"{label}: ability count must be 1..4")
            continue
        for ability in abilities:
            if not isinstance(ability, dict):
                errors.append(f"{label}: every ability must be an object")
                continue
            ability_label = f"{label}.{ability.get('name', '<missing ability>')}"
            require_text(ability, "name", ability_label, errors)
            require_text(ability, "description", ability_label, errors)
            if ability.get("intent") not in ABILITY_INTENTS:
                errors.append(f"{ability_label}: unsupported intent {ability.get('intent')!r}")
            require_int(ability, "base_damage", ability_label, errors)
            require_int(ability, "cast_time", ability_label, errors, minimum=1)
            require_int(ability, "heal_amount", ability_label, errors)
            require_int(ability, "shield_amount", ability_label, errors)
            if not isinstance(ability.get("is_wipe"), bool):
                errors.append(f"{ability_label}: is_wipe must be true or false")

    return seen


def validate_encounters(data: dict, enemy_ids: set[str], errors: list[str]) -> int:
    floors = data.get("floors", [])
    if not isinstance(floors, list):
        errors.append("encounters.json: floors must be a list")
        return 0

    encounter_count = 0
    seen: set[str] = set()

    def check_encounter(encounter: dict, label: str) -> None:
        nonlocal encounter_count
        if not isinstance(encounter, dict):
            errors.append(f"{label}: encounter must be an object")
            return
        encounter_id = encounter.get("id")
        require_text(encounter, "id", label, errors)
        if encounter_id in seen:
            errors.append(f"{label}: duplicate encounter id {encounter_id!r}")
        if isinstance(encounter_id, str):
            seen.add(encounter_id)
        enemies = encounter.get("enemies", [])
        if not isinstance(enemies, list) or not 1 <= len(enemies) <= 3:
            errors.append(f"{label}: enemies must contain 1..3 ids")
            return
        for enemy_id in enemies:
            if enemy_id not in enemy_ids:
                errors.append(f"{label}: unknown enemy id {enemy_id!r}")
        encounter_count += 1

    for floor in floors:
        if not isinstance(floor, dict):
            errors.append("encounters.json: every floor must be an object")
            continue
        floor_label = f"floor {floor.get('floor', '?')}"
        require_int(floor, "floor", floor_label, errors, minimum=1)
        normals = floor.get("normal", [])
        if not isinstance(normals, list) or not normals:
            errors.append(f"{floor_label}: normal must be a non-empty list")
        else:
            for index, encounter in enumerate(normals):
                check_encounter(encounter, f"{floor_label}.normal[{index}]")
        check_encounter(floor.get("elite", {}), f"{floor_label}.elite")
        check_encounter(floor.get("boss", {}), f"{floor_label}.boss")

    return encounter_count


def main() -> int:
    errors: list[str] = []
    cards = load_json("cards.json", errors)
    enemies = load_json("enemies.json", errors)
    encounters = load_json("encounters.json", errors)

    card_ids = validate_cards(cards, errors)
    enemy_ids = validate_enemies(enemies, errors)
    encounter_count = validate_encounters(encounters, enemy_ids, errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print(
        "Content validation passed: "
        f"{len(card_ids)} cards, {len(enemy_ids)} enemies, {encounter_count} encounters"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
