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

The title screen now shows a first-launch consent prompt:

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
- Prefer default-off and require explicit consent before uploads.

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
10. Verify first-launch consent copy before public testing.

## Acceptance Criteria

- A full local run produces readable CSV rows for draft, combat, rewards, events, shop/rest, level-ups, tutorials, and game over.
- `scripts/analyze_metrics.py` prints useful balance tables from local logs.
- Turning telemetry off prevents all network upload.
- Network failure never blocks or crashes the game.
- No payload contains personal identifiers or local file paths.
- Cloud upload is batched at run end and capped to a fixed event limit.

## Implementation Status

Implemented in-game telemetry plumbing is now present.

- Local CSV metrics are written under `logs/`, matching the existing `logs/economy_metrics.csv` location.
- `src/systems/telemetry.h/.c` owns the shared CSV writer, JSON event buffer, run-start reset, upload flush, upload result logging, and shutdown behavior.
- `settings.cfg` supports `telemetry_opt_in=0/1`; the Settings screen exposes a telemetry toggle with consent copy.
- `settings.cfg` also supports `telemetry_prompt_seen=0/1`; the title screen shows a first-launch consent prompt until the player chooses.
- Telemetry defaults to off. Uploads are skipped unless the player opts in through the prompt or Settings.
- Windows native upload is implemented with WinHTTP and linked through CMake.
- Web/non-Windows upload paths currently fail silently and log a local skipped/failed upload row.
- Run-end upload flush is hooked from the game over summary path.
- `scripts/analyze_metrics.py` summarizes local run, draft, combat, reward, card play, relic, event, shop, rest, level-up, tutorial, death, and economy metrics.

Current metric CSV files:

- `logs/economy_metrics.csv`
- `logs/run_metrics.csv`
- `logs/combat_metrics.csv`
- `logs/death_metrics.csv`
- `logs/draft_metrics.csv`
- `logs/card_reward_metrics.csv`
- `logs/card_play_metrics.csv`
- `logs/event_metrics.csv`
- `logs/relic_metrics.csv`
- `logs/shop_metrics.csv`
- `logs/rest_metrics.csv`
- `logs/level_up_metrics.csv`
- `logs/discard_metrics.csv`
- `logs/tutorial_metrics.csv`
- `logs/telemetry_uploads.csv`

Important implementation note: the cloud endpoint in `src/systems/telemetry.c` is still a placeholder:

```c
#define TELEMETRY_ENDPOINT_HOST L"raidparty-telemetry.your-domain.workers.dev"
#define TELEMETRY_ENDPOINT_PATH L"/ingest"
```

The game is functional without replacing this. Upload attempts will simply fail and write a local row to `logs/telemetry_uploads.csv` once telemetry is opted in.

## After-Code Setup Checklist

These steps are outside the game code and should be completed before public testing or Steam Early Access.

### 1. Decide the Telemetry Policy

- Keep telemetry opt-in, not silent default-on.
- Decide whether the first public build will use only local metrics or cloud upload too.
- Write final player-facing wording for Settings and the first-launch prompt.
- Keep the promise narrow: anonymous gameplay metrics only, no names, Steam IDs, usernames, machine names, local paths, save file contents, or raw logs.

### 2. Verify the First-Launch Consent Prompt

The Settings toggle and title-screen consent prompt now exist. Before public testing, verify the final copy with the rest of the privacy text.

Required behavior:

- Show once on first launch or first run start.
- Explain exactly what is collected.
- `Allow` sets `telemetry_opt_in=1` and saves settings.
- `No Thanks` sets `telemetry_opt_in=0` and saves settings.
- Player can change the decision later in Settings.

### 3. Deploy the Cloudflare Worker

Recommended first backend:

- Cloudflare Worker
- Cloudflare D1 database
- One generic `events` table

Create a D1 database, then run:

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

Bind the D1 database to the Worker as:

```text
TELEMETRY_DB
```

Deploy an `/ingest` route that:

