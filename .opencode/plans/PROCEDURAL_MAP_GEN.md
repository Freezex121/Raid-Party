# Procedural Map Generation Plan

## Overview

Replace hand-authored `maps.json` with a procedural generator. Node rendering, clicking, encounter dispatch, and all other systems are unchanged. Encounters are already organized by `(area, floor)` in `encounters.json` — they work identically regardless of how many combat nodes the map has.

---

## Algorithm

### Step 1 – Row Generation

Each floor is a row-based directed graph. Row count and column count vary by area and floor.

**Row count:**
```
rows = 3 + floor + r % 2
```
- Floor 0: 3–4 rows
- Floor 1: 4–5 rows
- Floor 2: 5–6 rows
- Floor 3: 6–7 rows
- Floor 4: 7–8 rows

**Column count per row** follows a bell curve — narrow at the start and end, widest in the middle:

```
mid = rows / 2
for each row r:
    half = (r < mid) ? r : (rows - 1 - r)
    cols[r] = 1 + half               // expands and contracts linearly
    if cols[r] > max_cols: cols[r] = max_cols
```

**Rare 5-column rows:** For areas with `max_columns >= 4`, there's a 15% chance per floor to get one extra-wide row (5 columns) at the midpoint instead of the area's usual max. Rolled once per floor generation.

**Area configs:**

| Parameter | Greenwood (3 floors) | Venom Mire (4 floors) | Cinder Spire (5 floors) |
|-----------|---------------------|----------------------|----------------------|
| min_rows | 3 | 3 | 4 |
| max_columns | 3 | 4 | 4 |
| min_rest | 1 | 1 | 1 |
| min_shop | 0 | 1 | 1 |
| max_event | 1 | 2 | 2 |
| elite_each | 0 | 1 | 1 |
| branch_chance | 50 | 60 | 70 |

Greenwood (3 floors, difficulty 1.0) gets narrower maps with fewer special nodes. Cinder Spire (5 floors, difficulty 1.45) gets wider, more branching, more elites.

### Step 2 – Build Nodes with Type Assignment

**Phase A – Fill grid with combat nodes:**

```
nodes = []
for each row r, for each column c in 0..cols[r]-1:
    create node at (r, c) with type = NODE_COMBAT
```

**Phase B – Assign lane roles for path diversity:**

Since merge-left connections are restricted to the last 2 rows, each column index stays independent for most of the map. Special node types are assigned **per column lane** so every path offers different content.

1. Count columns at the widest row (`max_lanes = max(cols_per_row)`)
2. Build a lane role list, one entry per column, cycling through roles:

```
lane_roles[max_lanes]
roles pool = [REST, ELITE, SHOP, COMBAT, EVENT, COMBAT, COMBAT]
shuffle(roles pool, using floor as a seed offset)
assign first max_lanes entries to lane_roles[0..max_lanes-1]
```

This ensures lane 0 might be a rest path, lane 1 an elite path, lane 2 a shop path, etc. — and the assignment shifts each floor so it's not always the same column.

3. Overwrite start and boss:
- `nodes[last_row][0]` → `NODE_BOSS`

4. Place special nodes per lane. For each special type, pick a valid row within the lane's column range:

| Type | Lane Role | Row Range | Count |
|------|-----------|-----------|-------|
| NODE_REST | REST | `> rows*0.5` (last half) | `min_rest` |
| NODE_SHOP | SHOP | `rows*0.3` to `rows*0.7` (middle) | `min_shop` |
| NODE_ELITE | ELITE | `rows*0.25` to `rows-2` | `elite_each` + floor bonus |
| NODE_EVENT | EVENT | `1` to `rows*0.5` (first half) | 0–`max_event` |

**Placement rules:**
- For each special type, find the assigned lane column. Pick any row in the valid range where that column has a combat node.
- If no node is available in the assigned lane (e.g., lane has too few rows), fall back to the nearest lane.
- Never place two non-combat nodes on the same row.
- If multiple elites: spread across different elite-lane columns.

**Elite count scaling:**

Base elites = area's `elite_each`. Additional:

| Floor position | Bonus elites |
|----------------|-------------|
| First 30% of floors | 0 |
| Middle 40% | 0–1 |
| Last 30% | 1–2 |

Cinder Spire floor 4: base 1 + bonus 1 = 2 elites.
Greenwood floor 1: base 0 + bonus 0 = 0 elites.

**Example output for a 3-lane floor:**

```
Row 0: [COMBAT]                    ← all combat, path splits below
Row 1: [COMBAT] [COMBAT] [COMBAT]
Row 2: [COMBAT] [COMBAT] [SHOP]    ← lane 2 has the shop
Row 3: [REST]  [ELITE]  [COMBAT]   ← lane 0 = rest, lane 1 = elite
Row 4: [BOSS]                      ← all paths rejoin
```

Player sees three distinct paths:
- Left path: healing rest → boss
- Middle path: tough elite → boss
- Right path: gold sink shop → boss

