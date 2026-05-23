#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "systems/party.h"
#include "systems/deck.h"
#include "systems/map.h"
#include "combat/combat.h"
#include "data/encounter_defs.h"

#define MAX_REWARD_CARDS 4

typedef enum {
    SCREEN_TITLE,
    SCREEN_DRAFT,
    SCREEN_MAP,
    SCREEN_RUN,
    SCREEN_REST,
    SCREEN_SHOP,
    SCREEN_REWARD,
    SCREEN_DISCARD,
    SCREEN_GAME_OVER
} GameScreen;

typedef struct {
    GameScreen screen;
    GameScreen pending_screen;
    bool transition_active;
    bool transition_switched;
    float transition_alpha;
    float transition_timer;
    int selected_classes[MAX_PARTY_SIZE];
    int selected_count;
    int max_party_size;
    Party run_party;
    bool run_party_active;
    Deck run_deck;
    CombatState combat;
    MapState map;
    const EncounterDef *encounter;
    bool encounter_is_elite;
    bool encounter_is_boss;
    const CardDef *reward_cards[MAX_REWARD_CARDS];
    bool reward_upgraded[MAX_REWARD_CARDS];
    int reward_count;
    int reward_picks_remaining;
    bool reward_picked[MAX_REWARD_CARDS];
    int gold;
    int discard_count;
    int discard_selected;
    int discard_uids[2];
    bool run_won;
    int result_floor;
    int result_bosses_defeated;
    char result_reason[128];
} GameState;

extern GameState g_state;

void game_init(void);
void game_change_screen(GameScreen screen);
void game_update_transition(float dt);
void game_draw_transition(void);
bool game_transition_allows_update(void);

#endif
