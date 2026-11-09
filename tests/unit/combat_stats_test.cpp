#include "pch.h"

#include "doctest/doctest.h"
#include "game/aipower.h"
#include "game/cmbtdrw1.h"
#include "game/cmbtmov1.h"
#include "game/combat1.h"
#include "game/combinit.h"

namespace {
    s_combat_data combat_data[MAX_COMBAT_SHIPS];
    s_colony colonies[1];
    s_missile missiles[MAX_COMBAT_FLYERS];
    s_planet_data planets[1];
    s_player players[MAX_PLAYERS];
    s_ship_data ships[MAX_COMBAT_SHIPS];

    const game_world::s_view* AI_World_() {
        static const game_world::s_view world{
            .players = players,
            .ships = ships,
            .colonies = colonies,
            .settings = &MOX::settings,
        };
        return &world;
    }
    int16_t best_armor;
    int16_t best_bomb_weapon;
    int16_t best_point_defense_weapon;
    int16_t best_ship_shield;
    bool has_phase_shifter;
    bool has_sub_space_teleporter;
    bool has_energy_absorber;
    bool has_damper_field;
    bool has_hard_shields;
    bool has_time_warp[4];
    int16_t planet_defenses;
    bool target_is_legal;
    bool weapon_in_range;
    bool sub_space_teleporter_enabled;
    int16_t teleporter_used;
    int16_t ship_center_offset_x;
    int16_t ship_center_offset_y;
    bool font_style_selected;
    s_colors selected_font_colors;
    std::string last_hotkey;
    int16_t next_hotkey_field;
    int16_t sent_net_id;
    uint16_t sent_packet_type;
    s_net_tactical_combat_message sent_tactical_message;
    int sent_message_count;
    struct s_draw_position {
        int16_t x;
        int16_t y;
    };
    std::vector<s_draw_position> drawn_positions;
    s_help_box* active_help_list;
    int16_t active_help_box_count;
    bool all_specials_set;
    char weapon_name[] = "Weapon";
    char weapon_name_plural[] = "Weapons";
} // namespace

namespace TEST_TRACTOR_POWER {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(players, 0, sizeof(players));

        MOX::combat_total_ship_count = 2;

        s_combat_data* target = &MOX::combat_data[0];
        target->owner = 10;
        target->size_class = SHIP_SIZE_LARGE;
        target->ftl_type = DRIVE_NUCLEAR_DRIVE;
        target->combat_speed_base = 10;
        target->drive_current_hits = 10;
        target->drive_max_hits = 10;
        target->black_hole_source_idx = COMBAT_SHIP_NONE;
        target->stasis_source_idx = COMBAT_SHIP_NONE;

        s_combat_data* tractor = &MOX::combat_data[1];
        tractor->special_cooldown = 0;
        tractor->stasis_source_idx = COMBAT_SHIP_NONE;
        tractor->weapons[0].weapon_id = WEAPON_TRACTOR_BEAM;
        tractor->weapons[0].count = 3;

        const uint8_t defaults[SHIP_SIZE_COUNT] = {1, 2, 3, 4, 5, 6};
        memcpy(COMBAT1::tractors_to_stop, defaults, sizeof(defaults));
    }

} // namespace TEST_TRACTOR_POWER

namespace TEST_EXPECTED_DAMAGE {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(TECHDATA::shields, 0, sizeof(TECHDATA::shields));
        memset(MOX::ordnance_bonus, 0, sizeof(MOX::ordnance_bonus));
        memset(MOX::fighter_beam_type, 0, sizeof(MOX::fighter_beam_type));
        memset(MOX::fighter_bomb_type, 0, sizeof(MOX::fighter_bomb_type));
        has_energy_absorber = false;
        has_damper_field = false;
        has_hard_shields = false;
        planet_defenses = 0;
        target_is_legal = false;
        weapon_in_range = false;
        MOX::in_nebula_flag = 0;
        TECHDATA::hard_shields_damage_reduction = 3;
        CMBTFIRE::fighter_beam_no_plus_one = 0;
        CMBTFIRE::fighters_use_point_defense = 0;
        CMBTFIRE::planet_non_enveloping = 0;
        CMBTMIS::missile_defense = {0, 50, 30};
    }
} // namespace TEST_EXPECTED_DAMAGE

namespace TEST_MISSILE_BEARING {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(MOX::combat_grid, 0, sizeof(MOX::combat_grid));

        MOX::cur_ship = 0;
        MOX::speedx2_flag = 0;
        MOX::net_flag = 0;
        MOX::combat_grid[0][0] = 2000;

        MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[0].source_unit_idx = 1;
        MOX::missile[0].target_unit_idx = 0;
        MOX::missile[0].x = 100;
        MOX::missile[0].y = 50;
        MOX::missile[0].travel_dist = 60;
        MOX::missile[0].facing_dir = 3;
    }
} // namespace TEST_MISSILE_BEARING

DOCTEST_TEST_CASE("ship legality rejects coordinates outside the combat grid") {
    const int16_t max_x = (MOX::combat_grid_width - 1) * 20;
    const int16_t max_y = (MOX::combat_grid_height - 1) * 20;
    memset(MOX::combat_grid, 0, sizeof(MOX::combat_grid));
    MOX::combat_grid[0][0] = 2000;
    MOX::combat_grid[MOX::combat_grid_width - 1][MOX::combat_grid_height - 1] = 2000;

    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, 0, 0, 0, 0) == 1);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, 20, 0, 20, 0) == 0);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, max_x, max_y, max_x, max_y) == 1);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, max_x, max_y, max_x, max_y - 1) == 0);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, max_x, 0, max_x - 1, 0) == 0);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, -20, 0, 0, 0) == 0);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(1, 0, -20, 0, 0) == 0);
}

namespace TEST_STRICT_COMBAT_SEQUENCE {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(has_time_warp, 0, sizeof(has_time_warp));
        memset(MOX::ship_turn_order, -1, sizeof(MOX::ship_turn_order));
        memset(MOX::side_end_of_turn, 0, sizeof(MOX::side_end_of_turn));

        MOX::NUM_PLAYERS = 2;
        MOX::cmbt_colony_id = -1;
        MOX::combat_total_ship_count = 4;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::cur_ship = -1;
        MOX::cur_ptr = -1;
        MOX::combat_turn = 0;
        MOX::auto_combat_flag[0] = 0;
        MOX::auto_combat_flag[1] = 0;
        MOX::combat_init = 1;
        MOX::PLAYER_NUM = 0;
        MOX::game_type = 0;
        MOX::take_control_flag = 0;
        COMBAT1::ship_initiative = 2;
        COMBAT1::wait_movement_cost = 0;
        COMBAT1::Reset_Combat_Side_Waits();

        MOX::combat_data[0].unit_status = 5;
        for (int16_t ship_idx = 1; ship_idx < 4; ++ship_idx) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];
            ship->owner = ship_idx == 2 ? 1 : 0;
            ship->ship_idx = ship_idx;
            ship->unit_status = 0;
            ship->stasis_source_idx = COMBAT_SHIP_NONE;
            ship->movement_left = 10;
            MOX::player[ship->owner].objectives = PLAYER_OBJECTIVE_HUMAN;
        }

        MOX::combat_data[1].tac_val_attack = 30;
        MOX::combat_data[2].tac_val_attack = 20;
        MOX::combat_data[3].tac_val_attack = 10;
        MOX::ship_turn_order[0] = 1;
        MOX::ship_turn_order[1] = 2;
        MOX::ship_turn_order[2] = 3;
        MOX::ship_turn_order[3] = -10;
        COMBAT1::Reset_Strict_Combat_Sequence();
    }

    void Set_Classic_() {
        COMBAT1::ship_initiative = 1;
        COMBAT1::Reset_Combat_Side_Waits();
    }
} // namespace TEST_STRICT_COMBAT_SEQUENCE

namespace MOX {
    s_combat_data* combat_data = ::combat_data;
    s_colony* colony = colonies;
    s_missile* missile = missiles;
    s_planet_data* planet = planets;
    s_player* player = players;
    s_ship_data* ship = ships;
    s_animation_header* screen_seg;
    s_animation_header* combat_map_planet_seg;
    int16_t ship_frame;
    s_settings settings;
    int16_t cmbt_colony_id;
    int16_t combat_total_ship_count;
    int16_t cur_ship;
    s_net_tactical_combat_message new_message;
    int16_t msl_impact_warning_flag;
    int16_t saved_msl_warn;
    int16_t order_info_flag;
    int16_t ship_status_display;
    s_help_box help_entry_list[50];
    int16_t cur_ptr;
    int16_t ship_turn_order[MAX_COMBAT_SHIPS];
    int16_t side_end_of_turn[15];
    int16_t NUM_PLAYERS;
    int16_t combat_turn;
    uint16_t combat_init;
    uint16_t missile_warning;
    int16_t PLAYER_NUM;
    int8_t game_type;
    int16_t auto_combat_flag[2];
    int16_t net_flag;
    int16_t speedx2_flag;
    int16_t auto_combat;
    int16_t take_control_flag;
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    int16_t tactical_net_choice;
    uint8_t combat_done_waiting;
    int16_t cur_combat_x;
    int16_t cur_combat_y;
    int16_t in_nebula_flag;
    int16_t ordnance_bonus[15];
    int16_t combat_grid[MAX_COMBAT_GRID_WIDTH][MAX_COMBAT_GRID_HEIGHT];
    int16_t fighter_beam_type[9];
    int16_t fighter_bomb_type[9];
} // namespace MOX

namespace fields {
    void __cdecl Set_Help_List(s_help_box* help_data, int16_t count) {
        active_help_list = help_data;
        active_help_box_count = count;
    }

    int16_t __cdecl Add_Hot_Key(const char* hotkey) {
        last_hotkey = hotkey;
        return next_hotkey_field;
    }
} // namespace fields

namespace game_random {
    uint32_t __cdecl Get_Random_Seed() {
        return 0x12345678;
    }
} // namespace game_random

namespace NETMOX {
    void __cdecl Net_Debug_Log(const char*, ...) {
    }

