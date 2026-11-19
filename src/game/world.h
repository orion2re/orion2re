#pragma once

#include <array>
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
        // Players, ships, stars and nebulas still use legacy storage.
        std::map<id_player, s_player> players;
        std::map<id_ship, s_ship_data> ships;
        std::map<id_star, s_star_data> stars;
        std::map<id_planet, s_planet_data> planets;
        std::map<id_colony, s_colony> colonies;
        std::map<id_leader, s_leader_data> leaders;
        std::map<id_nebula, s_nebula> nebulas;
        std::map<id_player, std::vector<SETTLER::s_settler>> settlers;
        uint64_t next_planet_id = 0;
        uint64_t next_colony_id = 0;
        // Legacy gameplay order is not an identity. Only vacant positions
        // have reservation records; live colony records belong to colonies.
        std::array<id_colony, MAX_COLONIES> colony_order;
        std::map<int16_t, s_colony> vacant_colony_positions;

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

    s_colony* Find_Colony(s_world_state* world, id_colony id);
    const s_colony* Find_Colony(const s_world_state* world, id_colony id);
    s_colony& Colony(s_world_state* world, id_colony id);
    const s_colony& Colony(const s_world_state* world, id_colony id);
    int32_t Colony_Count(const s_world_state* world);
    std::optional<id_colony> Create_Colony(s_world_state* world, const s_colony& colony);
    bool Erase_Colony(s_world_state* world, id_colony id);

    struct s_colony_slots {
        s_world_state* state = nullptr;
        s_colony* fixture = nullptr;

        s_colony_slots() = default;
        s_colony_slots(s_colony* records) : fixture(records) {}
        explicit s_colony_slots(s_world_state* world) : state(world) {}

        explicit operator bool() const { return state != nullptr || fixture != nullptr; }
        bool operator==(std::nullptr_t) const { return !static_cast<bool>(*this); }
        bool operator==(const s_colony* records) const { return fixture == records && state == nullptr; }
        s_colony& operator[](int32_t position) const;
        id_colony Id_At(int32_t position) const;
        int16_t Position_Of(const s_colony* colony) const;
        bool Activate(int16_t position, int16_t player_lane = -1) const;
        bool Install(int16_t position, id_colony id, const s_colony& record) const;
        void Retire(int16_t position) const;
        void Reset() const;
    };

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
