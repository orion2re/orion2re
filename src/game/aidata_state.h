#pragma once

#include "ai_constants.h"
#include "ai_world.h"
#include "consts.h"
#include "orion2_consts.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace AIDATA {
    struct s_route_key {
        uint32_t player;
        uint32_t source;
        uint32_t destination;

        bool operator==(const s_route_key&) const = default;
    };

    struct s_route_hash {
        uint64_t operator()(const s_route_key& key) const {
            uint64_t hash = std::hash<uint32_t>{}(key.player);
            for (uint32_t id : {key.source, key.destination}) {
                hash ^= std::hash<uint32_t>{}(id) + 0x9e3779b9u +
                    (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    struct s_route {
        uint32_t next_star;
        uint16_t turns;
    };

    struct s_route_choices {
        std::optional<s_route> normal;
        std::optional<s_route> extended;
    };

    struct s_direct_route_row {
        std::vector<uint8_t> turns;
        std::vector<uint64_t> available;
    };

    // Dense byte rows, indexed through live IDs rather than ID high-water marks.
    struct s_direct_route_table {
        std::unordered_map<uint32_t, uint32_t> destinations;
        std::unordered_map<uint64_t, s_direct_route_row> rows;

        void Store(uint32_t source, uint32_t destination, uint32_t player, uint8_t turns) {
            const uint32_t offset = destinations.try_emplace(destination, (uint32_t)destinations.size()).first->second;
            auto& row = rows[(uint64_t{player} << 32) | source];
            if (row.turns.size() < destinations.size()) row.turns.resize(destinations.size());
            const uint32_t word = offset / 64;
            if (row.available.size() <= word) row.available.resize(word + 1);
            row.turns[offset] = turns;
            row.available[word] |= UINT64_C(1) << (offset % 64);
        }

        std::optional<uint16_t> Find(uint32_t source, uint32_t destination, uint32_t player) const {
            auto offset = destinations.find(destination);
            if (offset == destinations.end()) return std::nullopt;
            auto row = rows.find((uint64_t{player} << 32) | source);
            if (row == rows.end() || offset->second >= row->second.turns.size()) return std::nullopt;
            const uint32_t word = offset->second / 64;
            if (word >= row->second.available.size() ||
                (row->second.available[word] & (UINT64_C(1) << (offset->second % 64))) == 0) {
                return std::nullopt;
            }
            return row->second.turns[offset->second];
        }

        void Clear() {
            destinations.clear();
            rows.clear();
        }
    };

    struct s_route_cache {
        s_direct_route_table direct;
        std::unordered_map<s_route_key, s_route_choices, s_route_hash> intermediate;
        s_direct_route_table ship_direct;
        std::unordered_map<uint64_t, std::optional<s_route>> ship_intermediate;

        void Clear() {
            direct.Clear();
            intermediate.clear();
            ship_direct.Clear();
            ship_intermediate.clear();
        }
    };

    inline uint64_t Ship_Route_Key(uint32_t ship, uint32_t destination) {
        return (uint64_t{ship} << 32) | destination;
    }

    inline std::optional<uint16_t> N_Turns_From_To(const s_route_cache* routes,
        uint32_t source,
        uint32_t destination,
        uint32_t player) {
        return routes->direct.Find(source, destination, player);
    }

    inline std::optional<uint16_t> Ship_N_Turns_To(const s_route_cache* routes,
        uint32_t ship,
        uint32_t destination) {
        return routes->ship_direct.Find(ship, destination, 0);
    }

    struct s_colony_assessment {
        int8_t dominant_race = 0;
        int8_t population_limit = 0;
        bool food_buildings_relevant = false;
        bool pollution_controls_relevant = false;
        bool other_player_arrivals = false;
        int8_t conquered_population = 0;
    };

    struct s_empire_assessment {
        int16_t colony_count = 0;
        int16_t colony_deficit = 0;
        int16_t blockaded_colony_count = 0;
        int16_t colonized_star_count = 0;
        int16_t blockaded_star_count = 0;
        int16_t contact_count = 0;
        int16_t reachable_player_count = 0;
        std::array<bool, MAX_PLAYERS> reachable_players = {};
        std::array<int32_t, MAX_PLAYERS> strength_vs_player = {};
    };

    struct s_star_player_assessment {
        int8_t treaty_partners_in_range = 0;
        int8_t neutral_players_in_range = 0;
        int8_t hostile_players_in_range = 0;
        int8_t players_only_in_extended_range = 0;
        uint8_t covering_colony_count = 0;
        bool other_player_arrivals = false;
    };

    struct s_star_neighborhood {
        int8_t planet_count = 0;
        std::array<s_star_player_assessment, MAX_PLAYERS> per_player = {};
    };

    struct s_technology_assessment {
        uint8_t best_shield = 0;
        int8_t best_shield_delta = 0;
        uint8_t best_armor = 0;
        int8_t armor_delta = 0;
        uint8_t best_computer = 0;
        int8_t best_computer_delta = 0;
        uint8_t best_fuel = 0;
        uint8_t best_planet_shield = 0;
        int8_t planet_shield_delta = 0;
        uint8_t contact_count = 0;
        uint8_t best_drive = 0;
        uint8_t best_beam_score = 0;
        int8_t beam_score_delta = 0;
        uint8_t best_missile_score = 0;
        int8_t missile_score_delta = 0;
        uint8_t ground_tech_score = 0;
        int16_t ground_bonus = 0;
        int8_t ground_bonus_delta = 0;
        bool colonization_idle = false;
    };

    struct s_fleet_composition {
        std::array<int32_t, HULL_CLASS_COUNT> ship_counts = {};
        int32_t target_ship_size = 0;
        int32_t combat_ship_count = 0;
        int32_t combat_command_points_used = 0;
        int32_t active_combat_command_points_used = 0;
        int32_t weighted_ship_size = 0;
    };

    struct s_colonization_candidate {
        uint32_t planet_id = 0;
        int32_t contextual_value = 0;
    };

    struct s_colony_assignment {
        uint32_t colony_id = 0;
        uint8_t n_farmers = 0;
        uint8_t n_workers = 0;
        uint8_t first_assignable_pop_idx = 0;
        uint8_t cur_pop_idx = 0;
        uint8_t pop_end_idx = 0;
        uint8_t pop_tail_idx = 0;
        uint8_t pollution_at_start = 0;
        int16_t best_reassignment_score = 0;
        int16_t worst_reassignment_score = 0;
        bool eats_minerals = false;
    };

    struct s_ship_assessment {
        uint16_t strengths[MAX_PLAYERS] = {};
        uint16_t strengths_in_nebula[MAX_PLAYERS] = {};
        uint16_t bomb_strengths[PLANETARY_SHIELD_COUNT] = {};
        int32_t refit_desirability = 0;
    };

    struct s_planet_assessment {
        uint16_t base_values[MAX_PLAYERS] = {};
        uint16_t contextual_values[MAX_PLAYERS] = {};
        int16_t stage_weights[MAX_PLAYERS] = {};
    };

    struct s_star_assessment {
        s_star_neighborhood neighborhood;
        uint8_t in_range = 0;
        uint8_t in_extended_range = 0;
        uint32_t base_stage_weights[MAX_PLAYERS] = {};
        uint32_t stage_weights[MAX_PLAYERS] = {};
        int32_t possible_strengths[MAX_PLAYERS][MAX_PLAYERS][FORECAST_TURN_COUNT] = {};
        int32_t enroute_strengths[MAX_PLAYERS][MAX_PLAYERS][FORECAST_TURN_COUNT] = {};
        int8_t enroute_ships[MAX_PLAYERS][FORECAST_TURN_COUNT] = {};
        int8_t enroute_human_ships[FORECAST_TURN_COUNT] = {};
        int8_t enroute_nonstealthy_ships[MAX_PLAYERS] = {};
    };

    struct s_build_quantity {
        uint8_t current = 0;
        uint8_t additional_wanted = 0;
    };

    struct s_colony_ship_need {
        uint8_t current = 0;
        uint8_t additional_wanted = 0;
        uint8_t enroute = 0;
    };

    struct s_combat_strength_need {
        int32_t total = 0;
        int32_t excluding_queued_builds = 0;
        int32_t additional_wanted = 0;
    };

    struct s_build_needs {
        s_fleet_composition ship_sizes;
        int32_t total_strength_vs_colonies = 0;
        s_build_quantity transports;
        int8_t freighter_groups_wanted = 0;
        s_build_quantity spies;
        s_colony_ship_need colony_ships;
        s_combat_strength_need combat_strength;
    };

    struct s_refit_candidate {
        uint32_t ship_id;
        int32_t desirability;
    };

    struct s_colony_workspace {
        std::vector<s_colony_assignment> colonies;
        uint32_t blockaded_count = 0;
        bool complete = false;
    };

    struct s_movement_workspace {
        std::vector<s_colonization_candidate> colonizable;
        std::unordered_map<uint32_t, std::array<int8_t, FORECAST_TURN_COUNT>> flee_stars;
        std::unordered_map<uint32_t, int8_t> nonplayer_here;
        bool complete = false;
    };

    struct s_combat_ship_group {
        uint32_t next;
        uint32_t end;
    };

    struct s_player_assessment {
        s_empire_assessment empire;
        s_technology_assessment technology;
        s_build_needs build;
        std::vector<s_refit_candidate> refits;
        std::unordered_map<uint32_t, s_combat_ship_group> combat_ship_groups;
        std::optional<s_combat_ship_group> in_transit_combat_ships;
        s_colony_workspace colony_work;
        s_movement_workspace movement;

        s_combat_ship_group* Find_Combat_Ships(uint32_t star) {
            auto group = combat_ship_groups.find(star);
            return group == combat_ship_groups.end() ? nullptr : &group->second;
        }

        const s_combat_ship_group* Find_Combat_Ships(uint32_t star) const {
            auto group = combat_ship_groups.find(star);
            return group == combat_ship_groups.end() ? nullptr : &group->second;
        }
    };

    struct s_state {
        s_world_view world;
        std::unordered_map<uint32_t, s_ship_assessment> ships;
        std::unordered_map<uint32_t, s_planet_assessment> planets;
        std::unordered_map<uint32_t, s_star_assessment> stars;
        std::unordered_map<uint32_t, s_colony_assessment> colonies;
        std::array<s_player_assessment, MAX_PLAYERS> players;
        s_route_cache routes;
        std::vector<uint32_t> combat_ships;
        int32_t stardate = 0;
        bool initialized = false;
        bool complete = false;
    };

    // Temporary session owner. Only integration boundaries may access this object.
    extern s_state state;

    void Begin_Turn(s_state* ai, int32_t stardate);
    void Complete_Turn(s_state* ai);
    void Reset(s_state* ai);
} // namespace AIDATA