    int32_t __cdecl Mox_Send_Message(int16_t recipient_net_id, uint16_t msg_type, void* packet, uint32_t packet_len) {
        DOCTEST_REQUIRE(packet_len == sizeof(sent_tactical_message));
        sent_net_id = recipient_net_id;
        sent_packet_type = msg_type;
        memcpy(&sent_tactical_message, packet, sizeof(sent_tactical_message));
        sent_message_count++;
        return 1;
    }
} // namespace NETMOX

namespace TECHDATA {
    s_tech_armor_data armor[ARMOR_COUNT];
    s_tech_shield_data shields[SHIELD_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
    s_tech_weapon_mod_data weapon_modifications[WEAPON_MOD_COUNT];
    uint8_t hard_shields_damage_reduction;
} // namespace TECHDATA

namespace COMBINIT {
    int16_t __cdecl Calc_Combat_Shield_Max(int16_t) {
        return 0;
    }

    int16_t __cdecl Does_Combat_Ship_Have_Special(
        int16_t ship_idx, int16_t special_idx) {
        if (special_idx == SPECIAL_ENERGY_ABSORBER) {
            return has_energy_absorber;
        }
        if (special_idx == SPECIAL_DAMPER_FIELD) {
            return has_damper_field;
        }
        if (special_idx == SPECIAL_HARD_SHIELDS) {
            return has_hard_shields;
        }
        if (special_idx == SPECIAL_PHASE_SHIFTER) {
            return has_phase_shifter;
        }
        if (special_idx == SPECIAL_SUB_SPACE_TELEPORTER) {
            return has_sub_space_teleporter;
        }
        if (special_idx == SPECIAL_TIME_WARP_FACILITATOR) {
            return has_time_warp[ship_idx];
        }
        return 0;
    }

    void __cdecl Ship_Center_Offsets(int16_t, int16_t* out_x, int16_t* out_y) {
        *out_x = ship_center_offset_x;
        *out_y = ship_center_offset_y;
    }
} // namespace COMBINIT

namespace CMBTMIS {
    int16_t __cdecl Missile_Owner(int16_t missile_idx) {
        return MOX::missile[missile_idx].owner;
    }

    int32_t __cdecl Get_Missile_Hits(int16_t, int16_t, uint16_t) {
        return 1;
    }

    int64_t __cdecl Missile_Dcv(int16_t, int16_t) {
        return 0;
    }

    char __cdecl Move_Missile(int16_t missile_idx, int16_t, int16_t, int16_t) {
        s_missile* missile = &MOX::missile[missile_idx];
        missile->x -= 2;
        missile->y -= 1;
        missile->travel_dist -= 2;
        missile->facing_dir = 9;
        return 0;
    }
} // namespace CMBTMIS

namespace CMBTFIR2 {
    int16_t __cdecl Being_Captured(int16_t) {
        return 0;
    }

    int32_t __cdecl Total_Internal_Space(int16_t) {
        return 0;
    }

    int16_t __cdecl Used_Teleporter(int16_t) {
        return teleporter_used;
    }

    void __cdecl Set_Combat_Ship_Done(int16_t ship_idx) {
        MOX::combat_data[ship_idx].combat_status_flags |= 4;
    }

    int16_t __cdecl Combat_Ship_Done(int16_t ship_idx) {
        return (MOX::combat_data[ship_idx].combat_status_flags & 4) != 0;
    }
} // namespace CMBTFIR2

namespace CMBTFIRE {
    s_mount_damage mount_dmg[HIGH_ENERGY_FOCUS_PRESENCE_COUNT] = {
        {50, 100, 150},
        {100, 150, 200},
    };

    int32_t Energy_Absorber_Blocked_Damage(int32_t damage) {
        return damage / 4;
    }

    int32_t Damper_Field_Remaining_Damage(int32_t damage) {
        return damage / 4;
    }

    int16_t __cdecl Does_Combat_Planet_Have_Defenses() {
        return planet_defenses;
    }

    int32_t __cdecl Ground_Defense_Structure(int16_t armor_bonus) {
        return ((int32_t)(armor_bonus + 100) *
                   ground_defense_armor_multiplier) /
            100;
    }

    bool Enveloping_Applies_To_Target(int16_t) {
        return true;
    }

    void __cdecl Get_Beam_Weapon_Modifiers(
        int16_t,
        int16_t,
        uint16_t,
        int16_t,
        int32_t* hit_bonus_out,
        int32_t* damage_bonus_out) {
        *hit_bonus_out = 0;
        *damage_bonus_out = 100;
    }

    int8_t __cdecl Special_Is_Turned_On(int16_t, int16_t) {
        return sub_space_teleporter_enabled;
    }

    int16_t __cdecl Weapon_In_Range(int16_t, int16_t, int16_t, int16_t, int16_t) {
        return weapon_in_range;
    }

    int16_t __cdecl Target_Is_Legal_For_Weapon(int16_t, uint16_t, int16_t, int16_t) {
        return target_is_legal;
    }

    int16_t __cdecl Is_Combat_Ship_Dead(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].unit_status == 5;
    }
} // namespace CMBTFIRE

namespace INITSHIP {
    int16_t __cdecl Best_Armor(int16_t) {
        return best_armor;
    }

    int16_t __cdecl Best_Bomb_Weapon(int16_t) {
        return best_bomb_weapon;
    }

    int16_t __cdecl Best_Beam_Weapon(int16_t, uint16_t) {
        return WEAPON_LASER_CANNON;
    }

    int16_t __cdecl Best_Missile_Weapon_NT(int16_t) {
        return WEAPON_NUCLEAR_MISSILE;
    }

    int16_t __cdecl Best_Point_Defense_Weapon(int16_t) {
        return best_point_defense_weapon;
    }

    int16_t __cdecl Best_Ship_Shield(int16_t) {
        return best_ship_shield;
    }
} // namespace INITSHIP

namespace DESIGN {
    int32_t __cdecl Weapon_Space(
        int16_t, int16_t, int16_t quantity, int16_t, int16_t, int16_t) {
        return quantity;
    }
} // namespace DESIGN

namespace GEO {
    int32_t __cdecl Actual_Expected_Damage(
        int32_t min_damage,
        int32_t,
        int32_t,
        int64_t,
        int32_t points_blocked) {
        return min_damage - points_blocked;
    }
} // namespace GEO

namespace graphics {
    void __cdecl Dot(int16_t x, int16_t y, uint8_t) {
        drawn_positions.push_back({x, y});
    }

    void __cdecl Interlaced_Fill(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {
    }
} // namespace graphics

namespace animate {
    void __cdecl Draw(int16_t x, int16_t y, s_animation_header*) {
        drawn_positions.push_back({x, y});
    }
} // namespace animate

namespace allocate {
    void __cdecl Push_Block(void*) {
    }

    void* __cdecl Allocate_Next_Block(void*, uint32_t size) {
        static uint8_t bitmap[400];
        DOCTEST_REQUIRE(size <= sizeof(bitmap));
        return bitmap;
    }

    void __cdecl Pop_Block(void*) {
    }
} // namespace allocate

namespace bitmap {
    void __cdecl Draw_Picture_To_Bitmap(s_animation_header*, s_animation_header*) {
    }
} // namespace bitmap

namespace replace {
    void __cdecl Replace_Color(s_animation_header*, int16_t, uint8_t) {
    }
} // namespace replace

namespace fonts {
    void __cdecl Set_Font_Style_Shadow_Down(int16_t, const s_colors* colors) {
        font_style_selected = colors != nullptr;
        if (colors != nullptr) {
            selected_font_colors = *colors;
        }
    }

    void __cdecl Set_Outline_Color(uint16_t, uint16_t) {
    }

    void __cdecl Set_Font_Spacing(int16_t) {
    }

    void __cdecl Print_Integer_Right(int16_t, int16_t, int32_t) {
    }

    void __cdecl Print(int16_t, int16_t, const char*) {
    }
} // namespace fonts

namespace KEN {
    void __cdecl Ken_Get_Text_Message(int32_t, char* buffer, int16_t buffer_size) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }
    }
} // namespace KEN

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void*, uint16_t) {
        return all_specials_set ? 1 : 0;
    }

    void __cdecl Copy_Structure(void* dest, void* src, uint32_t size) {
        memcpy(dest, src, size);
    }

    void __cdecl Set_Bit_Field(void* field, uint16_t bit) {
        ((uint8_t*)field)[bit / 8] |= (uint8_t)(1 << (bit % 8));
    }

    void __cdecl Clear_Bit_Field(void* field, uint16_t bit) {
        ((uint8_t*)field)[bit / 8] &= (uint8_t) ~(1 << (bit % 8));
    }

    void __cdecl Clear_Structure(void* dest, uint32_t size) {
        memset(dest, 0, size);
    }
} // namespace struct_

namespace angle {
    int16_t __cdecl Get_Angle(int32_t, int32_t) {
        return 0;
    }
} // namespace angle

namespace special {
    int16_t __cdecl Range(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        const int16_t dx = abs(x2 - x1);
        const int16_t dy = abs(y2 - y1);
        return dx > dy ? dx + dy / 2 : dy + dx / 2;
    }

    void __cdecl Absolute_Interpolate_Line(
        int16_t* x_ptr,
        int16_t* y_ptr,
        int16_t,
        int16_t,
        int16_t x2,
        int16_t y2,
        int16_t) {
        *x_ptr = x2;
        *y_ptr = y2;
    }
} // namespace special

