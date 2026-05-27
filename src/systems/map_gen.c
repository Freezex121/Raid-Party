#include "map_gen.h"
#include "map.h"
#include "data/area_defs.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ── Tunable Constants ──────────────────────────────────────────
// Tweak these to adjust map generation without touching the algorithm.

#define MAX_NODES_PER_FLOOR 64
#define MAX_CONNS_PER_NODE 4

// Row count:  3 + floor ± 1
#define ROW_BASE 5
#define ROW_VARIANCE 2

// Column curve: widest row = min(MAX_COLS_AREA, 1 + distance_from_end)
#define MAX_COLS_GREENWOOD 4
#define MAX_COLS_VENOM_MIRE 4
#define MAX_COLS_CINDER_SPIRE 4

// Chance (percent) to get a rare 5-column row at the midpoint
#define RARE_5_COL_CHANCE 15

// Minimum service nodes per floor
#define MIN_REST_PER_FLOOR 1
#define MIN_SHOP_GREENWOOD 0
#define MIN_SHOP_VENOM_MIRE 1
#define MIN_SHOP_CINDER_SPIRE 1

// Event range
#define MAX_EVENT_GREENWOOD 1
#define MAX_EVENT_VENOM_MIRE 2
#define MAX_EVENT_CINDER_SPIRE 2

// Base elites per floor (bonus added for later floors)
#define ELITE_EACH_GREENWOOD 1
#define ELITE_EACH_VENOM_MIRE 1
#define ELITE_EACH_CINDER_SPIRE 1

// Branch and merge chances (percent)
#define BRANCH_GREENWOOD 50
#define BRANCH_VENOM_MIRE 60
#define BRANCH_CINDER_SPIRE 70
#define MERGE_CHANCE 20

// ── Area Config ────────────────────────────────────────────────

typedef struct {
    int max_cols;
    int min_shop;
    int max_event;
    int elite_each;
    int branch_chance;
} AreaGenConfig;

static AreaGenConfig config_for_area(const char *area_id)
{
    AreaGenConfig cfg = { 3, 0, 1, 0, 50 };
    if (!area_id) return cfg;
    if (strcmp(area_id, "greenwood_breach") == 0)
    {
        cfg.max_cols = MAX_COLS_GREENWOOD;
        cfg.min_shop = MIN_SHOP_GREENWOOD;
        cfg.max_event = MAX_EVENT_GREENWOOD;
        cfg.elite_each = ELITE_EACH_GREENWOOD;
        cfg.branch_chance = BRANCH_GREENWOOD;
    }
    else if (strcmp(area_id, "venom_mire") == 0)
    {
        cfg.max_cols = MAX_COLS_VENOM_MIRE;
        cfg.min_shop = MIN_SHOP_VENOM_MIRE;
        cfg.max_event = MAX_EVENT_VENOM_MIRE;
        cfg.elite_each = ELITE_EACH_VENOM_MIRE;
        cfg.branch_chance = BRANCH_VENOM_MIRE;
    }
    else if (strcmp(area_id, "cinder_spire") == 0)
    {
        cfg.max_cols = MAX_COLS_CINDER_SPIRE;
        cfg.min_shop = MIN_SHOP_CINDER_SPIRE;
        cfg.max_event = MAX_EVENT_CINDER_SPIRE;
        cfg.elite_each = ELITE_EACH_CINDER_SPIRE;
        cfg.branch_chance = BRANCH_CINDER_SPIRE;
    }
    return cfg;
}

// ── Grid Helpers ───────────────────────────────────────────────

typedef struct {
    int row;
    int col;
    NodeType type;
    int conns[MAX_CONNS_PER_NODE];
    int conn_count;
} GenNode;

static int node_index(GenNode *grid, int row, int col, int *cols_per_row, int rows)
{
    if (row < 0 || row >= rows) return -1;
    if (col < 0 || col >= cols_per_row[row]) return -1;
    int idx = 0;
    for (int r = 0; r < row; r++)
        idx += cols_per_row[r];
    return idx + col;
}

static GenNode *node_at(GenNode *grid, int *cols_per_row, int rows, int row, int col)
{
    int idx = node_index(grid, row, col, cols_per_row, rows);
    return (idx >= 0) ? &grid[idx] : NULL;
}

