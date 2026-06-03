#include "screens.h"
#include "game.h"
#include "assets.h"
#include "constants.h"
#include "data/card_defs.h"
#include "systems/relic.h"
#include "systems/telemetry.h"
#include "util/log.h"
#include "ui/deck_browser.h"
#include "ui/theme.h"
#include "ui/layout.h"
#include "ui/ui.h"
#include "util/text.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define UPGRADE_COST 30
#define SUPER_UPGRADE_COST 60
#define REMOVE_COST 20
#define CARD_SALE_COST 25
#define CARD_REROLL_COST 5
#define HP_BOOST_COST 15
#define BOON_COST 20
#define FATE_INTEREST_BOON_COST 30
#define ENCHANT_ECHO_COST 75
#define ENCHANT_LIFESTEAL_COST 45
#define ENCHANT_RETAIN_COST 36
#define ENCHANT_INTERRUPT_COST 45
#define ENCHANT_TAUNT_COST 30
#define ENCHANT_REMOVE_CURSE_COST 20

typedef enum {
    SHOP_MAIN,
    SHOP_UPGRADE_1,
    SHOP_UPGRADE_2,
    SHOP_REMOVE,
    SHOP_HP_BOOST,
    SHOP_ENCHANT,
} ShopMode;

static ShopMode mode = SHOP_MAIN;
static int hovered_deck = -1;
static int enchant_selected = -1;
static int enchant_option = -1;
static DeckBrowser shop_browser;
static char msg[128] = "";
static bool lucky_coin_given = false;
static int active_shop_key = -99999;
static const CardDef *shop_card = NULL;
static Button enchant_back_btn;
static bool enchant_back_btn_ready = false;

static void log_shop_metric(const char *action, int cost, int gold_before, bool success, const char *card_id)
{
    char run_id[16], area[16], floor[16], cost_text[16], before[16], after[16], success_text[8];
    snprintf(run_id, sizeof(run_id), "%d", g_state.telemetry_run_id);
    snprintf(area, sizeof(area), "%d", g_state.current_area);
    snprintf(floor, sizeof(floor), "%d", g_state.map.floor + 1);
    snprintf(cost_text, sizeof(cost_text), "%d", cost);
    snprintf(before, sizeof(before), "%d", gold_before);
    snprintf(after, sizeof(after), "%d", g_state.gold);
    snprintf(success_text, sizeof(success_text), "%d", success ? 1 : 0);
    const char *fields[] = {
        run_id,
        area,
        floor,
        action ? action : "",
        cost_text,
        before,
        after,
        success_text,
        card_id ? card_id : ""
    };
    telemetry_csv_append(
        "shop_metrics.csv",
        "timestamp,run_id,area,floor,action,cost,gold_before,gold_after,success,card_id",
        fields,
        9);

    char json[512];
    snprintf(json, sizeof(json),
        "\"area\":%d,\"floor\":%d,\"action\":\"%s\",\"cost\":%d,\"gold_before\":%d,\"gold_after\":%d,\"success\":%s,\"card_id\":\"%s\"",
        g_state.current_area,
        g_state.map.floor + 1,
        action ? action : "",
        cost,
        gold_before,
        g_state.gold,
        success ? "true" : "false",
        card_id ? card_id : "");
    telemetry_push_json("shop_purchase", json);
}

static Rectangle shop_browser_bounds(void)
{
    return layout_deck_browser_viewport();
}

static int shop_cost(int base_cost)
{
    return meta_discounted_cost(&g_state.meta, base_cost);
}

static int valid_card_count(const Deck *deck)
{
    int count = 0;
    if (!deck) return 0;
    for (int i = 0; i < deck->card_count; i++)
        if (deck->cards[i].def)
            count++;
    return count;
}

static int enchant_keyword_cost(int keyword)
{
    switch (keyword)
    {
        case 0: return ENCHANT_ECHO_COST;
        case 1: return ENCHANT_LIFESTEAL_COST;
        case 2: return ENCHANT_RETAIN_COST;
        case 3: return ENCHANT_INTERRUPT_COST;
        case 4: return ENCHANT_TAUNT_COST;
        default: return 0;
    }
}

typedef enum {
    ENCHANT_OPTION_ECHO,
    ENCHANT_OPTION_LIFESTEAL,
    ENCHANT_OPTION_RETAIN,
    ENCHANT_OPTION_INTERRUPT,
    ENCHANT_OPTION_TAUNT,
    ENCHANT_OPTION_REMOVE_CURSE,
    ENCHANT_OPTION_COUNT
} EnchantOption;

static const char *enchant_option_name(int option)
{
    switch ((EnchantOption)option)
    {
        case ENCHANT_OPTION_ECHO:         return "ECHO";
        case ENCHANT_OPTION_LIFESTEAL:    return "LIFESTEAL";
        case ENCHANT_OPTION_RETAIN:       return "RETAIN";
        case ENCHANT_OPTION_INTERRUPT:    return "INTERRUPT";
        case ENCHANT_OPTION_TAUNT:        return "TAUNT";
        case ENCHANT_OPTION_REMOVE_CURSE: return "REMOVE CURSE";
        default:                          return "";
    }
}

