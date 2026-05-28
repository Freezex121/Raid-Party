# Metrics & Telemetry Plan

## Overview

Two-layer system:

1. **Local CSV files** — every decision is logged to `logs/` as structured CSVs. Always-on, zero networking, for your own playtesting.
2. **Cloudflare telemetry upload** — at run end, the local CSV data is batched into a JSON payload and POSTed to a Cloudflare Worker. Opt-out toggle in Settings (default: on, anonymous).

---

## Telemetry Architecture

```
Game (C/raylib)
    │
    │   at run end, if not opted out
    │
    ├─→ HTTP POST → Cloudflare Worker → D1 Database
    │   payload: { run_id, timestamp, metrics_batch }
    │
    └─→ logs/metrics_*.csv (always, local)
```

### Cloudflare Stack

| Component | Purpose | Cost |
|-----------|---------|------|
| Worker | Receives POST, validates, inserts into D1 | Free (100k req/day) |
| D1 Database | SQLite-compatible, stores all events | Free (5GB storage) |
| Dashboard query | Run `SELECT` via Cloudflare dashboard or a simple query endpoint | Free |

### Worker Endpoint

**URL:** `POST https://raidparty-telemetry.your-domain.workers.dev/ingest`

**Payload format:**
```json
{
  "game_version": "0.1.0",
  "run_id": 47,
  "timestamp": 1715000000,
  "ascension": 5,
  "won": true,
  "area": 2,
  "floors_cleared": 5,
  "party_classes": ["guardian", "cleric", "mage"],
  "events": [
    {"type": "card_pick", ...},
    {"type": "draft", ...},
    {"type": "event_choice", ...},
    {"type": "combat_result", ...},
    {"type": "relic_claim", ...}
  ]
}
```

The Worker inserts each event as a row in D1. You query with SQL:
```sql
SELECT card_id, COUNT(*) as picks
FROM card_picks
GROUP BY card_id
ORDER BY picks DESC;
```

---

## 2. Telemetry Toggle in Settings

### Settings.cfg
Add a single new line:
```
telemetry_opt_in=1
```
Default: `1` (opted in). `0` means no data is sent.

### Settings Screen

Add a toggle button in the VISUAL panel (or a new row below it):

```
[TELEMETRY: ON]
```

Clicking toggles `g_state.telemetry_opt_in` between `1` and `0`.

The button label reflects state: `"TELEMETRY: ON"` / `"TELEMETRY: OFF"`.

Below the toggle, a small note:
```
"Sends anonymous usage data to help improve the game.
 No personal information is collected."
```

### File: `src/game.h`
- Add `bool telemetry_opt_in;` to `GameState`

### File: `src/game.c`
- Initialize to `1` in `game_init()` (after `game_settings_load()`)
- Save/load in `game_settings_save()` / `game_settings_load()`

### File: `src/screens/settings_screen.c`
- Add telemetry toggle button below the audio sliders
- On click: toggle `g_state.telemetry_opt_in`, save settings

---

## 3. HTTP Upload Implementation

### Platform Strategy

| Build | HTTP Library | Implementation |
|-------|-------------|---------------|
| Windows native | `WinHTTP` | Built into Windows, no extra dependency |
| Steam | `ISteamHTTP` | After Steamworks integration, preferred |
| Web/Emscripten | `emscripten_fetch` | Native browser `fetch` |

For the initial EA build, Windows `WinHTTP` is the simplest path.

### File: `src/systems/telemetry.h`

```c
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>

void telemetry_init(void);
void telemetry_push(const char *category, const char *json_data);
void telemetry_flush(void);  // sends buffered data, called at run end
void telemetry_shutdown(void);

#endif
```

### File: `src/systems/telemetry.c`

- In-memory buffer of JSON events (array of strings, max ~200 per run)
- `telemetry_push()` appends a JSON object to the buffer
- `telemetry_flush()` builds the full payload and POSTs to Cloudflare via `WinHTTP`
- `WinHTTP` is synchronous for simplicity (small payload, ~1-5KB, negligible delay)
- If `g_state.telemetry_opt_in == 0`, flush is a no-op

