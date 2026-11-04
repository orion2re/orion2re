#include "pch.h"

namespace ai_state {
    namespace {
        struct s_allocation {
            void* data;
            size_t size;
        };

        struct s_workspace {
            s_snapshot state;
            std::array<s_allocation, 23> allocations = {};
            size_t allocation_count = 0;
            std::vector<uint8_t> n_turns_from_to;
            std::vector<AIDATA::s_turns_from_to_intermediate_cache_entry>
                n_turns_from_to_intermediate;
            std::vector<int8_t> ship_n_turns_to;
            std::vector<AIDATA::s_ship_n_turns_to_intermediate_cache_entry>
                ship_n_turns_to_intermediate;
            size_t fixed_allocated_bytes = 0;

            ~s_workspace() {
                for (size_t index = 0; index < allocation_count; ++index) {
                    std::free(allocations[index].data);
                }
            }
        };

        std::unique_ptr<s_workspace> g_workspace;
        bool g_active;
        bool g_completed;

        void* Allocate(s_workspace* workspace, size_t size) {
            if (workspace->allocation_count >= workspace->allocations.size()) {
                allocate::Allocation_Error((int32_t)size);
            }
            void* allocation = std::calloc(1, size);
            if (allocation == nullptr) {
                allocate::Allocation_Error((int32_t)size);
            }
            workspace->allocations[workspace->allocation_count++] = {allocation, size};
            workspace->fixed_allocated_bytes += size;
            return allocation;
        }

        template <typename T>
        void Resize(std::vector<T>* storage, size_t count) {
            try {
                storage->resize(std::max<size_t>(count, 1));
            } catch (const std::bad_alloc&) {
                allocate::Allocation_Error((int32_t)(count * sizeof(T)));
            }
        }

        bool Debugger_Enabled() {
            return debug_overlay::g_enabled != 0;
        }

        void Configure_Route_Tables(s_workspace* owner) {
            s_snapshot* snapshot = &owner->state;
            const size_t stars = snapshot->star_count;
            const size_t players = snapshot->player_count;
            const size_t ships = snapshot->ship_count;
            Resize(&owner->n_turns_from_to, stars * stars * players);
            Resize(&owner->n_turns_from_to_intermediate, stars * stars * players);
            Resize(&owner->ship_n_turns_to, ships * stars);
            Resize(&owner->ship_n_turns_to_intermediate, ships * stars);

            snapshot->n_turns_from_to = owner->n_turns_from_to.data();
            snapshot->n_turns_from_to_intermediate =
                owner->n_turns_from_to_intermediate.data();
            snapshot->ship_n_turns_to = owner->ship_n_turns_to.data();
            snapshot->ship_n_turns_to_intermediate =
                owner->ship_n_turns_to_intermediate.data();

            memset(snapshot->n_turns_from_to, -1, owner->n_turns_from_to.size() * sizeof(uint8_t));
            memset(snapshot->n_turns_from_to_intermediate, -1, owner->n_turns_from_to_intermediate.size() * sizeof(AIDATA::s_turns_from_to_intermediate_cache_entry));
            memset(snapshot->ship_n_turns_to, -1, owner->ship_n_turns_to.size() * sizeof(int8_t));
            memset(snapshot->ship_n_turns_to_intermediate, -1, owner->ship_n_turns_to_intermediate.size() * sizeof(AIDATA::s_ship_n_turns_to_intermediate_cache_entry));

            snapshot->allocated_bytes = owner->fixed_allocated_bytes +
                owner->n_turns_from_to.size() * sizeof(uint8_t) +
                owner->n_turns_from_to_intermediate.size() *
                    sizeof(AIDATA::s_turns_from_to_intermediate_cache_entry) +
                owner->ship_n_turns_to.size() * sizeof(int8_t) +
                owner->ship_n_turns_to_intermediate.size() *
                    sizeof(AIDATA::s_ship_n_turns_to_intermediate_cache_entry);
        }