static int enchant_option_cost(int option)
{
    if (option == ENCHANT_OPTION_REMOVE_CURSE)
        return shop_cost(ENCHANT_REMOVE_CURSE_COST);
    return shop_cost(enchant_keyword_cost(option));
}

static bool card_can_add_enchantment(const CardInstance *inst, int keyword)
{
    if (!inst || !inst->def || card_instance_has_any_override(inst)) return false;

    switch (keyword)
    {
        case 0: return !card_instance_has_echo(inst) &&
            (inst->def->damage > 0 || inst->def->heal > 0 || inst->def->shield > 0);
        case 1: return card_instance_lifesteal(inst) <= 0 &&
            inst->def->damage > 0 && inst->def->class != CLASS_NONE;
        case 2: return !card_instance_has_retain(inst);
        case 3: return !card_instance_has_interrupt(inst) && inst->def->target == TARGET_ENEMY;
        case 4: return !card_instance_has_taunt(inst) && inst->def->class != CLASS_NONE;
        default: return false;
    }
}

static bool card_has_removable_curse(const CardInstance *inst)
{
    return inst && inst->def &&
        (inst->fleeting_override == 1 || inst->exhaust_override == 1);
}

static bool card_can_apply_enchant_option(const CardInstance *inst, int option)
{
    if (option == ENCHANT_OPTION_REMOVE_CURSE)
        return card_has_removable_curse(inst);
    return option >= 0 && option < ENCHANT_OPTION_REMOVE_CURSE &&
        card_can_add_enchantment(inst, option);
}

static int deck_min_enchant_cost(void)
{
    int minimum = 0;
    for (int i = 0; i < g_state.run_deck.card_count; i++)
    {
        CardInstance *inst = &g_state.run_deck.cards[i];
        if (card_has_removable_curse(inst) &&
            (minimum == 0 || enchant_option_cost(ENCHANT_OPTION_REMOVE_CURSE) < minimum))
            minimum = enchant_option_cost(ENCHANT_OPTION_REMOVE_CURSE);

        for (int keyword = 0; keyword <= 4; keyword++)
        {
            int cost = enchant_option_cost(keyword);
            if (card_can_add_enchantment(inst, keyword) &&
                (minimum == 0 || cost < minimum))
                minimum = cost;
        }
    }
    return minimum;
}

static const CardDef *random_party_card(void)
{
    const CardDef *pool[96];
    int count = 0;

    for (int i = 0; i < g_state.selected_count; i++)
    {
        ClassType ct = (ClassType)g_state.selected_classes[i];
        if (ct < 0 || ct >= CLASS_COUNT || !class_card_sets[ct]) continue;
        for (int c = 0; c < class_card_counts[ct] && count < 96; c++)
            pool[count++] = &class_card_sets[ct][c];
    }

    for (int c = 0; c < utility_card_count && count < 96; c++)
        pool[count++] = &utility_cards[c];

    if (count <= 0)
        return utility_card_count > 0 ? &utility_cards[0] : card_def_by_id("util_prep");
    return pool[rand() % count];
}

static void reset_shop_for_visit(void)
{
    int key = g_state.map.floor * 1000 + g_state.map.current_index;
    if (key == active_shop_key) return;
    active_shop_key = key;
    assets_play_music(MUSIC_SHOP);
    mode = SHOP_MAIN;
    hovered_deck = -1;
    enchant_selected = -1;
    enchant_option = -1;
    enchant_back_btn_ready = false;
    deck_browser_reset(&shop_browser);
    msg[0] = '\0';
    lucky_coin_given = false;
    shop_card = random_party_card();
}

static void complete_shop(void)
{
    int ci = g_state.map.current_index;
    if (ci >= 0 && ci < g_state.map.node_count)
        g_state.map.nodes[ci].completed = true;
    g_state.map.current_index = -1;
    map_unlock_next(&g_state.map);
    active_shop_key = -99999;
    lucky_coin_given = false;
    mode = SHOP_MAIN;
    enchant_selected = -1;
    enchant_option = -1;
    enchant_back_btn_ready = false;
    game_change_screen(SCREEN_MAP);
}

static int shop_boon_cost(void)
{
    return shop_cost(relic_has(g_state.relics, g_state.relic_count, RELIC_FATES_INTEREST) ? FATE_INTEREST_BOON_COST : BOON_COST);
}

static int shop_boon_turns(void)
{
    return relic_has(g_state.relics, g_state.relic_count, RELIC_FATES_INTEREST) ? 3 : 1;
}

static void buy_boon(bool energy)
{
    int cost = shop_boon_cost();
    int gold_before = g_state.gold;
    if (!game_spend_gold(cost, energy ? "shop_boon_energy" : "shop_boon_draw"))
    {
        snprintf(msg, sizeof(msg), "Need %dg.", cost);
        log_shop_metric(energy ? "energy_boon" : "draw_boon", cost, gold_before, false, "");
        return;
    }

    if (energy)
    {
        g_state.next_combat_energy_bonus += 1;
        assets_play_sfx(SFX_SHOP_PURCHASE);
        snprintf(msg, sizeof(msg), "Next combat: +1 energy.");
    }
    else
    {
        g_state.next_combat_draw_bonus += 2;
        assets_play_sfx(SFX_SHOP_PURCHASE);
        snprintf(msg, sizeof(msg), "Next combat: +2 draw.");
    }

    int turns = shop_boon_turns();
    if (g_state.next_combat_boon_turns < turns)
        g_state.next_combat_boon_turns = turns;
    log_shop_metric(energy ? "energy_boon" : "draw_boon", cost, gold_before, true, "");
}

