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
} GameState;

extern GameState g_state;

void game_init(void);
void game_change_screen(GameScreen screen);
void game_go_to_level_up_or(GameScreen destination);
void game_update_transition(float dt);
void game_draw_transition(void);
bool game_transition_allows_update(void);
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