- Accepts only `POST`.
- Rejects payloads with more than 512 events.
- Rejects malformed JSON.
- Inserts one row per event.
- Returns `200 OK` on success.
- Does not require CORS for Windows native uploads.

Before public release, strongly consider adding a simple ingest secret or signed header so random traffic cannot freely write to the database. If you do that, add the header in `telemetry_http_post()` before shipping.

### 4. Point the Game at the Real Endpoint

After the Worker is deployed, update these constants in `src/systems/telemetry.c`:

```c
#define TELEMETRY_ENDPOINT_HOST L"your-real-worker-host.workers.dev"
#define TELEMETRY_ENDPOINT_PATH L"/ingest"
```

Then rebuild and verify `logs/telemetry_uploads.csv` records successful uploads after a completed opted-in run.

### 5. Steam and Privacy Requirements

Before enabling cloud telemetry in a Steam build:

- Add a privacy policy link on the Steam store page.
- Mention anonymous gameplay telemetry in the privacy policy.
- Mention that telemetry is optional and can be disabled in Settings.
- Do not send Steam ID unless you update the policy, UI copy, schema, and retention rules.
- Decide whether to keep WinHTTP in the Steam Windows build or later replace it with `ISteamHTTP`.

Recommended for Early Access:

- Ship opt-in telemetry only.
- Avoid Steam ID collection.
- Avoid persistent install IDs until you truly need retention analysis.

### 6. Web Build Follow-Up

The current upload implementation is Windows native only.

If the web build needs telemetry:

- Add an `emscripten_fetch` implementation inside `telemetry_flush_upload()`.
- Keep the same opt-in setting.
- Add CORS support to the Worker for the web host.
- Test browser privacy behavior and blocked network cases.

### 7. QA Matrix

Before calling telemetry functional, test each case:

- Fresh settings file: telemetry is off.
- Settings toggle on: `settings.cfg` writes `telemetry_opt_in=1`.
- Settings toggle off: `settings.cfg` writes `telemetry_opt_in=0`.
- Opted-out completed run: local CSVs exist, no upload attempt should be required.
- Opted-in completed run with valid endpoint: `logs/telemetry_uploads.csv` records success.
- Opted-in completed run with network disconnected: failure is logged locally and the game does not pause, crash, or show an error.
- Payload inspection: no local paths, Steam IDs, usernames, machine names, raw save data, or free-form personal text.
- Buffer cap: more than 512 events in a run drops extras and logs a `buffer_full` upload row.

### 8. Analysis Workflow

For local playtests:

```text
python scripts/analyze_metrics.py
```

For another log folder:

```text
python scripts/analyze_metrics.py --logs path/to/logs
```

Suggested weekly balance routine:

- Export/collect playtest logs.
- Run the analyzer.
- Check area win rates first.
- Check encounter wipe rates second.
- Compare card pick rates against card play rates.
- Check shop failures and event unavailable-choice rates for gold pressure.
- Check tutorial skips and first-seen coverage for onboarding friction.

### 9. Dashboard Later

A dashboard is not required before Early Access, but useful later.

Good first dashboard panels:

- Runs, wins, and win rate by area and ascension.
- Wipe rate by encounter and encounter type.
- Average combat turns by encounter.
- Card offered/picked/played rates.
- Relic pick rates.
- Shop spend and failed purchase rates.
- Rest heal versus upgrade choices.
- Level-up perk pick rates.
- Tutorial shown/closed/skipped counts.

### 10. Schema Discipline

- Keep `schema_version` stable for the current payload.
- Increment it when event names or field meanings change.
- Keep old dashboard queries aware of older schema versions.
- Add new fields rather than renaming existing fields whenever possible.

### 11. Current Validation Note

`cmake --build build --config Debug --target RaidParty` passes.

`scripts/analyze_metrics.py` runs and summarizes existing local economy data.

`scripts/validate_content.py` currently fails because it expects `assets/data/maps.json`, while the current runtime map system is procedural and does not load that file through `content_load_all()`. Decide separately whether to restore JSON-authored maps or update the validator to understand procedural maps.
