#pragma once
#include <cstdint>
#include <optional>
#include "object_ids.h"
#include "types.h"

struct s_wormhole_data;
namespace game { struct s_game_state; }

namespace SHIPMOVE {
    extern uint8_t jump_gate_speed_bonus;
    extern uint16_t warp_interdictor_range;
    extern bool jump_gate_pass_nebula;
    extern s_ratio16 phase_shifter_speed_multiplier;

    struct s_ship_communication_range {
        uint8_t tachyon;
        uint8_t sub_space;
    };

    extern s_ship_communication_range ship_communication_range;

    void __cdecl Allocate_New_Ship_Slots();
    void __cdecl Move_All_Ships_Toward_Stars();
    void __cdecl Reset_Needless_Ignore_Combat_Ships_Flags();

    void __cdecl Update_All_Ship_Ranges();

    bool Star_In_Normal_Range_Of_Player(const game::s_game_state& state, const s_star_data& star, game::id_player player);

    bool Star_In_Extended_Range_Of_Player(const game::s_game_state& state, const s_star_data& star, game::id_player player);

    bool Star_In_Range_Of_Player(const game::s_game_state& state, const s_star_data& star, game::id_player player, int16_t dist);

    void __cdecl Update_Player_Ship_Range(s_player* player);

    // Individual ship speed before fleet, leader, or route effects.
    int32_t Effective_FTL_Speed(game::owner_slot owner, int16_t drive, bool has_phase_shifter);



    bool Confirm_Officer_Change(game::id_leader leader, s_ship_data* target, int8_t target_eta);
    void Recalculate_Moving_Fleet(const s_ship_data* ship);
    void __cdecl Recalculate_All_Moving_Fleets();

    game::id_ship Create_Nonplayer_Ship(game::owner_slot owner, const s_ship_design* design, int16_t x, int16_t y);

    bool __cdecl Player_Has_Ships_Pestering_Other_Player(game::id_player player, game::id_player other);

    void __cdecl Unmark_Just_Built_AI_Ships();

    int16_t Player_N_Turns_From_Star_To_Star(const game::s_game_state& state, game::id_player player, game::id_star source, game::id_star destination);

    int16_t __cdecl Ship_Destination_Star(s_ship_data* ship);

    int16_t __cdecl Ship_Range(s_ship_data* ship);
    int16_t Ship_Range(const game::s_game_state& state, s_ship_data* ship);

    bool Can_Order_Ship(const game::s_game_state& state, const s_ship_data& ship);

    bool __cdecl Ship_Is_Moving(const s_ship_data* ship);

    void __cdecl Make_Ship_Arrive_At_Star(s_ship_data* ship, int16_t star_idx);
    // Stop at the current position, without arrival/visibility/mission effects.
    bool Stop_Ship_Transit(const game::s_game_state& state, s_ship_data* ship, game::s_star_destination destination);
    void __cdecl Update_Ship_ETAs(game::id_player player_id);

    void __cdecl Do_Hyperspace_Beast(game::id_player player_idx);

    void __cdecl Do_Wormhole_Event(game::id_player player_idx, s_wormhole_data* wormhole);
} // namespace SHIPMOVE
