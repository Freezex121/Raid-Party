#ifndef META_H
#define META_H

#include <stdbool.h>
#include "systems/achievements.h"
#include "systems/relic.h"

#define META_SLOT4_COST 20
#define META_SLOT5_COST 40
#define META_TRAVEL_FUND_MAX_RANK 3
#define META_TRAVEL_FUND_GOLD_PER_RANK 10
#define META_ASCENSION_MAX 10
#define META_LEGACY_MAX_RANK 3
#define META_CLASS_UNLOCK_COST 20
#define META_SHIELD_CAP_MAX_RANK 6
#define META_SHIELD_CAP_BASE_PERCENT 50
#define META_SHIELD_CAP_PERCENT_PER_RANK 25
#define META_RELIC_UNLOCK_ECHO_BELL (1 << 0)
#define META_RELIC_UNLOCK_SPLIT_PRISM (1 << 1)
#define META_RELIC_UNLOCK_BLOOD_AMBER (1 << 2)
#define META_RELIC_UNLOCK_TITAN_HEART (1 << 3)
#define META_RELIC_UNLOCK_FRUGAL_TOME (1 << 4)
#define META_RELIC_UNLOCK_ALL (META_RELIC_UNLOCK_ECHO_BELL | META_RELIC_UNLOCK_SPLIT_PRISM | META_RELIC_UNLOCK_BLOOD_AMBER | META_RELIC_UNLOCK_TITAN_HEART | META_RELIC_UNLOCK_FRUGAL_TOME)

typedef enum {
    META_CONTENT_PARTY_SLOT_IV,
    META_CONTENT_FORMATION_DRILLS,
    META_CONTENT_PARTY_SLOT_V,
    META_CONTENT_CLASS_PALADIN,
    META_CONTENT_CLASS_WARLOCK,
    META_CONTENT_CLASS_BARD,
    META_CONTENT_RELIC_ECHO_BELL,
    META_CONTENT_RELIC_SPLIT_PRISM,
    META_CONTENT_RELIC_BLOOD_AMBER,
    META_CONTENT_RELIC_TITAN_HEART,
    META_CONTENT_RELIC_FRUGAL_TOME,
    META_CONTENT_CARD_TACTICAL_SHIFT,
    META_CONTENT_CARD_IRON_INTERCEPT,
    META_CONTENT_CARD_SANCTUARY,
    META_CONTENT_CARD_PRISMATIC_BURST,
    META_CONTENT_RELIC_DUELIST_SIGIL,
    META_CONTENT_RELIC_FELLOWSHIP_STANDARD,
    META_CONTENT_RELIC_CHRONICLE_QUILL,
    META_CONTENT_EVENT_CHAMPIONS_FEAST,
    META_CONTENT_EVENT_CROWDED_STAGE,
    META_CONTENT_EVENT_HALL_OF_RECORDS,
    META_CONTENT_COUNT
} MetaContentUnlock;

#define META_CONTENT_UNLOCK_ALL ((1 << META_CONTENT_COUNT) - 1)

typedef struct {
    int runs_completed;
    int wins;
    int best_floor;
    int bosses_defeated_total;
    int renown;
    bool meta_progress_unlocked;
    int highest_area_unlocked;
    int starting_gold_rank;
    int ascension_level;
    int max_ascension_unlocked;
    int starting_relic_rank;
    int interrupts_total;
    bool slot4_unlocked;
    bool slot5_unlocked;
    bool paladin_unlocked;
    bool warlock_unlocked;
    bool bard_unlocked;
    bool achievements[ACH_COUNT];
    int achievement_times[ACH_COUNT];
    int achievement_timestamps[ACH_COUNT];
    int achievement_party[ACH_COUNT];
    bool start_prep;
    bool start_energize;
    bool start_fortify;
    bool start_rejuv;
    int dmg_bonus;
    int shield_bonus;
    int first_draw_bonus;
    bool seasoned_adventurer;
    bool master_raider;
    int opening_damage_bonus;
    int execute_draw_rank;
    int weak_enemy_damage_bonus;
    int elite_damage_bonus;
    int boss_damage_bonus;
    int warlock_damage_bonus;
    int combat_start_shield;
    int max_hp_bonus_rank;
    int shield_cap_rank;
    bool fortify_upgraded;
    int paladin_shield_bonus;
    bool emergency_barrier_unlocked;
    bool last_stand_unlocked;
    int starting_energy_bonus;
    bool prep_upgraded;
    bool rejuv_upgraded;
    int heal_bonus;
    int camp_bonus_rank;
    int bard_draw_bonus;
    int shop_discount_rank;
    int reward_reroll_discount_rank;
    int reward_choice_bonus;
    int reward_upgrade_chance_rank;
    int gold_conversion_rank;
    int relic_choice_bonus;
    int relic_unlock_flags;
    int content_unlock_flags;
    bool formation_drills;
    int ascension_beaten;
    bool tutorial_seen_elite;
    bool tutorial_seen_boss;
    bool tutorial_seen_shop;
    bool tutorial_seen_event;
    bool tutorial_seen_rest;
    bool tutorial_seen_level_up;
    bool tutorial_seen_discard;
    bool tutorial_seen_game_over;
    bool tutorial_seen_meta_shop;
} MetaProgress;