DOCTEST_TEST_CASE("fighter garrison AI strength is independent of tactical slot count") {
    memset(players, 0, sizeof(players));
    memset(&MOX::settings, 0, sizeof(MOX::settings));
    memset(TECHDATA::shields, 0, sizeof(TECHDATA::shields));
    memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));

    best_bomb_weapon = WEAPON_BOMBER;
    best_point_defense_weapon = WEAPON_INTERCEPTOR;
    best_ship_shield = 0;
    TECHDATA::weapons[best_point_defense_weapon].max_damage = 10;

    COMBINIT::ground_defense.garrison_interceptors = 5;
    COMBINIT::ground_defense.garrison_bombers = 3;
    COMBINIT::ground_defense.garrison_heavy_fighters = 2;

    struct s_case {
        bool bomber_bays;
        bool heavy_fighter_bays;
        uint16_t expected_strength;
    };
    const s_case cases[] = {
        {false, false, 100},
        {true, false, 60},
        {true, true, 40},
    };

    for (const s_case& test_case : cases) {
        MOX::player[0].tech_applications[TECH_APP_BOMBER_BAYS] =
            test_case.bomber_bays ? TECH_RESEARCH_STATUS_RESEARCHED : 0;
        MOX::player[0].tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] =
            test_case.heavy_fighter_bays ? TECH_RESEARCH_STATUS_RESEARCHED : 0;

        for (uint8_t slot_count = 1; slot_count <= 3; ++slot_count) {
            COMBINIT::ground_defense.garrison_slots = slot_count;
            DOCTEST_CHECK(AIPOWER::Fighter_Garrison_Strength(AI_World_(), legacy_world::Player_Id(0), legacy_world::Player_Id(1)) ==
                test_case.expected_strength);
        }
    }

    COMBINIT::ground_defense.garrison_slots = 2;
}

DOCTEST_TEST_CASE("Stellar Converter AI strength is independent of tactical amount") {
    memset(colonies, 0, sizeof(colonies));
    COMBINIT::ground_defense.stellar_amount = 2;
    MOX::colony[0].buildings[BUILDING_STELLAR_CONVERTER] = 1;

    MOX::settings.strategic_combat_flag = 1;
    DOCTEST_CHECK(AIPOWER::Colony_Ground_Strength_Vs_Player(AI_World_(), legacy_world::Colony_Id(0), legacy_world::Player_Id(1)) == 200);

    MOX::settings.strategic_combat_flag = 0;
    DOCTEST_CHECK(AIPOWER::Colony_Ground_Strength_Vs_Player(AI_World_(), legacy_world::Colony_Id(0), legacy_world::Player_Id(1)) == 500);

    COMBINIT::ground_defense.stellar_amount = 1;
}

DOCTEST_TEST_CASE("effective weapon damage zero-extends tactical weapon counts") {
    memset(combat_data, 0, sizeof(combat_data));

    const uint8_t counts[] = {127, 128, 255};
    for (uint8_t count : counts) {
        MOX::combat_data[0].weapons[0].count = count;
        DOCTEST_CHECK(CMBTAI::Get_Effective_Weapon_Damage(0, 0) == count);
    }
}

DOCTEST_TEST_CASE("ground defense AI hits retain the full configured structure value") {
    memset(combat_data, 0, sizeof(combat_data));
    memset(TECHDATA::armor, 0, sizeof(TECHDATA::armor));

    best_armor = 0;
    MOX::combat_data[0].owner = 0;
    TECHDATA::armor[best_armor].ships_bonus = 900;
    CMBTFIRE::ground_defense_armor_multiplier = 10000;

    planet_defenses = 1;
    DOCTEST_CHECK(CMBTAI::Get_Total_Hits(0) == 100000);

    planet_defenses = 0x0F;
    DOCTEST_CHECK(CMBTAI::Get_Total_Hits(0) == 400000);

    CMBTFIRE::ground_defense_armor_multiplier = 100;
    planet_defenses = 0;
}

DOCTEST_TEST_CASE("disabled spent weapons remain red") {
    memset(combat_data, 0, sizeof(combat_data));
    memset(players, 0, sizeof(players));
    memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));

    MOX::combat_total_ship_count = 1;
    MOX::cur_ship = 0;
    MOX::auto_combat = 0;
    MOX::take_control_flag = 0;
    MOX::combat_data[0].owner = 0;
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;

    s_combat_weapon_slot* slot = &MOX::combat_data[0].weapons[0];
    slot->weapon_id = WEAPON_LASER_CANNON;
    slot->count = 1;
    slot->arc = WEAPON_FIRING_ARC_ALL_SECTORS;
    slot->active = -1;
    slot->shots_left = 0;
    slot->status = 0;

    TECHDATA::weapons[slot->weapon_id].name = weapon_name;
    TECHDATA::weapons[slot->weapon_id].name_plural = weapon_name_plural;
    TECHDATA::weapons[slot->weapon_id].type = WEAPON_TYPE_BEAM;

    font_style_selected = false;
    memset(&selected_font_colors, 0, sizeof(selected_font_colors));

    CMBTDRW1::Draw_Weapon_Status_Display(-1, -1);

    const uint8_t expected_colors[8] = {
        0x48,
        0x4a,
        0x4a,
        0x4a,
        0x4a,
        0x4a,
        0x4a,
        0x4a,
    };
    DOCTEST_REQUIRE(font_style_selected);
    for (int i = 0; i < 8; ++i) {
        DOCTEST_CHECK(selected_font_colors.colors[i] == expected_colors[i]);
    }
}

DOCTEST_TEST_CASE("weapon status is empty without a selected ship") {
    MOX::combat_total_ship_count = 1;
    for (int16_t cur_ship : {-1, 1}) {
        MOX::cur_ship = cur_ship;
        font_style_selected = false;
        CMBTDRW1::Draw_Weapon_Status_Display(-1, -1);
        DOCTEST_CHECK_FALSE(font_style_selected);
    }
}

DOCTEST_TEST_CASE("tactical planets use size-specific draw offsets") {
    struct s_planet_draw_offset_case {
        int8_t size;
        int16_t x;
        int16_t y;
    };

    const s_planet_draw_offset_case cases[] = {
        {PLANET_SIZE_TINY, 2, 2},
        {PLANET_SIZE_SMALL, 0, 1},
        {PLANET_SIZE_MEDIUM, 7, 7},
        {PLANET_SIZE_LARGE, 4, 4},
        {PLANET_SIZE_HUGE, 6, 6},
    };

    MOX::cmbt_colony_id = 0;
    MOX::colony[0].planet = 0;

    for (const s_planet_draw_offset_case test_case : cases) {
        MOX::planet[0].size = test_case.size;

        int16_t x = -1;
        int16_t y = -1;
        CMBTDRW1::Get_Draw_Ship_Offsets(0, &x, &y);

        DOCTEST_CHECK(x == test_case.x);
        DOCTEST_CHECK(y == test_case.y);
    }

    MOX::cmbt_colony_id = -1;
    int16_t x = -1;
    int16_t y = -1;
    CMBTDRW1::Get_Draw_Ship_Offsets(0, &x, &y);

    DOCTEST_CHECK(x == 7);
    DOCTEST_CHECK(y == 7);
}

DOCTEST_TEST_CASE("combat help hides ship-specific entries without a current ship") {
    MOX::order_info_flag = 0;
    MOX::ship_status_display = 0;
    MOX::missile_warning = 0;
    MOX::combat_total_ship_count = 2;

    for (int16_t cur_ship : {-1, 2}) {
        MOX::cur_ship = cur_ship;
        memset(MOX::help_entry_list, 0, sizeof(MOX::help_entry_list));
        active_help_list = nullptr;
        COMBAT1::Set_Combat_Help_Fields();
        DOCTEST_CHECK(active_help_list == MOX::help_entry_list);
        DOCTEST_CHECK(active_help_box_count == 18);
        for (int j = 0; j < 8; ++j) {
            DOCTEST_CHECK(MOX::help_entry_list[9 + j].help_id == -1);
        }
    }

    MOX::ship_status_display = 1;
    MOX::cur_ship = -1;
    COMBAT1::Set_Combat_Help_Fields();
    for (int j = 0; j < 8; ++j) {
        DOCTEST_CHECK(MOX::help_entry_list[9 + j].help_id == -1);
    }
}

DOCTEST_TEST_CASE("combat special help is limited to eight visible rows") {
    MOX::order_info_flag = 1;
    MOX::ship_status_display = 1;
    MOX::combat_total_ship_count = 1;
    MOX::cur_ship = 0;
    all_specials_set = true;
    for (int j = 0; j < 8; ++j) {
        TECHDATA::specials[j].tech_app_id = 100 + j;
    }
    memset(MOX::help_entry_list, 0, sizeof(MOX::help_entry_list));
    COMBAT1::Set_Combat_Help_Fields();
    for (int j = 0; j < 8; ++j) {
        DOCTEST_CHECK(MOX::help_entry_list[10 + j].help_id == 100 + j);
    }
    DOCTEST_CHECK(MOX::help_entry_list[18].help_id == 0x275);
    DOCTEST_CHECK(active_help_box_count == 19);
    all_specials_set = false;
}

DOCTEST_TEST_CASE("auto combat disable packet tolerates no selected ship") {
    MOX::combat_total_ship_count = 2;
    MOX::cur_ship = -1;
    MOX::auto_combat = 1;
    MOX::auto_combat_flag[0] = 1;
    s_net_tactical_combat_message message = {};
    message.viewport_x = 0;
    CMBTMOV1::Tactical_Combat_Receive_Message(0, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, sizeof(message), &message);
    DOCTEST_CHECK(MOX::auto_combat_flag[0] == 0);
    DOCTEST_CHECK(MOX::auto_combat == 1);

    MOX::auto_combat_flag[0] = 1;
    message.viewport_x = -1;
    CMBTMOV1::Tactical_Combat_Receive_Message(0, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, sizeof(message), &message);
    message.viewport_x = 2;
    CMBTMOV1::Tactical_Combat_Receive_Message(0, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, sizeof(message), &message);
    DOCTEST_CHECK(MOX::auto_combat_flag[0] == 1);

    message.viewport_x = 0;
    MOX::cur_ship = 1;
    MOX::combat_attacker_player = 0;
    MOX::combat_data[1].owner = 0;
    CMBTMOV1::Tactical_Combat_Receive_Message(0, TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED, sizeof(message), &message);
    DOCTEST_CHECK(MOX::auto_combat == 0);
}