        void Reset_Debug_State(s_snapshot* snapshot) {
            memset(snapshot->player_ship_sizes, 0, sizeof(snapshot->player_ship_sizes));
            memset(snapshot->player_total_strength_vs_colonies, 0, sizeof(snapshot->player_total_strength_vs_colonies));
            memset(snapshot->player_n_transports, 0, sizeof(snapshot->player_n_transports));
            memset(snapshot->player_n_freighter_groups_wanted, 0, sizeof(snapshot->player_n_freighter_groups_wanted));
            memset(snapshot->player_n_spies, 0, sizeof(snapshot->player_n_spies));
            memset(snapshot->player_n_colony_ships, 0, sizeof(snapshot->player_n_colony_ships));
            memset(snapshot->player_total_strength_vs_ships, 0, sizeof(snapshot->player_total_strength_vs_ships));
            snapshot->n_all_ships = 0;
            for (s_player_decisions& player : snapshot->players) {
                player.colony_phase_ran = false;
                player.movement_phase_ran = false;
                player.colony_state.clear();
                player.colonies.clear();
                player.movement.player_idx = -1;
                player.movement.colonizable_planets.clear();
                player.movement.flee_map.clear();
                player.movement.nonplayer_map.clear();
                player.movement.flee_stars.clear();
                player.movement.nonplayer_stars.clear();
            }
        }

        void Bind(s_snapshot* snapshot) {
            AIDATA::ship_strengths = snapshot == nullptr ? nullptr : snapshot->ship_strengths;
            AIDATA::ship_strengths_in_nebula = snapshot == nullptr ? nullptr : snapshot->ship_strengths_in_nebula;
            AIDATA::ship_bomb_strengths = snapshot == nullptr ? nullptr : snapshot->ship_bomb_strengths;
            AIDATA::planet_stage_weights = snapshot == nullptr ? nullptr : snapshot->planet_stage_weights;
            AIDATA::base_star_stage_weights = snapshot == nullptr ? nullptr : snapshot->base_star_stage_weights;
            AIDATA::star_stage_weights = snapshot == nullptr ? nullptr : snapshot->star_stage_weights;
            AIDATA::n_turns_from_to = snapshot == nullptr ? nullptr : snapshot->n_turns_from_to;
            AIDATA::n_turns_from_to_intermediate = snapshot == nullptr ? nullptr : snapshot->n_turns_from_to_intermediate;
            AIDATA::ship_n_turns_to = snapshot == nullptr ? nullptr : snapshot->ship_n_turns_to;
            AIDATA::ship_n_turns_to_intermediate = snapshot == nullptr ? nullptr : snapshot->ship_n_turns_to_intermediate;
            AIDATA::possible_ship_strengths = snapshot == nullptr ? nullptr : snapshot->possible_ship_strengths;
            AIDATA::ai_colony_info = snapshot == nullptr ? nullptr : snapshot->ai_colony_info;
            AIDATA::ai_player_info = snapshot == nullptr ? nullptr : snapshot->ai_player_info;
            AIDATA::ai_star_info = snapshot == nullptr ? nullptr : snapshot->ai_star_info;
            AIDATA::star_in_range_of_player = snapshot == nullptr ? nullptr : snapshot->star_in_range_of_player;
            AIDATA::star_in_extended_range_of_player = snapshot == nullptr ? nullptr : snapshot->star_in_extended_range_of_player;
            AIDATA::enroute_ship_strengths = snapshot == nullptr ? nullptr : snapshot->enroute_ship_strengths;
            AIDATA::enroute_ships = snapshot == nullptr ? nullptr : snapshot->enroute_ships;
            AIDATA::enroute_human_ships = snapshot == nullptr ? nullptr : snapshot->enroute_human_ships;
            AIDATA::enroute_nonstealthy_ships = snapshot == nullptr ? nullptr : snapshot->enroute_nonstealthy_ships;
            AIDATA::all_ships = snapshot == nullptr ? nullptr : snapshot->all_ships;
            AIDATA::i_all_combat_ships = snapshot == nullptr ? nullptr : snapshot->i_all_combat_ships;
            AIDATA::aitech = snapshot == nullptr ? nullptr : snapshot->aitech;
            AIDATA::airefit = snapshot == nullptr ? nullptr : snapshot->airefit;
            AIDATA::ship_refit_desirabilities = snapshot == nullptr ? nullptr : snapshot->ship_refit_desirabilities;
            AIDATA::base_planet_values = snapshot == nullptr ? &AIDATA::g_base_planet_values : snapshot->base_planet_values;
            AIDATA::contextual_planet_values = snapshot == nullptr ? &AIDATA::g_contextual_planet_values : snapshot->contextual_planet_values;
            AIDATA::n_all_ships = snapshot == nullptr ? 0 : snapshot->n_all_ships;
            AIDATA::route_star_count = snapshot == nullptr ? 0 : snapshot->star_count;
            AIDATA::route_player_count = snapshot == nullptr ? 0 : snapshot->player_count;
            AIDATA::route_ship_count = snapshot == nullptr ? 0 : snapshot->ship_count;
        }
    } // namespace

