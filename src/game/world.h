#pragma once

#include <map>
#include <optional>
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
        // Players, ships, stars, colonies and nebulas still use legacy storage.
        std::map<id_player, s_player> players;
        std::map<id_ship, s_ship_data> ships;
        std::map<id_star, s_star_data> stars;
        std::map<id_planet, s_planet_data> planets;
        std::map<id_colony, s_colony> colonies;
        std::map<id_leader, s_leader_data> leaders;
        std::map<id_nebula, s_nebula> nebulas;
        std::map<id_player, std::vector<SETTLER::s_settler>> settlers;
        uint64_t next_planet_id = 0;

        s_world_state();
    };

    extern s_world_state world;

    s_planet_data* Find_Planet(s_world_state* world, id_planet id);
    const s_planet_data* Find_Planet(const s_world_state* world, id_planet id);
    s_planet_data& Planet(s_world_state* world, id_planet id);
    const s_planet_data& Planet(const s_world_state* world, id_planet id);
    int32_t Planet_Count(const s_world_state* world);
    std::optional<id_planet> Create_Planet(s_world_state* world, const s_planet_data& planet);
    bool Insert_Planet(s_world_state* world, id_planet id, const s_planet_data& planet);
    bool Erase_Planet(s_world_state* world, id_planet id);
    void Import_Legacy_Planets(s_world_state* world, std::span<const s_planet_data> records);

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