DOCTEST_TEST_CASE("current ship blip is skipped without a current ship") {
    MOX::combat_total_ship_count = 2;
    for (int16_t cur_ship : {-1, 2}) {
        MOX::cur_ship = cur_ship;
        drawn_positions.clear();
        CMBTDRW1::Draw_Current_Ship_Blip();
        DOCTEST_CHECK(drawn_positions.empty());
    }
}

DOCTEST_TEST_CASE("current ship minimap pulse matches static blips on classic and expanded grids") {
    memset(combat_data, 0, sizeof(combat_data));
    memset(missiles, 0, sizeof(missiles));
    MOX::cmbt_colony_id = -1;
    MOX::combat_total_ship_count = 2;
    MOX::cur_ship = 1;
    MOX::combat_attacker_player = 0;
    MOX::combat_data[1].owner = 0;
    MOX::combat_data[1].x = 40;
    const int16_t saved_height = MOX::combat_grid_height;
    const int16_t heights[] = {ORIGINAL_COMBAT_GRID_HEIGHT, 69, 136, MAX_COMBAT_GRID_HEIGHT};

    for (int16_t height : heights) {
        MOX::combat_grid_height = height;
        const int16_t rows[] = {0, 1, static_cast<int16_t>(height / 2), static_cast<int16_t>(height - 1)};
        for (int16_t y : rows) {
            MOX::combat_data[1].y = y;
            const int16_t expected_y = 364 + y * 102 / height;
            for (int8_t size = 0; size < SHIP_SIZE_COUNT; ++size) {
                MOX::combat_data[1].size_class = size;
                drawn_positions.clear();
                CMBTDRW1::Draw_Reduced_Combat_Map();
                DOCTEST_REQUIRE_FALSE(drawn_positions.empty());
                const s_draw_position static_position = drawn_positions.front();
                DOCTEST_CHECK(static_position.x == 560);
                DOCTEST_CHECK(static_position.y == expected_y);

                for (int16_t frame = 0; frame < 8; frame += 2) {
                    MOX::ship_frame = frame;
                    drawn_positions.clear();
                    CMBTDRW1::Draw_Current_Ship_Blip();
                    DOCTEST_REQUIRE_FALSE(drawn_positions.empty());
                    DOCTEST_CHECK(drawn_positions.front().x == static_position.x);
                    DOCTEST_CHECK(drawn_positions.front().y == static_position.y);
                    DOCTEST_CHECK(drawn_positions.front().y >= 364);
                    DOCTEST_CHECK(drawn_positions.front().y < 466);
                }
            }
        }
    }
    MOX::combat_grid_height = saved_height;
}

DOCTEST_TEST_CASE("current planet minimap pulse matches static planet markers") {
    memset(combat_data, 0, sizeof(combat_data));
    memset(missiles, 0, sizeof(missiles));
    MOX::cmbt_colony_id = 0;
    MOX::colony[0].planet = 0;
    MOX::combat_total_ship_count = 1;
    MOX::cur_ship = 0;
    MOX::ship_frame = 2;
    MOX::combat_data[0].x = 40;
    const int16_t saved_height = MOX::combat_grid_height;
    const int16_t heights[] = {ORIGINAL_COMBAT_GRID_HEIGHT, MAX_COMBAT_GRID_HEIGHT};

    for (int16_t height : heights) {
        MOX::combat_grid_height = height;
        MOX::combat_data[0].y = height / 2;
        for (int8_t size = PLANET_SIZE_TINY; size < PLANET_SIZE_COUNT; ++size) {
            MOX::planet[0].size = size;
            drawn_positions.clear();
            CMBTDRW1::Draw_Reduced_Combat_Map();
            DOCTEST_REQUIRE_FALSE(drawn_positions.empty());
            const s_draw_position static_position = drawn_positions.front();
            DOCTEST_CHECK(static_position.x == 560 + (size == PLANET_SIZE_TINY ? 0 : size - 7));
            DOCTEST_CHECK(static_position.y == 415 + (size == PLANET_SIZE_TINY ? 0 : size - 7));

            drawn_positions.clear();
            CMBTDRW1::Draw_Current_Ship_Blip();
            DOCTEST_REQUIRE_FALSE(drawn_positions.empty());
            DOCTEST_CHECK(drawn_positions.front().x == static_position.x);
            DOCTEST_CHECK(drawn_positions.front().y == static_position.y);
        }
    }
    MOX::combat_grid_height = saved_height;
    MOX::cmbt_colony_id = -1;
}

DOCTEST_TEST_CASE("high-slot flyers use the expanded minimap scale") {
    memset(missiles, 0, sizeof(missiles));
    MOX::cmbt_colony_id = -1;
    MOX::combat_total_ship_count = 0;
    const int16_t saved_height = MOX::combat_grid_height;
    MOX::combat_grid_height = MAX_COMBAT_GRID_HEIGHT;
    s_missile* missile = &MOX::missile[MAX_COMBAT_FLYERS - 1];
    missile->type = WEAPON_NUCLEAR_MISSILE;
    missile->x = 40 * 20 + 8;
    missile->y = (MAX_COMBAT_GRID_HEIGHT - 1) * 20 + 8;

    drawn_positions.clear();
    CMBTDRW1::Draw_Reduced_Combat_Map();
    DOCTEST_REQUIRE(drawn_positions.size() == 1);
    DOCTEST_CHECK(drawn_positions.front().x == 560);
    DOCTEST_CHECK(drawn_positions.front().y == 465);

    memset(missiles, 0, sizeof(missiles));
    MOX::combat_grid_height = saved_height;
}

DOCTEST_TEST_CASE("interactive speed loss preserves movement already spent") {
    int16_t reserve = 0;

    int16_t movement = COMBAT1::Adjust_Movement_After_Speed_Change(
        10, 4, 10, 7, &reserve);

    DOCTEST_CHECK(movement == 1);
    DOCTEST_CHECK(reserve == 6);

    movement = COMBAT1::Adjust_Movement_After_Speed_Change(
        4, 10, 10, movement, &reserve);

    DOCTEST_CHECK(movement == 7);
    DOCTEST_CHECK(reserve == 0);
}

DOCTEST_TEST_CASE("interactive immobilization never makes movement negative") {
    int16_t reserve = 0;

    int16_t movement = COMBAT1::Adjust_Movement_After_Speed_Change(
        10, 0, 10, 3, &reserve);

    DOCTEST_CHECK(movement == 0);
    DOCTEST_CHECK(reserve == 3);

    movement = COMBAT1::Adjust_Movement_After_Speed_Change(
        0, 10, 10, movement, &reserve);

    DOCTEST_CHECK(movement == 3);
    DOCTEST_CHECK(reserve == 0);
}

DOCTEST_TEST_CASE("strict combat sequencing reverses the waited ship order") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();

    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 1);
}

DOCTEST_TEST_CASE("ship selection clears a lingering missile warning") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::missile_warning = 1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::missile_warning == 0);

    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();
    MOX::missile_warning = 1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::missile_warning == 0);
}

DOCTEST_TEST_CASE("initiative turn order retains stasis ships for later release") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::combat_data[3].stasis_source_idx = 1;

    DOCTEST_CHECK(COMBAT1::Combat_Ship_Participates_In_Turn_Order(3, false));
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Participates_In_Pass(3, false));

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);

    MOX::combat_data[3].stasis_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
}

DOCTEST_TEST_CASE("turn-order stasis retention follows initiative and Time Warp gates") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::combat_data[3].stasis_source_idx = 1;

    struct s_case {
        uint16_t initiative;
        bool time_warp_only;
        bool time_warp_special;
        bool expected;
    };
    const s_case cases[] = {
        {0, false, false, false},
        {1, false, false, true},
        {1, true, false, false},
        {1, true, true, true},
    };

    for (const s_case& test_case : cases) {
        MOX::combat_init = test_case.initiative;
        MOX::cur_ship = 3;
        has_time_warp[3] = test_case.time_warp_special;
        DOCTEST_CHECK(COMBAT1::Combat_Ship_Participates_In_Turn_Order(
                          3, test_case.time_warp_only) == test_case.expected);
    }
}

DOCTEST_TEST_CASE("strict owner-change advance preserves a capture-restored slot") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);

    MOX::ship_turn_order[MOX::cur_ptr] = -1;
    COMBAT1::Restore_Captured_Ship_To_Turn_Order(
        MOX::ship_turn_order, MAX_COMBAT_SHIPS, MOX::cur_ship);
    MOX::combat_data[MOX::cur_ship].owner = 1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
    DOCTEST_CHECK(MOX::ship_turn_order[MOX::cur_ptr] == 1);
}

DOCTEST_TEST_CASE("tactical combat turn limit uses the configured maximum") {
    memset(combat_data, 0, sizeof(combat_data));
    MOX::combat_total_ship_count = 3;
    MOX::combat_attacker_player = 0;
    MOX::combat_data[0].unit_status = 5;
    MOX::combat_data[1].owner = 0;
    MOX::combat_data[1].unit_status = 0;
    MOX::combat_data[1].is_captured = 0;
    MOX::combat_data[2].owner = 1;
    MOX::combat_data[2].unit_status = 0;
    MOX::combat_data[2].is_captured = 0;
    planet_defenses = 0;

    COMBAT1::max_combat_turns = 5;
    MOX::combat_turn = 5;
    DOCTEST_CHECK(COMBAT1::Check_For_Winner() == 0);
    MOX::combat_turn = 6;
    DOCTEST_CHECK(COMBAT1::Check_For_Winner() == -1);
    COMBAT1::max_combat_turns = 50;
}

DOCTEST_TEST_CASE("strict combat wait cost saturates at remaining movement") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = UINT16_MAX;
    MOX::combat_data[1].movement_left = 3;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK((MOX::combat_data[1].movement_left == 0));
}

DOCTEST_TEST_CASE("strict combat wait charges each ship only once") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = 4;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK((MOX::combat_data[1].movement_left == 6));
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 3);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK((MOX::combat_data[3].movement_left == 6));
}

