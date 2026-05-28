# Metrics and Telemetry Plan

## Summary

Build metrics in two phases:

1. Local metrics first: structured CSV files in `logs/`, always available for solo playtesting and development.
2. Optional cloud telemetry later: explicit player consent, batched upload at run end, no gameplay dependency on network success.

The immediate goal is better balance and onboarding decisions, not a full analytics platform. Local metrics plus a good analyzer should answer the most important questions before Early Access.

## Goals

- Understand class pick rates, card pick rates, card play rates, relic value, event choices, combat difficulty, shop behavior, rest choices, level-up choices, and onboarding friction.
- Keep instrumentation lightweight and easy to debug.
- Never block gameplay on metrics or telemetry.
- Make cloud telemetry optional, clearly explained, and safe to disable.
- Prefer aggregate gameplay data over player-identifying data.

## Non-Goals

- No Steam ID collection in the first version.
- No personal information collection.
- No real-time analytics dashboard required before Early Access.
- No networking requirement for local development or playtesting.
- No gameplay behavior should depend on upload success.

## Architecture

```text
Game
  |
  | always
  v
Local CSV logs in logs/
  |
  | optional, later, only if consented
  v
Run-end JSON batch -> Cloudflare Worker -> D1 events table
```

## Phase 1: Local Metrics

Local metrics are the first implementation target. They are useful immediately and avoid privacy/platform complexity.

### Files

- `logs/economy_metrics.csv`: existing gold flow metrics.
- `logs/run_metrics.csv`: one row per completed run.
- `logs/combat_metrics.csv`: one row per combat result.
- `logs/draft_metrics.csv`: one row per run start.
- `logs/card_reward_metrics.csv`: one row per reward pick or skip.
- `logs/card_play_metrics.csv`: one row per card played.
- `logs/event_metrics.csv`: one row per event choice.
- `logs/relic_metrics.csv`: one row per relic claim.
- `logs/shop_metrics.csv`: one row per shop purchase/reroll/leave.
- `logs/rest_metrics.csv`: one row per rest choice.
- `logs/level_up_metrics.csv`: one row per level-up choice.
- `logs/tutorial_metrics.csv`: one row per tutorial shown or skipped.

### Local Analyzer

Add `scripts/analyze_metrics.py`.

The analyzer should print:

- Area win rates.
- Encounter wipe rates.
- Average combat turns by encounter type.
- Draft class pick rates.
- Card offered, picked, skipped, and play rates.
- Card pick rate versus play rate.
- Relic pick rates.
- Event choice rates and unavailable-choice rates.
- Shop purchase rates and gold pressure.
- Rest heal versus upgrade rates.
- Level-up perk pick rates.
- Tutorial first-seen counts and skip counts.

This should be implemented before cloud upload.

## Phase 2: Consent and Settings

Cloud telemetry should not be default-on silently. Use explicit consent.

### Settings

Add to `settings.cfg`:

```text
telemetry_opt_in=0
```

Default should be `0` until a clear consent prompt exists.

### Settings Screen

Add a toggle:

```text
TELEMETRY: OFF
```

Supporting copy:

```text
Sends anonymous gameplay metrics to help balance the game.
No names, Steam IDs, chat, or personal information are collected.
You can turn this off at any time.
```

### First-Launch Consent

Before Early Access, add a first-launch consent prompt:

```text
Help improve Raid Paper Legends?

Send anonymous gameplay metrics such as card choices, combat outcomes,
shop purchases, and run results. No personal information is collected.

[Allow] [No Thanks]
```

If the player chooses:

- Allow: set `telemetry_opt_in=1`.
- No Thanks: set `telemetry_opt_in=0`.

## Phase 3: Event Buffer API

Add `src/systems/telemetry.h`.

```c
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>

void telemetry_init(void);
void telemetry_push_json(const char *event_type, const char *json_data);
void telemetry_flush_local_only(void);
void telemetry_flush_upload(void);
void telemetry_shutdown(void);

#endif
```

### Behavior

- `telemetry_push_json()` stores events in an in-memory run buffer.
- Local CSV logging should still happen at the event site or through helper functions.
- Upload buffer limit should be fixed, for example 512 events per run.
- If the buffer fills, drop extra events and log one `telemetry_dropped` local row.
- `telemetry_flush_upload()` is a no-op unless `g_state.telemetry_opt_in` is true.
- Upload failure should be silent in-game and logged locally.

## Phase 4: Metrics Categories

### Draft

Logged when the player begins a run.

```json
{
  "type": "draft",
  "area": 0,
  "ascension": 3,
  "party_size": 3,
  "classes": ["guardian", "cleric", "mage"],
  "max_party_size": 4,
  "party_not_full": true
}
```

Answers:

- Which classes are overpicked or ignored?
- Do players actually run fewer than max party size?
- Are unlockable classes being used?

### Card Reward Pick

Logged when a card is picked, skipped, rerolled, or an extra choice is bought.