static Rectangle sale_card_rect(void)
{
    return (Rectangle){ 60.0f, 86.0f, 96.0f, 120.0f };
}

static Rectangle sale_buy_button(void)
{
    return (Rectangle){ 28.0f, 214.0f, (float)BTN_WIDE, (float)BTN_H };
}

static Rectangle sale_reroll_button(void)
{
    return (Rectangle){ 48.0f, 244.0f, (float)BTN_MED, (float)BTN_H };
}

static Rectangle option_rect(int col, int row)
{
    return (Rectangle){ 212.0f + col * 204.0f, 74.0f + row * 58.0f, (float)BTN_FULL, 44.0f };
}

static Rectangle leave_button(void)
{
    return (Rectangle){ 520.0f, 286.0f, (float)BTN_NARROW, (float)BTN_H };
}

static Rectangle enchant_option_rect(int option)
{
    return (Rectangle){ 448.0f, 90.0f + option * 27.0f, 172.0f, 22.0f };
}

static Rectangle enchant_apply_button(void)
{
    return (Rectangle){ 448.0f, 264.0f, 172.0f, (float)BTN_H };
}

static Rectangle enchant_back_button(void)
{
    return (Rectangle){ 448.0f, 294.0f, 172.0f, (float)BTN_H };
}

static void ensure_enchant_back_button(void)
{
    if (enchant_back_btn_ready)
        return;
    enchant_back_btn = button_create(
        enchant_back_button(),
        "BACK",
        (Color){ 72, 74, 94, 255 },
        (Color){ 104, 108, 136, 255 },
        WHITE);
    enchant_back_btn_ready = true;
}

static bool clicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r);
}