DOCTEST_TEST_CASE("strict automatic wait cost is deferred until the wait stage") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = 4;
    MOX::player[MOX::combat_data[1].owner].objectives =
        PLAYER_OBJECTIVE_MILITARIST;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_AUTOMATIC) == 0);
    DOCTEST_CHECK((MOX::combat_data[1].movement_left == 10));
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK((MOX::combat_data[1].movement_left == 6));
}

DOCTEST_TEST_CASE("strict sequencing includes ships restored during the wait stage") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::ship_turn_order[2] = -10;
    COMBAT1::Reset_Strict_Combat_Sequence();

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 2);

    COMBAT1::Restore_Captured_Ship_To_Turn_Order(
        MOX::ship_turn_order, MAX_COMBAT_SHIPS, 3);
    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
}

DOCTEST_TEST_CASE("classic selection skips retreating and already-done ships") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();
    MOX::combat_data[1].is_retreating = 1;
    CMBTFIR2::Set_Combat_Ship_Done(2);

    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
}

DOCTEST_TEST_CASE("strict combat sequencing skips side-ended ships") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::side_end_of_turn[0] = 1;

    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_CHECK(CMBTFIR2::Combat_Ship_Done(1) == 1);
    DOCTEST_CHECK(CMBTFIR2::Combat_Ship_Done(3) == 1);
}

DOCTEST_TEST_CASE("strict side wait moves every remaining friendly ship to the wait stage") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = 4;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_CHECK((MOX::combat_data[1].movement_left == 6));
    DOCTEST_CHECK((MOX::combat_data[3].movement_left == 6));
}

DOCTEST_TEST_CASE("strict side wait records stasis ships before functional selection") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = 4;
    MOX::combat_data[3].stasis_source_idx = 1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_CHECK((MOX::combat_data[3].movement_left == 6));
    DOCTEST_CHECK(MOX::ship_turn_order[2] == -1);

    MOX::combat_data[3].stasis_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK((MOX::combat_data[3].movement_left == 6));
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Side_Wait_Is_Available());
}

DOCTEST_TEST_CASE("strict side-waited stasis ships remain in the wait stage") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = 4;
    MOX::combat_data[3].stasis_source_idx = 1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);

    MOX::combat_data[3].stasis_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK((MOX::combat_data[3].movement_left == 6));
}

DOCTEST_TEST_CASE("strict side wait in the wait stage is a safe no-op") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::wait_movement_cost = 4;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 3);
    const int16_t movement = MOX::combat_data[3].movement_left;
    const int16_t pointer = MOX::cur_ptr;

    DOCTEST_CHECK(COMBAT1::Select_Next_Strict_Ship(
                      COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(MOX::cur_ptr == pointer);
    DOCTEST_CHECK((MOX::combat_data[3].movement_left == movement));
}

DOCTEST_TEST_CASE("strict exhaustion clears cursors and writes the queue sentinel") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_DONE) == 1);
    DOCTEST_CHECK(MOX::cur_ship == -1);
    DOCTEST_CHECK(MOX::cur_ptr == -1);
    DOCTEST_CHECK(MOX::ship_turn_order[0] == -10);
}

DOCTEST_TEST_CASE("classic side wait yields to the opponent and dual waits resume attacker") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::ship_initiative = 1;
    COMBAT1::Reset_Combat_Side_Waits();

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(
                        COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(
                        COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::combat_data[MOX::cur_ship].owner ==
        MOX::combat_attacker_player);
}

DOCTEST_TEST_CASE("classic initial selection starts at the first eligible ship") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();

    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ptr == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
}

DOCTEST_TEST_CASE("classic done visits each queue entry once and terminates") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_DONE) == 1);
    DOCTEST_CHECK(MOX::ship_turn_order[0] == -1);
    DOCTEST_CHECK(MOX::ship_turn_order[1] == -1);
    DOCTEST_CHECK(MOX::ship_turn_order[2] == -1);
    DOCTEST_CHECK(MOX::cur_ship == -1);
    DOCTEST_CHECK(MOX::cur_ptr == -1);
}

DOCTEST_TEST_CASE("classic wait advances without consuming the current queue slot") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
    DOCTEST_CHECK(MOX::ship_turn_order[0] == 1);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_DONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
}

DOCTEST_TEST_CASE("classic automatic advances once after caller consumes the ship") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();
    MOX::cur_ptr = 0;
    MOX::cur_ship = 1;
    MOX::ship_turn_order[0] = -1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(
                        COMBAT_SHIP_ACTION_AUTOMATIC) == 0);
    DOCTEST_CHECK(MOX::cur_ptr == 1);
    DOCTEST_CHECK(MOX::cur_ship == 2);
}

DOCTEST_TEST_CASE("classic selection skips invalid and side-ended queue entries") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();
    MOX::combat_data[1].is_captured = 1;
    MOX::side_end_of_turn[1] = 1;

    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
}

DOCTEST_TEST_CASE("classic selection skips an undefended planet") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();
    MOX::combat_data[0].unit_status = 0;
    MOX::combat_data[0].owner = 1;
    MOX::ship_turn_order[0] = 0;
    MOX::ship_turn_order[1] = 1;
    MOX::ship_turn_order[2] = 2;
    MOX::ship_turn_order[3] = 3;
    MOX::ship_turn_order[4] = -10;
    planet_defenses = 0;

    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
}

DOCTEST_TEST_CASE("classic side wait is ignored when the opponent is absent") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();
    MOX::ship_turn_order[1] = -1;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 1);
    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(
                      COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
}

DOCTEST_TEST_CASE("classic side wait resets between tactical turns") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    TEST_STRICT_COMBAT_SEQUENCE::Set_Classic_();

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Classic_Ship(
                        COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_REQUIRE(MOX::cur_ship == 2);
    COMBAT1::Reset_Combat_Side_Waits();
    MOX::cur_ptr = -1;
    MOX::cur_ship = -1;
    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
}

DOCTEST_TEST_CASE("side wait has no effect when initiative is disabled") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::ship_initiative = 0;
    MOX::combat_init = 0;
    MOX::cur_ptr = 1;
    MOX::cur_ship = 2;

    DOCTEST_CHECK(COMBAT1::Select_Next_Classic_Ship(
                      COMBAT_SHIP_ACTION_SIDE_WAIT) == 0);
    DOCTEST_CHECK(MOX::cur_ptr == 1);
    DOCTEST_CHECK(MOX::cur_ship == 2);
}

DOCTEST_TEST_CASE("side wait uses the 1.50 tactical message value") {
    DOCTEST_CHECK(TACTICAL_NET_MSG_SIDE_WAIT == 10017);
    DOCTEST_CHECK(COMBAT_SHIP_ACTION_SIDE_WAIT != COMBAT_SHIP_ACTION_WAIT);
}

DOCTEST_TEST_CASE("combat side wait field is local and consumes only its field id") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    next_hotkey_field = 77;
    last_hotkey.clear();
    MOX::cur_ship = 1;

    DOCTEST_CHECK(COMBAT1::Combat_Side_Wait_Is_Available());
    DOCTEST_CHECK(COMBAT1::Add_Combat_Side_Wait_Field() == 77);
    DOCTEST_CHECK(last_hotkey == "h");

    int16_t ordinary_input = 76;
    DOCTEST_CHECK_FALSE(COMBAT1::Consume_Combat_Side_Wait_Input(&ordinary_input));
    DOCTEST_CHECK(ordinary_input == 76);
    int16_t side_wait_input = 77;
    DOCTEST_CHECK(COMBAT1::Consume_Combat_Side_Wait_Input(&side_wait_input));
    DOCTEST_CHECK(side_wait_input == 0);

    MOX::cur_ship = 2;
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Side_Wait_Is_Available());
    DOCTEST_CHECK(COMBAT1::Add_Combat_Side_Wait_Field() == -100);
}

DOCTEST_TEST_CASE("combat side wait field is absent when initiative is disabled") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::combat_init = 0;
    MOX::cur_ship = 1;

    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Side_Wait_Is_Available());
    DOCTEST_CHECK(COMBAT1::Add_Combat_Side_Wait_Field() == -100);
    DOCTEST_CHECK(COMBAT1::Select_After_Combat_Side_Wait() == 0);
    DOCTEST_CHECK(MOX::cur_ship == 1);
}

DOCTEST_TEST_CASE("combat side wait field is absent during the strict wait stage") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);
    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_WAIT) == 0);

    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Side_Wait_Is_Available());
    DOCTEST_CHECK(COMBAT1::Add_Combat_Side_Wait_Field() == -100);
}

DOCTEST_TEST_CASE("combat side wait message has value 10017 and zero arguments") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    sent_message_count = 0;
    memset(&sent_tactical_message, 0xff, sizeof(sent_tactical_message));
    MOX::cur_combat_x = 12;
    MOX::cur_combat_y = 34;
    MOX::player[1].network_player_id = 9;

    DOCTEST_CHECK(COMBAT1::Send_Combat_Side_Wait_Message(1) == 1);
    DOCTEST_CHECK(sent_message_count == 1);
    DOCTEST_CHECK(sent_net_id == 9);
    DOCTEST_CHECK(sent_packet_type == TACTICAL_NET_PACKET_ACTION);
    DOCTEST_CHECK(sent_tactical_message.viewport_x == 12);
    DOCTEST_CHECK(sent_tactical_message.viewport_y == 34);
    DOCTEST_CHECK(sent_tactical_message.random_seed == 0x12345678);
    DOCTEST_CHECK(sent_tactical_message.message_code == TACTICAL_NET_MSG_SIDE_WAIT);
    DOCTEST_CHECK(sent_tactical_message.arg1 == 0);
    DOCTEST_CHECK(sent_tactical_message.arg2 == 0);
    DOCTEST_CHECK(sent_tactical_message.arg3 == 0);
}

