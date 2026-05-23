#include "content_loader.h"
#include "area_defs.h"
#include "card_defs.h"
#include "enemy_defs.h"
#include "encounter_defs.h"
#include "event_defs.h"
#include "systems/map.h"
#include "systems/party.h"
#include "systems/relic.h"
#include "util/log.h"

bool content_load_all(void)
{
    bool ok = true;
    ok = area_defs_load_json("assets/data/areas.json") && ok;
    ok = party_defs_load_json("assets/data/classes.json") && ok;
    ok = card_defs_load_json("assets/data/cards.json") && ok;
    ok = enemy_defs_load_json("assets/data/enemies.json") && ok;
    ok = encounter_defs_load_json("assets/data/encounters.json") && ok;
    ok = relic_defs_load_json("assets/data/relics.json") && ok;
    ok = event_defs_load_json("assets/data/events.json") && ok;
    ok = map_load_json("assets/data/maps.json") && ok;

    if (ok)
    {
        LOG_I(
            CAT_SCREEN,
            "Runtime JSON content loaded: %d areas, %d cards, %d enemies, %d relics, %d events",
            area_defs_count(),
            card_defs_loaded_count(),
            enemy_defs_loaded_count(),
            relic_loaded_count(),
            event_defs_count()
        );
    }
    return ok;
}