### WinHTTP POST Pseudocode
```c
static bool http_post(const char *url, const char *body)
{
    HINTERNET session = WinHttpOpen(L"RaidParty/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    HINTERNET connect = WinHttpConnect(session, L"raidparty-telemetry.your-domain.workers.dev", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", L"/ingest", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    
    // Set content-type header
    LPCWSTR headers = L"Content-Type: application/json\r\n";
    WinHttpSendRequest(request, headers, wcslen(headers), (void*)body, strlen(body), strlen(body), 0);
    WinHttpReceiveResponse(request, NULL);
    
    // Cleanup handles
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return true;
}
```

---

## 4. Metrics Categories

### 4a. Draft Metrics — Class Pick Rates

**Logged at:** `draft_screen.c` — when "BEGIN RUN" is clicked

**Payload:**
```json
{
  "type": "draft",
  "area": 0,
  "ascension": 3,
  "party_size": 3,
  "classes": ["guardian", "cleric", "mage"]
}
```

**Why:** Track which classes are over/underpicked. If Bard is picked 8% of the time, it needs buffs or a design rethink.

### 4b. Card Pick Metrics

**Logged at:** `card_reward_screen.c` — when a card is chosen or skipped

**Payload:**
```json
{
  "type": "card_pick",
  "encounter": "elite",
  "offered": ["holy_strike", "backstab", "renew", "preparation"],
  "picked": "renew",
  "pick_number": 1,
  "picks_remaining": 0,
  "gold": 45
}
```

**Why:** Pick rate = `picked_count / offered_count`. High pick rate → possibly overtuned. Low pick rate → possibly undertuned or class is underpicked. **Fortify being picked every time it appears is exactly what this catches.**

### 4c. Event Choice Metrics

**Logged at:** `event_screen.c` — after a choice resolves

**Payload:**
```json
{
  "type": "event_choice",
  "event_id": "strange_merchant",
  "choice_index": 2,
  "choice_label": "Buy Scroll (25g)",
  "choice_effect": "pay_gold_add_card",
  "choice_gold": 25,
  "gold_before": 45,
  "gold_after": 20,
  "could_afford": true
}
```

**Why:** If a choice is never picked but always affordable, it's unappealing. If never picked because unaffordable, the economy needs adjustment.

### 4d. Combat Metrics

**Logged at:** `run_screen.c` — when combat ends (victory/defeat)

**Payload:**
```json
{
  "type": "combat_result",
  "area": 0,
  "floor": 2,
  "encounter": "elite",
  "result": "victory",
  "turns": 7,
  "deaths": 0,
  "gold_reward": 18
}
```

**Why:** Track which encounters/areas have the highest wipe rates. Calibrate difficulty.

### 4e. Relic Pick Metrics

**Logged at:** `relic_reward_screen.c` — when a relic is claimed

**Payload:**
```json
{
  "type": "relic_claim",
  "offered": ["echo_bell", "blood_amber", "spirit_stone"],
  "picked": "echo_bell",
  "source": "boss_reward"
}
```

**Why:** See which relics players value most when given a choice.

---

## 5. Run-End Summary (Sent in One POST)

At `game_over_screen.c`, after `meta_record_run()`:

1. Call `telemetry_flush()` which:
   - Builds a complete JSON payload with all buffered events plus run summary
   - POSTs to Cloudflare Worker (if opted in and connected)
   - Clears the buffer

The run summary appended to the payload:
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

---

## 6. New Files Summary

| File | Lines | Purpose |
|------|-------|---------|
| `src/systems/telemetry.h` | ~15 | Header for telemetry buffer + flush |
| `src/systems/telemetry.c` | ~120 | Buffer management, WinHTTP POST, JSON serialization |
| `src/systems/telemetry_worker.js` | ~50 | Cloudflare Worker (deployed separately, bundled in docs) |
| `scripts/analyze_metrics.py` | ~150 | Reads local CSVs, prints summary tables |

## Files Modified