DOCTEST_TEST_CASE("strict initiative comparator uses complete attack values and stable ties") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    MOX::combat_data[1].tac_val_attack = 19;
    MOX::combat_data[2].tac_val_attack = 11;
    MOX::combat_data[3].tac_val_attack = 10;
    MOX::combat_data[1].current_speed = 0;
    MOX::combat_data[2].current_speed = 0;
    MOX::combat_data[3].current_speed = 0;

    int16_t first = 1;
    int16_t second = 2;
    int16_t third = 3;
    DOCTEST_CHECK(COMBAT1::qsort_ship_initiative(&first, &second) < 0);
    DOCTEST_CHECK(COMBAT1::qsort_ship_initiative(&second, &third) < 0);
    DOCTEST_CHECK(COMBAT1::qsort_ship_initiative(&first, &third) < 0);

    MOX::combat_data[1].tac_val_attack = 10;
    MOX::combat_data[2].tac_val_attack = 10;
    MOX::combat_data[1].crew_quality = CREW_QUALITY_REGULAR;
    MOX::combat_data[2].crew_quality = CREW_QUALITY_ELITE;
    DOCTEST_CHECK(COMBAT1::qsort_ship_initiative(&first, &second) > 0);
}

DOCTEST_TEST_CASE("strict initiative compares Antaran owner 8 by crew quality") {
    TEST_STRICT_COMBAT_SEQUENCE::Reset_();
    COMBAT1::ship_initiative = 2;
    MOX::combat_data[1].owner = NONPLAYER_SHIP_TYPE_ANTARAN;
    MOX::combat_data[1].tac_val_attack = 10;
    MOX::combat_data[1].crew_quality = CREW_QUALITY_REGULAR;
    MOX::combat_data[2].owner = 0;
    MOX::combat_data[2].tac_val_attack = 10;
    MOX::combat_data[2].crew_quality = CREW_QUALITY_ELITE;
    MOX::combat_data[3].unit_status = 5;

    DOCTEST_REQUIRE(COMBAT1::Select_Next_Strict_Ship(
                        COMBAT_SHIP_ACTION_NONE) == 0);
    DOCTEST_CHECK(MOX::cur_ship == 2);
}

DOCTEST_TEST_CASE("sphere safety scoring handles thousands of flyer groups") {
    memset(combat_data, 0, sizeof(combat_data));
    memset(missiles, 0, sizeof(missiles));
    MOX::combat_total_ship_count = 1;
    MOX::combat_data[0].owner = 0;
    ship_center_offset_x = 0;
    ship_center_offset_y = 0;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    for (int16_t i = 0; i < MAX_COMBAT_FLYERS_PER_SIDE; ++i) {
        MOX::missile[i].type = WEAPON_INTERCEPTOR;
        MOX::missile[i].quantity = 396;
        MOX::missile[i].owner = 0;
    }
    DOCTEST_CHECK_FALSE(CMBTAI::Safe_To_Fire_Sphere_Weapon(0));
    for (int16_t i = 0; i < MAX_COMBAT_FLYERS_PER_SIDE; ++i) {
        MOX::missile[i].owner = 1;
    }
    DOCTEST_CHECK(CMBTAI::Safe_To_Fire_Sphere_Weapon(0));
    memset(missiles, 0, sizeof(missiles));
}

DOCTEST_TEST_CASE("tactical special references work beyond the byte index domain") {
    TEST_TRACTOR_POWER::Reset_();
    MOX::combat_data[256] = MOX::combat_data[0];
    MOX::combat_data[255] = MOX::combat_data[1];
    MOX::combat_total_ship_count = 257;
    MOX::combat_data[1].special_cooldown = COMBAT_SHIP_NONE;
    MOX::combat_data[255].special_cooldown = 256;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(256) == 0);
    MOX::combat_data[255].special_cooldown = COMBAT_SHIP_NONE;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(256) == 10);
    MOX::combat_data[256].stasis_source_idx = 255;
    DOCTEST_CHECK(CMBTMOV1::Ship_Is_In_Stasis(256) == 1);
    DOCTEST_CHECK(COMBAT1::Get_Number_Of_Ships_In_Stasis(255) == 1);
    MOX::combat_data[256].stasis_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_CHECK(CMBTMOV1::Ship_Is_In_Stasis(256) == 0);
    MOX::combat_data[256].black_hole_source_idx = 255;
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Black_Hole(256));
    MOX::combat_data[256].black_hole_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_CHECK_FALSE(CMBTMOV1::Ship_In_Black_Hole(256));
}

DOCTEST_TEST_CASE("large battlefield movement does not overflow or cap distance at 81") {
    TEST_TRACTOR_POWER::Reset_();
    MOX::combat_grid_height = MAX_COMBAT_GRID_HEIGHT;
    MOX::combat_data[0].x = 0;
    MOX::combat_data[0].y = 0;
    const int16_t facing = CMBTMOV1::Get_Facing(0, 0, 80, 339);
    MOX::combat_data[0].facing_dir = (int8_t)facing;
    DOCTEST_CHECK(CMBTMOV1::Move_Cost(0, 80, 339) == 349);
    memset(MOX::combat_grid, -1, sizeof(MOX::combat_grid));
    MOX::combat_grid[80][339] = 2000;
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(0, 1600, 6780, 1600, 6780) == 1);
    DOCTEST_CHECK(CMBTMOV1::Ship_In_Legal_Square(0, 1600, 6800, 1600, 6800) == 0);
    MOX::combat_grid_height = ORIGINAL_COMBAT_GRID_HEIGHT;
}

DOCTEST_TEST_CASE("tractor speed uses the configured hull threshold") {
    TEST_TRACTOR_POWER::Reset_();

    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 0);

    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 4;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 3);
}

DOCTEST_TEST_CASE("tractor speed zero-extends native weapon counts") {
    TEST_TRACTOR_POWER::Reset_();
    MOX::combat_data[1].weapons[0].count = (int8_t)200;
    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 255;

    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 3);
}

DOCTEST_TEST_CASE("large tractor aggregates remain positive") {
    TEST_TRACTOR_POWER::Reset_();
    MOX::combat_total_ship_count = 43;
    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 255;
    for (int16_t ship_idx = 1; ship_idx < 43; ++ship_idx) {
        s_combat_data* tractor = &MOX::combat_data[ship_idx];
        tractor->special_cooldown = 0;
        tractor->stasis_source_idx = COMBAT_SHIP_NONE;
        for (int weapon_idx = 0; weapon_idx < 8; ++weapon_idx) {
            tractor->weapons[weapon_idx].weapon_id = WEAPON_TRACTOR_BEAM;
            tractor->weapons[weapon_idx].count = 99;
        }
    }

    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 0);
}

DOCTEST_TEST_CASE("retreat validation preserves rotation-time capture state") {
    TEST_TRACTOR_POWER::Reset_();
    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 4;
    s_combat_data* ship = &MOX::combat_data[0];
    ship->is_retreating = 1;

    DOCTEST_REQUIRE(COMBAT1::Validate_Retreating_Ship(0));

    ship->owner = 1;
    ship->is_captured = 1;
    ship->is_retreating = 0;

    DOCTEST_CHECK_FALSE(COMBAT1::Validate_Retreating_Ship(0));
    DOCTEST_CHECK(ship->owner == 1);
    DOCTEST_CHECK(ship->is_captured == 1);
    DOCTEST_CHECK(ship->unit_status == 0);
    DOCTEST_CHECK(ship->is_retreating == 0);
}

DOCTEST_TEST_CASE("retreat validation rejects rotation-time immobilization") {
    TEST_TRACTOR_POWER::Reset_();
    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 4;
    s_combat_data* ship = &MOX::combat_data[0];
    ship->is_retreating = 1;
    ship->movement_left = 3;

    DOCTEST_REQUIRE(COMBAT1::Validate_Retreating_Ship(0));

    ship->drive_current_hits = 0;

    DOCTEST_CHECK_FALSE(COMBAT1::Validate_Retreating_Ship(0));
    DOCTEST_CHECK(ship->unit_status == 0);
    DOCTEST_CHECK(ship->is_retreating == 0);
    DOCTEST_CHECK((ship->movement_left == 0));
}

DOCTEST_TEST_CASE("retreat validation rejects telepathic capture without a captured flag") {
    TEST_TRACTOR_POWER::Reset_();
    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 4;
    s_combat_data* ship = &MOX::combat_data[0];
    ship->is_retreating = 1;

    DOCTEST_REQUIRE(COMBAT1::Validate_Retreating_Ship(0));

    ship->owner = 1;
    ship->is_captured = 0;
    ship->is_retreating = 0;

    DOCTEST_CHECK_FALSE(COMBAT1::Validate_Retreating_Ship(0));
    DOCTEST_CHECK(ship->owner == 1);
    DOCTEST_CHECK(ship->is_captured == 0);
    DOCTEST_CHECK(ship->unit_status == 0);
    DOCTEST_CHECK(ship->is_retreating == 0);
}

DOCTEST_TEST_CASE("Trans-Dimensional combat speed uses the configured bonus") {
    TEST_TRACTOR_POWER::Reset_();
    MOX::combat_total_ship_count = 1;
    MOX::combat_data[0].owner = 0;
    MOX::player[0].traits[TRAIT_TRANS_DIMENSIONAL] = 1;
    COMBAT1::td_combat_speed_bonus = 7;

    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 17);

    COMBAT1::td_combat_speed_bonus = 4;
}

DOCTEST_TEST_CASE("real driveless ships retain Trans-Dimensional and tractor modifiers") {
    TEST_TRACTOR_POWER::Reset_();
    s_combat_data* target = &MOX::combat_data[0];
    target->ship_idx = 0;
    target->owner = 0;
    target->ftl_type = DRIVE_NO_DRIVE;
    MOX::player[0].traits[TRAIT_TRANS_DIMENSIONAL] = 1;
    COMBAT1::td_combat_speed_bonus = 7;

    MOX::combat_total_ship_count = 1;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 7);

    MOX::combat_total_ship_count = 2;
    COMBAT1::tractors_to_stop[SHIP_SIZE_LARGE] = 4;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 2);

    target->ship_idx = -1;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 0);

    target->ship_idx = 0;
    target->black_hole_source_idx = 1;
    DOCTEST_CHECK(COMBAT1::Calc_Current_Speed(0) == 0);

    COMBAT1::td_combat_speed_bonus = 4;
}