```json
{
  "type": "card_pick",
  "encounter": "elite",
  "area": 0,
  "floor": 2,
  "offered": ["holy_strike", "backstab", "renew"],
  "picked": "renew",
  "action": "pick",
  "pick_number": 1,
  "picks_remaining": 0,
  "gold": 45
}
```

Answers:

- Which cards are always picked when offered?
- Which cards are skipped?
- Which rewards are worth rerolling for?

### Card Play

Logged every time a card is played.

```json
{
  "type": "card_play",
  "card_id": "shield_bash",
  "class": "guardian",
  "energy_cost": 2,
  "combat_turn": 3,
  "target_type": "enemy",
  "upgraded": 1,
  "exhaust": false,
  "consume": false
}
```

Answers:

- Which cards are picked but not played?
- Which classes gain XP fastest?
- Are high-energy cards worth their cost?
- Are consume cards being used or hoarded?

### Combat Result

Logged when combat ends.

```json
{
  "type": "combat_result",
  "area": 0,
  "floor": 2,
  "encounter_id": "forest_elite_01",
  "encounter_type": "elite",
  "result": "victory",
  "turns": 7,
  "deaths": 0,
  "interrupts": 1,
  "gold_reward": 18
}
```

Answers:

- Which encounters kill runs?
- Are elites worth their risk?
- Are bosses too long or too short?

### Death Event

Logged when a party member dies.

```json
{
  "type": "death_event",
  "class": "mage",
  "area": 0,
  "floor": 2,
  "encounter_id": "forest_elite_01",
  "combat_turn": 5,
  "source": "enemy_cast"
}
```

Answers:

- Which classes die most?
- Are backline classes too fragile?
- Are certain enemy casts deleting players?

### Event Choice

Logged after an event choice resolves or fails.

```json
{
  "type": "event_choice",
  "event_id": "strange_merchant",
  "choice_index": 2,
  "choice_label": "Buy Scroll",
  "choice_effect": "pay_gold_add_card",
  "choice_gold": 25,
  "gold_before": 45,
  "gold_after": 20,
  "could_afford": true,
  "chosen": true
}
```

Answers:

- Which choices are unattractive?
- Which choices are unavailable due to gold pressure?
- Which events are too punishing or too generous?

### Relic Claim

Logged when a relic is claimed.

```json
{
  "type": "relic_claim",
  "source": "boss_reward",
  "offered": ["echo_bell", "blood_amber", "spirit_stone"],
  "picked": "echo_bell",
  "area": 1,
  "floor": 3
}
```

Answers:

- Which relics are auto-picks?
- Which relics are ignored?
- Are boss rewards exciting enough?

### Shop Purchase

Logged on shop purchase, reroll, failed purchase, and leave.

```json
{
  "type": "shop_purchase",
  "action": "upgrade_card",
  "cost": 30,
  "gold_before": 44,
  "gold_after": 14,
  "success": true,
  "card_id": "renew",
  "area": 0,
  "floor": 2
}
```

Answers:

- Are prices too high?
- Which shop services are ignored?
- Does gold economy support meaningful choices?

### Rest Choice

Logged when the player uses a rest site.

```json
{
  "type": "rest_choice",
  "choice": "upgrade",
  "card_id": "shield_bash",
  "party_missing_hp": 18,
  "upgradeable_cards": 6,
  "area": 0,
  "floor": 2
}
```

Answers:

- Do players heal too often?
- Are upgrades too valuable compared with survival?
- Are rest sites meaningful?

### Level-Up Choice

Logged when a perk is chosen.

```json
{
  "type": "level_up_choice",
  "class": "guardian",
  "level": 3,
  "offered": ["hp_plus_4", "guardian_mastery"],
  "picked": "guardian_mastery",
  "combat_xp_this_fight": 12
}
```

Answers:

- Which perks dominate?
- Are class-specific perks exciting?
- Are certain PMs leveling too fast or too slow?

### Tutorial

Logged when tutorial overlays are shown, closed, or skipped.

```json
{
  "type": "tutorial_seen",
  "tutorial_id": "combat_threat",
  "action": "shown",
  "screen": "run",
  "runs_completed": 0
}
```

Answers:

- Which tutorials are being skipped?
- Are players reaching key onboarding moments?
- Does tutorial order match actual play order?

### Run Summary

Logged and flushed at game over.

```json
{
  "type": "run_summary",
  "won": true,
  "area": 2,
  "floors_cleared": 5,
  "bosses_defeated": 4,
  "deaths": 1,
  "interrupts": 3,
  "ascension": 5,
  "renown_gained": 24,
  "gold_earned": 340,
  "deck_size": 18,
  "relic_count": 5
}
```

Answers:

- Are runs too easy or too hard?
- Does ascension scale properly?
- Does run length feel right?

## Phase 5: Cloud Upload

Only implement after local metrics and analyzer are useful.

### Upload Rules

