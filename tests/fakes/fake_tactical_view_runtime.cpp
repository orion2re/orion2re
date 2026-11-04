#include "pch.h"
#include "fake_tactical_view_runtime.h"

namespace {
    s_combat_data units[MAX_COMBAT_SHIPS];
    s_player players[MAX_PLAYERS];
} // namespace
namespace MOX {
    s_combat_data* combat_data = units;
    s_player* player = players;
    int16_t PLAYER_NUM = 0, NUM_PLAYERS = 2;
    int16_t combat_total_ship_count = 6, combat_attacker_player = 0, combat_defender_player = 1, combat_turn = 0;
    int16_t combat_grid_width = ORIGINAL_COMBAT_GRID_WIDTH, combat_grid_height = ORIGINAL_COMBAT_GRID_HEIGHT;
    uint16_t combat_init = 1;
} // namespace MOX
namespace RUSS {
    int8_t all_ai_game = 0;
}

namespace TEST_TACTICAL_VIEW {
    void Reset() {
        std::memset(units, 0, sizeof(units));
        std::memset(players, 0, sizeof(players));
        MOX::combat_data = units;
        MOX::player = players;
        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = 2;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::combat_total_ship_count = 6;
        MOX::combat_turn = 0;
        MOX::combat_init = 1;
        RUSS::all_ai_game = 0;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            units[i].unit_status = 5;
            units[i].stasis_source_idx = COMBAT_SHIP_NONE;
            units[i].ship_idx = i;
            units[i].size_class = 2;
            units[i].x = i % 70;
            units[i].y = i / 70;
        }
        for (int16_t i = 1; i < 5; ++i) {
            units[i].unit_status = 0;
            units[i].owner = i == 1 ? 0 : 1;
            units[i].previous_owner = units[i].owner;
        }
        std::strcpy(units[1].name, "Observer ship");
        std::strcpy(units[2].name, "Opponent ship");
        std::strcpy(units[3].name, "Undisclosed contact name");
        std::strcpy(units[4].name, "Hidden record");
        units[3].special_status_flag = units[4].special_status_flag = 4;
        units[4].size_class = 6;
        units[1].structure_max = 100;
        units[1].structure_damage = 20;
        units[1].armor_remaining = 50;
        units[1].ftl_type = 1;
        units[1].drive_current_hits = 10;
        units[1].current_speed = 12;
        units[1].movement_left = 7;
        units[1].stasis_source_idx = 2;
    }
} // namespace TEST_TACTICAL_VIEW
