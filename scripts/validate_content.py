#!/usr/bin/env python3
"""Validate Raid Party JSON content manifests."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "assets" / "data"

CARD_TYPES = {"attack", "skill", "power"}
CLASSES = {"guardian", "cleric", "mage", "rogue", "shaman", "ranger", "paladin", "warlock", "bard", "utility", "curse"}
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
STATUS_EFFECTS = {
    "burning",
    "renew",
    "trap",
    "totem_heal",
    "bleed",
    "weakness",
    "energy_drain",
    "marked",
    "conductive",
    "blight",
}
ABILITY_INTENTS = {"attack", "tank_buster", "aoe", "wipe", "buff", "heal", "shield"}
RELIC_TRIGGERS = {"combat_start", "combat_reward", "card_reward", "combat", "shop", "passive", "rest"}
EVENT_EFFECTS = {
    "heal_party",
    "gain_gold_add_curse",
    "remove_card",
    "upgrade_random_card_hurt_party",
    "pay_gold_gain_relic",
    "none",
    "pay_gold_add_card",
    "gain_gold",
    "gain_reroll_token",
    "pay_gold_upgrade_random_card",
    "gain_gold_hurt_party",
    "add_card_add_curse",
    "gain_relic_add_curse",
    "duplicate_random_card_hurt_party",
    "transform_random_card",
    "gain_max_hp",
}
NODE_TYPES = {"start", "combat", "elite", "rest", "shop", "event", "boss"}


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
        if "consume" in card and not isinstance(card.get("consume"), bool):
            errors.append(f"{label}: consume must be true or false")

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
            if "status" in ability and ability.get("status") not in STATUS_EFFECTS:
                errors.append(f"{ability_label}: unsupported status {ability.get('status')!r}")
            if "status_amount" in ability:
                require_int(ability, "status_amount", ability_label, errors)
            if "status_turns" in ability:
                require_int(ability, "status_turns", ability_label, errors)

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


def validate_relics(data: dict, errors: list[str]) -> set[str]:
    relics = data.get("relics", [])
    if not isinstance(relics, list):
        errors.append("relics.json: relics must be a list")
        return set()

    seen: set[str] = set()
    for relic in relics:
        if not isinstance(relic, dict):
            errors.append("relics.json: every relic must be an object")
            continue
        label = relic.get("id", "<missing relic id>")
        require_text(relic, "id", label, errors)
        require_text(relic, "name", label, errors)
        require_text(relic, "icon", label, errors)
        require_text(relic, "description", label, errors)
        relic_id = relic.get("id")
        if relic_id in seen:
            errors.append(f"{label}: duplicate relic id")
        if isinstance(relic_id, str):
            seen.add(relic_id)
        if relic.get("trigger") not in RELIC_TRIGGERS:
            errors.append(f"{label}: unsupported relic trigger {relic.get('trigger')!r}")
        if "rarity" in relic:
            require_int(relic, "rarity", label, errors, minimum=1)
    return seen


def validate_events(data: dict, card_ids: set[str], errors: list[str]) -> set[str]:
    events = data.get("events", [])
    if not isinstance(events, list):
        errors.append("events.json: events must be a list")
        return set()

    seen: set[str] = set()
    for event in events:
        if not isinstance(event, dict):
            errors.append("events.json: every event must be an object")
            continue
        label = event.get("id", "<missing event id>")
        require_text(event, "id", label, errors)
        require_text(event, "name", label, errors)
        require_text(event, "body", label, errors)
        event_id = event.get("id")
        if event_id in seen:
            errors.append(f"{label}: duplicate event id")
        if isinstance(event_id, str):
            seen.add(event_id)

        choices = event.get("choices", [])
        if not isinstance(choices, list) or len(choices) != 3:
            errors.append(f"{label}: events must have exactly 3 choices")
            continue
        for choice in choices:
            if not isinstance(choice, dict):
                errors.append(f"{label}: every choice must be an object")
                continue
            choice_label = f"{label}.{choice.get('label', '<missing choice>')}"
            require_text(choice, "label", choice_label, errors)
            require_text(choice, "description", choice_label, errors)
            if choice.get("effect") not in EVENT_EFFECTS:
                errors.append(f"{choice_label}: unsupported event effect {choice.get('effect')!r}")
            if choice.get("effect") in {"gain_gold_add_curse", "add_card_add_curse", "gain_relic_add_curse"} and choice.get("curse") not in card_ids:
                errors.append(f"{choice_label}: unknown curse card {choice.get('curse')!r}")
    return seen


def validate_classes(data: dict, errors: list[str]) -> set[str]:
    classes = data.get("classes", [])
    if not isinstance(classes, list):
        errors.append("classes.json: classes must be a list")
        return set()

    seen: set[str] = set()
    playable = CLASSES - {"utility", "curse"}
    for cls in classes:
        if not isinstance(cls, dict):
            errors.append("classes.json: every class must be an object")
            continue
        label = cls.get("id", "<missing class id>")
        require_text(cls, "id", label, errors)
        require_text(cls, "name", label, errors)
        require_text(cls, "role", label, errors)
        require_int(cls, "hp", label, errors, minimum=1)
        class_id = cls.get("id")
        if class_id in seen:
            errors.append(f"{label}: duplicate class id")
        if isinstance(class_id, str):
            seen.add(class_id)
        if class_id not in playable:
            errors.append(f"{label}: unsupported playable class id {class_id!r}")
    missing = playable - seen
    for class_id in sorted(missing):
        errors.append(f"classes.json: missing playable class {class_id!r}")
    return seen


def validate_areas(data: dict, errors: list[str]) -> tuple[int, int]:
    areas = data.get("areas", [])
    if not isinstance(areas, list) or not areas:
        errors.append("areas.json: areas must be a non-empty list")
        return 0, 0
    seen: set[str] = set()
    max_floor_count = 0
    for index, area in enumerate(areas):
        if not isinstance(area, dict):
            errors.append("areas.json: every area must be an object")
            continue
        label = area.get("id", f"area[{index}]")
        require_text(area, "id", label, errors)
        require_text(area, "name", label, errors)
        require_text(area, "description", label, errors)
        area_id = area.get("id")
        if area_id in seen:
            errors.append(f"{label}: duplicate area id")
        if isinstance(area_id, str):
            seen.add(area_id)
        require_int(area, "floor_count", label, errors, minimum=1)
        require_int(area, "difficulty_percent", label, errors, minimum=100)
        floor_count = area.get("floor_count", 0)
        if isinstance(floor_count, int) and not isinstance(floor_count, bool):
            max_floor_count = max(max_floor_count, floor_count)
    return len(seen), max_floor_count


def validate_maps(data: dict, area_ids: set[str], errors: list[str]) -> int:
    floors = data.get("floors", [])
    if not isinstance(floors, list):
        errors.append("maps.json: floors must be a list")
        return 0

    floor_count = 0
    for floor in floors:
        if not isinstance(floor, dict):
            errors.append("maps.json: every floor must be an object")
            continue
        label = f"map floor {floor.get('floor', '?')}"
        require_int(floor, "floor", label, errors, minimum=1)
        area_id = floor.get("area") or floor.get("area_id")
        if area_id is not None:
            if not isinstance(area_id, str) or not area_id.strip():
                errors.append(f"{label}: area must be a non-empty string")
            elif area_id not in area_ids:
                errors.append(f"{label}: unknown area id {area_id!r}")
        nodes = floor.get("nodes", [])
        if not isinstance(nodes, list) or not nodes:
            errors.append(f"{label}: nodes must be a non-empty list")
            continue
        has_start = False
        has_boss = False
        for index, node in enumerate(nodes):
            node_label = f"{label}.nodes[{index}]"
            if not isinstance(node, dict):
                errors.append(f"{node_label}: node must be an object")
                continue
            if node.get("type") not in NODE_TYPES:
                errors.append(f"{node_label}: unsupported node type {node.get('type')!r}")
            has_start = has_start or node.get("type") == "start"
            has_boss = has_boss or node.get("type") == "boss"
            require_int(node, "row", node_label, errors)
            require_int(node, "col", node_label, errors)
            if "x" in node:
                require_int(node, "x", node_label, errors)
            if "y" in node:
                require_int(node, "y", node_label, errors)
            conns = node.get("connections", [])
            if not isinstance(conns, list):
                errors.append(f"{node_label}: connections must be a list")
                continue
            for conn in conns:
                if not isinstance(conn, int) or isinstance(conn, bool) or conn < 0 or conn >= len(nodes):
                    errors.append(f"{node_label}: invalid connection index {conn!r}")
                    continue
                src_y = node.get("y", node.get("row", 0))
                dst_y = nodes[conn].get("y", nodes[conn].get("row", 0))
                if isinstance(src_y, int) and isinstance(dst_y, int) and dst_y < src_y:
                    errors.append(f"{node_label}: connection to node {conn} moves upward")
        if not has_start:
            errors.append(f"{label}: missing start node")
        if not has_boss:
            errors.append(f"{label}: missing boss node")
        floor_count += 1
    return floor_count


def main() -> int:
    errors: list[str] = []
    areas = load_json("areas.json", errors)
    cards = load_json("cards.json", errors)
    classes = load_json("classes.json", errors)
    enemies = load_json("enemies.json", errors)
    encounters = load_json("encounters.json", errors)
    relics = load_json("relics.json", errors)
    events = load_json("events.json", errors)
    maps = load_json("maps.json", errors)

    area_count, max_area_floors = validate_areas(areas, errors)
    area_ids = {
        area["id"]
        for area in areas.get("areas", [])
        if isinstance(area, dict) and isinstance(area.get("id"), str)
    } if isinstance(areas.get("areas", []), list) else set()
    class_ids = validate_classes(classes, errors)
    card_ids = validate_cards(cards, errors)
    enemy_ids = validate_enemies(enemies, errors)
    encounter_count = validate_encounters(encounters, enemy_ids, errors)
    relic_ids = validate_relics(relics, errors)
    event_ids = validate_events(events, card_ids, errors)
    map_floor_count = validate_maps(maps, area_ids, errors)

    encounter_floor_count = len(encounters.get("floors", [])) if isinstance(encounters.get("floors"), list) else 0
    if max_area_floors > 0 and map_floor_count < max_area_floors:
        errors.append(f"maps.json: needs at least {max_area_floors} floors for configured areas")
    if max_area_floors > 0 and encounter_floor_count < max_area_floors:
        errors.append(f"encounters.json: needs at least {max_area_floors} floors for configured areas")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print(
        "Content validation passed: "
        f"{area_count} areas, {len(class_ids)} classes, {len(card_ids)} cards, {len(enemy_ids)} enemies, "
        f"{encounter_count} encounters, {len(relic_ids)} relics, {len(event_ids)} events, "
        f"{map_floor_count} map floors"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