static void add_conn(GenNode *node, int target)
{
    if (node->conn_count >= MAX_CONNS_PER_NODE) return;
    for (int i = 0; i < node->conn_count; i++)
        if (node->conns[i] == target) return;
    node->conns[node->conn_count++] = target;
}

// ── Node value for weighted path balancing ─────────────────────
// Higher = more beneficial. Positive types go to the weakest lane,
// negative types (elite) go to the strongest lane, keeping all
// paths roughly equal.

static int node_value(NodeType t)
{
    switch (t)
    {
        case NODE_SHOP:   return 12;
        case NODE_EVENT:  return 10;
        case NODE_REST:   return 3;
        case NODE_ELITE:  return -5;
        default:          return 1;
    }
}

// ── Main Generator ─────────────────────────────────────────────

bool map_generate_procedural(MapState *map, int floor, const char *area_id)
{
    // Seed for determinism
    static int gen_count = 0;
    gen_count++;
    int seed = (int)time(NULL) ^ (floor * 1000) ^ (gen_count * 7);
    srand(seed);

    const AreaGenConfig cfg = config_for_area(area_id);

    // Determine row count
    int rows = ROW_BASE + floor + (rand() % (ROW_VARIANCE + 1));
    if (rows < ROW_BASE) rows = ROW_BASE;
    if (rows > 10) rows = 10;

    // Determine columns per row (bell curve)
    int cols_per_row[10];
    int mid = rows / 2;
    int max_cols = cfg.max_cols;

    // Rare 5-column row?
    bool rare_5 = (max_cols >= 4 && (rand() % 100) < RARE_5_COL_CHANCE);

    int total_nodes = 0;
    for (int r = 0; r < rows; r++)
    {
        int dist = (r < mid) ? r : (rows - 1 - r);
        cols_per_row[r] = 1 + dist;
        if (cols_per_row[r] > max_cols) cols_per_row[r] = max_cols;
        if (rare_5 && r == mid) cols_per_row[r] = 5;
        total_nodes += cols_per_row[r];
    }
    cols_per_row[0] = 1;
    cols_per_row[rows - 1] = 1;

    if (total_nodes <= 0 || total_nodes > MAX_NODES_PER_FLOOR)
    {
        LOG_E(CAT_SCREEN, "Map gen: invalid node count %d", total_nodes);
        return false;
    }

    // Allocate grid
    GenNode *grid = (GenNode *)calloc((size_t)total_nodes, sizeof(GenNode));
    if (!grid) return false;

    int idx = 0;
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols_per_row[r]; c++)
        {
            grid[idx].row = r;
            grid[idx].col = c;
            grid[idx].type = NODE_COMBAT;
            idx++;
        }
    }

    // Override boss
    GenNode *boss = node_at(grid, cols_per_row, rows, rows - 1, 0);
    if (boss) boss->type = NODE_BOSS;

    // Rare 5-column row: assign 3 distinct non-combat types at the midpoint
    bool row_has_special[10];
    memset(row_has_special, 0, sizeof(row_has_special));
    if (rare_5)
    {
        int mid_row = mid;
        if (mid_row > 0 && mid_row < rows - 1)
        {
            NodeType fill_types[] = { NODE_SHOP, NODE_EVENT, NODE_REST, NODE_ELITE };
            for (int i = 3; i > 0; i--)
            {
                int j = rand() % (i + 1);
                NodeType t = fill_types[i];
                fill_types[i] = fill_types[j];
                fill_types[j] = t;
            }
            int assign_cols[] = { 0, 2, 4 };
            for (int i = 0; i < 3; i++)
            {
                GenNode *n = node_at(grid, cols_per_row, rows, mid_row, assign_cols[i]);
                if (n && n->type == NODE_COMBAT)
                {
                    n->type = fill_types[i];
                    row_has_special[mid_row] = true;
                }
            }
        }
    }

    // ── Lane-based type assignment ──────────────────────────────
    // Each column is a distinct path lane. Special nodes are placed
    // in specific lanes so every path offers different content.

    // Find max lanes (widest row)
    int max_lanes = 0;
    for (int r = 0; r < rows; r++)
        if (cols_per_row[r] > max_lanes) max_lanes = cols_per_row[r];

    // Build per-type counts
    int floor_pct = (floor * 100) / 5;
    int elite_total = cfg.elite_each;
    if (floor_pct > 70) elite_total += 2;
    else if (floor_pct > 40) elite_total += 1;
    int event_total = rand() % (cfg.max_event + 1);

    // Total special nodes that need distinct lanes
    int special_types[8];
    int special_count = 0;
    for (int i = 0; i < MIN_REST_PER_FLOOR; i++) special_types[special_count++] = NODE_REST;
    for (int i = 0; i < cfg.min_shop; i++)        special_types[special_count++] = NODE_SHOP;
    for (int i = 0; i < event_total; i++)          special_types[special_count++] = NODE_EVENT;
    for (int i = 0; i < elite_total; i++)          special_types[special_count++] = NODE_ELITE;

    // ── Weighted lane assignment ─────────────────────────────
    // Path values: higher = more beneficial. Positive types go to
    // the weakest lane, negative types (elite) go to the strongest
    // lane, keeping all paths roughly equal in total value.
    int lane_value[5];
    memset(lane_value, 0, sizeof(lane_value));
    for (int i = 0; i < total_nodes; i++)
        if (grid[i].type == NODE_COMBAT)
            lane_value[grid[i].col] += node_value(NODE_COMBAT);

    // Sort special types by absolute value descending so large
    // modifiers are placed first (shop 12 before rest 3).
    // Bubble sort by |value| descending.
    for (int i = 0; i < special_count - 1; i++)
    {
        for (int j = 0; j < special_count - 1 - i; j++)
        {
            int va = abs(node_value(special_types[j]));
            int vb = abs(node_value(special_types[j + 1]));
            if (va < vb)
            {
                int t = special_types[j];
                special_types[j] = special_types[j + 1];
                special_types[j + 1] = t;
            }
        }
    }

    // Greedy assignment
    for (int si = 0; si < special_count && si < max_lanes; si++)
    {
        NodeType stype = special_types[si];
        int val = node_value(stype);

        // Pick the lane with lowest value for positive, highest for negative
        int best_lane = 0;
        int best_val = lane_value[0];
        for (int c = 1; c < max_lanes; c++)
        {
            if ((val > 0 && lane_value[c] < best_val) ||
                (val < 0 && lane_value[c] > best_val))
            {
                best_val = lane_value[c];
                best_lane = c;
            }
        }

        // Row range for this type
        int row_lo = 1, row_hi = rows - 2;
        switch (stype)
        {
            case NODE_REST:  row_lo = rows * 2 / 3; if (row_lo < 1) row_lo = 1; break;
            case NODE_SHOP:  row_lo = rows / 3; row_hi = rows * 2 / 3; if (row_hi > rows - 2) row_hi = rows - 2; break;
            case NODE_EVENT: row_hi = rows / 2; break;
            case NODE_ELITE: row_lo = rows / 3; break;
            default: break;
        }

        // Find a row in this lane where a combat node exists
        int placed = -1;
        for (int r = row_lo; r <= row_hi && placed < 0; r++)
        {
            if (cols_per_row[r] > best_lane && !row_has_special[r])
            {
                GenNode *n = node_at(grid, cols_per_row, rows, r, best_lane);
                if (n && n->type == NODE_COMBAT)
                {
                    n->type = stype;
                    row_has_special[r] = true;
                    placed = r;
                }
            }
        }

        // Fallback: try nearby lanes if no spot in the best lane
        if (placed < 0)
        {
            for (int offset = 1; offset < max_lanes; offset++)
            {
                int try_lane = best_lane + offset;
                if (try_lane < max_lanes)
                {
                    for (int r = row_lo; r <= row_hi && placed < 0; r++)
                    {
                        if (cols_per_row[r] > try_lane && !row_has_special[r])
                        {
                            GenNode *n = node_at(grid, cols_per_row, rows, r, try_lane);
                            if (n && n->type == NODE_COMBAT) { n->type = stype; row_has_special[r] = true; placed = r; }
                        }
                    }
                }
                try_lane = best_lane - offset;
                if (try_lane >= 0 && placed < 0)
                {
                    for (int r = row_lo; r <= row_hi && placed < 0; r++)
                    {
                        if (cols_per_row[r] > try_lane && !row_has_special[r])
                        {
                            GenNode *n = node_at(grid, cols_per_row, rows, r, try_lane);
                            if (n && n->type == NODE_COMBAT) { n->type = stype; row_has_special[r] = true; placed = r; }
                        }
                    }
                }
                if (placed >= 0) break;
            }
        }

        // Update lane value if successfully placed
        if (placed >= 0)
            lane_value[best_lane] += val;
    }

    // Fill empty lanes with an event so every path has something unique
    for (int c = 0; c < max_lanes; c++)
    {
        // Check if lane c has any non-combat node
        bool has_special = false;
        for (int i = 0; i < total_nodes; i++)
            if (grid[i].col == c && grid[i].type != NODE_COMBAT && grid[i].type != NODE_BOSS)
                { has_special = true; break; }
        if (has_special) continue;

        // Find a valid row in the first half where this lane exists
        for (int r = 1; r < rows / 2 && r < rows - 2; r++)
        {
            if (cols_per_row[r] > c && !row_has_special[r])
            {
                GenNode *n = node_at(grid, cols_per_row, rows, r, c);
                if (n && n->type == NODE_COMBAT)
                {
                    n->type = NODE_EVENT;
                    row_has_special[r] = true;
                    break;
                }
            }
        }
    }

    // Generate connections
    for (int r = 0; r < rows - 1; r++)
    {
        for (int c = 0; c < cols_per_row[r]; c++)
        {
            GenNode *node = node_at(grid, cols_per_row, rows, r, c);
            if (!node) continue;

            // Straight down
            if (c < cols_per_row[r + 1])
            {
                int tgt = node_index(grid, r + 1, c, cols_per_row, rows);
                if (tgt >= 0) add_conn(node, tgt);
            }

            // Branch right
            if (c + 1 < cols_per_row[r + 1] && (rand() % 100) < cfg.branch_chance)
            {
                int tgt = node_index(grid, r + 1, c + 1, cols_per_row, rows);
                if (tgt >= 0) add_conn(node, tgt);
            }

            // Merge left — only in the last 2 rows so branches stay separate
            bool can_merge = (r >= rows - 3);
            if (c > 0 && c - 1 < cols_per_row[r + 1] && can_merge && (rand() % 100) < MERGE_CHANCE)
            {
                int tgt = node_index(grid, r + 1, c - 1, cols_per_row, rows);
                if (tgt >= 0) add_conn(node, tgt);
            }
        }
    }

    // Remove crossing connections: when (r,c1)→(r+1,d1) and (r,c2)→(r+1,d2)
    // cross (c1 < c2 but d1 > d2), remove the non-straight-down one.
    for (int r = 0; r < rows - 1; r++)
    {
        for (int ca = 0; ca < cols_per_row[r]; ca++)
        {
            GenNode *a = node_at(grid, cols_per_row, rows, r, ca);
            if (!a) continue;
            for (int cb = ca + 1; cb < cols_per_row[r]; cb++)
            {
                GenNode *b = node_at(grid, cols_per_row, rows, r, cb);
                if (!b) continue;
                // Check every pair of connections from a and b
                for (int ia = 0; ia < a->conn_count; ia++)
                {
                    for (int ib = 0; ib < b->conn_count; ib++)
                    {
                        int da = grid[a->conns[ia]].col;
                        int db = grid[b->conns[ib]].col;
                        // Cross if column ordering flips: ca < cb but da > db
                        if (da >= db) continue;
                        // Connection from a (col ca) goes to col da
                        // Connection from b (col cb) goes to col db
                        // Since ca < cb but da > db, they cross
                        // Remove the one that is NOT straight-down
                        // Straight-down means source col == dest col
                        bool a_straight = (ca == da);
                        bool b_straight = (cb == db);
                        if (a_straight && !b_straight)
                        {
                            // Remove b's connection to db
                            for (int k = ib; k < b->conn_count - 1; k++)
                                b->conns[k] = b->conns[k + 1];
                            b->conn_count--;
                            ib--;
                        }
                        else if (!a_straight && b_straight)
                        {
                            // Remove a's connection to da
                            for (int k = ia; k < a->conn_count - 1; k++)
                                a->conns[k] = a->conns[k + 1];
                            a->conn_count--;
                            ia--;
                        }
                        else if (!a_straight && !b_straight)
                        {
                            // Neither is straight: remove the merge-left (da < ca)
                            if (da < ca)
                            {
                                for (int k = ia; k < a->conn_count - 1; k++)
                                    a->conns[k] = a->conns[k + 1];
                                a->conn_count--;
                                ia--;
                            }
                            else
                            {
                                for (int k = ib; k < b->conn_count - 1; k++)
                                    b->conns[k] = b->conns[k + 1];
                                b->conn_count--;
                                ib--;
                            }
                        }
                        // If both are straight-down, they can't cross (same col)
                    }
                }
            }
        }
    }

    // Merge guarantee: every node must have at least one outgoing connection.
    // After crossing removal, some nodes may need a replacement.
    for (int r = 0; r < rows - 1; r++)
    {
        for (int c = 0; c < cols_per_row[r]; c++)
        {
            GenNode *node = node_at(grid, cols_per_row, rows, r, c);
            if (!node || node->conn_count > 0) continue;
            int best_c = (c < cols_per_row[r + 1]) ? c : (cols_per_row[r + 1] - 1);
            int tgt = node_index(grid, r + 1, best_c, cols_per_row, rows);
            if (tgt >= 0) add_conn(node, tgt);
        }
    }

    // Incoming guarantee: every non-start node must have at least one connection
    // from the previous row.
    for (int r = 1; r < rows; r++)
    {
        for (int c = 0; c < cols_per_row[r]; c++)
        {
            GenNode *node = node_at(grid, cols_per_row, rows, r, c);
            if (!node) continue;

            // Check if any node in the previous row connects to this node
            bool has_incoming = false;
            for (int pc = 0; pc < cols_per_row[r - 1]; pc++)
            {
                GenNode *prev = node_at(grid, cols_per_row, rows, r - 1, pc);
                if (!prev) continue;
                for (int i = 0; i < prev->conn_count; i++)
                {
                    if (prev->conns[i] == node_index(grid, r, c, cols_per_row, rows))
                    {
                        has_incoming = true;
                        break;
                    }
                }
                if (has_incoming) break;
            }

            if (!has_incoming)
            {
                // Find the closest node in the previous row and force a connection
                int best_pc = (c < cols_per_row[r - 1]) ? c : (cols_per_row[r - 1] - 1);
                GenNode *prev = node_at(grid, cols_per_row, rows, r - 1, best_pc);
                if (prev)
                {
                    int tgt = node_index(grid, r, c, cols_per_row, rows);
                    if (tgt >= 0) add_conn(prev, tgt);
                }
            }
        }
    }

    // Apply jitter and copy grid into MapState
    map->nodes = (MapNode *)calloc((size_t)total_nodes, sizeof(MapNode));
    if (!map->nodes)
    {
        free(grid);
        return false;
    }
    map->node_count = total_nodes;
    map->floor = floor;
    map->current_index = -1;
    map->boss_index = -1;

    for (int i = 0; i < total_nodes; i++)
    {
        MapNode *out = &map->nodes[i];
        out->type = grid[i].type;
        out->row = grid[i].row;
        out->col = grid[i].col;
        out->x = 0;
        out->y = 0;
        out->completed = false;
        out->available = false;
        out->event_id = NULL;

        if (grid[i].conn_count > 0)
        {
            out->conns = (int *)calloc((size_t)grid[i].conn_count, sizeof(int));
            if (out->conns)
            {
                out->conn_count = grid[i].conn_count;
                for (int j = 0; j < grid[i].conn_count; j++)
                    out->conns[j] = grid[i].conns[j];
            }
        }

        if (grid[i].type == NODE_BOSS)
            map->boss_index = i;
    }

    free(grid);

    LOG_I(CAT_SCREEN, "Map gen: area=%s floor=%d nodes=%d rows=%d boss=%d",
          area_id ? area_id : "default", floor, total_nodes, rows, map->boss_index);
    return true;
}
