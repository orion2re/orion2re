#ifndef ORION2RE_LUA_GAME_H
#define ORION2RE_LUA_GAME_H

#include <cstdint>
#include <string>

#include "game/object_ids.h"

namespace orion2re {
    using game::id_colony;
    using game::id_planet;
    using game::NO_COLONY;
    using game::NO_PLANET;
}

namespace orion2re::lua::game {

    struct s_authoritative_population_growth_scope {
        bool active = false;

        explicit s_authoritative_population_growth_scope(bool active);
        ~s_authoritative_population_growth_scope();
        s_authoritative_population_growth_scope(
            const s_authoritative_population_growth_scope&) = delete;
        s_authoritative_population_growth_scope& operator=(
            const s_authoritative_population_growth_scope&) = delete;
    };

    struct s_semantic_state_replacement {
        std::uint64_t prior_event_sequence = 1;
        std::uint64_t prior_topology_generation = 0;
        std::uint64_t prior_ship_world = 0;
        bool active = false;
    };

    struct s_technology_setup_event {
        std::uint64_t setup_sequence = 1;
    };

    struct s_ship_destroyed_event {
        std::int16_t destroyed_combat_ship = -1;
        std::int16_t killer_combat_ship = -1;
        std::int16_t destruction_cause = 0;
    };

    struct s_player_setup_event {
        ::game::id_player player_id = ::game::NO_PLAYER;
    };

    struct s_home_system_generated_event {
        ::game::id_player player_id = ::game::NO_PLAYER;
        std::int16_t system_id = -1;
    };

    struct s_planet_generated_event {
        id_planet planet_id = NO_PLANET;
    };

    struct s_population_growth_context {
        id_colony colony_id = NO_COLONY;
        id_planet planet_id = NO_PLANET;
        ::game::id_player owner_id = ::game::NO_PLAYER;
        std::int16_t race_id = -1;
        std::int16_t race_population = 0;
        std::int16_t total_population = 0;
        std::int16_t population_limit = 0;
        std::int16_t climate = 0;
        bool housing = false;
        bool population_boom = false;
        bool plague = false;
    };

    struct s_combat_beam_damage_context {
        ::game::owner_slot attacker{-1};
        ::game::owner_slot target{-1};
        std::int16_t weapon_id = 0;
        std::uint16_t beam_ordinal = 0;
        std::uint16_t beam_count = 0;
    };

    bool Set_Player_Race_Trait(
        ::game::id_player player_id, const char* trait_id, bool enabled);
    void Reset_Semantic_State();
    void Begin_Semantic_State_Replacement(
        s_semantic_state_replacement* replacement);
    void Commit_Semantic_State_Replacement(
        s_semantic_state_replacement* replacement);
    void Abort_Semantic_State_Replacement(
        s_semantic_state_replacement* replacement);
    bool Technology_Setup(const s_technology_setup_event* event,
        std::string* diagnostic);
    void Player_Setup(const s_player_setup_event* event);
    void Home_System_Generated(const s_home_system_generated_event* event);
    void Planet_Generated(const s_planet_generated_event* event);
    void Ship_Destroyed(const s_ship_destroyed_event* event);
    std::int16_t Modify_Population_Growth(
        const s_population_growth_context* context, std::int64_t growth);
    std::int16_t Modify_Combat_Beam_Damage(
        const s_combat_beam_damage_context* context, std::int64_t damage);
} // namespace orion2re::lua::game

#endif
