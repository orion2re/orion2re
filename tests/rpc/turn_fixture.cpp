#include "turn_fixture.h"
#include "game/tactical_view.h"
#include "game/boarding.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace TEST_TACTICAL_TURN {
    namespace {
        s_combat_data units[MAX_COMBAT_SHIPS];
        s_player players[MAX_PLAYERS];
        s_ship_data ships[MAX_SHIPS];
        s_missile missiles[MAX_COMBAT_FLYERS];
        s_mouse_list mouse_grid[COMBAT_MOUSE_COUNT];
        s_animation_header button;
        char laser_name[] = "Laser Cannons";
        char missile_name[] = "Nuclear Missile";
        char torpedo_name[] = "Anti-Matter Torpedo";
        char fighter_name[] = "Interceptor";
        std::filesystem::path media_root;
        uint8_t glyph[256];
        std::string boarding_button;

        void Write_Media(const std::string& path, const std::string& bytes) {
            auto file = media_root / "data" / path;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream out(file, std::ios::binary);
            out.write(bytes.data(), std::streamsize(bytes.size()));
        }
        void Boarding_Media() {
            if (!media_root.empty()) return;
            media_root = std::filesystem::temp_directory_path() / ("orion-boarding-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            // Synthetic media only: transparent one-pixel frames and blank glyphs.
            std::string sprite(1024, '\0');
            auto* header = reinterpret_cast<s_animation_header*>(sprite.data());
            header->width = header->height = 1;
            header->frame_count = 2;
            header->flags = ANIMATION_FLAG_HAS_PALETTE;
            auto* palette = Animation_Palette_Header_(header);
            palette->count = 1;
            for (int i = 0; i < 3; ++i) Set_Animation_Frame_Offset_(header, i, 1000);
            unaligned::Store<uint16_t>(sprite.data() + 1002, 1); // Skip the sole row.
            boarding_button = sprite;
            for (int i = 0; i < 120; ++i) {
                char entry[4];
                snprintf(entry, sizeof(entry), "%03d", i);
                Write_Media(std::string("COMBAT.LBX/") + entry, sprite);
                if (i < 4) Write_Media(std::string("TEXTBOX.LBX/") + entry, sprite);
            }
            std::string messages(4 + 100, '\0');
            unaligned::Store<uint16_t>(messages.data(), 1);
            unaligned::Store<uint16_t>(messages.data() + 2, 100);
            for (int i = 0; i < 600; ++i) {
                char entry[4];
                snprintf(entry, sizeof(entry), "%03d", i);
                snprintf(messages.data() + 4, 100, "Synthetic message %d", i / 6);
                Write_Media(std::string("KENTEXT.LBX/") + entry, messages);
                Write_Media(std::string("KENTEXT1.LBX/") + entry, messages);
            }
            if (!vfs::Init(media_root.string().c_str())) std::abort();
            MOX::screen_seg = (s_animation_header*)allocate::Allocate_Space(allocate::Global_Screen_Space());
            MOX::global_cache_seg = buffer::Allocate_Buffer_Space(0x174508);
            std::fill_n(glyph, sizeof(glyph), uint8_t(0x80));
            fonts::font_data = glyph;
            fonts::font_header = {};
            for (int i = 0; i < 6; ++i) {
                fonts::font_header.font_heights[i] = 1;
                fonts::font_header.font_spacing[i] = 1;
                std::fill_n(fonts::font_header.font_glyph_widths[i], 256, uint8_t(1));
            }
            video::g_screen_res_x = 640;
            video::g_screen_res_y = 480;
            video::screen_pixel_size = 640 * 480;
            video::Allocate_Page_Buffers(640, 480);
            video::Allocate_Refresh_Lists(640 * 480, 480);
            graphics::Reset_Window();
            video::Set_Page_Off();
            remap::Sort_Palette_List();
        }
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
            unit.officer_idx = -1;
            ships[i].officer_index = -1;
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
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].name = missile_name;
        TECHDATA::weapons[WEAPON_ANTI_MATTER_TORP].type = WEAPON_TYPE_TORPEDO;
        TECHDATA::weapons[WEAPON_ANTI_MATTER_TORP].name = torpedo_name;
        TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
        TECHDATA::weapons[WEAPON_INTERCEPTOR].name = fighter_name;
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
    void Boarding(bool reports) {
        Boarding_Media();
        auto* button = reinterpret_cast<s_animation_header*>(boarding_button.data());
        MOX::weapons_button_seg = MOX::specials_button_seg = MOX::auto_button_seg = button;
        MOX::scan_button_seg = MOX::board_button_seg = MOX::flee_button_seg = button;
        MOX::wait_button_seg = MOX::cmbt_done_button_seg = MOX::cmbt_info_button_seg = button;
        auto& actor = units[1];
        auto& target = units[2];
        actor.marine_count = 12;
        target.marine_count = 2;
        target.combat_speed_base = 0;
        target.current_speed = 0;
        actor.crew_quality = 1;
        MOX::crew_data[1].ground = 200;
        BOARDING::boarding = {3, 0, 50, 2};
        BOARDING::board_range = {3, 12};
        BOARDING::board_in_black_hole = 1;
        BOARDING::kill_excess_marines_after_capture = 0;
        BOARDING::telepathic_multiple_capture = 0;
        // Native headless presentation setting; resolution/RNG and textbox owners are real.
        MOX::apply_damage_to_planet = 0;
        MOX::super_fast_flag = reports ? 0 : 1;
        fields::Deactivate_Auto_Function();
        COMBAT1::Assign_Combat_Grids(1);
        tactical_control::Invalidate();
    }
    void Release_Boarding() {
        if (media_root.empty()) return;
        fields::Deactivate_Auto_Function();
        buffer::Free_Buffer_Space();
        std::free((allocate::s_alloc_header*)MOX::screen_seg - 1);
        MOX::screen_seg = nullptr;
        MOX::global_cache_seg = nullptr;
        video::Free_Page_Buffers();
        std::free((allocate::s_alloc_header*)video::refresh_scan_list - 1);
        std::free((allocate::s_alloc_header*)video::refresh_stencil_list - 1);
        video::refresh_scan_list = video::refresh_stencil_list = nullptr;
        vfs::Shutdown();
        std::filesystem::remove_all(media_root);
        media_root.clear();
    }
    void Apply(const tactical_control::s_operation* operation) {
        int16_t next = 0;
        // Done exhausts this fixture's pass; no assets are loaded for next-actor drawing.
        if (operation->kind == tactical_control::DONE) MOX::super_fast_flag = 0;
        COMBAT1::Apply_Tactical_Operation(operation, 0, &next);
        if (next) tactical_events::Select_Unit(-1);
    }
    void Add_Missile(int16_t slot, int16_t weapon, int16_t owner) {
        CMBTMIS::Kill_Missile(slot);
        auto& missile = MOX::missile[slot];
        missile = {};
        missile.type = weapon;
        missile.owner = int8_t(owner);
        missile.quantity = 6;
        missile.source_unit_idx = owner == 1 ? 2 : 1;
        missile.target_unit_idx = owner == 1 ? 1 : 2;
        missile.x = 280;
        missile.y = 200;
        missile.speed = 3;
        missile.travel_dist = 100;
        missile.is_active = 1;
        tactical_view::Missile_Changed(slot);
    }
} // namespace TEST_TACTICAL_TURN
