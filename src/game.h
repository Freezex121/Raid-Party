#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "systems/party.h"
#include "systems/deck.h"
#include "systems/map.h"
#include "systems/meta.h"
#include "systems/relic.h"
#include "combat/combat.h"
#include "data/encounter_defs.h"

#define MAX_REWARD_CARDS 5

#define TUTORIAL_STEP_MAP 0
#define TUTORIAL_STEP_COMBAT_PARTY 1
#define TUTORIAL_STEP_COMBAT_THREAT 2
#define TUTORIAL_STEP_COMBAT_INTENT 3
#define TUTORIAL_STEP_COMBAT_CARDS 4
#define TUTORIAL_STEP_COMBAT_END 5
#define TUTORIAL_STEP_REWARD 6
#define TUTORIAL_STEP_ELITE 20
#define TUTORIAL_STEP_BOSS 21
#define TUTORIAL_STEP_SHOP 22
#define TUTORIAL_STEP_EVENT 23
#define TUTORIAL_STEP_REST 24
#define TUTORIAL_STEP_LEVEL_UP 25
#define TUTORIAL_STEP_DISCARD 26
#define TUTORIAL_STEP_GAME_OVER 27
#define TUTORIAL_STEP_META_SHOP 28
#define TUTORIAL_STEP_DONE 99

typedef enum {
    SCREEN_TITLE,
    SCREEN_CODEX,
    SCREEN_META_SHOP,
    SCREEN_DRAFT,
    SCREEN_MAP,
    SCREEN_RUN,
    SCREEN_REST,
    SCREEN_SHOP,
    SCREEN_EVENT,
    SCREEN_REWARD,
    SCREEN_RELIC_REWARD,
    SCREEN_DISCARD,
    SCREEN_GAME_OVER,
    SCREEN_DECK,
    SCREEN_ACHIEVEMENTS,
    SCREEN_LEVEL_UP,
    SCREEN_SETTINGS
} GameScreen;

typedef struct {
    GameScreen screen;
    GameScreen pending_screen;
    GameScreen settings_return_screen;
    GameScreen post_combat_destination;
    bool transition_active;
    bool transition_switched;
    float transition_alpha;
    float transition_timer;
    int selected_classes[MAX_PARTY_SIZE];
    int selected_count;
    int max_party_size;
    int selected_area;
    int current_area;
    int window_scale;
    bool fullscreen;
    bool telemetry_opt_in;
    bool telemetry_prompt_seen;
    float master_volume;
    float music_volume;
    float sfx_volume;
    MetaProgress meta;
    Party run_party;
    bool run_party_active;
    Deck run_deck;
    RelicId relics[MAX_RUN_RELICS];
    int relic_count;
    CombatState combat;
    MapState map;
    const EncounterDef *encounter;
    bool encounter_is_elite;
    bool encounter_is_boss;
    bool relic_reward_pending;
    RelicId relic_reward_choices[RELIC_REWARD_CHOICES];
    int relic_reward_count;
    const CardDef *reward_cards[MAX_REWARD_CARDS];
    int reward_upgrade_level[MAX_REWARD_CARDS];
    int reward_count;
    int reward_picks_remaining;
    bool reward_picked[MAX_REWARD_CARDS];
    int gold;
    int next_combat_energy_bonus;
    int next_combat_draw_bonus;
    int next_combat_boon_turns;
    int discard_count;
    int discard_selected;
    int discard_uids[2];
    int titan_heart_bonus;
    int current_floor;
    bool frugal_used_this_floor;
    bool lucky_coin_used;
    bool run_won;
    int result_area;
    int result_floor;
    int result_bosses_defeated;
    int result_achievement_renown;
    char result_achievement_names[160];
    bool result_recorded;
    int result_unlocked_party_size;
    int result_unlocked_area;
    int result_renown_gained;
    int result_gold_renown;
    char result_reason[128];
    int run_deaths;
    int run_interrupts;
    int run_best_combat_turns;
    int telemetry_run_id;
    bool tutorial_active;
    bool tutorial_reward_pending;
    int tutorial_step;
} GameState;

extern GameState g_state;

void game_init(void);
void game_change_screen(GameScreen screen);
void game_update_transition(float dt);
void game_draw_transition(void);
bool game_transition_allows_update(void);
void game_draw_tutorial_overlay(Rectangle highlight, const char *text);
void game_draw_tutorial_overlay_ex(Rectangle highlight, const char *title, const char *body, const char *footer, int step, int total_steps);
void game_skip_tutorial(void);
bool game_tutorial_handle_skip(void);
bool game_tutorial_handle_close(void);
bool game_start_tutorial_once(bool *seen_flag, int step);
void game_draw_gold_overlay(void);
void game_gain_gold(int amount, const char *context);
bool game_spend_gold(int amount, const char *context);
void game_log_gold_conversion(int gold_amount, int renown_amount);
void game_open_settings(GameScreen return_screen);
void game_set_window_scale(int scale);
void game_set_fullscreen(bool fullscreen);
void game_set_master_volume(float volume);
void game_set_music_volume(float volume);
void game_set_sfx_volume(float volume);
Rectangle game_render_destination(void);
void game_update_mouse_transform(void);
void game_settings_save(void);

#endif