    void Begin_Turn() {
        g_active = false;
        g_completed = false;
        if (g_workspace == nullptr) {
            g_workspace.reset(new s_workspace);
            s_workspace* owner = g_workspace.get();
            s_snapshot* snapshot = &owner->state;
            snapshot->ship_strengths = (uint16_t(*)[MAX_SHIPS][MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->ship_strengths));
            snapshot->ship_strengths_in_nebula = (uint16_t(*)[MAX_SHIPS][MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->ship_strengths_in_nebula));
            snapshot->ship_bomb_strengths = (uint16_t(*)[MAX_SHIPS][4])Allocate(owner, sizeof(*snapshot->ship_bomb_strengths));
            snapshot->planet_stage_weights = (int16_t(*)[MAX_PLANETS][MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->planet_stage_weights));
            snapshot->base_star_stage_weights = (uint32_t(*)[MAX_STARS][MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->base_star_stage_weights));
            snapshot->star_stage_weights = (uint32_t(*)[MAX_STARS][MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->star_stage_weights));
            snapshot->possible_ship_strengths = (int32_t(*)[MAX_STARS][MAX_PLAYERS][MAX_PLAYERS][10])Allocate(owner, sizeof(*snapshot->possible_ship_strengths));
            snapshot->ai_colony_info = (s_ai_colony_info(*)[MAX_COLONIES])Allocate(owner, sizeof(*snapshot->ai_colony_info));
            snapshot->ai_player_info = (s_ai_player_info(*)[MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->ai_player_info));
            snapshot->ai_star_info = (s_ai_star_info(*)[MAX_STARS])Allocate(owner, sizeof(*snapshot->ai_star_info));
            snapshot->star_in_range_of_player = (uint8_t(*)[MAX_STARS])Allocate(owner, sizeof(*snapshot->star_in_range_of_player));
            snapshot->star_in_extended_range_of_player = (uint8_t(*)[MAX_STARS])Allocate(owner, sizeof(*snapshot->star_in_extended_range_of_player));
            snapshot->enroute_ship_strengths = (int32_t(*)[MAX_STARS][MAX_PLAYERS][MAX_PLAYERS][10])Allocate(owner, sizeof(*snapshot->enroute_ship_strengths));
            snapshot->enroute_ships = (int8_t(*)[MAX_STARS][MAX_PLAYERS][10])Allocate(owner, sizeof(*snapshot->enroute_ships));
            snapshot->enroute_human_ships = (int8_t(*)[MAX_STARS][10])Allocate(owner, sizeof(*snapshot->enroute_human_ships));
            snapshot->enroute_nonstealthy_ships = (int8_t(*)[MAX_STARS][MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->enroute_nonstealthy_ships));
            snapshot->all_ships = (int16_t(*)[MAX_SHIPS])Allocate(owner, sizeof(*snapshot->all_ships));
            snapshot->i_all_combat_ships = (int16_t(*)[MAX_PLAYERS][MAX_STARS + 1]) Allocate(owner, sizeof(*snapshot->i_all_combat_ships));
            snapshot->aitech = (s_ai_tech_info*)Allocate(owner, sizeof(*snapshot->aitech));
            snapshot->airefit = (s_ai_refit_player(*)[MAX_PLAYERS])Allocate(owner, sizeof(*snapshot->airefit));
            snapshot->ship_refit_desirabilities = (int32_t(*)[MAX_SHIPS])Allocate(owner, sizeof(*snapshot->ship_refit_desirabilities));
            snapshot->base_planet_values = (uint16_t(*)[MAX_PLAYERS][MAX_PLANETS])Allocate(owner, sizeof(*snapshot->base_planet_values));
            snapshot->contextual_planet_values = (uint16_t(*)[MAX_PLAYERS * MAX_PLANETS]) Allocate(owner, sizeof(*snapshot->contextual_planet_values));
        }
        s_workspace* owner = g_workspace.get();
        s_snapshot* snapshot = &owner->state;
        for (size_t index = 0; index < owner->allocation_count; ++index) {
            memset(owner->allocations[index].data, 0, owner->allocations[index].size);
        }
        Reset_Debug_State(snapshot);
        snapshot->stardate = MOX::stardate;
        snapshot->player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        snapshot->star_count = std::clamp<int16_t>(MOX::NUM_STARS, 0, MAX_STARS);
        snapshot->planet_count = std::clamp<int16_t>(MOX::planet_count, 0, MAX_PLANETS);
        snapshot->colony_count = std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES);
        snapshot->ship_count = std::clamp<int16_t>(MOX::NUM_SHIPS, 0, MAX_SHIPS);
        Configure_Route_Tables(owner);
        g_active = Debugger_Enabled();
        Bind(snapshot);
    }

    void Complete_Turn() {
        if (g_workspace == nullptr) return;
        g_workspace->state.n_all_ships = AIDATA::n_all_ships;
        g_completed = g_active;
        g_active = false;
    }

    void Shutdown() {
        g_active = false;
        g_completed = false;
        g_workspace.reset();
        Bind(nullptr);
    }

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
    void Capture_Build_State() {
        if (!g_active) return;
        s_snapshot* snapshot = &g_workspace->state;
        memcpy(snapshot->player_ship_sizes, AIBUILD::player_ship_sizes,
               sizeof(snapshot->player_ship_sizes));
        memcpy(snapshot->player_total_strength_vs_colonies,
               AIBUILD::player_total_strength_vs_colonies,
               sizeof(snapshot->player_total_strength_vs_colonies));
        memcpy(snapshot->player_n_transports, AIBUILD::player_n_transports,
               sizeof(snapshot->player_n_transports));
        memcpy(snapshot->player_n_freighter_groups_wanted,
               AIBUILD::player_n_freighter_groups_wanted,
               sizeof(snapshot->player_n_freighter_groups_wanted));
        memcpy(snapshot->player_n_spies, AIBUILD::player_n_spies,
               sizeof(snapshot->player_n_spies));
        memcpy(snapshot->player_n_colony_ships, AIBUILD::player_n_colony_ships,
               sizeof(snapshot->player_n_colony_ships));
        memcpy(snapshot->player_total_strength_vs_ships,
               AIBUILD::player_total_strength_vs_ships,
               sizeof(snapshot->player_total_strength_vs_ships));
    }

    s_aidudes_colony_info* Prepare_Colony_State(int16_t player_idx) {
        if (!g_active || player_idx < 0 || player_idx >= MAX_PLAYERS) return nullptr;
        s_player_decisions& player = g_workspace->state.players[player_idx];
        player.colony_state.assign(MAX_COLONIES, {});
        return player.colony_state.data();
    }

    void Capture_Colony_State(int16_t player_idx,
        const s_aidudes_colony_info* colonies, int16_t colony_count,
        int16_t blockaded_count) {
        if (!g_active || player_idx < 0 || player_idx >= MAX_PLAYERS ||
            colonies == nullptr) return;
        s_player_decisions& player = g_workspace->state.players[player_idx];
        player.colony_phase_ran = true;
        player.colonies.clear();
        colony_count = std::clamp<int16_t>(colony_count, 0, MAX_COLONIES);
        if (player.colony_state.data() != colonies) {
            player.colony_state.assign(colonies, colonies + colony_count);
        } else {
            player.colony_state.resize(colony_count);
        }
        for (int16_t index = 0; index < colony_count; ++index) {
            const s_aidudes_colony_info& source = colonies[index];
            player.colonies.push_back({
                source.colony_idx, source.n_farmers, source.n_workers,
                source.first_assignable_pop_idx, source.pop_end_idx,
                source.pollution_at_start, source.best_worker_to_farmer_score,
                source.worst_worker_to_farmer_score, index < blockaded_count,
                source.eats_minerals != 0,
            });
        }
    }

    void Prepare_Movement_State(int16_t player_idx,
        s_ai_colonizable_planet** planets, int8_t** flee_stars,
        int8_t** nonplayer_stars) {
        if (planets == nullptr || flee_stars == nullptr || nonplayer_stars == nullptr) return;
        *planets = nullptr;
        *flee_stars = nullptr;
        *nonplayer_stars = nullptr;
        if (!g_active || player_idx < 0 || player_idx >= MAX_PLAYERS) return;
        s_movement_decision& movement = g_workspace->state.players[player_idx].movement;
        movement.colonizable_planets.assign(MAX_PLANETS, {});
        movement.flee_map.assign(MAX_STARS * AIMOVE::ENEMY_STRENGTH_TURN_WINDOW, 0);
        movement.nonplayer_map.assign(MAX_STARS, 0);
        *planets = movement.colonizable_planets.data();
        *flee_stars = movement.flee_map.data();
        *nonplayer_stars = movement.nonplayer_map.data();
    }

    void Capture_Movement_State(int16_t player_idx,
        const s_ai_colonizable_planet* planets, int16_t planet_count,
        const int8_t* flee_stars, const int8_t* nonplayer_stars) {
        if (!g_active || player_idx < 0 || player_idx >= MAX_PLAYERS) return;
        s_player_decisions& player = g_workspace->state.players[player_idx];
        player.movement_phase_ran = true;
        player.movement.player_idx = player_idx;
        if (planets != nullptr && planet_count >= 0) {
            planet_count = std::clamp<int16_t>(planet_count, 0, MAX_PLANETS);
            if (player.movement.colonizable_planets.data() == planets) {
                player.movement.colonizable_planets.resize(planet_count);
            } else {
                player.movement.colonizable_planets.assign(planets, planets + planet_count);
            }
        }
        player.movement.flee_stars.clear();
        player.movement.nonplayer_stars.clear();
        for (int16_t star = 0; star < g_workspace->state.star_count; ++star) {
            if (flee_stars != nullptr && flee_stars[star * AIMOVE::ENEMY_STRENGTH_TURN_WINDOW + 9] != 0) {
                player.movement.flee_stars.push_back(star);
            }
            if (nonplayer_stars != nullptr && nonplayer_stars[star] != 0) {
                player.movement.nonplayer_stars.push_back(star);
            }
        }
    }