void meta_set_defaults(MetaProgress *meta);
void meta_load(MetaProgress *meta);
void meta_save(const MetaProgress *meta);
int meta_party_slots(const MetaProgress *meta);
int meta_starting_gold(const MetaProgress *meta);
int meta_next_travel_fund_cost(const MetaProgress *meta);
int meta_next_legacy_cost(const MetaProgress *meta);
bool meta_area_unlocked(const MetaProgress *meta, int area_index);
bool meta_class_unlocked(const MetaProgress *meta, int class_index);
bool meta_unlock_class(MetaProgress *meta, int class_index);
int meta_next_upgrade_cost(int rank);
int meta_starting_deck_bonuses(const MetaProgress *meta);
int meta_dmg_bonus(const MetaProgress *meta);
int meta_shield_bonus(const MetaProgress *meta);
int meta_first_draw_bonus(const MetaProgress *meta);
int meta_opening_damage_bonus(const MetaProgress *meta);
int meta_execute_draw_rank(const MetaProgress *meta);
int meta_weak_enemy_damage_bonus(const MetaProgress *meta);
int meta_elite_damage_bonus(const MetaProgress *meta);
int meta_boss_damage_bonus(const MetaProgress *meta);
int meta_warlock_damage_bonus(const MetaProgress *meta);
int meta_combat_start_shield(const MetaProgress *meta);
int meta_max_hp_bonus(const MetaProgress *meta);
int meta_shield_cap_percent(const MetaProgress *meta);
int meta_paladin_shield_bonus(const MetaProgress *meta);
bool meta_has_emergency_barrier(const MetaProgress *meta);
bool meta_has_last_stand(const MetaProgress *meta);
int meta_starting_energy_bonus(const MetaProgress *meta);
int meta_heal_bonus(const MetaProgress *meta);
int meta_camp_bonus_gold(const MetaProgress *meta);
int meta_bard_draw_bonus(const MetaProgress *meta);
int meta_shop_discount(const MetaProgress *meta);
int meta_discounted_cost(const MetaProgress *meta, int base_cost);
int meta_reward_reroll_cost(const MetaProgress *meta, int base_cost);
int meta_reward_choice_bonus(const MetaProgress *meta);
int meta_reward_upgrade_chance_percent(const MetaProgress *meta);
int meta_gold_conversion_divisor(const MetaProgress *meta);
int meta_relic_choice_bonus(const MetaProgress *meta);
const char *meta_content_key(MetaContentUnlock id);
MetaContentUnlock meta_content_from_key(const char *key);
const char *meta_content_display_name(const char *key, bool revealed);
AchievementId meta_content_required_achievement(const char *key);
const char *meta_content_unlock_event(const char *key);
void meta_content_register_unlock_event(const char *unlock_key, const char *unlock_event);
bool meta_content_active(const MetaProgress *meta, const char *key);
bool meta_content_eligible(const MetaProgress *meta, const char *key);
void meta_content_activate(MetaProgress *meta, const char *key);
const char *meta_relic_unlock_key(RelicId id);
void meta_content_achievement_rewards(AchievementId id, bool achieved, char *out, int out_size);
int meta_relic_unlock_flag(RelicId id);
bool meta_relic_available(const MetaProgress *meta, RelicId id);
void meta_unlock_relic(MetaProgress *meta, RelicId id);
int meta_renown_bonus_per_boss(const MetaProgress *meta);
int meta_renown_bonus_per_clear(const MetaProgress *meta);
int meta_record_run(
    MetaProgress *meta,
    bool won,
    int area_index,
    int floor_reached,
    int bosses_defeated,
    int party_size,
    const int *party_classes,
    int deaths,
    int relic_count,
    int interrupts,
    int best_combat_turns,
    int area_count,
    int *achievement_renown,
    char *achievement_names,
    int achievement_names_size);

#endif