This is a real strategic choice, not a fake one.

### Step 3 – Generate Connections

Each node at `(r, c)` connects to nodes in row `r+1`:

```
for each row r in 0..rows-2:
    for each column c in 0..cols[r]-1:
        // Always connect straight down
        if c < cols[r+1]:
            connect(r,c) → (r+1, c)
        // Maybe branch right
        if c+1 < cols[r+1] and (rand() % 100) < config.branch_chance:
            connect(r,c) → (r+1, c+1)
        // Maybe branch left (merging paths)
        if c > 0 and c-1 < cols[r+1] and (rand() % 100) < 30:
            connect(r,c) → (r+1, c-1)
```

**Merge guarantee:** If two nodes in row `R` only have one valid connection target between them (e.g., cols shrink from 3→2), the second node's missing connection gets forced. This prevents dead branches — all paths reach the boss.

### Step 4 – Position & Jitter

- Set each node's `(row, col)` based on grid assignment
- Leave `x = 0, y = 0` — `calc_fallback_positions()` auto-computes pixel positions from (row, col)
- Then apply `±5`px random offset to `x` and `y`:

```c
void jitter_position(MapNode *node) {
    node->x += (rand() % 11) - 5;
    node->y += (rand() % 11) - 5;
}
```

Start node gets `available = true`.

---

## Determinism / Seeding

```c
int seed = area_index * 10000 + floor * 100 + g_state.result_bosses_defeated;
srand(seed);
```

- New run → `result_bosses_defeated` resets → different seed → different map
- Mid-run continue → same boss count, floor, area → same map
- Ascension has no effect on seed

---

## Integration

### New file: `src/systems/map_gen.h`
```c
#ifndef MAP_GEN_H
#define MAP_GEN_H

#include "map.h"

bool map_generate_procedural(MapState *map, int floor, const char *area_id);

#endif
```

### New file: `src/systems/map_gen.c`
- ~300 lines: config struct, row/column calculation, node creation, type assignment, connection generation, jitter
- Single public function: `map_generate_procedural()`
- Seeds `rand()` before any generation calls
- Allocates `map->nodes` with `calloc()`, fills `node_count`, `boss_index`

### Modified: `src/systems/map.c`

`map_generate()` becomes:
```c
void map_generate(MapState *map, int floor, const char *area_id)
{
    if (!map) return;
    map_clear(map);
    if (floor < 0) floor = 0;

    map->floor = floor;
    map->current_index = -1;
    map->boss_index = -1;

    if (!map_generate_procedural(map, floor, area_id))
        return;

    calc_fallback_positions(map->nodes, map->node_count);
    apply_jitter(map->nodes, map->node_count);
    remove_backward_connections(map->nodes, map->node_count);

    int start = map_find_start(map);
    if (start >= 0)
        map->nodes[start].available = true;
}
```

**Removed from map.c (~200 lines):** `floor_layouts`, `NodeDef`, `FloorLayout`, `map_load_json()`, `free_floor_layouts()`, `find_layout()`, `find_layout_exact()`, `area_matches()`, `max_floor_for_area()`, `map_loaded_floor_count()`, `map_loaded_floor_count_for_area()`.

**Kept in map.c:** `map_clear()`, `map_unlock_next()`, `map_find_start()`, `calc_fallback_positions()`, `remove_backward_connections()`, `connection_allows_forward()`.

### Modified: `src/screens/map_screen.c`

Replace `map_loaded_floor_count_for_area(area_id)` with `area_floor_count(g_state.current_area)` for the floor clamping check on line ~84.

### Modified: `src/data/content_loader.c`

Remove `map_load_json("assets/data/maps.json")` line.

### Deleted: `assets/data/maps.json`

Entire file removed — 293 lines gone.

---

## Files Summary

| File | Change |
|------|--------|
| `src/systems/map_gen.h` | **New** — declaration |
| `src/systems/map_gen.c` | **New** — ~300 lines, generation algorithm |
| `src/systems/map.h` | **No change** |
| `src/systems/map.c` | Replace `map_generate()` body. Remove static layout data/code (~200 lines removed). |
| `src/screens/map_screen.c` | Update floor clamping to use `area_floor_count()` |
| `src/data/content_loader.c` | Remove `map_load_json()` line |
| `assets/data/maps.json` | **Deleted** |

---

## Guarantees

- **Connected:** All nodes reachable from start, all paths lead to boss node
- **Playable:** At least 1 rest per floor, shops and events in sensible ranges
- **Varied:** Branch patterns, row counts, and type distributions differ per run
- **Deterministic:** Same seed → same map — no surprises on save/load
- **Encounter-safe:** `encounter_for_area_floor()` uses `%` wrapping, so any number of combat nodes works fine
- **Upgrade-safe:** The `event_id` field on `MapNode` is retained in the struct but the generator doesn't set it (adds no event overrides for now)