#else
    void Capture_Build_State() {}
    s_aidudes_colony_info* Prepare_Colony_State(int16_t) { return nullptr; }
    void Capture_Colony_State(int16_t, const s_aidudes_colony_info*, int16_t, int16_t) {}
    void Prepare_Movement_State(int16_t, s_ai_colonizable_planet** planets,
        int8_t** flee_stars, int8_t** nonplayer_stars) {
        if (planets != nullptr) *planets = nullptr;
        if (flee_stars != nullptr) *flee_stars = nullptr;
        if (nonplayer_stars != nullptr) *nonplayer_stars = nullptr;
    }
    void Capture_Movement_State(int16_t, const s_ai_colonizable_planet*, int16_t,
                                 const int8_t*, const int8_t*) {}
#endif

    const s_snapshot* Get_Active() {
        return g_active ? &g_workspace->state : nullptr;
    }

    const s_snapshot* Get_Completed() {
        return g_completed ? &g_workspace->state : nullptr;
    }

    bool Is_Valid(const s_snapshot* snapshot) {
        return snapshot != nullptr &&
               snapshot->player_count >= 0 && snapshot->player_count <= MAX_PLAYERS &&
               snapshot->star_count >= 0 && snapshot->star_count <= MAX_STARS &&
               snapshot->planet_count >= 0 && snapshot->planet_count <= MAX_PLANETS &&
               snapshot->colony_count >= 0 && snapshot->colony_count <= MAX_COLONIES &&
               snapshot->ship_count >= 0 && snapshot->ship_count <= MAX_SHIPS &&
               snapshot->ship_strengths != nullptr &&
               snapshot->ship_strengths_in_nebula != nullptr &&
               snapshot->ship_bomb_strengths != nullptr &&
               snapshot->planet_stage_weights != nullptr &&
               snapshot->base_star_stage_weights != nullptr &&
               snapshot->star_stage_weights != nullptr &&
               snapshot->n_turns_from_to != nullptr &&
               snapshot->n_turns_from_to_intermediate != nullptr &&
               snapshot->ship_n_turns_to != nullptr &&
               snapshot->ship_n_turns_to_intermediate != nullptr &&
               snapshot->possible_ship_strengths != nullptr &&
               snapshot->ai_colony_info != nullptr &&
               snapshot->ai_player_info != nullptr &&
               snapshot->ai_star_info != nullptr &&
               snapshot->star_in_range_of_player != nullptr &&
               snapshot->star_in_extended_range_of_player != nullptr &&
               snapshot->enroute_ship_strengths != nullptr &&
               snapshot->enroute_ships != nullptr &&
               snapshot->enroute_human_ships != nullptr &&
               snapshot->enroute_nonstealthy_ships != nullptr &&
               snapshot->all_ships != nullptr &&
               snapshot->i_all_combat_ships != nullptr &&
               snapshot->aitech != nullptr && snapshot->airefit != nullptr &&
               snapshot->ship_refit_desirabilities != nullptr &&
               snapshot->base_planet_values != nullptr &&
               snapshot->contextual_planet_values != nullptr;
    }
} // namespace ai_state