void shop_screen_update(void)
{
    reset_shop_for_visit();

    if (!g_state.tutorial_active)
        game_start_tutorial_once(&g_state.meta.tutorial_seen_shop, TUTORIAL_STEP_SHOP);
    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_SHOP)
    {
        if (game_tutorial_handle_close())
            return;
    }

    if (!lucky_coin_given && relic_has(g_state.relics, g_state.relic_count, RELIC_LUCKY_COIN))
    {
        game_gain_gold(10, "lucky_coin_shop");
        lucky_coin_given = true;
    }

    if (mode == SHOP_MAIN)
    {
        bool can_upg1 = deck_browser_has_upgradeable_at(&g_state.run_deck, 1);
        bool can_upg2 = deck_browser_has_upgradeable_at(&g_state.run_deck, 2);
        bool can_remove = valid_card_count(&g_state.run_deck) > 3;
        bool deck_space = g_state.run_deck.card_count < MAX_DECK_SIZE;
        int min_enchant_cost = deck_min_enchant_cost();
        int card_sale_cost = shop_cost(CARD_SALE_COST);
        int card_reroll_cost = shop_cost(CARD_REROLL_COST);
        int upgrade_cost = shop_cost(UPGRADE_COST);
        int super_upgrade_cost = shop_cost(SUPER_UPGRADE_COST);
        int remove_cost = shop_cost(REMOVE_COST);
        int hp_boost_cost = shop_cost(HP_BOOST_COST);

        if (clicked(sale_buy_button()))
        {
            int gold_before = g_state.gold;
            if (!deck_space)
            {
                snprintf(msg, sizeof(msg), "Deck is full.");
                log_shop_metric("buy_card", card_sale_cost, gold_before, false, shop_card && shop_card->id ? shop_card->id : "");
            }
            else if (!game_spend_gold(card_sale_cost, "shop_buy_card"))
            {
                snprintf(msg, sizeof(msg), "Need %dg.", card_sale_cost);
                log_shop_metric("buy_card", card_sale_cost, gold_before, false, shop_card && shop_card->id ? shop_card->id : "");
            }
            else
            {
                deck_add_card(&g_state.run_deck, shop_card);
                assets_play_sfx(SFX_SHOP_PURCHASE);
                snprintf(msg, sizeof(msg), "Bought %s.", shop_card ? shop_card->name : "a card");
                log_shop_metric("buy_card", card_sale_cost, gold_before, true, shop_card && shop_card->id ? shop_card->id : "");
                shop_card = random_party_card();
            }
        }
        else if (clicked(sale_reroll_button()))
        {
            int gold_before = g_state.gold;
            if (!game_spend_gold(card_reroll_cost, "shop_reroll_card"))
            {
                snprintf(msg, sizeof(msg), "Need %dg.", card_reroll_cost);
                log_shop_metric("reroll_card", card_reroll_cost, gold_before, false, "");
            }
            else
            {
                shop_card = random_party_card();
                snprintf(msg, sizeof(msg), "Shop card rerolled.");
                log_shop_metric("reroll_card", card_reroll_cost, gold_before, true, "");
            }
        }
        else if (clicked(option_rect(0, 0)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < upgrade_cost)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", upgrade_cost);
                log_shop_metric("upgrade_card_select", upgrade_cost, gold_before, false, "");
            }
            else if (!can_upg1)
            {
                snprintf(msg, sizeof(msg), "No base cards can improve.");
                log_shop_metric("upgrade_card_select", upgrade_cost, gold_before, false, "");
            }
            else
            {
                mode = SHOP_UPGRADE_1;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(1, 0)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < super_upgrade_cost)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", super_upgrade_cost);
                log_shop_metric("max_upgrade_select", super_upgrade_cost, gold_before, false, "");
            }
            else if (!can_upg2)
            {
                snprintf(msg, sizeof(msg), "No upgraded cards can improve.");
                log_shop_metric("max_upgrade_select", super_upgrade_cost, gold_before, false, "");
            }
            else
            {
                mode = SHOP_UPGRADE_2;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(0, 1)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < remove_cost)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", remove_cost);
                log_shop_metric("remove_card_select", remove_cost, gold_before, false, "");
            }
            else if (!can_remove)
            {
                snprintf(msg, sizeof(msg), "Deck too small.");
                log_shop_metric("remove_card_select", remove_cost, gold_before, false, "");
            }
            else
            {
                mode = SHOP_REMOVE;
                deck_browser_reset(&shop_browser);
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(1, 1)))
        {
            int gold_before = g_state.gold;
            if (g_state.gold < hp_boost_cost)
            {
                snprintf(msg, sizeof(msg), "Need %dg.", hp_boost_cost);
                log_shop_metric("train_hp_select", hp_boost_cost, gold_before, false, "");
            }
            else
            {
                mode = SHOP_HP_BOOST;
                msg[0] = '\0';
            }
        }
        else if (clicked(option_rect(0, 2)))
        {
            buy_boon(true);
        }
        else if (clicked(option_rect(1, 2)))
        {
            buy_boon(false);
        }
        else if (min_enchant_cost > 0 && g_state.gold >= min_enchant_cost && clicked(option_rect(0, 3)))
        {
            mode = SHOP_ENCHANT;
            enchant_selected = -1;
            enchant_option = -1;
            deck_browser_reset(&shop_browser);
            msg[0] = '\0';
        }
        else if (clicked(leave_button()))
        {
            log_shop_metric("leave", 0, g_state.gold, true, "");
            complete_shop();
        }
    }
    else if (mode == SHOP_UPGRADE_1 || mode == SHOP_UPGRADE_2 || mode == SHOP_REMOVE)
    {
        int target_level = mode == SHOP_UPGRADE_1 ? 1 : (mode == SHOP_UPGRADE_2 ? 2 : 0);
        int selected = deck_browser_update(&shop_browser, &g_state.run_deck, shop_browser_bounds(), target_level);
        hovered_deck = shop_browser.hovered_deck_index;

        if (selected >= 0)
        {
            CardInstance *inst = &g_state.run_deck.cards[selected];
            if (mode == SHOP_UPGRADE_1)
            {
                int cost = shop_cost(UPGRADE_COST);
                int gold_before = g_state.gold;
                if (game_spend_gold(cost, "shop_upgrade"))
                {
                    inst->upgrade_level = 1;
                    assets_play_sfx(SFX_SHOP_PURCHASE);
                    snprintf(msg, sizeof(msg), "%s upgraded.", inst->def ? inst->def->name : "Card");
                    log_shop_metric("upgrade_card", cost, gold_before, true, inst->def && inst->def->id ? inst->def->id : "");
                    mode = SHOP_MAIN;
                }
                else
                    log_shop_metric("upgrade_card", cost, gold_before, false, inst->def && inst->def->id ? inst->def->id : "");
            }
            else if (mode == SHOP_UPGRADE_2)
            {
                int cost = shop_cost(SUPER_UPGRADE_COST);
                int gold_before = g_state.gold;
                if (game_spend_gold(cost, "shop_super_upgrade"))
                {
                    inst->upgrade_level = 2;
                    assets_play_sfx(SFX_SHOP_PURCHASE);
                    snprintf(msg, sizeof(msg), "%s maxed.", inst->def ? inst->def->name : "Card");
                    log_shop_metric("max_upgrade_card", cost, gold_before, true, inst->def && inst->def->id ? inst->def->id : "");
                    mode = SHOP_MAIN;
                }
                else
                    log_shop_metric("max_upgrade_card", cost, gold_before, false, inst->def && inst->def->id ? inst->def->id : "");
            }
            else
            {
                int cost = shop_cost(REMOVE_COST);
                int gold_before = g_state.gold;
                if (game_spend_gold(cost, "shop_remove"))
                {
                    const CardDef *def = inst->def;
                    int uid = inst->uid;
                    deck_remove_card_by_uid(&g_state.run_deck, uid);
                    assets_play_sfx(SFX_SHOP_PURCHASE);
                    snprintf(msg, sizeof(msg), "Removed %s.", def ? def->name : "a card");
                    log_shop_metric("remove_card", cost, gold_before, true, def && def->id ? def->id : "");
                    mode = SHOP_MAIN;
                }
                else
                    log_shop_metric("remove_card", cost, gold_before, false, inst->def && inst->def->id ? inst->def->id : "");
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            mode = SHOP_MAIN;
            hovered_deck = -1;
        }
    }
    else if (mode == SHOP_HP_BOOST)
    {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < g_state.run_party.count; i++)
        {
            Rectangle r = { (float)(VIRT_W / 2 - BTN_FULL / 2), 86.0f + i * 38.0f, (float)BTN_FULL, 28.0f };
            if (!CheckCollisionPointRec(mouse, r) || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                continue;
            int cost = shop_cost(HP_BOOST_COST);
            int gold_before = g_state.gold;
            if (game_spend_gold(cost, "shop_hp_boost"))
            {
                PartyMember *pm = &g_state.run_party.members[i];
                pm->max_hp += 5;
                pm->hp += 5;
                if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;
                pm->alive = true;
                assets_play_sfx(SFX_SHOP_PURCHASE);
                snprintf(msg, sizeof(msg), "%s gained +5 max HP.", pm->name);
                log_shop_metric("train_hp", cost, gold_before, true, class_name(pm->class));
                mode = SHOP_MAIN;
            }
            else
                log_shop_metric("train_hp", cost, gold_before, false, "");
            break;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            mode = SHOP_MAIN;
    }
    else if (mode == SHOP_ENCHANT)
    {
        ensure_enchant_back_button();
        button_update(&enchant_back_btn);
        if (enchant_back_btn.pressed_this_frame ||
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
            IsKeyPressed(KEY_ESCAPE))
        {
            mode = SHOP_MAIN;
            hovered_deck = -1;
            enchant_selected = -1;
            enchant_option = -1;
            return;
        }

        int selected = deck_browser_update(&shop_browser, &g_state.run_deck, shop_browser_bounds(), -1);
        hovered_deck = shop_browser.hovered_deck_index;
        if (selected >= 0)
        {
            enchant_selected = selected;
            enchant_option = -1;
            msg[0] = '\0';
        }

        CardInstance *inst = enchant_selected >= 0 &&
            enchant_selected < g_state.run_deck.card_count ?
            &g_state.run_deck.cards[enchant_selected] : NULL;
        if (inst && inst->def)
        {
            for (int i = 0; i < ENCHANT_OPTION_COUNT; i++)
            {
                if (!card_can_apply_enchant_option(inst, i))
                    continue;
                if (clicked(enchant_option_rect(i)))
                {
                    enchant_option = i;
                    snprintf(msg, sizeof(msg), "%s selected.", enchant_option_name(i));
                    return;
                }
            }
        }

        if (clicked(enchant_apply_button()))
        {
            if (!inst || !inst->def)
            {
                snprintf(msg, sizeof(msg), "Pick a card first.");
                return;
            }
            if (!card_can_apply_enchant_option(inst, enchant_option))
            {
                snprintf(msg, sizeof(msg), "Pick an available enchantment.");
                return;
            }

            int cost = enchant_option_cost(enchant_option);
            int gold_before = g_state.gold;
            const char *card_id = inst->def->id ? inst->def->id : "";
            bool remove_curse = enchant_option == ENCHANT_OPTION_REMOVE_CURSE;
            if (!game_spend_gold(cost, remove_curse ? "shop_enchant_remove_curse" : "shop_enchant"))
            {
                snprintf(msg, sizeof(msg), "Need %dg.", cost);
                log_shop_metric(remove_curse ? "enchant_remove_curse" : "enchant_card", cost, gold_before, false, card_id);
                return;
            }

            if (remove_curse)
            {
                inst->fleeting_override = -1;
                inst->exhaust_override = -1;
                snprintf(msg, sizeof(msg), "Curse removed from %s.", inst->def->name);
            }
            else
            {
                if (enchant_option == ENCHANT_OPTION_ECHO) inst->echo_override = 1;
                else if (enchant_option == ENCHANT_OPTION_LIFESTEAL) inst->lifesteal_override = 1;
                else if (enchant_option == ENCHANT_OPTION_RETAIN) inst->retain_override = 1;
                else if (enchant_option == ENCHANT_OPTION_INTERRUPT) inst->interrupt_override = 1;
                else if (enchant_option == ENCHANT_OPTION_TAUNT) inst->taunt_override = 1;
                snprintf(msg, sizeof(msg), "%s gained %s.", inst->def->name, enchant_option_name(enchant_option));
            }

            assets_play_sfx(SFX_SHOP_PURCHASE);
            log_shop_metric(remove_curse ? "enchant_remove_curse" : "enchant_card", cost, gold_before, true, card_id);
            mode = SHOP_MAIN;
            hovered_deck = -1;
            enchant_selected = -1;
            enchant_option = -1;
        }
    }
}

static void draw_shop_panel(Rectangle r, const char *title, Color accent)
{
    DrawRectangleRec(r, (Color){ 13, 14, 24, 230 });
    DrawRectangleLinesEx(r, 1.0f, (Color){ accent.r, accent.g, accent.b, 155 });
    if (title && title[0])
        draw_text_box((Rectangle){ r.x + 8.0f, r.y + 7.0f, r.width - 16.0f, 12.0f },
            title, 10, 0, accent, TEXT_ALIGN_CENTER);
}

static void draw_shop_button(Rectangle r, const char *label, const char *body, bool enabled, Color accent)
{
    Vector2 mouse = GetMousePosition();
    bool hover = enabled && CheckCollisionPointRec(mouse, r);
    Color bg = !enabled ? (Color){ 28, 29, 38, 230 } :
        hover ? (Color){ accent.r, accent.g, accent.b, 210 } : (Color){ accent.r / 3, accent.g / 3, accent.b / 3, 235 };
    Color title_col = enabled ? RAYWHITE : (Color){ 110, 112, 130, 215 };
    Color body_col = enabled ? (Color){ 185, 190, 215, 220 } : (Color){ 95, 98, 115, 200 };

    Texture2D tex = r.height >= 40.0f ? g_assets.btn_large : g_assets.btn_standard;
    draw_9slice(tex, r.height >= 40.0f ? 8 : 6, r.height >= 40.0f ? 8 : 6, r, bg);

    int label_h = label && label[0] ? measure_text_box(label, (int)r.width - 16, 10, 0) : 0;
    if (label_h <= 0 && label && label[0]) label_h = ui_line_height(10);
    int body_h = body && body[0] ? measure_text_box(body, (int)r.width - 16, 10, 0) : 0;
    if (body_h <= 0 && body && body[0]) body_h = ui_line_height(10);
    int gap = label_h > 0 && body_h > 0 ? 2 : 0;
    int total_h = label_h + gap + body_h;
    int y = (int)(r.y + (r.height - total_h) * 0.5f);

    draw_text_box((Rectangle){ r.x + 8.0f, (float)y, r.width - 16.0f, (float)label_h },
        label, 10, 0, title_col, TEXT_ALIGN_CENTER);
    if (body && body[0])
        draw_text_box((Rectangle){ r.x + 8.0f, (float)(y + label_h + gap), r.width - 16.0f, (float)body_h },
            body, 10, 0, body_col, TEXT_ALIGN_CENTER);
}

static void draw_enchant_option_button(int option, const CardInstance *inst)
{
    Rectangle r = enchant_option_rect(option);
    bool enabled = card_can_apply_enchant_option(inst, option);
    bool selected = enabled && enchant_option == option;
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = !enabled ? (Color){ 24, 25, 34, 235 } :
        selected ? (Color){ 92, 62, 138, 245 } :
        hover ? (Color){ 70, 58, 108, 245 } :
        (Color){ 42, 38, 62, 240 };
    Color border = selected ? (Color){ 225, 185, 255, 255 } :
        hover ? (Color){ 190, 160, 235, 235 } :
        (Color){ 85, 76, 112, 205 };
    Color text = enabled ? RAYWHITE : (Color){ 96, 98, 116, 205 };

    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, selected ? 2.0f : 1.0f, border);

    char label[48];
    snprintf(label, sizeof(label), "%s - %dg", enchant_option_name(option), enchant_option_cost(option));
    draw_text_box((Rectangle){ r.x + 4.0f, r.y + 5.0f, r.width - 8.0f, 12.0f },
        label, 10, 0, text, TEXT_ALIGN_CENTER);
}

