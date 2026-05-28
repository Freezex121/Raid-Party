#!/usr/bin/env python3
"""Analyze Raid Paper Legends telemetry from the Cloudflare D1 export CSV.

Usage:
    # From a downloaded CSV file:
    python scripts/analyze_metrics.py telemetry_export.csv

    # Fetch directly from Cloudflare:
    python scripts/analyze_metrics.py --url "https://raidparty-telemetry.boxeldergames.workers.dev/export?key=raidparty2026"

    # Fetch and save to file:
    python scripts/analyze_metrics.py --url "https://raidparty-telemetry.boxeldergames.workers.dev/export?key=raidparty2026" --save telemetry.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
import urllib.request
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


def fetch_csv(url: str) -> str:
    """Download CSV from a URL and return as text."""
    try:
        req = urllib.request.Request(url, headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
        })
        resp = urllib.request.urlopen(req)
        return resp.read().decode("utf-8")
    except Exception as e:
        print(f"Error fetching URL: {e}", file=sys.stderr)
        sys.exit(1)


def read_csv(path: str) -> list[dict[str, str]]:
    """Read a CSV file and return list of rows."""
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def parse_csv_text(text: str) -> list[dict[str, str]]:
    """Parse CSV text into list of rows."""
    return list(csv.DictReader(text.splitlines()))


def parse_data(row: dict[str, str]) -> dict[str, Any]:
    """Parse the `data` JSON column, return empty dict on failure."""
    try:
        return json.loads(row.get("data", "{}"))
    except (json.JSONDecodeError, TypeError):
        return {}


def pct(part: int, total: int) -> str:
    if total <= 0:
        return "0.0%"
    return f"{(part / total) * 100:.1f}%"


def print_table(title: str, rows: list[tuple], headers: tuple[str, ...], limit: int = 15) -> None:
    print(f"\n== {title} ==")
    if not rows:
        print("  (no data)")
        return
    rows = rows[:limit]
    widths = [len(h) for h in headers]
    for row in rows:
        for i, value in enumerate(row):
            widths[i] = max(widths[i], len(str(value)))
    print("  ".join(h.ljust(widths[i]) for i, h in enumerate(headers)))
    print("  ".join("-" * widths[i] for i in range(len(headers))))
    for row in rows:
        print("  ".join(str(value).ljust(widths[i]) for i, value in enumerate(row)))


# ── Section Summarizers ─────────────────────────────────────────


def summarize_drafts(rows: list[dict[str, str]]) -> None:
    class_counts: Counter[str] = Counter()
    size_counts: Counter[int] = Counter()
    area_counts: Counter[int] = Counter()
    for r in rows:
        d = parse_data(r)
        for cls in d.get("classes", []):
            class_counts[cls] += 1
        size_counts[d.get("party_size", 0)] += 1
        area_counts[d.get("area", -1)] += 1

    print_table("Class Pick Rates", class_counts.most_common(), ("Class", "Picks"))
    print_table("Party Sizes", sorted(size_counts.items()), ("Size", "Runs"))
    print_table("Runs Per Area", sorted(area_counts.items()), ("Area", "Runs"))
    if rows:
        print(f"\nTotal runs tracked: {len(rows)}")


def summarize_card_picks(rows: list[dict[str, str]]) -> None:
    offered: Counter[str] = Counter()
    picked: Counter[str] = Counter()
    skip_count = 0
    for r in rows:
        d = parse_data(r)
        for card in d.get("offered", []):
            offered[card] += 1
        chosen = d.get("picked", "")
        if chosen == "skip":
            skip_count += 1
        elif chosen:
            picked[chosen] += 1

    pick_rows = []
    for card, count in picked.most_common():
        offer_count = offered.get(card, 0)
        pick_rows.append((card, count, offer_count, pct(count, offer_count)))
    print_table("Card Pick Rates", pick_rows, ("Card", "Picked", "Offered", "Pick Rate"))
    if skip_count:
        print(f"\nSkips: {skip_count} / {len(rows)} ({pct(skip_count, len(rows))})")


def summarize_combat(rows: list[dict[str, str]]) -> None:
    by_encounter: dict[str, list[dict]] = defaultdict(list)
    by_type: dict[str, list[dict]] = defaultdict(list)
    by_area_floor: dict[str, list[dict]] = defaultdict(list)

    for r in rows:
        d = parse_data(r)
        enc_id = d.get("encounter_id", "?")
        enc_type = d.get("encounter_type", "?")
        area_label = f"Area {d.get('area', '?')} Floor {d.get('floor', '?')}"
        by_encounter[enc_id].append(d)
        by_type[enc_type].append(d)
        by_area_floor[area_label].append(d)

    encounter_rows = []
    for enc, combats in by_encounter.items():
        defeats = sum(1 for c in combats if c.get("result") == "defeat")
        avg_turns = sum(c.get("turns", 0) or 0 for c in combats) / max(1, len(combats))
        encounter_rows.append((enc, defeats, len(combats), pct(defeats, len(combats)), f"{avg_turns:.1f}"))
    encounter_rows.sort(key=lambda r: (-int(r[1]), r[0]))

    type_rows = []
    for kind, combats in sorted(by_type.items()):
        defeats = sum(1 for c in combats if c.get("result") == "defeat")
        avg_turns = sum(c.get("turns", 0) or 0 for c in combats) / max(1, len(combats))
        type_rows.append((kind, defeats, len(combats), pct(defeats, len(combats)), f"{avg_turns:.1f}"))

    area_floor_rows = []
    for label, combats in sorted(by_area_floor.items()):
        defeats = sum(1 for c in combats if c.get("result") == "defeat")
        avg_turns = sum(c.get("turns", 0) or 0 for c in combats) / max(1, len(combats))
        area_floor_rows.append((label, defeats, len(combats), pct(defeats, len(combats)), f"{avg_turns:.1f}"))

    print_table("Encounter Wipe Rates", encounter_rows, ("Encounter", "Wipes", "Total", "Wipe Rate", "Avg Turns"))
    print_table("By Type", type_rows, ("Type", "Wipes", "Total", "Wipe Rate", "Avg Turns"))
    print_table("By Area/Floor", area_floor_rows, ("Area/Floor", "Wipes", "Total", "Wipe Rate", "Avg Turns"))


def summarize_events(rows: list[dict[str, str]]) -> None:
    chosen: Counter[str] = Counter()
    blocked: Counter[str] = Counter()
    for r in rows:
        d = parse_data(r)
        eid = d.get("event_id", "?")
        label = d.get("choice_label", "?")
        key = f"{eid}: {label}"
        if d.get("could_afford") == 0 or d.get("could_afford") is False:
            blocked[key] += 1
        else:
            chosen[key] += 1

    combined: Counter[str] = Counter()
    combined.update(chosen)
    combined.update(blocked)

    choice_rows = []
    for key, total in combined.most_common():
        picks = chosen.get(key, 0)
        choice_rows.append((key, picks, total, pct(picks, total)))

    print_table("Event Choice Rates", choice_rows, ("Choice", "Picked", "Seen", "Pick Rate"))


def summarize_run_summaries(rows: list[dict[str, str]]) -> None:
    total = len(rows)
    if total == 0:
        return
    wins = sum(1 for r in rows if parse_data(r).get("won") is True)
    deaths = sum(parse_data(r).get("deaths", 0) or 0 for r in rows)
    avg_deaths = deaths / max(1, total)
    avg_gold = sum(parse_data(r).get("gold_earned", 0) or 0 for r in rows) / max(1, total)
    renown_total = sum(parse_data(r).get("renown_gained", 0) or 0 for r in rows)

    by_area: dict[int, list[dict]] = defaultdict(list)
    for r in rows:
        d = parse_data(r)
        by_area[d.get("area", -1)].append(d)

    area_rows = []
    for area, runs in sorted(by_area.items()):
        area_wins = sum(1 for r in runs if r.get("won") is True)
        area_rows.append((f"Area {area}", area_wins, len(runs), pct(area_wins, len(runs))))

    print(f"\n== Run Summary ==")
    print(f"Total runs: {total}  Wins: {wins}  Win rate: {pct(wins, total)}")
    print(f"Total deaths: {deaths}  Avg deaths/run: {avg_deaths:.2f}")
    print(f"Total renown earned: {renown_total}  Avg gold/run: {avg_gold:.0f}")
    print_table("Win Rate By Area", area_rows, ("Area", "Wins", "Runs", "Win Rate"))


def summarize_relics(rows: list[dict[str, str]]) -> None:
    offered: Counter[str] = Counter()
    picked: Counter[str] = Counter()
    for r in rows:
        d = parse_data(r)
        for relic in d.get("offered", []):
            offered[relic] += 1
        chosen = d.get("picked", "")
        if chosen:
            picked[chosen] += 1

    relic_rows = []
    for relic, count in picked.most_common():
        offer_count = offered.get(relic, 0)
        relic_rows.append((relic, count, offer_count, pct(count, offer_count)))
    print_table("Relic Pick Rates", relic_rows, ("Relic", "Picked", "Offered", "Pick Rate"))


def summarize_deaths(rows: list[dict[str, str]]) -> None:
    by_class: Counter[str] = Counter()
    by_source: Counter[str] = Counter()
    by_encounter: Counter[str] = Counter()
    for r in rows:
        d = parse_data(r)
        by_class[d.get("class", "?")] += 1
        by_source[d.get("source", "?")] += 1
        by_encounter[d.get("encounter_id", "?")] += 1
    print_table("Deaths By Class", by_class.most_common(), ("Class", "Deaths"))
    print_table("Death Sources", by_source.most_common(), ("Source", "Deaths"))
    print_table("Deadliest Encounters", by_encounter.most_common(), ("Encounter", "Deaths"))


def summarize_tutorial(rows: list[dict[str, str]]) -> None:
    by_tutorial: Counter[str] = Counter()
    by_action: Counter[str] = Counter()
    for r in rows:
        d = parse_data(r)
        by_tutorial[d.get("tutorial_id", "?")] += 1
        by_action[d.get("action", "?")] += 1
    print_table("Tutorial Events", by_tutorial.most_common(), ("Tutorial", "Count"))
    print_table("Tutorial Actions", by_action.most_common(), ("Action", "Count"))


# ── Main ────────────────────────────────────────────────────────


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze Raid Paper Legends telemetry from Cloudflare CSV export")
    parser.add_argument("file", nargs="?", help="Path to exported CSV file")
    parser.add_argument("--url", help="Fetch CSV from URL (Cloudflare export endpoint)")
    parser.add_argument("--save", help="Save fetched CSV to file")
    args = parser.parse_args()

    if args.url:
        print(f"Fetching from: {args.url}")
        text = fetch_csv(args.url)
        if args.save:
            Path(args.save).write_text(text, encoding="utf-8")
            print(f"Saved to: {args.save}")
        rows = parse_csv_text(text)
    elif args.file:
        rows = read_csv(args.file)
    else:
        parser.print_help()
        print("\nProvide either a file path or --url.")
        return 1

    if not rows:
        print("No data found.")
        return 1

    print(f"Loaded {len(rows)} telemetry events.\n")

    # Group by type
    by_type: dict[str, list[dict[str, str]]] = defaultdict(list)
    for r in rows:
        by_type[r.get("type", "unknown")].append(r)

    for t, count in sorted(by_type.items(), key=lambda x: -len(x[1])):
        print(f"  {t}: {count} events")

    # Run each summarizer
    if "draft" in by_type:
        summarize_drafts(by_type["draft"])
    if "card_pick" in by_type:
        summarize_card_picks(by_type["card_pick"])
    if "combat_result" in by_type:
        summarize_combat(by_type["combat_result"])
    if "event_choice" in by_type:
        summarize_events(by_type["event_choice"])
    if "run_summary" in by_type:
        summarize_run_summaries(by_type["run_summary"])
    if "relic_claim" in by_type:
        summarize_relics(by_type["relic_claim"])
    if "death_event" in by_type:
        summarize_deaths(by_type["death_event"])
    if "tutorial_seen" in by_type:
        summarize_tutorial(by_type["tutorial_seen"])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