| File | Change |
|------|--------|
| `src/game.h` | Add `bool telemetry_opt_in` to `GameState` |
| `src/game.c` | Init/save/load telemetry_opt_in in settings |
| `src/screens/settings_screen.c` | Add telemetry toggle button + info text |
| `src/screens/draft_screen.c` | Push draft event on run start |
| `src/screens/card_reward_screen.c` | Push card pick event on pick/skip |
| `src/screens/event_screen.c` | Push event choice on choice resolve |
| `src/screens/relic_reward_screen.c` | Push relic claim on claim |
| `src/screens/run_screen.c` | Push combat result on combat end |
| `src/screens/game_over_screen.c` | Push run summary and call `telemetry_flush()` |
| `src/CMakeLists.txt` | Add `telemetry.c` to sources |

---

## 7. Cloudflare Worker (`src/systems/telemetry_worker.js`)

A simple worker deployed to Cloudflare:

```js
// Deploy to Cloudflare Workers
// Route: POST /ingest

export default {
  async fetch(request, env) {
    if (request.method !== "POST") return new Response("POST only", { status: 405 });
    
    const payload = await request.json();
    const { run_id, events } = payload;
    if (!run_id || !events) return new Response("Bad request", { status: 400 });
    
    const db = env.TELEMETRY_DB;
    
    for (const event of events) {
      const { type, ...data } = event;
      const timestamp = Math.floor(Date.now() / 1000);
      
      await db.prepare(
        "INSERT INTO events (run_id, timestamp, type, data) VALUES (?1, ?2, ?3, ?4)"
      ).bind(run_id, timestamp, type, JSON.stringify(data)).run();
    }
    
    return new Response("OK", { status: 200 });
  }
};
```

**D1 Schema:**
```sql
CREATE TABLE events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER NOT NULL,
  timestamp INTEGER NOT NULL,
  type TEXT NOT NULL,
  data TEXT NOT NULL
);

CREATE INDEX idx_events_type ON events(type);
CREATE INDEX idx_events_run ON events(run_id);
```

**Query examples (via Cloudflare dashboard):**

```sql
-- Most picked cards
SELECT json_extract(data, '$.picked') as card, COUNT(*) as picks
FROM events WHERE type = 'card_pick' AND json_extract(data, '$.picked') != 'skip'
GROUP BY card ORDER BY picks DESC;

-- Card pick rates (with offered count)
SELECT 
  json_extract(data, '$.picked') as card,
  COUNT(*) as picks,
  (SELECT COUNT(*) FROM events e2 WHERE e2.type = 'card_pick' 
   AND instr(json_extract(e2.data, '$.offered'), json_extract(events.data, '$.picked')) > 0
  ) as offered
FROM events WHERE type = 'card_pick' AND json_extract(data, '$.picked') != 'skip'
GROUP BY card ORDER BY picks DESC;

-- Class pick rates
SELECT class, COUNT(*) as runs
FROM events, json_each(json_extract(data, '$.classes')) as c
WHERE type = 'draft'
GROUP BY class ORDER BY runs DESC;

-- Area win rates
SELECT 
  json_extract(data, '$.area') as area,
  SUM(CASE WHEN json_extract(data, '$.won') = 'true' THEN 1 ELSE 0 END) as wins,
  COUNT(*) as total
FROM events WHERE type = 'run_summary'
GROUP BY area;
```

---

## 8. Privacy Considerations

- **No personal data collected:** No Steam ID, no IP logged by the Worker (Cloudflare handles IPs transparently)
- **Run ID is a counter:** Not linked to Steam account or username
- **Opt-out default:** Player must explicitly disable
- **GDPR-friendly:** Anonymous aggregate data only
- **Disclosure:** Mention telemetry in the game's description and on the Settings screen

---

## Implementation Order

| Step | Task | Depends On |
|------|------|------------|
| 1 | Create `telemetry.h` / `telemetry.c` with buffer + WinHTTP POST | Nothing |
| 2 | Add `telemetry_opt_in` to GameState + settings save/load | Step 1 |
| 3 | Add toggle to Settings screen | Step 2 |
| 4 | Add telemetry_push calls to draft/card/event/relic/combat screens | Step 1 |
| 5 | Add telemetry_flush call to game_over_screen | Step 4 |
| 6 | Deploy Cloudflare Worker + D1 database | Nothing (independent) |
| 7 | Create `analyze_metrics.py` for local CSV analysis | Existing CSV logging |