static void draw_upgrade_preview(const CardDef *cd, int level, Rectangle tip)
{
    int preview_y = (int)(tip.y + tip.height + 5.0f);
    if (!cd || !card_upgrade_changes_values_at(cd, level))
    {
        draw_text_box((Rectangle){ tip.x, (float)preview_y, tip.width, 24.0f },
            "Upgrade has no value changes", 10, 0, (Color){ 160, 160, 180, 220 }, TEXT_ALIGN_LEFT);
        return;
    }

    int od = card_damage(cd, level), oh = card_heal(cd, level), os = card_shield(cd, level);
    int nd = card_damage(cd, level + 1), nh = card_heal(cd, level + 1), ns = card_shield(cd, level + 1);
    int y = preview_y;
    if (nd != od) { char b[32]; snprintf(b, sizeof(b), "DMG %d>%d", od, nd); DrawText(b, (int)tip.x, y, 10, (Color){ 220, 110, 100, 255 }); y += 14; }
    if (nh != oh) { char b[32]; snprintf(b, sizeof(b), "HEAL %d>%d", oh, nh); DrawText(b, (int)tip.x, y, 10, (Color){ 105, 220, 125, 255 }); y += 14; }
    if (ns != os) { char b[32]; snprintf(b, sizeof(b), "SHIELD %d>%d", os, ns); DrawText(b, (int)tip.x, y, 10, (Color){ 125, 190, 255, 255 }); }
}