DOCTEST_TEST_CASE("ship officers prevent automatic out-of-ammo retreat") {
    memset(combat_data, 0, sizeof(combat_data));
    has_energy_absorber = false;
    has_damper_field = false;
    has_hard_shields = false;
    has_phase_shifter = false;
    has_sub_space_teleporter = false;

    MOX::combat_data[0].officer_idx = -1;
    DOCTEST_CHECK(CMBTAI::Is_Ship_Out_Of_Ammo(0) == 1);

    MOX::combat_data[0].officer_idx = 0;
    DOCTEST_CHECK(CMBTAI::Is_Ship_Out_Of_Ammo(0) == 0);
}

DOCTEST_TEST_CASE("ship range exposes tactical squares without changing legacy range") {
    memset(combat_data, 0, sizeof(combat_data));
    MOX::combat_data[0].x = 10;
    MOX::combat_data[0].y = 10;
    MOX::combat_data[1].x = 16;
    MOX::combat_data[1].y = 10;

    DOCTEST_CHECK(CMBTAI::Range_To_Ship_Squares(0, 1) == 6);
    DOCTEST_CHECK(CMBTAI::Range_To_Ship(0, 1) == 2);
}

DOCTEST_TEST_CASE("ship to point range measures pixels before scaling") {
    memset(combat_data, 0, sizeof(combat_data));
    ship_center_offset_x = 0;
    ship_center_offset_y = 0;

    DOCTEST_CHECK(CMBTAI::Range_To_XY(0, 2, 19) == 1);

    ship_center_offset_x = 20;
    DOCTEST_CHECK(CMBTAI::Range_To_XY(0, 20, 0) == 0);
    DOCTEST_CHECK(CMBTAI::Range_To_XY_From_Corner(0, 20, 0) == 1);

    ship_center_offset_x = 0;
}

DOCTEST_TEST_CASE("missile target selection excludes range thirty") {
    TEST_EXPECTED_DAMAGE::Reset_();
    target_is_legal = true;
    weapon_in_range = true;
    ship_center_offset_x = 0;
    ship_center_offset_y = 0;

    MOX::combat_data[1].owner = 1;
    s_combat_weapon_slot* slot = &MOX::combat_data[1].weapons[0];
    slot->weapon_id = WEAPON_LASER_CANNON;
    slot->count = 1;
    slot->shots_left = 1;
    slot->ammo = -1;
    slot->active = 1;
    slot->arc = WEAPON_FIRING_ARC_ALL_SECTORS;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;

    s_missile* missile = &MOX::missile[0];
    missile->type = WEAPON_NUCLEAR_MISSILE;
    missile->owner = 0;
    missile->quantity = 1;
    missile->target_unit_idx = 1;
    missile->x = 1700;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 2;
    MOX::ordnance_bonus[0] = 100;

    int32_t value = 0;
    DOCTEST_CHECK(CMBTAI::Get_Current_Missile_Target(1, &value) == 0);
    DOCTEST_CHECK(value > -5000);

    missile->x = 1760;
    DOCTEST_CHECK(CMBTAI::Get_Current_Missile_Target(1, &value) == -1);
    DOCTEST_CHECK(value == -20000);

    weapon_in_range = false;
}

DOCTEST_TEST_CASE("round teleporter excludes square corners") {
    memset(combat_data, 0, sizeof(combat_data));
    MOX::combat_data[0].x = 20;
    MOX::combat_data[0].y = 20;
    CMBTMOV1::sub_space_teleporter_range = 20;

    CMBTMOV1::round_teleporter = 0;
    DOCTEST_CHECK(CMBTMOV1::Teleporter_Destination_In_Range(
        0, 40 * 20, 40 * 20));

    CMBTMOV1::round_teleporter = 1;
    DOCTEST_CHECK_FALSE(CMBTMOV1::Teleporter_Destination_In_Range(
        0, 40 * 20, 40 * 20));
    DOCTEST_CHECK(CMBTMOV1::Teleporter_Destination_In_Range(
        0, 40 * 20, 20 * 20));
}

DOCTEST_TEST_CASE("Sub-Space Teleporter range is independent of ship speed") {
    memset(combat_data, 0, sizeof(combat_data));
    for (int x = 0; x < MOX::combat_grid_width; ++x) {
        for (int y = 0; y < MOX::combat_grid_height; ++y) {
            MOX::combat_grid[x][y] = -1;
        }
    }
    MOX::combat_data[0].x = 40;
    MOX::combat_data[0].y = 34;
    has_sub_space_teleporter = true;
    has_phase_shifter = false;
    sub_space_teleporter_enabled = true;
    teleporter_used = 0;
    CMBTMOV1::round_teleporter = 0;
    CMBTMOV1::sub_space_teleporter_range = 5;

    COMBAT1::Set_Legal_Moves(0, 30, 0);

    DOCTEST_CHECK(MOX::combat_grid[45][34] == 2000);
    DOCTEST_CHECK(MOX::combat_grid[46][34] == -1);

    has_sub_space_teleporter = false;
    has_phase_shifter = true;
    for (int x = 0; x < MOX::combat_grid_width; ++x) {
        for (int y = 0; y < MOX::combat_grid_height; ++y) {
            MOX::combat_grid[x][y] = -1;
        }
    }

    COMBAT1::Set_Legal_Moves(0, 7, 0);

    DOCTEST_CHECK(MOX::combat_grid[47][34] == 2000);
    DOCTEST_CHECK(MOX::combat_grid[48][34] == -1);

    has_sub_space_teleporter = true;
    for (int x = 0; x < MOX::combat_grid_width; ++x) {
        for (int y = 0; y < MOX::combat_grid_height; ++y) {
            MOX::combat_grid[x][y] = -1;
        }
    }

    COMBAT1::Set_Legal_Moves(0, 7, 0);

    DOCTEST_CHECK(MOX::combat_grid[47][34] == 2000);
    DOCTEST_CHECK(MOX::combat_grid[48][34] == -1);

    has_sub_space_teleporter = false;
    has_phase_shifter = false;
}

DOCTEST_TEST_CASE("destroyed cloak devices update combat cloak state") {
    int8_t cloak_state = 1;
    COMBAT1::Handle_Destroyed_Cloak(&cloak_state, 0, false, false, false);
    DOCTEST_CHECK(cloak_state == 2);

    cloak_state = 4;
    COMBAT1::Handle_Destroyed_Cloak(&cloak_state, 10, true, true, false);
    DOCTEST_CHECK(cloak_state == 1);

    cloak_state = 6;
    COMBAT1::Handle_Destroyed_Cloak(&cloak_state, 10, true, true, false);
    DOCTEST_CHECK(cloak_state == 2);

    cloak_state = 7;
    COMBAT1::Handle_Destroyed_Cloak(&cloak_state, 10, true, true, false);
    DOCTEST_CHECK(cloak_state == 3);

    cloak_state = 4;
    COMBAT1::Handle_Destroyed_Cloak(&cloak_state, 10, true, false, false);
    DOCTEST_CHECK(cloak_state == 6);
}

DOCTEST_TEST_CASE("combat damage storage uses the percentage of lost hit points") {
    DOCTEST_CHECK(COMBINIT::Damage_Percent_From_Hits(14, 10) == 28);
    DOCTEST_CHECK(COMBINIT::Damage_Percent_From_Hits(9, 6) == 33);
    DOCTEST_CHECK(COMBINIT::Damage_Percent_From_Hits(20, 13) == 35);
    DOCTEST_CHECK(COMBINIT::Damage_Percent_From_Hits(0, 0) == 0);
}

DOCTEST_TEST_CASE("combat damage storage reconstructs surviving fighter units") {
    DOCTEST_CHECK(COMBINIT::Surviving_Weapon_Count_From_Hits(7, 3) == 3);
    DOCTEST_CHECK(COMBINIT::Surviving_Weapon_Count_From_Hits(6, 3) == 2);
    DOCTEST_CHECK(COMBINIT::Surviving_Weapon_Count_From_Hits(99, 1) == 99);
    DOCTEST_CHECK(COMBINIT::Surviving_Weapon_Count_From_Hits(128, 1) == 128);
    DOCTEST_CHECK(COMBINIT::Surviving_Weapon_Count_From_Hits(0, 3) == 0);
    DOCTEST_CHECK(COMBINIT::Surviving_Weapon_Count_From_Hits(7, 0) == 0);
}

DOCTEST_TEST_CASE("simulated rotation restores missile bearing") {
    TEST_MISSILE_BEARING::Reset_();

    int16_t target_facing = 1;
    CMBTMOV1::Rotate_Ship_Until_Impact(0, &target_facing);

    DOCTEST_CHECK((MOX::missile[0].x == 100));
    DOCTEST_CHECK((MOX::missile[0].y == 50));
    DOCTEST_CHECK((MOX::missile[0].travel_dist == 60));
    DOCTEST_CHECK(MOX::missile[0].facing_dir == 3);
}

DOCTEST_TEST_CASE("missile threat uses the requested missile") {
    TEST_EXPECTED_DAMAGE::Reset_();
    MOX::ordnance_bonus[0] = 100;
    MOX::combat_data[1].shield_current_hits = 1;
    TECHDATA::shields[0].shield_class = 10;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 5;
    TECHDATA::weapons[WEAPON_MERCULITE_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_MERCULITE_MISSILE].max_damage = 20;
    MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
    MOX::missile[0].owner = 0;
    MOX::missile[1].type = WEAPON_MERCULITE_MISSILE;
    MOX::missile[1].owner = 0;

    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 1) == 1);
}

DOCTEST_TEST_CASE("overloaded launched missile can penetrate shields") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 10;
    TECHDATA::shields[0].shield_class = 12;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;
    MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
    MOX::missile[0].owner = 0;
    MOX::missile[0].specials = WEAPON_MOD_FLAG_OVERLOADED;
    MOX::ordnance_bonus[0] = 100;
    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 1);
}

