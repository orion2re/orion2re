#include "turn_fixture.h"

namespace TEST_TACTICAL_TURN {
    namespace {
        s_combat_data units[MAX_COMBAT_SHIPS];
        s_player players[MAX_PLAYERS];
        s_ship_data ships[MAX_SHIPS];
        s_missile missiles[MAX_COMBAT_FLYERS];
        s_mouse_list mouse_grid[COMBAT_MOUSE_COUNT];
        s_animation_header button;
        char laser_name[] = "Laser Cannons";
    } // namespace
    void Reset() {
        tactical_control::Close_Decision();
        std::memset(units, 0, sizeof(units));
        std::memset(players, 0, sizeof(players));
        std::memset(ships, 0, sizeof(ships));
        std::memset(missiles, 0, sizeof(missiles));
        MOX::combat_data = units;
        MOX::player = players;
        MOX::ship = ships;
        MOX::missile = missiles;
        MOX::combat_mouse_grid = mouse_grid;
        MOX::NUM_PLAYERS = 2;
        MOX::PLAYER_NUM = 0;
        MOX::game_type = 0;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::combat_total_ship_count = 4;
        MOX::combat_grid_width = ORIGINAL_COMBAT_GRID_WIDTH;
        MOX::combat_grid_height = ORIGINAL_COMBAT_GRID_HEIGHT;
        MOX::cmbt_colony_id = -1;
        MOX::combat_init = 0;
        MOX::combat_turn = 0;
        MOX::cur_ptr = 0;
        MOX::cur_ship = 1;
        MOX::net_flag = 0;
        MOX::auto_combat = 0;
        MOX::take_control_flag = 0;
        MOX::tactical_net_choice = 0;
        fields::Set_Input_Delay(0);
        MOX::super_fast_flag = 1;
        MOX::apply_damage_to_planet = 1;
        MOX::order_info_flag = 1;
        MOX::skan_flag = MOX::board_flag = 0;
        MOX::msl_impact_warning_flag = 0;
        MOX::missile_warning = 0;
        MOX::cur_combat_x = MOX::cur_combat_y = 0;
        RUSS::all_ai_game = 0;
        COMBAT1::ship_initiative = 0;
        std::memset(MOX::side_end_of_turn, 0, sizeof(MOX::side_end_of_turn));
        std::memset(MOX::auto_combat_flag, 0, sizeof(MOX::auto_combat_flag));
        std::fill_n(MOX::ship_turn_order, MAX_COMBAT_SHIPS, int16_t(-1));
        MOX::ship_turn_order[0] = 1;
        MOX::ship_turn_order[2] = -10;
        for (auto& player : players) player.objectives = PLAYER_OBJECTIVE_HUMAN;
        units[0].unit_status = 5;
        for (int i = 1; i < 4; ++i) {
            auto& unit = units[i];
            unit.owner = i == 2 ? 1 : 0;
            unit.previous_owner = unit.owner;
            unit.ship_idx = int16_t(i);
            unit.x = int16_t(10 + i * 3);
            unit.y = 10;
            unit.structure_max = 2000;
            unit.armor_remaining = 2000;
            unit.drive_current_hits = 100;
            unit.drive_max_hits = 100;
            unit.combat_speed_base = 20;
            unit.ftl_type = 1;
            unit.current_speed = 20;
            unit.movement_left = 20;
            unit.stasis_source_idx = COMBAT_SHIP_NONE;
            unit.black_hole_source_idx = COMBAT_SHIP_NONE;
            unit.special_cooldown = COMBAT_SHIP_NONE;
            strlcpy(unit.name, i == 2 ? "Opponent" : "Pilot ship", sizeof(unit.name));
        }
        units[1].tac_val_attack = 100;
        units[1].weapons[0] = {WEAPON_LASER_CANNON, 2, WEAPON_FIRING_ARC_ALL_SECTORS, 0, -1, 1, 2, 0};
        units[2].weapons[0] = {};
        MOX::ordnance_bonus[0] = MOX::ordnance_bonus[1] = 100;
        TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 2;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 5;
        TECHDATA::weapons[WEAPON_LASER_CANNON].name_plural = laser_name;
        // Synthetic button geometry only; all gameplay and RNG functions are production.
        button.width = 10;
        button.height = 10;
        MOX::weapons_button_seg = MOX::specials_button_seg = MOX::auto_button_seg = &button;
        MOX::scan_button_seg = MOX::board_button_seg = MOX::flee_button_seg = &button;
        MOX::wait_button_seg = MOX::cmbt_done_button_seg = &button;
        MOX::cmbt_info_button_seg = &button;
        COMBAT1::Assign_Combat_Grids(1);
        COMBAT1::Reset_Strict_Combat_Sequence();
        game_random::Set_Random_Seed(12345);
    }
    void Apply(const tactical_control::s_operation* operation) {
        int16_t next = 0;
        // Done exhausts this fixture's pass; no assets are loaded for next-actor drawing.
        if (operation->kind == tactical_control::DONE) MOX::super_fast_flag = 0;
        COMBAT1::Apply_Tactical_Operation(operation, 0, &next);
        if (next) tactical_events::Select_Unit(-1);
    }
} // namespace TEST_TACTICAL_TURN