void shop_screen_draw(void)
{
    theme_draw_background();

    if (mode == SHOP_MAIN)
    {
        draw_text_box((Rectangle){ 80.0f, 18.0f, 480.0f, 22.0f }, "SHOP", 18, 0, (Color){ 220, 200, 60, 255 }, TEXT_ALIGN_CENTER);

        Rectangle sale_panel = { 24.0f, 58.0f, 168.0f, 220.0f };
        Rectangle service_panel = { 204.0f, 58.0f, 416.0f, 202.0f };
        int card_sale_cost = shop_cost(CARD_SALE_COST);
        int card_reroll_cost = shop_cost(CARD_REROLL_COST);
        int upgrade_cost = shop_cost(UPGRADE_COST);
        int super_upgrade_cost = shop_cost(SUPER_UPGRADE_COST);
        int remove_cost = shop_cost(REMOVE_COST);
        int hp_boost_cost = shop_cost(HP_BOOST_COST);
        char panel_label[48];
        snprintf(panel_label, sizeof(panel_label), "CARD FOR SALE - %dg", card_sale_cost);
        draw_shop_panel(sale_panel, panel_label, (Color){ 90, 160, 230, 230 });
        draw_shop_panel(service_panel, "SERVICES", (Color){ 220, 200, 90, 230 });

        Rectangle card_rect = sale_card_rect();
        if (shop_card)
            theme_draw_card_art_seeded(card_rect, shop_card, 0, theme_card_seed_from_id(shop_card->id, 91u));

        bool deck_space = g_state.run_deck.card_count < MAX_DECK_SIZE;
        char cost_label[48];
        snprintf(cost_label, sizeof(cost_label), "BUY CARD - %dg", card_sale_cost);
        draw_shop_button(sale_buy_button(), cost_label, "", g_state.gold >= card_sale_cost && deck_space, (Color){ 80, 150, 220, 255 });
        snprintf(cost_label, sizeof(cost_label), "NEW CARD - %dg", card_reroll_cost);
        draw_shop_button(sale_reroll_button(), cost_label, "", g_state.gold >= card_reroll_cost, (Color){ 95, 190, 110, 255 });

        bool can_upg1 = g_state.gold >= upgrade_cost && deck_browser_has_upgradeable_at(&g_state.run_deck, 1);
        bool can_upg2 = g_state.gold >= super_upgrade_cost && deck_browser_has_upgradeable_at(&g_state.run_deck, 2);
        bool can_remove = g_state.gold >= remove_cost && valid_card_count(&g_state.run_deck) > 3;
        int boon_cost = shop_boon_cost();
        bool can_boon = g_state.gold >= boon_cost;
        int min_enchant_cost = deck_min_enchant_cost();
        bool can_enchant = min_enchant_cost > 0 && g_state.gold >= min_enchant_cost;

        snprintf(cost_label, sizeof(cost_label), "UPGRADE - %dg", upgrade_cost);
        draw_shop_button(option_rect(0, 0), cost_label, "base card -> upgraded", can_upg1, (Color){ 80, 140, 220, 255 });
        snprintf(cost_label, sizeof(cost_label), "MAX UPGRADE - %dg", super_upgrade_cost);
        draw_shop_button(option_rect(1, 0), cost_label, "upgraded card -> max", can_upg2, (Color){ 205, 165, 70, 255 });
        snprintf(cost_label, sizeof(cost_label), "REMOVE CARD - %dg", remove_cost);
        draw_shop_button(option_rect(0, 1), cost_label, "thin your deck", can_remove, (Color){ 220, 100, 100, 255 });
        snprintf(cost_label, sizeof(cost_label), "TRAIN HP - %dg", hp_boost_cost);
        draw_shop_button(option_rect(1, 1), cost_label, "+5 max HP to one PM", g_state.gold >= hp_boost_cost, (Color){ 90, 200, 120, 255 });

        char boon_label[32];
        snprintf(boon_label, sizeof(boon_label), "ENERGY BOON - %dg", boon_cost);
        draw_shop_button(option_rect(0, 2), boon_label, "+1 next combat", can_boon, (Color){ 230, 205, 70, 255 });
        snprintf(boon_label, sizeof(boon_label), "DRAW BOON - %dg", boon_cost);
        draw_shop_button(option_rect(1, 2), boon_label, "+2 next combat", can_boon, (Color){ 135, 190, 245, 255 });
        char enchant_label[32];
        snprintf(enchant_label, sizeof(enchant_label), "ENCHANT CARD - %dg+", min_enchant_cost > 0 ? min_enchant_cost : shop_cost(ENCHANT_TAUNT_COST));
        draw_shop_button(option_rect(0, 3), enchant_label, "add/remove effects", can_enchant, (Color){ 175, 130, 230, 255 });

        if (g_state.next_combat_energy_bonus > 0 || g_state.next_combat_draw_bonus > 0)
        {
            char boon[96];
            snprintf(boon, sizeof(boon), "Queued boon: +%d energy, +%d draw for %d turn%s",
                g_state.next_combat_energy_bonus,
                g_state.next_combat_draw_bonus,
                g_state.next_combat_boon_turns,
                g_state.next_combat_boon_turns == 1 ? "" : "s");
            draw_text_box((Rectangle){ 210.0f, 266.0f, 278.0f, 24.0f }, boon, 10, 0, (Color){ 230, 220, 140, 230 }, TEXT_ALIGN_LEFT);
        }

        draw_shop_button(leave_button(), "LEAVE", "", true, (Color){ 120, 120, 145, 255 });
        if (msg[0])
            draw_text_box((Rectangle){ 210.0f, 292.0f, 278.0f, 28.0f }, msg, 10, 0, (Color){ 230, 205, 115, 240 }, TEXT_ALIGN_LEFT);
    }
    else if (mode == SHOP_UPGRADE_1 || mode == SHOP_UPGRADE_2)
    {
        int target_level = mode == SHOP_UPGRADE_1 ? 1 : 2;
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f },
            target_level == 1 ? "PICK A CARD TO UPGRADE" : "PICK A CARD TO MAX",
            18, 0, RAYWHITE, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f },
            "Pick a card to continue.", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);
        deck_browser_draw(&shop_browser, &g_state.run_deck, target_level, RAYWHITE);
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
        {
            CardInstance *inst = &g_state.run_deck.cards[hovered_deck];
            Rectangle tip = theme_draw_card_tooltip_limited(layout_deck_inspector_panel(), inst->def, inst->upgrade_level, 268);
            draw_upgrade_preview(inst->def, inst->upgrade_level, tip);
        }
    }
    else if (mode == SHOP_REMOVE)
    {
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f }, "PICK A CARD TO REMOVE", 18, 0, (Color){ 220, 120, 120, 255 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 34.0f, 480.0f, 14.0f }, "Pick a card to remove.", 10, 0, (Color){ 160, 160, 180, 180 }, TEXT_ALIGN_CENTER);
        deck_browser_draw(&shop_browser, &g_state.run_deck, 0, (Color){ 255, 100, 100, 255 });
        if (hovered_deck >= 0 && g_state.run_deck.cards[hovered_deck].def)
            theme_draw_card_tooltip(layout_deck_inspector_panel(), g_state.run_deck.cards[hovered_deck].def, g_state.run_deck.cards[hovered_deck].upgrade_level);
    }
    else if (mode == SHOP_ENCHANT)
    {
        draw_text_box((Rectangle){ 80.0f, 16.0f, 480.0f, 22.0f },
            "ENCHANT A CARD", 18, 0, (Color){ 190, 150, 245, 255 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 36.0f, 35.0f, 568.0f, 14.0f },
            "Pick a deck card, choose an effect, then apply.",
            10, 0, (Color){ 170, 180, 205, 210 }, TEXT_ALIGN_CENTER);

        deck_browser_draw(&shop_browser, &g_state.run_deck, -1, (Color){ 180, 220, 255, 255 });

        Rectangle enchant_panel = { 440.0f, 54.0f, 188.0f, 270.0f };
        draw_shop_panel(enchant_panel, "ENCHANTMENTS", (Color){ 190, 150, 245, 230 });

        CardInstance *inst = enchant_selected >= 0 &&
            enchant_selected < g_state.run_deck.card_count ?
            &g_state.run_deck.cards[enchant_selected] : NULL;
        const char *selected_name = inst && inst->def ? inst->def->name : "Pick a card from your deck";
        draw_text_box((Rectangle){ 448.0f, 73.0f, 172.0f, 12.0f },
            selected_name, 10, 0, inst && inst->def ? RAYWHITE : (Color){ 135, 138, 158, 220 }, TEXT_ALIGN_CENTER);

        for (int i = 0; i < ENCHANT_OPTION_COUNT; i++)
            draw_enchant_option_button(i, inst);

        char apply_label[32];
        int apply_cost = enchant_option_cost(enchant_option);
        bool can_apply = inst && inst->def &&
            card_can_apply_enchant_option(inst, enchant_option) &&
            g_state.gold >= apply_cost;
        if (enchant_option >= 0)
            snprintf(apply_label, sizeof(apply_label), "APPLY - %dg", apply_cost);
        else
            snprintf(apply_label, sizeof(apply_label), "APPLY");
        draw_shop_button(enchant_apply_button(), apply_label, "", can_apply, (Color){ 90, 185, 120, 255 });
        ensure_enchant_back_button();
        button_draw_9slice(&enchant_back_btn);

        if (msg[0])
            draw_text_box((Rectangle){ 24.0f, 324.0f, 400.0f, 16.0f },
                msg, 10, 0, (Color){ 230, 205, 115, 240 }, TEXT_ALIGN_LEFT);
    }
    else if (mode == SHOP_HP_BOOST)
    {
        draw_text_box((Rectangle){ 80.0f, 36.0f, 480.0f, 22.0f }, "TRAIN A HERO", 18, 0, (Color){ 120, 230, 150, 255 }, TEXT_ALIGN_CENTER);
        draw_text_box((Rectangle){ 80.0f, 58.0f, 480.0f, 14.0f }, "Pick one party member.", 10, 0, (Color){ 170, 180, 205, 210 }, TEXT_ALIGN_CENTER);
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < g_state.run_party.count; i++)
        {
            PartyMember *pm = &g_state.run_party.members[i];
            Rectangle r = { (float)(VIRT_W / 2 - BTN_FULL / 2), 86.0f + i * 38.0f, (float)BTN_FULL, 28.0f };
            bool hover = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, hover ? (Color){ 36, 72, 48, 245 } : (Color){ 22, 36, 30, 235 });
            DrawRectangleLinesEx(r, 1.0f, hover ? RAYWHITE : (Color){ 95, 210, 130, 180 });
            char line[96];
            snprintf(line, sizeof(line), "%s  %d/%d HP  ->  max %d", pm->name, pm->hp, pm->max_hp, pm->max_hp + 5);
            draw_text_box((Rectangle){ r.x + 8.0f, r.y + 8.0f, r.width - 16.0f, 12.0f },
                line, 10, 0, RAYWHITE, TEXT_ALIGN_LEFT);
        }
    }

    if (g_state.tutorial_active && g_state.tutorial_step == TUTORIAL_STEP_SHOP)
    {
        game_draw_tutorial_overlay_ex((Rectangle){ 24.0f, 58.0f, 596.0f, 220.0f },
            "Shops",
            "Spend gold on cards, upgrades, removals, HP training, or next-combat boons. Grey options are unaffordable or unavailable.",
            "Click to continue  |  Right-click/Esc: skip",
            0,
            0);
    }
}