DOCTEST_TEST_CASE("Hard Shields stop inherent Shield Piercing in expected damage") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].natural_mods =
        WEAPON_NATURAL_FLAG_IGNORES_SHIELDS;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 12;
    TECHDATA::shields[0].shield_class = 10;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;
    MOX::ordnance_bonus[10] = 100;
    has_hard_shields = true;

    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      1, WEAPON_NUCLEAR_MISSILE, 1, 0, 10) == 0);

    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].natural_mods = 0;
    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      1,
                      WEAPON_NUCLEAR_MISSILE,
                      1,
                      WEAPON_MOD_FLAG_SHIELD_PIERCING,
                      10) == 0);
}

DOCTEST_TEST_CASE("armor-bypassing beams have no expected planetary damage") {
    TEST_EXPECTED_DAMAGE::Reset_();
    best_ship_shield = 0;
    MOX::combat_data[0].owner = 0;
    MOX::ordnance_bonus[0] = 100;
    MOX::combat_data[0].weapons[0].weapon_id = WEAPON_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 10;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;

    DOCTEST_CHECK(CMBTAI::Expected_Beam_Bomb_Damage(
                      0, 0, 0, 1, 1, TARGET_TYPE_SHIP) > 0);

    TECHDATA::weapons[WEAPON_LASER_CANNON].natural_mods =
        WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE;
    DOCTEST_CHECK(CMBTAI::Expected_Beam_Bomb_Damage(
                      0, 0, 0, 1, 1, TARGET_TYPE_SHIP) == 0);
}

DOCTEST_TEST_CASE("expected beam damage retains configured unsigned words") {
    TEST_EXPECTED_DAMAGE::Reset_();
    MOX::combat_data[0].owner = 0;
    MOX::ordnance_bonus[0] = 100;
    MOX::combat_data[0].weapons[0].weapon_id = WEAPON_LASER_CANNON;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = UINT16_MAX;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = UINT16_MAX;

    DOCTEST_CHECK(CMBTAI::Expected_Beam_Bomb_Damage(
                      0, 0, 1, 1, 1, TARGET_TYPE_SHIP) == UINT16_MAX);
}

DOCTEST_TEST_CASE("only the three standard torpedoes are unshootable") {
    TEST_EXPECTED_DAMAGE::Reset_();
    const int16_t unshootable_torpedoes[] = {
        WEAPON_ANTI_MATTER_TORP,
        WEAPON_PROTON_TORPEDO,
        WEAPON_PLASMA_TORPEDO,
    };
    MOX::ordnance_bonus[0] = 100;

    for (const int16_t weapon_id : unshootable_torpedoes) {
        TECHDATA::weapons[weapon_id].type = WEAPON_TYPE_TORPEDO;
        TECHDATA::weapons[weapon_id].max_damage = 300;
        MOX::missile[0].type = weapon_id;
        MOX::missile[0].owner = 0;

        DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 0);
    }
}

DOCTEST_TEST_CASE("Dragon Breath remains a shootable torpedo threat") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_DRAGON_BREATH].type = WEAPON_TYPE_TORPEDO;
    TECHDATA::weapons[WEAPON_DRAGON_BREATH].max_damage = 300;
    MOX::missile[0].type = WEAPON_DRAGON_BREATH;
    MOX::missile[0].owner = 0;
    MOX::ordnance_bonus[0] = 100;

    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 1);
}

DOCTEST_TEST_CASE("fighter threat follows the maximum damage clip setting") {
    struct s_case {
        uint8_t clip_damage;
        int16_t expected_threat;
    };
    const s_case cases[] = {
        {0, 1},
        {1, 0},
    };

    for (const s_case& value : cases) {
        TEST_EXPECTED_DAMAGE::Reset_();
        CMBTFIRE::fighter_beam_no_plus_one = value.clip_damage;
        TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
        TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 10;
        TECHDATA::shields[0].shield_class = 10;
        MOX::combat_data[1].shield_type = 0;
        MOX::combat_data[1].shield_current_hits = 1;
        MOX::fighter_beam_type[0] = WEAPON_LASER_CANNON;
        MOX::missile[0].type = WEAPON_INTERCEPTOR;
        MOX::missile[0].owner = 0;
        MOX::ordnance_bonus[0] = 100;

        DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) ==
            value.expected_threat);
    }
}

DOCTEST_TEST_CASE("fighter threat applies point defense before ordnance") {
    TEST_EXPECTED_DAMAGE::Reset_();
    CMBTFIRE::fighters_use_point_defense = 1;
    CMBTFIRE::fighter_beam_no_plus_one = 1;
    CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].pd = 50;
    TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 3;
    TECHDATA::shields[0].shield_class = 2;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;
    MOX::fighter_beam_type[0] = WEAPON_LASER_CANNON;
    MOX::missile[0].type = WEAPON_INTERCEPTOR;
    MOX::missile[0].owner = 0;
    MOX::ordnance_bonus[0] = 150;

    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 1);
}

DOCTEST_TEST_CASE("fighter expected damage retains the 1.50 absorber exception") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_BOMBER].type = WEAPON_TYPE_FIGHTER;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].max_damage = 20;
    MOX::fighter_bomb_type[0] = WEAPON_NUCLEAR_BOMB;
    MOX::ordnance_bonus[0] = 100;
    has_energy_absorber = true;

    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      1, WEAPON_BOMBER, 1, 0, 0) == 40);
}

DOCTEST_TEST_CASE("fighter expected damage retains the 1.50 Shield Piercing exception") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_BOMBER].type = WEAPON_TYPE_FIGHTER;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].max_damage = 5;
    TECHDATA::shields[0].shield_class = 10;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;
    MOX::fighter_bomb_type[0] = WEAPON_NUCLEAR_BOMB;
    MOX::ordnance_bonus[0] = 100;

    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      1,
                      WEAPON_BOMBER,
                      1,
                      WEAPON_MOD_FLAG_SHIELD_PIERCING,
                      0) == 0);
}

DOCTEST_TEST_CASE("1.50 expected damage keeps Enveloping against planets") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 10;
    MOX::ordnance_bonus[10] = 100;
    CMBTFIRE::planet_non_enveloping = 1;

    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      0,
                      WEAPON_NUCLEAR_MISSILE,
                      1,
                      WEAPON_MOD_FLAG_ENVELOPING,
                      10) == 40);
}

DOCTEST_TEST_CASE("cloak prediction applies the configured missile miss percentage") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 100;
    MOX::combat_data[1].special_status_flag = 1;
    MOX::ordnance_bonus[10] = 100;
    CMBTMIS::missile_defense.cloaking_device_missile_miss_percent = 30;

    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      1, WEAPON_NUCLEAR_MISSILE, 1, 0, 10) == 70);
}

DOCTEST_TEST_CASE("1.50 special expected damage retains negative estimates") {
    TEST_EXPECTED_DAMAGE::Reset_();
    target_is_legal = true;
    TECHDATA::weapons[WEAPON_PLASMA_WEB].type = WEAPON_TYPE_SPECIAL;
    TECHDATA::weapons[WEAPON_PLASMA_WEB].min_damage = 5;
    TECHDATA::weapons[WEAPON_PLASMA_WEB].max_damage = 5;
    TECHDATA::shields[0].shield_class = 10;
    MOX::combat_data[0].weapons[0].weapon_id = WEAPON_PLASMA_WEB;
    MOX::combat_data[0].weapons[0].count = 1;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;

    DOCTEST_CHECK(CMBTAI::Expected_Weapon_Damage(
                      0, 0, 1, TARGET_TYPE_SHIP, 0) == -20);
}

DOCTEST_TEST_CASE("nebula disables ordinary shields in missile prediction") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 10;
    TECHDATA::shields[0].shield_class = 15;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;
    MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
    MOX::missile[0].owner = 0;
    MOX::missile[0].specials = 0;
    MOX::ordnance_bonus[0] = 100;
    MOX::in_nebula_flag = 1;

    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 1);

    has_hard_shields = true;
    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 0);
}

DOCTEST_TEST_CASE("expected missile damage applies absorber before MIRV") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 10;
    MOX::combat_data[1].shield_current_hits = 0;
    MOX::combat_data[1].tac_val_evade = 0;
    MOX::ordnance_bonus[10] = 100;
    has_energy_absorber = true;
    DOCTEST_CHECK(CMBTAI::Expected_Missile_Damage(
                      1,
                      WEAPON_NUCLEAR_MISSILE,
                      1,
                      WEAPON_MOD_FLAG_MIRV,
                      10) == 32);
}

DOCTEST_TEST_CASE("heavy fighter threat checks each beam and bomb payload") {
    TEST_EXPECTED_DAMAGE::Reset_();
    TECHDATA::weapons[WEAPON_HEAVY_FIGHTER].type = WEAPON_TYPE_FIGHTER;
    TECHDATA::weapons[WEAPON_LASER_CANNON].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 5;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].type = WEAPON_TYPE_BOMB;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].max_damage = 20;
    TECHDATA::shields[0].shield_class = 10;
    MOX::combat_data[1].shield_type = 0;
    MOX::combat_data[1].shield_current_hits = 1;
    MOX::fighter_beam_type[0] = WEAPON_LASER_CANNON;
    MOX::fighter_bomb_type[0] = WEAPON_NUCLEAR_BOMB;
    MOX::missile[0].type = WEAPON_HEAVY_FIGHTER;
    MOX::missile[0].owner = 0;
    MOX::ordnance_bonus[0] = 100;
    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 1);

    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 20;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].max_damage = 5;
    DOCTEST_CHECK(CMBTAI::Will_Missile_Do_Damage(1, 0) == 1);
}

DOCTEST_TEST_CASE("simulated movement restores missile bearing") {
    TEST_MISSILE_BEARING::Reset_();

    int16_t destination_x = 40;
    int16_t destination_y = 0;
    int16_t destination_facing = 0;
    CMBTMOV1::Test_Move_Until_Impact(
        0, &destination_x, &destination_y, &destination_facing);

    DOCTEST_CHECK((MOX::missile[0].x == 100));
    DOCTEST_CHECK((MOX::missile[0].y == 50));
    DOCTEST_CHECK((MOX::missile[0].travel_dist == 60));
    DOCTEST_CHECK(MOX::missile[0].facing_dir == 3);
}