- Upload only at run end.
- Upload only if `telemetry_opt_in == 1`.
- Set short network timeouts.
- Fail silently in-game.
- Log upload success/failure locally.
- Do not retry forever.

### Initial Platform

Windows native can use WinHTTP because it is built into Windows. Later:

| Build | HTTP option |
| --- | --- |
| Windows native | WinHTTP |
| Steam | ISteamHTTP, after Steamworks integration |
| Web | emscripten_fetch |

### Payload

```json
{
  "schema_version": 1,
  "game_version": "0.1.0",
  "run_id": 47,
  "timestamp": 1715000000,
  "events": []
}
```

Avoid persistent install IDs at first. Add one later only if retention analysis becomes necessary and the privacy disclosure is updated.

## Cloudflare Worker

Use a single generic events table first.

```sql
CREATE TABLE events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER NOT NULL,
  timestamp INTEGER NOT NULL,
  type TEXT NOT NULL,
  schema_version INTEGER NOT NULL,
  game_version TEXT NOT NULL,
  data TEXT NOT NULL
);

CREATE INDEX idx_events_type ON events(type);
CREATE INDEX idx_events_run ON events(run_id);
CREATE INDEX idx_events_time ON events(timestamp);
```

Worker outline:

```js
export default {
  async fetch(request, env) {
    if (request.method !== "POST") {
      return new Response("POST only", { status: 405 });
    }

    let payload;
    try {
      payload = await request.json();
    } catch {
      return new Response("Bad JSON", { status: 400 });
    }

    const { run_id, events, schema_version, game_version } = payload;
    if (!run_id || !Array.isArray(events) || events.length > 512) {
      return new Response("Bad request", { status: 400 });
    }

    const timestamp = Math.floor(Date.now() / 1000);
    const db = env.TELEMETRY_DB;

    for (const event of events) {
      const { type, ...data } = event;
      if (!type || typeof type !== "string") continue;

      await db.prepare(
        "INSERT INTO events (run_id, timestamp, type, schema_version, game_version, data) VALUES (?1, ?2, ?3, ?4, ?5, ?6)"
      ).bind(run_id, timestamp, type, schema_version || 1, game_version || "unknown", JSON.stringify(data)).run();
    }

    return new Response("OK", { status: 200 });
  }
};
```

## Example Queries

### Card Pick Rate

```sql
SELECT
  json_extract(data, '$.picked') AS card,
  COUNT(*) AS picks
FROM events
WHERE type = 'card_pick'
  AND json_extract(data, '$.action') = 'pick'
GROUP BY card
ORDER BY picks DESC;
```

### Card Play Rate

```sql
SELECT
  json_extract(data, '$.card_id') AS card,
  COUNT(*) AS plays
FROM events
WHERE type = 'card_play'
GROUP BY card
ORDER BY plays DESC;
```

### Encounter Wipe Rate

```sql
SELECT
  json_extract(data, '$.encounter_id') AS encounter,
  SUM(CASE WHEN json_extract(data, '$.result') = 'defeat' THEN 1 ELSE 0 END) AS wipes,
  COUNT(*) AS total
FROM events
WHERE type = 'combat_result'
GROUP BY encounter
ORDER BY wipes DESC;
```

### Level-Up Pick Rate

```sql
SELECT
  json_extract(data, '$.picked') AS perk,
  COUNT(*) AS picks
FROM events
WHERE type = 'level_up_choice'
GROUP BY perk
ORDER BY picks DESC;
```

## Privacy Notes

- Treat cloud telemetry as consent-based, even if the payload is gameplay-only.
- Do not send Steam IDs, usernames, save paths, IP addresses, machine names, or free-form text.
- Do not send local file paths.
- Do not send raw logs.
- Explain telemetry in Settings and the Steam store/privacy text.
- Keep the game fully playable if telemetry is off.
- Prefer default-off until the first-launch consent prompt exists.

## Implementation Order

1. Add `scripts/analyze_metrics.py` for existing and new local CSVs.
2. Add missing local CSV metrics for draft, card rewards, card plays, combat, events, relics, shop, rest, level-ups, tutorials, and run summaries.
3. Add `telemetry_opt_in` to settings, default off.
4. Add Settings toggle and explanatory text.
5. Add `src/systems/telemetry.h/.c` with in-memory buffering, but keep upload disabled.
6. Add telemetry push calls alongside local logs.
7. Add run-end flush hook.
8. Deploy Cloudflare Worker and D1 schema.
9. Enable cloud upload behind the setting.
10. Add a first-launch consent prompt before public testing.

## Acceptance Criteria

- A full local run produces readable CSV rows for draft, combat, rewards, events, shop/rest, level-ups, tutorials, and game over.
- `scripts/analyze_metrics.py` prints useful balance tables from local logs.
- Turning telemetry off prevents all network upload.
- Network failure never blocks or crashes the game.
- No payload contains personal identifiers or local file paths.
- Cloud upload is batched at run end and capped to a fixed event limit.
