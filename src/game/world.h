#pragma once

#include <map>
#include <span>
#include <vector>

#include "object_ids.h"
#include "types.h"
#include "compat.h"
#include "consts.h"
#include "orion2_consts.h"
#include "pop.h"
#include "settler.h"
#include "orion2.h"

namespace game {
    struct s_world_state {
        // Only leaders are authoritative until the other collections migrate.
        std::map<id_player, s_player> players;
        std::map<id_ship, s_ship_data> ships;
        std::map<id_star, s_star_data> stars;
        std::map<id_planet, s_planet_data> planets;
        std::map<id_colony, s_colony> colonies;
        std::map<id_leader, s_leader_data> leaders;
        std::map<id_nebula, s_nebula> nebulas;

        s_world_state();
    };

    extern s_world_state world;

    s_leader_data* Find_Leader(s_world_state* world, id_leader id);
    const s_leader_data* Find_Leader(const s_world_state* world, id_leader id);

    // Checked access for existing legacy APIs; lookups never insert records.
    s_leader_data& Leader(s_world_state* world, int32_t id);
    const s_leader_data& Leader(const s_world_state* world, int32_t id);
    s_leader_data& Leader(s_world_state* world, id_leader id);
    const s_leader_data& Leader(const s_world_state* world, id_leader id);
    void Reset_Leaders(s_world_state* world);
    void Import_Leaders(s_world_state* world, std::span<const s_leader_data> records);
    // Fixed-roster compatibility storage, not the in-memory representation.
    std::vector<s_leader_data> Export_Leaders(const s_world_state* world);
} // namespace game
