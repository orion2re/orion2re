#pragma once

#include "consts.h"
#include "pop.h"
#include "object_ids.h"
#include <optional>

struct s_colony;
struct s_player;
struct s_settler_settle_result;
namespace game { struct s_world_state; }

namespace SETTLER {
    struct s_settler {
        std::optional<game::id_colony> source_colony;
        game::id_planet destination_planet;
        uint8_t race = 0;
        uint16_t eta = 0;
        POP::e_prof profession = POP::PROF_FARMER;
        bool operator==(const s_settler&) const = default;
    };

    extern bool allow_last_population_transfer;
    extern uint16_t max_pop_freighted;

    int32_t N_Settlers(const game::s_world_state* world, game::id_player player);
    int32_t N_Freighters_Player_Can_Scrap(game::s_world_state* world_state, int16_t player_idx);
    bool Room_For_Another(game::s_world_state* world_state, s_colony* colony, int16_t race_idx, int8_t include_freighters);
    int8_t Add_Settler_To_Colony(game::s_world_state* world_state, s_settler settler, s_colony* colony);
    void Move_Settlers(game::s_world_state* world_state);
    void Pop_Tries_To_Settle(game::s_world_state* world_state,
        s_colony* source_colony, int16_t selected_pop_idx,
        s_colony* dest_colony, s_settler_settle_result* result);
    int32_t Settler_ETA(s_player* player, int16_t source_star, int16_t dest_star);
    void Settle_Pop(game::s_world_state* world_state, s_colony* source_colony, int16_t pop_index, s_colony* dest_colony);
} // namespace SETTLER
