#include "pch.h"

#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "game/build_queue.h"
#include "game/serialization.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_network.h"

namespace MAINSCR {
    constexpr uint32_t k_max_game_data_payload_bytes = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;

    namespace {
        bool Commit_Pending_Lua_Setup(
            int16_t sender_net_id,
            orion2re::lua::e_lua_network_setup_kind expected_kind,
            const void* payload,
            uint32_t payload_len,
            uint32_t prefix_size) {
            if (sender_net_id != static_cast<int16_t>(netcode::Net_Session_Owner_ID()) ||
                !orion2re::lua::Has_Pending_Lua_Network_Setup()) {
                orion2re::lua::Discard_Pending_Lua_Network_Setup();
                return false;
            }
            std::string diagnostic;
            if (!orion2re::lua::Commit_Lua_Network_Setup(expected_kind,
                    payload,
                    payload_len,
                    prefix_size,
                    &diagnostic)) {
                exitm::Exit_With_Message(diagnostic.c_str());
            }
            return true;
        }

        bool Valid_Setup_Version(const char* version, size_t capacity) {
            const void* terminator = memchr(version, '\0', capacity);
            return terminator != nullptr && strcmp(version, GAME_BUILD_DATE) == 0;
        }

        void Apply_Setup_Config(const void* payload, uint32_t payload_len, uint32_t prefix_size) {
            if (payload == nullptr || payload_len <= prefix_size) {
                exitm::Exit_With_Message("network configuration packet missing");
            }
            std::string lua_diagnostic;
            if (!orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
                    orion2re::lua::Get_Active_Mod_Runtimes(),
                    &lua_diagnostic)) {
                exitm::Exit_With_Message(lua_diagnostic.c_str());
            }
            const uint8_t* snapshot = static_cast<const uint8_t*>(payload) + prefix_size;
            const uint32_t snapshot_size = payload_len - prefix_size;
            SERIAL::Reader_ reader(
                snapshot, snapshot_size, "network configuration packet truncated");
            if (!config::Apply_Config_Snapshot(
                    &reader, &config::synchronized_config_snapshot) ||
                reader.Length() != snapshot_size) {
                exitm::Exit_With_Message("incompatible network configuration");
            }
        }

        int16_t Scaled_Star_Map_Dimension(int16_t dimension) {
            return MAP_SCALE::Scale_Star_Dimension_(dimension, HAROLD::Map_Star_Scale_Percent());
        }

        bool Is_Extended_Max_Map_View() {
            return MAP_SCALE::Is_Extended_Max_Map_View_(
                MOX::NUM_STARS,
                ORIGINAL_MAX_STARS,
                MOX::cur_map_scale,
                MOX::max_map_scale);
        }

        bool Use_Classic_Maximum_Galaxy_Display() {
            return maximum_galaxy_classic_display &&
                MOX::settings.galaxy_size == GALAXY_SIZE_MAXIMUM &&
                Is_Extended_Max_Map_View();
        }

        bool Use_Main_Map_Overview_Markers() {
            return Use_Maximum_Galaxy_Overview() && MAP_SCALE::Use_Main_Map_Overview_Markers_(
                main_map_overview_markers,
                MOX::settings.galaxy_size == GALAXY_SIZE_MAXIMUM,
                MOX::NUM_STARS,
                ORIGINAL_MAX_STARS,
                MOX::cur_map_scale,
                MOX::max_map_scale);
        }

        void Draw_Overview_Star_Marker(
            int16_t center_x,
            int16_t center_y,
            uint16_t color_idx,
            bool use_visited_neutral_color) {
            s_animation_header* marker = MOX::star_colors_seg[color_idx];
            const int16_t draw_x = center_x - animate::Get_Width(marker) / 2;
            const int16_t draw_y = center_y - animate::Get_Height(marker) / 2;
            if (!use_visited_neutral_color) {
                animate::Draw(draw_x, draw_y, marker);
            } else {
                uint8_t saved_remap[256];
                memcpy(saved_remap, remap::picture_remap_color_list, sizeof(saved_remap));
                for (int16_t color = 0; color < 256; ++color) {
                    remap::picture_remap_color_list[color] = static_cast<uint8_t>(color);
                }

                constexpr uint8_t neutral_source_colors[] = {66, 67, 68, 69, 71, 72, 30, 31};
                constexpr uint8_t visited_grey_colors[] = {13, 16, 18, 20, 24, 26, 30, 31};
                for (size_t color = 0; color < sizeof(neutral_source_colors); ++color) {
                    remap::picture_remap_color_list[neutral_source_colors[color]] =
                        visited_grey_colors[color];
                }

                animate::Remap_Draw(draw_x, draw_y, marker);
                memcpy(remap::picture_remap_color_list, saved_remap, sizeof(saved_remap));
            }
        }

        void Draw_Visited_Uncolonized_Star_Marker(
            int16_t center_x,
            int16_t center_y,
            int16_t star_height) {
            const uint8_t player_color = static_cast<uint8_t>(
                MOX::player[MOX::PLAYER_NUM].color);
            if (player_color < MAX_PLAYERS) {
                const uint8_t palette_base = MOX::main_palette_player_colors[player_color];
                const uint8_t dot_color = palette_base == 32
                    ? 22
                    : static_cast<uint8_t>(palette_base + 5);
                const int16_t dot_x = center_x - 1;
                const int16_t dot_y = center_y + star_height / 2 + 1;
                graphics::Dot(dot_x, dot_y, dot_color);
                graphics::Dot(dot_x + 1, dot_y, dot_color);
                graphics::Dot(dot_x, dot_y + 1, dot_color);
                graphics::Dot(dot_x + 1, dot_y + 1, dot_color);
            }
        }

        void Draw_Scaled_Star_Picture(
            int16_t center_x,
            int16_t center_y,
            int16_t unscaled_dimension,
            s_animation_header* picture) {
            int16_t scale_percent = HAROLD::Map_Star_Scale_Percent();
            if (scale_percent >= 100) {
                animate::Draw(center_x - unscaled_dimension / 2, center_y - unscaled_dimension / 2, picture);
                return;
            }

            allocate::Push_Block(MOX::screen_seg);
            uint32_t source_size = bitmap::Get_Bitmap_Size(picture->width, picture->height);
            s_animation_header* source_bitmap = static_cast<s_animation_header*>(
                allocate::Allocate_Next_Block(MOX::screen_seg, source_size));
            s_animation_header* scaled_bitmap = static_cast<s_animation_header*>(
                allocate::Allocate_Next_Block(MOX::screen_seg, source_size));

            bitmap::Draw_Picture_To_Bitmap(source_bitmap, picture);
            bitmap::Scale_Bitmap(scaled_bitmap, source_bitmap, scale_percent, scale_percent);
            animate::Draw(center_x - scaled_bitmap->width / 2, center_y - scaled_bitmap->height / 2, scaled_bitmap);

            allocate::Pop_Block(MOX::screen_seg);
        }
    } // namespace

    int16_t zoom_in_button;
    int16_t zoom_out_button;
    int16_t whole_screen_field;
    int16_t game_button;
    int16_t next_button;
    int16_t design_button;
    int16_t colonies_button;
    int16_t planets_button;
    int16_t fleets_button;
    int16_t leaders_button;
    int16_t races_button;
    int16_t info_button;
    int16_t treasury_window_field;
    int16_t command_points_window_field;
    int16_t food_window_field;
    int16_t freighters_window_field;
    int16_t research_window_field;
    int16_t first_field;
    uint8_t debug_enabled;
    int16_t debug_field;
    int16_t exit_game_hot_key;
    int16_t influence_hot_key = -1000;
    uint8_t influence_view_mode = 0;
    int16_t main_screen_grid_y;
    int16_t main_screen_grid_x;
    s_animation_header* temp_bitmap;
    int16_t main_screen_scanned_star;
    int8_t moving_a_stack;
    int8_t track_time_flag;
    uint8_t wormhole_link_color = 4;
    int8_t clear_fleet_fields;
    int8_t mouse_btn_pressed;
    int8_t merging_relocations;
    s_mouse_list mouse_list_relocation[2] = {
        {2, 0, 0, 0, 639, 479},
        {18, 0, 22, 22, 526, 420},
    };
    uint8_t warn_about_cheaters;
    uint8_t zoom_box_counter;
    uint8_t zoom_box_color_increment;
    uint8_t zoom_box_increment_value;
    int8_t officer_screen_return_screen;
    int8_t next_turn_hit;
    int16_t zoom_box_x;
    int16_t zoom_box_y;
    int8_t fleet_ship_selection_status;
    int16_t fleet_icon_chosen;

    s_hotseat_system_box_state hotseat_system_box[MAX_PLAYERS];

    uint8_t hotseat_map_scale[MAX_PLAYERS];

    s_hotseat_fleet_box_state hotseat_fleet_box[MAX_PLAYERS];

    int8_t star_name_highlight_direction = 1;

    uint8_t star_name_highlight_value;

    uint8_t check_next_turn_exit_field;
    int16_t n_twinkling_stars;
    int16_t main_screen_ship_icon_scanned;
    s_colors ship_direction_line_green_colors = {{0x6E, 0x6F, 0x72, 0x74, 0x74, 0x72, 0x6F, 0x6E}};
    s_colors ship_direction_line_red_colors = {{0x49, 0x4A, 0x4D, 0x4F, 0x4F, 0x4D, 0x4A, 0x49}};
    uint8_t fltpop_field_selection_status;
    int8_t diplo_lights_anim_frame[MAX_PLAYERS];
    s_animation_header* player_lights_seg[MAX_PLAYERS];

    uint8_t already_faded_into_main_screen;
    uint8_t in_auto_next_turn_mode;

    int16_t omniscient_hot_key = -1000;
    int16_t all_tech_cheat = -1000;
    int16_t leader_cheat = -1000;
    int16_t more_money_cheat = -1000;
    int16_t get_tech_cheat = -1000;
    int16_t omniscience_cheat = -1000;
    int16_t all_ai_cheat = -1000;
    int16_t hi_score_cheat = -1000;
    int16_t cycle_player_cheat = -1000;
    int16_t reload_config_command = -1000;
    int16_t reload_interface_command = -1000;
    int16_t last_field;

    bool __cdecl Is_Main_Screen_Palette() {
        uint8_t screen = (uint8_t)MOX::previous_screen;

        if (screen == SCREEN_MAIN ||
            screen == SCREEN_UNK2 ||
            screen == SCREEN_COLONIZATION_IN_MAIN ||
            screen == SCREEN_BUILDING_OUTPOSTS_IN_MAIN ||
            screen == SCREEN_SCRAP_FREIGHTERS ||
            screen == SCREEN_DISTANCE_BETWEEN_STARS ||
            screen == SCREEN_TECH_CHANGE ||
            screen == SCREEN_GAME ||
            screen == SCREEN_REPORTS ||
            screen == SCREEN_SHOW_COMMAND_POINTS ||
            screen == SCREEN_TURN_SUMMARY ||
            screen == SCREEN_MAIN_NEXT_TURN) {
            return true;
        }

        return false;
    }

    bool Use_Maximum_Galaxy_Overview() {
        return Is_Extended_Max_Map_View() && !Use_Classic_Maximum_Galaxy_Display();
    }

    void __cdecl Draw_Stars() {
        n_twinkling_stars = 0;

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if ((Use_Main_Map_Overview_Markers() ||
                    MOX::star[star_idx].spectral_class != STAR_CLASS_BLACK_HOLE) &&
                Star_On_Screen(star_idx)) {
                Draw_A_Star(star_idx);
            }
        }
    }

    void __cdecl Draw_Influence_Overlay() {
        const bool draw_automatic_outline = Use_Maximum_Galaxy_Overview();
        if (influence_view_mode == 0 && !draw_automatic_outline) {
            return;
        }

        constexpr int16_t map_left = 22;
        constexpr int16_t map_top = 22;
        constexpr int16_t map_width = 506;
        constexpr int16_t map_height = 400;
        struct s_influence_source {
            int16_t center_x;
            int16_t center_y;
            int16_t radius;
            int16_t owner;
        };

        static uint8_t influence_owners[map_height][map_width];
        static s_influence_source cached_sources[MAX_STARS];
        static int16_t cached_star_count = -1;
        s_influence_source sources[MAX_STARS] = {};

        const bool viewer_is_omniscient = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);
        int16_t max_non_thorium_range = 0;
        for (int16_t fuel_idx = FUEL_NO_FUEL_CELLS; fuel_idx < FUEL_THORIUM_FUEL_CELLS; ++fuel_idx) {
            if (TECHDATA::fuel[fuel_idx].ship_range > max_non_thorium_range) {
                max_non_thorium_range = TECHDATA::fuel[fuel_idx].ship_range;
            }
        }

        for (int16_t star_idx = 0; star_idx < MAX_STARS; ++star_idx) {
            sources[star_idx].owner = -1;
        }

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            const int16_t owner = MOX::star[star_idx].owner;
            if (owner < 0 || owner >= MOX::NUM_PLAYERS || owner >= MAX_PLAYERS) {
                continue;
            }

            const bool source_is_known =
                viewer_is_omniscient ||
                HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx) ||
                HAROLD::Contact_With_One_Colony(star_idx);
            if (!source_is_known ||
                (owner != MOX::PLAYER_NUM && !HAROLD::Contact_With_Player(MOX::PLAYER_NUM, owner))) {
                continue;
            }

            s_influence_source& source = sources[star_idx];
            const bool cap_thorium_range =
                draw_automatic_outline &&
                MOX::player[owner].tech_applications[TECH_APP_THORIUM_FUEL_CELLS] ==
                    TECH_RESEARCH_STATUS_RESEARCHED;
            const int16_t influence_range = MAP_SCALE::Influence_Overlay_Range_(
                MOX::player[owner].ship_range,
                max_non_thorium_range,
                cap_thorium_range);
            source.radius = MAP_SCALE::Influence_Overlay_Screen_Radius_(
                MAP_SCALE::Influence_Overlay_Map_Radius_(influence_range),
                MOX::cur_map_scale,
                HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx),
                influence_range <= 0);
            if (source.radius < 1) {
                continue;
            }
            source.owner = owner;
            Get_Star_Draw_Coords(star_idx, &source.center_x, &source.center_y);
        }

        bool cache_valid = cached_star_count == MOX::NUM_STARS;
        for (int16_t star_idx = 0; cache_valid && star_idx < MAX_STARS; ++star_idx) {
            const s_influence_source& cached = cached_sources[star_idx];
            const s_influence_source& current = sources[star_idx];
            cache_valid =
                cached.center_x == current.center_x &&
                cached.center_y == current.center_y &&
                cached.radius == current.radius &&
                cached.owner == current.owner;
        }

        if (!cache_valid) {
            memset(influence_owners, 0, sizeof(influence_owners));

            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
                const s_influence_source& source = sources[star_idx];
                if (source.owner < 0) {
                    continue;
                }

                int16_t left = source.center_x - source.radius;
                int16_t right = source.center_x + source.radius;
                int16_t top = source.center_y - source.radius;
                int16_t bottom = source.center_y + source.radius;
                if (left < map_left) left = map_left;
                if (right >= map_left + map_width) right = map_left + map_width - 1;
                if (top < map_top) top = map_top;
                if (bottom >= map_top + map_height) bottom = map_top + map_height - 1;

                const int32_t radius_squared = source.radius * source.radius;
                const uint8_t owner_bit = static_cast<uint8_t>(1u << source.owner);
                for (int16_t screen_y = top; screen_y <= bottom; ++screen_y) {
                    const int32_t dy = screen_y - source.center_y;
                    for (int16_t screen_x = left; screen_x <= right; ++screen_x) {
                        const int32_t dx = screen_x - source.center_x;
                        if (dx * dx + dy * dy <= radius_squared) {
                            influence_owners[screen_y - map_top][screen_x - map_left] |= owner_bit;
                        }
                    }
                }
            }

            memcpy(cached_sources, sources, sizeof(sources));
            cached_star_count = MOX::NUM_STARS;
        }

        if (influence_view_mode == 1 && !draw_automatic_outline) {
            for (int16_t y = 0; y < map_height; ++y) {
                for (int16_t x = 0; x < map_width; ++x) {
                    const uint8_t owners = influence_owners[y][x];
                    if (owners == 0) {
                        continue;
                    }

                    const int16_t fill_slot = static_cast<int16_t>((y % 2) * 2 + (x % 2));
                    int16_t owner = -1;
                    int16_t assigned_slots = 0;

                    const uint8_t current_player_bit = static_cast<uint8_t>(1u << MOX::PLAYER_NUM);
                    if ((owners & current_player_bit) != 0) {
                        if (fill_slot == assigned_slots) {
                            owner = MOX::PLAYER_NUM;
                        }
                        assigned_slots++;
                    }

                    for (int16_t candidate = 0;
                         candidate < MOX::NUM_PLAYERS && assigned_slots <= fill_slot;
                         ++candidate) {
                        if (candidate == MOX::PLAYER_NUM || (owners & (1u << candidate)) == 0) {
                            continue;
                        }
                        if (fill_slot == assigned_slots) {
                            owner = candidate;
                        }
                        assigned_slots++;
                    }

                    if (owner < 0) {
                        continue;
                    }

                    const uint8_t player_color = static_cast<uint8_t>(MOX::player[owner].color);
                    if (player_color >= MAX_PLAYERS) {
                        continue;
                    }

                    const uint8_t palette_base = MOX::main_palette_player_colors[player_color];
                    const uint8_t fill_color = (palette_base == 32) ? 8 : static_cast<uint8_t>(palette_base + 1);

                    graphics::Dot(
                        map_left + x,
                        map_top + y,
                        fill_color);
                }
            }
        }

        for (int16_t owner = 0; owner < MOX::NUM_PLAYERS && owner < MAX_PLAYERS; ++owner) {
            const uint8_t player_color = static_cast<uint8_t>(MOX::player[owner].color);
            if (player_color >= MAX_PLAYERS) {
                continue;
            }

            const uint8_t owner_bit = static_cast<uint8_t>(1u << owner);
            const uint8_t palette_base = MOX::main_palette_player_colors[player_color];
            const uint8_t border_color = (palette_base == 32) ? 22 : static_cast<uint8_t>(palette_base + 5);

            for (int16_t y = 0; y < map_height; ++y) {
                for (int16_t x = 0; x < map_width; ++x) {
                    if ((influence_owners[y][x] & owner_bit) == 0) {
                        continue;
                    }

                    const bool is_border =
                        x == 0 || (influence_owners[y][x - 1] & owner_bit) == 0 ||
                        x == map_width - 1 || (influence_owners[y][x + 1] & owner_bit) == 0 ||
                        y == 0 || (influence_owners[y - 1][x] & owner_bit) == 0 ||
                        y == map_height - 1 || (influence_owners[y + 1][x] & owner_bit) == 0;

                    if (is_border) {
                        graphics::Dot(map_left + x, map_top + y, border_color);
                    }
                }
            }
        }
    }

    void __cdecl Update_Star_Owners() {
        for (int16_t i = 0; i < MOX::NUM_STARS; i++) {
            int16_t owner_id = HAROLD::Star_Owner(i, MOX::PLAYER_NUM);
            MOX::star[i].owner = (int8_t)owner_id;
        }
    }

    void __cdecl Get_PointXY_Draw_Coords(int16_t x, int16_t y, int16_t* out_x, int16_t* out_y) {
        int16_t scaled_x = HAROLD::Get_Scaled_Value(static_cast<int16_t>(x - MOX::cur_map_x));
        *out_x = scaled_x + 0x15;

        int16_t scaled_y = HAROLD::Get_Scaled_Value(static_cast<int16_t>(y - MOX::cur_map_y));
        *out_y = scaled_y + 0x15;
    }

    s_animation_header* __cdecl Get_Star_Picture_Seg(int8_t spectral_class, int8_t pict_type, int8_t size) {
        int16_t zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
        int16_t class_offset = (int16_t)spectral_class * 6;

        int16_t result_ax = zoom_level;
        if (spectral_class != STAR_CLASS_BLACK_HOLE) {
            result_ax += (int16_t)size;
        }
        result_ax += class_offset;

        int32_t final_index = (int32_t)result_ax;

        MOX::star_seg[final_index] = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
            "BUFFER0.LBX",
            final_index + 148,
            MOX::global_cache_seg));

        return MOX::star_seg[final_index];
    }

    void __cdecl Get_Star_Draw_Coords(int16_t star_idx, int16_t* out_x, int16_t* out_y) {
        const s_star_data& star = MOX::star[star_idx];

        int16_t rel_x = star.x - MOX::cur_map_x;
        *out_x = HAROLD::Get_Scaled_Value(rel_x) + 0x15;

        int16_t rel_y = star.y - MOX::cur_map_y;
        *out_y = HAROLD::Get_Scaled_Value(rel_y) + 0x15;
    }

    bool __cdecl Star_On_Screen(int16_t star_idx) {
        int16_t x;
        int16_t y;

        Get_Star_Draw_Coords(star_idx, &x, &y);

        if (x > 0x15 && x < 0x20f && y > 0x15 && y < 0x1a5) {
            return true;
        }

        return false;
    }

    void Draw_Mini_Main_Screen() {
        if (MOX::at_least_one_tactical_combat != 0) {
            MOX::at_least_one_tactical_combat = 0;
            Fade_Into_Main_Screen();
        }

        uint16_t saved_scale = (uint16_t)MOX::cur_map_scale;
        uint16_t saved_zoom = MOX::zoom_count;
        uint16_t saved_x = (uint16_t)MOX::cur_map_x;
        uint16_t saved_y = (uint16_t)MOX::cur_map_y;

        if (MOX::dont_draw_main_screen_to_back == 0) {
            Draw_Main_Screen_Filled(1);
        } else {
            Draw_Main_Screen_Filled(0);
            MOX::dont_draw_main_screen_to_back = 0;
        }

        Draw_Main_Main_Screen();

        int32_t temp_val = (int32_t)MOX::multi_colored_line_start + 1;
        int32_t sign_mask = temp_val >> 31;
        int32_t abs_val = (temp_val ^ sign_mask) - sign_mask;
        int32_t wrapped_val = ((abs_val & 7) ^ sign_mask) - sign_mask;
        MOX::multi_colored_line_start = (int16_t)wrapped_val;

        MOX::cur_map_scale = (int16_t)saved_scale;
        MOX::zoom_count = saved_zoom;
        MOX::cur_map_x = (int16_t)saved_x;
        MOX::cur_map_y = (int16_t)saved_y;

        if (MOX::settings.end_of_turn_wait == 0) {
            if (check_next_turn_exit_field != 0) {
                if (mouse::Mouse_Button() != 0) {
                    MOX::auto_turn_exit = 1;
                }
            }
        }

        if (check_next_turn_exit_field != 0) {
            animate::Draw(0x220, 0x1B9, MOX::darkened_turn_button_seg);
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MOX::skip_fields = 1;
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }
    }

    void Draw_Mini_Main_Offscreen() {
        fields::Disable_Draw_All_Fields();
        video::Set_Page_Off();
        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();
    }

    void __cdecl Draw_A_Star(int16_t star_idx) {
        s_star_data* star = &MOX::star[star_idx];

        int16_t screen_x;
        int16_t screen_y;
        Get_Star_Draw_Coords(star_idx, &screen_x, &screen_y);

        const int16_t owner = star->owner;
        const bool visited =
            HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx) != 0;
        const bool draw_visited_uncolonized_marker =
            MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(
                Use_Maximum_Galaxy_Overview(),
                star->spectral_class == STAR_CLASS_BLACK_HOLE,
                owner,
                visited);

        if (Use_Main_Map_Overview_Markers()) {
            const bool has_visible_owner = owner >= 0 && owner < MAX_PLAYERS;
            const uint8_t owner_color = has_visible_owner
                ? static_cast<uint8_t>(MOX::player[owner].color)
                : 0;
            const uint16_t color_idx = MAP_SCALE::Overview_Star_Color_Index_(
                star->spectral_class == STAR_CLASS_BLACK_HOLE,
                false,
                owner,
                MAX_PLAYERS,
                owner_color);
            const bool use_visited_neutral_color =
                MAP_SCALE::Uses_Visited_Neutral_Overview_Color_(
                    star->spectral_class == STAR_CLASS_BLACK_HOLE,
                    owner,
                    MAX_PLAYERS,
                    visited);
            Draw_Overview_Star_Marker(
                screen_x,
                screen_y,
                color_idx,
                use_visited_neutral_color);
            if (draw_visited_uncolonized_marker) {
                Draw_Visited_Uncolonized_Star_Marker(
                    screen_x,
                    screen_y,
                    animate::Get_Height(MOX::star_colors_seg[color_idx]));
            }
            return;
        }

        int16_t zoom_size = HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx);
        s_animation_header* star_seg = Get_Star_Picture_Seg(star->spectral_class, star->pict_type, star->size);

        if (static_cast<int8_t>(star->twinkling1) < 0) {
            animate::Reset_Animation_Frame(star_seg);
        } else {
            int8_t twinkling1 = (int8_t)(star->twinkling1 + 1);
            star->twinkling1 = twinkling1;

            int16_t frame_count = animate::Get_Animation_Frame_Count(star_seg);
            int16_t current_frame = (int16_t)(((int16_t)twinkling1 / 2) % frame_count);

            if (static_cast<int8_t>(star->twinkling2) <= twinkling1) {
                int16_t remaining = n_twinkling_stars - 1;
                if (remaining < 1) {
                    remaining = 0;
                }
                n_twinkling_stars = remaining;

                star->twinkling1 = -1;
                star->twinkling2 = -1;
            }

            animate::Set_Animation_Frame(star_seg, current_frame);
        }

        Draw_Scaled_Star_Picture(screen_x, screen_y, zoom_size, star_seg);
        if (draw_visited_uncolonized_marker) {
            Draw_Visited_Uncolonized_Star_Marker(screen_x, screen_y, zoom_size);
        }
    }

    void __cdecl Print_Star_Names() {
        if (Use_Maximum_Galaxy_Overview()) {
            return;
        }

        int16_t window_status;
        int8_t star_type;
        s_animation_header* star_data;
        char star_name[32];

        star_type = 0;
        star_data = 0;

        MISC::Save_Window_Status(&window_status);
        graphics::Set_Window(0x16, 0x16, 0x20F, 0x1A5);
        video::Clip_On();

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (MOX::star[i].spectral_class != STAR_CLASS_BLACK_HOLE) {
                Get_Star_Name(i, 0, &star_type, star_name, sizeof(star_name), &star_data);

                if (star_name[0] != '\0') {
                    Print_Star_Name(i, star_type, star_name, star_data);
                }
            }
        }

        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_ETA_Destination_Line() {
        int16_t x, y;
        int16_t dest_x, dest_y;
        s_colors color_buffer;

        int16_t zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
        int32_t zoom_scale = 3 - (int32_t)zoom_level;
        if (zoom_scale < 0) {
            zoom_scale = 0;
        }

        int16_t ship_stack_node = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
        int16_t ship_icon_id = HACCESS::Ship_Icon_Id_From_Node_Ptr(ship_stack_node);

        SHIPS::Get_Ship_Icon_Coords(ship_icon_id, &x, &y);

        SHIPS::Get_Ship_Icon_Dimensions(zoom_scale, &MOX::ship_icon_width[zoom_scale], &MOX::ship_icon_height[zoom_scale]);

        x += (uint8_t)MOX::ship_icon_width[zoom_scale] / 2;
        y += (uint8_t)MOX::ship_icon_height[zoom_scale] / 2;

        Get_Star_Draw_Coords(MOX::eta_star_id, &dest_x, &dest_y);

        if (MOX::g_ship_move_info.moving != 0 && MOX::g_ship_move_info.turns_left != 0) {
            color_buffer = ship_direction_line_green_colors;

            SHIPS::Draw_Directional_Multi_Colored_Line(x, y, dest_x, dest_y, &color_buffer, MOX::multi_colored_line_start);
        } else if ((MOX::g_ship_move_info.blackhole_blocks != 0 || MOX::g_ship_move_info.hyperspace_flux != 0) ||
            (MOX::g_ship_move_info.out_of_range != 0 && MOX::g_ship_move_info.immobile == 0)) {
            color_buffer = ship_direction_line_red_colors;

            SHIPS::Draw_Directional_Multi_Colored_Line(x, y, dest_x, dest_y, &color_buffer, MOX::multi_colored_line_start);
        }
    }

    void __cdecl Draw_Main_Main_Screen() {
        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Wormhole_Links();
        RUSS::Mox_Update();

        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Relocation_Links();
        RUSS::Mox_Update();

        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Stars();
        RUSS::Mox_Update();

        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Gate_Icons();
        RUSS::Mox_Update();

        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Print_Star_Names();
        RUSS::Mox_Update();

        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Black_Holes();
        RUSS::Mox_Update();

        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Ship_Icons(true);
        RUSS::Mox_Update();

        graphics::Reset_Window();
        video::Clip_Off();
        Print_Main_Screen_Data();
        RUSS::Mox_Update();

        if (MOX::game_type != 0) {
            Draw_Diplomacy_Request_Lights();
            RUSS::Mox_Update();
        }
    }

    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        Update_Star_Owners();
        SHIPS::Build_Ship_Icons();
        SHIPS::Set_Ship_Icon_XYs();
    }

    void __cdecl Draw_Wormhole_Links() {
        int16_t window_status;
        MISC::Save_Window_Status(&window_status);

        graphics::Set_Window(0x16, 0x16, 0x20F, 0x1A5);
        video::Clip_On();

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            int16_t wormhole_id = MOX::star[star_idx].wormhole_star_id;
            if (wormhole_id != -1) {
                bool is_visible = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx);
                if (!is_visible) {
                    is_visible = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);
                }

                if (is_visible) {
                    int16_t x1, y1, x2, y2;
                    MAINSCR::Get_Star_Draw_Coords(star_idx, &x1, &y1);
                    MAINSCR::Get_Star_Draw_Coords(wormhole_id, &x2, &y2);
                    line::Line(x1, y1, x2, y2, wormhole_link_color);
                }
            }
        }

        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_Paralax() {
        int16_t window_status;
        MISC::Save_Window_Status(&window_status);

        // Define active window for parallax background
        graphics::Set_Window(22, 22, 527, 421); // 0x16, 0x16, 0x20F, 0x1A5
        video::Clip_On();

        // Far Stars - Layer 0
        MOX::far_star_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("STARBG.LBX", 0, MOX::global_cache_seg));
        animate::Draw(MOX::far_star_x, MOX::far_star_y, MOX::far_star_seg);
        animate::Draw(MOX::far_star_x - 640, MOX::far_star_y, MOX::far_star_seg);
        animate::Draw(MOX::far_star_x - 640, MOX::far_star_y - 480, MOX::far_star_seg);
        animate::Draw(MOX::far_star_x, MOX::far_star_y - 480, MOX::far_star_seg);

        // Mid Stars - Layer 1
        MOX::mid_star_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("STARBG.LBX", 1, MOX::global_cache_seg));
        animate::Draw(MOX::mid_star_x, MOX::mid_star_y, MOX::mid_star_seg);
        animate::Draw(MOX::mid_star_x - 640, MOX::mid_star_y, MOX::mid_star_seg);
        animate::Draw(MOX::mid_star_x - 640, MOX::mid_star_y - 480, MOX::mid_star_seg);
        animate::Draw(MOX::mid_star_x, MOX::mid_star_y - 480, MOX::mid_star_seg);

        // Near Stars - Layer 2
        MOX::near_star_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("STARBG.LBX", 2, MOX::global_cache_seg));
        animate::Draw(MOX::near_star_x, MOX::near_star_y, MOX::near_star_seg);
        animate::Draw(MOX::near_star_x - 640, MOX::near_star_y, MOX::near_star_seg);
        animate::Draw(MOX::near_star_x - 640, MOX::near_star_y - 480, MOX::near_star_seg);
        animate::Draw(MOX::near_star_x, MOX::near_star_y - 480, MOX::near_star_seg);

        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_Relocation_Links() {
        int16_t window_status;
        s_colors colors = {0x6e, 0x6f, 0x70, 0x70, 0x6e, 0x6f, 0x70, 0x70};

        MISC::Save_Window_Status(&window_status);
        graphics::Set_Window(0x16, 0x16, 0x20F, 0x1A5);
        video::Clip_On();

        if (MOX::settings.show_relocation_lines != 0) {
            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
                if (HACCESS::Star_Has_Relocation(star_idx, MOX::PLAYER_NUM)) {
                    int16_t x1, y1, x2, y2;
                    MAINSCR::Get_Star_Draw_Coords(star_idx, &x1, &y1);
                    int16_t dest_idx = HACCESS::Relocation(star_idx, MOX::PLAYER_NUM);
                    MAINSCR::Get_Star_Draw_Coords(dest_idx, &x2, &y2);
                    SHIPS::Draw_Directional_Multi_Colored_Line(x1, y1, x2, y2, &colors, MOX::multi_colored_line_start);
                }
            }
        }

        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_Black_Holes() {
        if (Use_Main_Map_Overview_Markers()) {
            return;
        }

        static const int16_t zoom_dist[4] = {0x27, 0x21, 0x21, 0x18};
        int16_t window_status;
        int16_t star_idx;
        int16_t black_hole_slot = 0;
        int16_t x, y;

        int16_t zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
        MISC::Save_Window_Status(&window_status);
        graphics::Set_Window(22, 22, 527, 421); // 0x16, 0x16, 0x20f, 0x1a5
        video::Clip_On();

        if (MOX::NUM_STARS > 0) {
            for (star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
                if (MOX::star[star_idx].spectral_class == STAR_CLASS_BLACK_HOLE) {
                    Get_Star_Draw_Coords(star_idx, &x, &y);

                    s_animation_header* seg = Get_Star_Picture_Seg(
                        MOX::star[star_idx].spectral_class,
                        MOX::star[star_idx].pict_type,
                        MOX::star[star_idx].size);

                    int16_t scale_percent = HAROLD::Map_Star_Scale_Percent();
                    if (Use_Classic_Maximum_Galaxy_Display() ||
                        MAP_SCALE::Advance_Black_Hole_Animation_(scale_percent)) {
                        int16_t slot = black_hole_slot;
                        int8_t count = MOX::black_hole_anim_count[slot];
                        int16_t frame_count = animate::Get_Animation_Frame_Count(seg);
                        int8_t new_count = (int8_t)((count + 1) % (frame_count * 2));
                        MOX::black_hole_anim_count[slot] = new_count;
                        animate::Set_Animation_Frame(seg, new_count / 2);
                    } else {
                        animate::Set_Animation_Frame(seg, 0);
                    }

                    Draw_Scaled_Star_Picture(x, y, zoom_dist[zoom_level], seg);

                    black_hole_slot++;
                }
            }
        }

        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_A_Gate_Icon(int16_t star_idx) {
        if (!HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx)) {
            return;
        }

        int16_t star_size = HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx);
        int16_t zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
        int16_t zoom_offset = (zoom_level == 0) ? 2 : 3;

        // These calls are in disassembly but return value is ignored.
        HAROLD::Player_Has_Stargate(MOX::PLAYER_NUM, star_idx);
        HAROLD::Player_Has_Jumpgate(MOX::PLAYER_NUM, star_idx);

        bool has_artemis = HAROLD::Player_Has_Artemis_Net(&MOX::player[MOX::PLAYER_NUM], &MOX::star[star_idx]);
        bool has_portal = HAROLD::Player_Has_Dimensional_Portal(&MOX::player[MOX::PLAYER_NUM], &MOX::star[star_idx]);

        allocate::Push_Block(MOX::screen_seg);

        s_animation_header* temp_icons[4];
        for (int i = 0; i < 4; ++i) {
            uint32_t size = bitmap::Get_Bitmap_Size(18, 18);
            temp_icons[i] = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, size);
            bitmap::Create_Blank_Bitmap(18, 18, &temp_icons[i]->width);
        }

        int8_t icon_count = 0;
        int16_t max_icon_height = 0;

        if (has_portal) {
            s_animation_header* portal_pic = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", zoom_offset + 19, MOX::global_cache_seg);
            MOX::gate_icon_seg[zoom_offset] = portal_pic;
            bitmap::Draw_Picture_To_Bitmap(temp_icons[0], portal_pic);
            int16_t h = animate::Get_Height(portal_pic);
            if (h > 0) {
                max_icon_height = h;
            }
            icon_count = 1;
        }

        if (has_artemis) {
            s_animation_header* artemis_pic = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", zoom_offset + 31, MOX::global_cache_seg);
            MOX::gate_icon_seg[12 + zoom_offset] = artemis_pic;
            bitmap::Draw_Picture_To_Bitmap(temp_icons[icon_count], artemis_pic);
            int16_t h = animate::Get_Height(artemis_pic);
            if (h > max_icon_height) {
                max_icon_height = h;
            }
            icon_count++;
        }

        if (icon_count != 0) {
            int16_t star_x, star_y;
            Get_Star_Draw_Coords(star_idx, &star_x, &star_y);

            int16_t draw_x, draw_y;
            int16_t off_x, off_y, act_w, act_h;
            int16_t y_base;

            switch (icon_count) {
                case 1:
                    bitmap::Get_Bitmap_Actual_Size(temp_icons[0], &off_x, &off_y, &act_w, &act_h);
                    draw_x = star_x - act_w - 4;
                    draw_y = (star_size / 2) - max_icon_height + star_y;
                    if (has_portal) {
                        draw_y += 2;
                    }
                    animate::Draw(draw_x, draw_y, temp_icons[0]);
                    break;

                case 2:
                    bitmap::Get_Bitmap_Actual_Size(temp_icons[0], &off_x, &off_y, &act_w, &act_h);
                    y_base = (star_size / 2) - max_icon_height;
                    draw_y = y_base + star_y;
                    if (has_portal) {
                        draw_y += 2;
                    }
                    animate::Draw(star_x - act_w - 4, draw_y, temp_icons[0]);

                    bitmap::Get_Bitmap_Actual_Size(temp_icons[1], &off_x, &off_y, &act_w, &act_h);
                    animate::Draw(star_x + 4, y_base + star_y, temp_icons[1]);
                    break;

                case 3:
                case 4: {
                    uint32_t size = bitmap::Get_Bitmap_Size(75, 20);
                    s_animation_header* composite = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, size);
                    bitmap::Create_Blank_Bitmap(75, 20, &composite->width);

                    int16_t cursor_x = 0;
                    for (int i = 0; i < icon_count; ++i) {
                        bitmap::Add_Picture_To_Bitmap(cursor_x, 0, temp_icons[i], composite);
                        bitmap::Get_Bitmap_Actual_Size(temp_icons[i], &off_x, &off_y, &act_w, &act_h);
                        cursor_x += act_w + 1;
                    }

                    bitmap::Get_Bitmap_Actual_Size(composite, &off_x, &off_y, &act_w, &act_h);
                    draw_x = star_x - (act_w / 2);
                    draw_y = (star_size / 2) - max_icon_height + star_y;
                    animate::Draw(draw_x, draw_y, composite);
                    break;
                }
                default:
                    break;
            }
        }

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Draw_Gate_Icons() {
        int16_t star_idx = 0;

        if (MOX::NUM_STARS > 0) {
            do {
                Draw_A_Gate_Icon(star_idx);
                star_idx++;
            } while (star_idx < MOX::NUM_STARS);
        }
    }

    /**
     * @brief Returns a 3-character unit abbreviation string for research points based on current language.
     * 
     * @param out Buffer of at least 3 bytes to store the result (including null terminator).
     */
    void __cdecl Get_Research_Unit_String(char* out_buffer) {
        const char* src;

        switch (MOX::settings.language) {
            case 1:
                src = "FP";
                break;
            case 2:
                src = "PR";
                break;
            case 3:
                src = "PI";
                break;
            case 4:
                src = "PR";
                break;
            default:
                src = "RP";
                break;
        }

        out_buffer[0] = src[0];
        out_buffer[1] = src[1];
        out_buffer[2] = src[2];
    }

    void __cdecl Draw_Diplomacy_Request_Lights() {
        int16_t drawn_count = 0;
        uint8_t requesting_mask = NETMOX::Humans_Requesting_Diplomacy();

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if ((requesting_mask >> (i & 0x1f)) & 1) {
                uint8_t color_idx = static_cast<uint8_t>(MOX::player[i].color);
                s_animation_header* light_anim = player_lights_seg[color_idx];

                // Calculate horizontal position based on width of first light segment
                int16_t base_width = animate::Get_Width(player_lights_seg[0]);
                int16_t x_pos = 0x1fa - (base_width * drawn_count);

                // Discarded calls from original code
                animate::Get_Width(light_anim);
                animate::Get_Height(light_anim);

                int16_t saved_frame = animate::Get_Current_Frame(static_cast<s_animation_header*>(light_anim));

                // Render current animation frame for diplomacy alert
                animate::Set_Animation_Frame(light_anim, static_cast<uint8_t>(diplo_lights_anim_frame[color_idx]));
                animate::Draw(x_pos, 5, light_anim);

                // Advance animation frame
                uint8_t next_frame = diplo_lights_anim_frame[color_idx] + 1;
                diplo_lights_anim_frame[color_idx] = next_frame;

                if (animate::Get_Animation_Frame_Count(light_anim) <= static_cast<int16_t>(next_frame)) {
                    diplo_lights_anim_frame[color_idx] = 0;
                }

                drawn_count++;

                // Restore previous animation frame for the segment
                animate::Set_Animation_Frame(light_anim, saved_frame);
            }
        }
    }

    void __cdecl Draw_Nebulae() {
        if (MOX::NUM_NEBULAS == 0) {
            return;
        }

        s_nebula* nebula_list = MOX::nebula;

        for (uint16_t i = 0; i < static_cast<uint16_t>(MOX::NUM_NEBULAS); ++i) {
            int16_t zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
            s_animation_header* sprite_ptr = MOX::nebula_pict_seg[(uint8_t)nebula_list[i].type % 12][zoom_level];

            animate::Get_Width(sprite_ptr);
            animate::Get_Height(sprite_ptr);

            int16_t dx = nebula_list[i].x - MOX::cur_map_x;
            int16_t screen_x = HAROLD::Get_Scaled_Value(dx) + 0x15;

            int16_t dy = nebula_list[i].y - MOX::cur_map_y;
            int16_t screen_y = HAROLD::Get_Scaled_Value(dy) + 0x15;

            animate::Draw(static_cast<int32_t>(screen_x), static_cast<int32_t>(screen_y), sprite_ptr);
        }
    }

    void __cdecl Draw_Ship_Icons(bool force_draw) {
        int16_t selected_icon_id = -1;

        if (MOVEBOX::Moveable_Box_Selected(2)) {
            int16_t node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
            selected_icon_id = HACCESS::Ship_Icon_Id_From_Node_Ptr(node_ptr);
        }

        for (int16_t i = MOX::ship_icon_count - 1; i >= 0; --i) {
            int16_t x, y;
            SHIPS::Get_Ship_Icon_Coords(i, &x, &y);

            int16_t owner = HACCESS::Ship_Icon_Owner(i);
            s_animation_header* pict_seg = SHIPS::Get_Ship_Icon_Pict_Seg(owner, 0);

            MOX::ship_icon[i].x = x;
            MOX::ship_icon[i].y = y;

            if ((i != selected_icon_id && i != MAINSCR::main_screen_ship_icon_scanned) || force_draw) {
                animate::Reset_Animation_Frame(pict_seg);
                animate::Draw(x, y, pict_seg);
            }
        }
    }

    void __cdecl Draw_Main_Screen_Filled(int16_t should_copy_to_back) {
        graphics::Reset_Window();
        video::Clip_Off();
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
        graphics::Set_Window(0, 0, 0x27F, 0x1DF);
        video::Clip_On();

        Draw_Paralax();

        MOX::main_screen_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("BUFFER0.LBX", 0, MOX::global_cache_seg));
        animate::Draw(0, 0, MOX::main_screen_seg);

        Draw_Wormhole_Links();
        Draw_Relocation_Links();

        if (MOX::game_type == 2 || MOX::game_type == 3) {
            MOX::mainscrn_diplo_lights_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("BUFFER0.LBX", 0xD, MOX::global_cache_seg));
            animate::Draw(415, 0, MOX::mainscrn_diplo_lights_seg);
        }

        if (MOX::game_type == 2 || MOX::game_type == 3 || MOX::game_type == 1) {
            MOX::mainscrn_plyr_done_lights_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("BUFFER0.LBX", 0xD, MOX::global_cache_seg));
            animate::Draw(74, 0, MOX::mainscrn_plyr_done_lights_seg);
        }

        video::Clip_Off();
        graphics::Reset_Window();

        graphics::Set_Window(22, 22, 527, 421);
        video::Clip_On();
        Draw_Nebulae();
        Draw_Influence_Overlay();

        video::Clip_Off();
        graphics::Reset_Window();

        if (should_copy_to_back == 1) {
            video::Copy_Off_To_Back();
        }

        Print_Star_Names();
        Print_Main_Screen_Data();
    }

    void __cdecl Set_Post_Fitted_Font_Style(uint8_t c1, uint8_t c2) {
        int16_t style = MISC::Get_Font_Style();
        int16_t spacing = MISC::Get_Font_Spacing();
        s_colors colors;

        MISC::Get_Mox_Font_Colors(style, (char)c1, (char)c2, &colors);

        fonts::Set_Outline_Color(2, 1);
        fonts::Set_Font_Style_Outline(style, &colors);
        fonts::Set_Font_Spacing(spacing);
    }

    void __cdecl Set_Popup_Draw_XY(int16_t popup_type, int16_t ship_icon_idx, int16_t* out_w, int16_t* out_h) {
        int16_t draw_x;
        int16_t draw_y;

        if (popup_type == 2) {
            draw_x = MOX::ship_icon[ship_icon_idx].x - 21;
            draw_y = MOX::ship_icon[ship_icon_idx].y - 21;
        } else {
            Get_Star_Draw_Coords(ship_icon_idx, &draw_x, &draw_y);
        }

        Popup_XY(popup_type, draw_x, draw_y, out_w, out_h);
    }

    void __cdecl Popup_XY(int16_t index, int16_t x, int16_t y, int16_t* out_w, int16_t* out_h) {
        if (x < 253 && y < 200) {
            *out_w = 527 - MOX::moveable_box[index].width;
            if (index == 0) {
                *out_h = 421 - MOX::moveable_box[0].height;
            } else {
                *out_h = 421;
            }
        } else if (x < 253 && y > 200) {
            *out_w = 527 - MOX::moveable_box[index].width;
            *out_h = 22;
        } else if (x > 253 && y < 200) {
            *out_w = 22;
            if (index == 0) {
                *out_h = 421 - MOX::moveable_box[0].height;
            } else {
                *out_h = 421;
            }
        } else {
            *out_w = 22;
            *out_h = 22;
        }
    }

    void __cdecl Print_Star_Name(int16_t star_id, int8_t use_bitmap, char* star_name, void* buffer) {
        HAROLD::Map_Scale_To_Zoom_Level();
        int16_t zoom_size = HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_id);

        int16_t draw_x, draw_y;
        Get_Star_Draw_Coords(star_id, &draw_x, &draw_y);

        int16_t text_center_x = draw_x + 1;
        int16_t half_zoom_size = zoom_size / 2;
        int16_t text_y = half_zoom_size + (draw_y - (int16_t)MOX::star[star_id].size);

        if (use_bitmap != 0) {
            int16_t width = fonts::Get_String_Width(star_name);
            animate::Draw(text_center_x - (width / 2), text_y, (s_animation_header*)buffer);
            return;
        }

        int16_t style = HAROLD::Zoom_Level_Font_Style();
        s_colors colors;
        fonts::Get_Colors(&colors);
        fonts::Set_Outline_Color(2, 1);
        fonts::Set_Font_Style_Outline(style, &colors);
        fonts::Set_Font_Spacing(1);

        int16_t width = fonts::Get_String_Width(star_name);
        int16_t start_x = text_center_x - (width / 2);

        if (MOX::max_map_scale == MOX::cur_map_scale) {
            if (start_x < 22) { // 0x16
                fonts::Print(22, text_y, star_name);
            } else if (start_x + width > 527) { // 0x20f
                fonts::Print_Right(527, text_y, star_name);
            } else {
                fonts::Print_Centered(text_center_x, text_y, star_name);
            }
        } else {
            fonts::Print_Centered(text_center_x, text_y, star_name);
        }

        int8_t surrender_to = MOX::star[star_id].surrender_to[MOX::PLAYER_NUM];
        if (static_cast<uint8_t>(surrender_to) < MAX_PLAYERS) {
            // Player colors derived from stack-initialized byte array
            static const uint8_t surrender_colors[MAX_PLAYERS] = {0x80, 0x7b, 0x78, 0x2c, 0x86, 0x9f, 0x83, 0x7e};

            HAROLD::Set_Player_Font_Style(0, false);
            fonts::Set_Font_Spacing(1);

            int16_t line_width = fonts::Get_String_Width(star_name);
            uint8_t player_color_idx = MOX::player[surrender_to].color;
            uint8_t draw_color = surrender_colors[player_color_idx];

            int16_t line_start_x = draw_x - (line_width / 2);
            int16_t font_height = fonts::Get_Font_Height();
            int16_t line_y = half_zoom_size + (font_height - (int16_t)MOX::star[star_id].size) + draw_y;

            line::Line(line_start_x, line_y, line_width + line_start_x, line_y, draw_color);

            if (MOX::cur_map_scale < 30) { // 0x1e
                uint32_t secondary_color = draw_color + 1;
                if (player_color_idx == 3 || player_color_idx == 5) {
                    secondary_color = draw_color - 3;
                }
                line::Line(line_start_x, line_y + 1, line_width + line_start_x, line_y + 1, secondary_color);
            }
        }
    }

    void __cdecl Do_System_Popup(int16_t system_id) {
        FLEETPOP::Set_Fleet_Movement_Icon_Selections(0, 0);

        MOX::moveable_box[0].id = system_id;

        bool box_was_selected = (MOVEBOX::Moveable_Box_Selected(2) != 0);

        if (box_was_selected) {
            MOVEBOX::Vanish_Fleet_Movement_Box();
            SYS::Clear_System_Display_Loaded_Flag();
        }

        SYS::Init_System_Display(MOX::moveable_box[0].id);

        int16_t star_x, star_y;
        Get_Star_Draw_Coords(system_id, &star_x, &star_y);

        Set_Popup_Draw_XY(0, system_id, &MOX::moveable_box[0].x, &MOX::moveable_box[0].y);

        if (MOX::settings.expanding_help != 0) {
            Draw_Exploding_Plasma_Box(
                star_x,
                star_y,
                MOX::moveable_box[0].x,
                MOX::moveable_box[0].y,
                MOX::moveable_box[0].width,
                MOX::moveable_box[0].height,
                0);
        } else if (box_was_selected) {
            fields::Clear_Fields();
            Clear_Main_Screen_Fields();
            Draw_Mini_Main_Offscreen();
        }

        MOVEBOX::Move_Box_To_Top(0);
    }

    void __cdecl Fade_Into_Main_Screen() {
        int16_t box_status = MOVEBOX::Moveable_Box_Selected(0);

        if (already_faded_into_main_screen != 0) {
            already_faded_into_main_screen = 0;
            return;
        }

        RUSS::Mox_Update();
        MAINSCR2::main_screen_fade_in_flag = 0;
        MAINSCR2::force_main_screen_fade = 0;
        already_faded_into_main_screen = 1;

        fields::Disable_Draw_All_Fields();
        video::Set_Refresh_Stencil();
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        ERIC::Fast_Fade_Out();
        RUSS::Mox_Update();

        fonts::Load_Palette(0, 0, 255);
        RUSS::Mox_Update();
        palstore::Darken_Palette(100);
        RUSS::Mox_Update();

        MAINSCR::Add_Map_Fields();
        RUSS::Mox_Update();

        if (static_cast<int8_t>(box_status) != 0) {
            bool expanding_help_enabled = (MOX::settings.expanding_help != 0);
            if (expanding_help_enabled) {
                MOX::settings.expanding_help = 0;
            }
            RUSS::Mox_Update();
            MAINSCR::Do_System_Popup(MOX::moveable_box[0].id);
            if (expanding_help_enabled) {
                MOX::settings.expanding_help = 1;
            }
        }

        video::Set_Page_Off();
        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Update();
        palette::Clear_Palette_Changes(0, 255);
        RUSS::Mox_Sync_Update();

        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        bool skip_fade_in = false;
        if (MOX::reloaded_ship_palette != 0) {
            if (MOVEBOX::Moveable_Box_Selected(2) == 0) {
                ERIC::Set_Fade_Percent(0);
                skip_fade_in = true;
            }
        }

        if (!skip_fade_in) {
            ERIC::Fast_Fade_In();
            RUSS::Mox_Update();
        }

        MOX::reloaded_ship_palette = 0;
        fields::Enable_Draw_All_Fields();
    }

    void __cdecl Get_Star_Name(int16_t star_idx, int8_t is_selected, int8_t* out_uses_bitmap, char* out_name, int32_t out_name_size, s_animation_header** out_bitmap_ptr) {
        if (out_name_size <= 0) {
            *out_uses_bitmap = 0;
            return;
        }

        bool galactic_lore = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);
        *out_name = '\0';
        *out_uses_bitmap = 0;
        HAROLD::Map_Scale_To_Zoom_Level();

        int16_t color_offset = (is_selected != 0) ? (int16_t)MOX::selected_star_color_offset : 0;
        bool is_outpost = (bool)HAROLD::Star_Is_Outpost_Star(star_idx);
        s_star_data* star_ptr = &MOX::star[star_idx];
        int8_t owner = star_ptr->owner;

        auto apply_font_offsets = [&](int16_t font_style) {
            s_colors local_colors;
            s_colors* p_colors = MISC::Font_Colors2(font_style, 1, 12);
            for (int i = 0; i < 8; ++i) local_colors.colors[i] = p_colors->colors[i];

            if (color_offset > 0) {
                int16_t shift = (color_offset * 9) / 5;
                for (int i = 1; i < 8; ++i) {
                    int val = (int)local_colors.colors[i] + shift;
                    if (val < (int)local_colors.colors[0]) val = (int)local_colors.colors[0];
                    if (val > 16) val = 16;
                    local_colors.colors[i] = (uint8_t)val;
                }
            }
            fonts::Set_Font_Style(font_style, &local_colors);
        };

        if (!galactic_lore) {
            bool has_contact = (owner >= 0 && owner < MAX_PLAYERS) && HAROLD::Contact_With_One_Colony(star_idx);
            if (!has_contact) {
                if (!HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx)) return;

                strlcpy(out_name, star_ptr->name, (size_t)out_name_size);

                int16_t font_style = HAROLD::Zoom_Level_Font_Style();
                apply_font_offsets(font_style);

                if (is_outpost) {
                    MISC::Decrease_Font_Color_Range(2);
                    strings::String_To_Lower(out_name);
                }
                return;
            }
        }

        // Logic for Lore or Contacted
        if (galactic_lore && MOX::PLAYER_NUM != owner && !HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx)) {
            snprintf(out_name, (size_t)out_name_size, HAROLD::s___s__00556ae4, star_ptr->name);
        } else {
            strlcpy(out_name, star_ptr->name, (size_t)out_name_size);
        }

        if (owner >= 0 && owner < MAX_PLAYERS) {
            HAROLD::Set_Player_Font_Style(owner, (bool)is_selected);
        } else {
            int16_t font_style = HAROLD::Zoom_Level_Font_Style();
            apply_font_offsets(font_style);
        }

        if (is_outpost) {
            MISC::Decrease_Font_Color_Range(1);
            strings::String_To_Lower(out_name);
        }

        if (HAROLD::N_Colony_Owners(star_idx) > 1) {
            *out_uses_bitmap = Print_Star_Name_To_Bitmap(star_idx, out_name, out_bitmap_ptr);
        }
    }

    bool __cdecl Print_Star_Name_To_Bitmap(int16_t star_idx, char* star_name, s_animation_header** out_bitmap_ptr) {
        int16_t owner_counts[MAX_PLAYERS];
        int16_t num_owners;
        s_colors standard_colors;
        s_colors player_colors;

        bool is_omniscient = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);
        HAROLD::Colony_Owners(star_idx, owner_counts, &num_owners, !is_omniscient);

        if (num_owners <= 1) {
            return false;
        }

        int16_t total_colony_count = 0;
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            total_colony_count += owner_counts[i];
        }

        fonts::Get_Colors(&standard_colors);
        fonts::Set_Font_Spacing(1);
        allocate::Push_Block(MOX::screen_seg);

        uint32_t bitmap_size = bitmap::Get_Bitmap_Size(200, 20);
        s_animation_header* bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);
        *out_bitmap_ptr = bitmap;

        bitmap::Create_Blank_Bitmap(200, 20, bitmap);
        fonts::Print_To_Bitmap(0, 0, star_name, bitmap);

        int16_t current_x = 0;
        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
            if (owner_counts[i] > 0) {
                int16_t width, height, d1, d2;
                MISC::Get_Bitmap_String_Range(owner_counts[i], total_colony_count, &width, &d1, &d2, &height, star_name);
                HAROLD::Get_Player_Font_Colors(i, fonts::current_font_style, &player_colors, false);
                MISC::Swap_Bitmap_Colors(current_x, 0, current_x + width, height, &standard_colors, &player_colors, 8, bitmap);
                current_x += width;
            }
        }

        allocate::Pop_Block(MOX::screen_seg);
        return true;
    }

    void Add_Map_Fields() {
        if (MOX::zoom_in == 1) {
            Clear_Main_Screen_Fields();
            for (int i = 0; i < 5; ++i) {
                MOX::moveable_box[i].drag_field = -1000;
                MOX::moveable_box[i].close_btn = -1000;
            }

            zoom_in_button = fields::Add_Button_Field(244, 428, "", MOX::main_zoom_in_button_seg, "+", 0x28);
            zoom_out_button = fields::Add_Button_Field(244, 455, "", MOX::main_zoom_out_button_seg, "-", 0x28);
            whole_screen_field = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0x29);
            return;
        }

        MOVEBOX::Moveable_Box_Selected(2);
        whole_screen_field = (int16_t)0xfc18;

        game_button = fields::Add_Button_Field(249, 5, "", MOX::main_game_button_seg, "G", 0x29);
        next_button = fields::Add_Button_Field(544, 441, "", MOX::main_next_button_seg, "T", 0x28);
        zoom_in_button = fields::Add_Button_Field(244, 428, "", MOX::main_zoom_in_button_seg, "+", 0x28);
        zoom_out_button = fields::Add_Button_Field(244, 455, "", MOX::main_zoom_out_button_seg, "-", 0x28);

        colonies_button = fields::Add_Irregular_Button_Field(17, 434, 79, 471, "", MOX::main_colonies_button_seg, 0, 425, "C", 0x28);
        planets_button = fields::Add_Irregular_Button_Field(91, 434, 154, 471, "", MOX::main_planets_button_seg, 0, 425, "P", 0x28);
        fleets_button = fields::Add_Irregular_Button_Field(167, 434, 230, 471, "", MOX::main_fleets_button_seg, 0, 425, "F", 0x28);
        leaders_button = fields::Add_Irregular_Button_Field(312, 435, 379, 471, "", MOX::main_leaders_button_seg, 306, 425, "L", 0x28);
        races_button = fields::Add_Irregular_Button_Field(386, 435, 453, 471, "", MOX::main_races_button_seg, 306, 425, "R", 0x28);
        info_button = fields::Add_Irregular_Button_Field(462, 435, 526, 471, "", MOX::main_info_button_seg, 306, 425, "I", 0x28);

        treasury_window_field = fields::Add_Hidden_Field(545, 49, 613, 119, "", 0x29);
        command_points_window_field = fields::Add_Hidden_Field(545, 122, 613, 192, "", 0x29);
        food_window_field = fields::Add_Hidden_Field(545, 197, 613, 267, "", 0x29);
        freighters_window_field = fields::Add_Hidden_Field(545, 270, 613, 340, "", 0x29);
        research_window_field = fields::Add_Hidden_Field(545, 344, 613, 415, "", 0x29);

        Add_Moveable_Box_Fields();

        int16_t window_status;
        MISC::Save_Window_Status(&window_status);
        graphics::Set_Window(22, 22, 527, 421);
        video::Clip_On();
        MISC::Restore_Window_Status(window_status);

        first_field = MOX::star_button[0];

        if (debug_enabled) {
            debug_field = fields::Add_Hidden_Field(630, 470, 639, 479, "", 0);
        } else {
            debug_field = (int16_t)0xfc18;
        }

        graphics::Reset_Window();
        exit_game_hot_key = fields::Add_Multi_Hot_Key_Field("Q");
        influence_hot_key = fields::Add_Multi_Hot_Key_Field("V");

        MOX::main_screen_grid_field = fields::Add_Grid_Field(22, 22, 1, 1, 506, 400, &MOX::main_screen_grid_x, &MOX::main_screen_grid_y, 0);
    }

    void __cdecl Set_Player_Cheated_Flag(int16_t player_idx, int8_t cheated_this_turn) {
        MOX::player[player_idx].cheated = 1;
        MOX::player[player_idx].cheated_this_turn = cheated_this_turn;
    }

    void __cdecl Give_Me_All_Tech_Advances() {
        char buffer[80];
        int16_t tech_app_idx = 1;
        int16_t num_granted = 0;

        s_player* current_player = &MOX::player[MOX::PLAYER_NUM];

        do {
            if (COLCALC::Player_May_Get_Tech_App(current_player, (int)tech_app_idx)) {
                num_granted++;
                COLCALC::Player_Gets_Tech_App(current_player, tech_app_idx);
            }
            tech_app_idx++;
        } while (tech_app_idx < 204); // 0xcc

        current_player->current_research_field = 0;
        current_player->research_breakthrough = 0;

        const char* format_str = HAROLD::H_Message(0xdc);
        snprintf(buffer, sizeof(buffer), format_str, (int)num_granted);

        HAROLD::User_Box(&MOX::draw_main_screen_to_back, buffer, 0);
    }

    bool __cdecl Check_Config_Reload_Hot_Key(int16_t key) {
        const bool reload_all = key == reload_config_command;
        if (!reload_all && key != reload_interface_command) {
            return false;
        }

        config::s_cfg_diagnostic diagnostic;
        const bool network_game = MOX::game_type == 2 || MOX::game_type == 3;
        if (reload_all && network_game) {
            HAROLD::User_Box(
                &MOX::draw_main_screen_to_back,
                "You may not reload config in a multiplayer game.",
                0);
        } else if (!(reload_all
                           ? config::Reload_All_Active_Config_Groups(&diagnostic)
                           : config::Reload_Active_Config_Groups(
                                 network_game, &diagnostic))) {
            HAROLD::User_Box(
                &MOX::draw_main_screen_to_back,
                diagnostic.message.c_str(),
                0);
        } else {
            if (reload_all) {
                HAROLD::User_Box(
                    &MOX::draw_main_screen_to_back, "config reloaded", 0);
            }
            MOX::draw_main_screen_to_back = 1;
        }
        return true;
    }

    /**
     * Handles main-screen cheat and debug hotkeys.
     *
     * @param key Activated hotkey field id.
     * @param unused_param Unused legacy parameter kept for compatibility.
     */
    void __cdecl Check_Hot_Keys(int16_t key, int32_t unused_param) {
        (void)unused_param;
        char message_buffer[80];
        char dialog_text[200];

        if (key == influence_hot_key) {
            influence_view_mode = static_cast<uint8_t>((influence_view_mode + 1) % 3);
            MOX::draw_main_screen_to_back = 1;
            return;
        }

        if (Check_Config_Reload_Hot_Key(key)) {
            return;
        }

        if (key == cycle_player_cheat) {
            for (int16_t offset = 1; offset <= MOX::NUM_PLAYERS; ++offset) {
                const int16_t player_idx = static_cast<int16_t>(
                    (MOX::PLAYER_NUM + offset) % MOX::NUM_PLAYERS);
                if (MOX::player[player_idx].eliminated != 0) {
                    continue;
                }
                MOX::PLAYER_NUM = player_idx;
                Update_Ship_Icons_In_Between_Turns();
                Redraw_Main_Screen(0);
                Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 7);
                break;
            }
            return;
        }

        if (key == leader_cheat) {
            OFFICER::Leader_Cheat_Popup();
            Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 7);
            return;
        }

        if ((key == omniscient_hot_key && capture::Check_Release_Version() == 0) ||
            key == omniscience_cheat) {
            s_player* player = &MOX::player[MOX::PLAYER_NUM];
            player->traits[TRAIT_OMNISCIENCE] = (player->traits[TRAIT_OMNISCIENCE] == 0);

            const char* msg_src;
            if (player->traits[TRAIT_OMNISCIENCE] == 0) {
                msg_src = HAROLD::H_Message(214); // HESTR_0D6_YOU_HAVE_CHOSEN_POORLY
            } else {
                msg_src = HAROLD::H_Message(213); // HESTR_0D5_USE_THE_FORCE_LUKE_OMN
                Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 1);
            }

            strlcpy(dialog_text, msg_src, sizeof(dialog_text));
            HAROLD::User_Box(&MOX::draw_main_screen_to_back, dialog_text, 0);
            MOX::draw_main_screen_to_back = 1;
            Update_Star_Owners();
        }

        if (key == all_tech_cheat) {
            Give_Me_All_Tech_Advances();
            Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 2);
            return;
        }

        if (key == more_money_cheat) {
            s_player* player = &MOX::player[MOX::PLAYER_NUM];
            player->bc += 1000;
            const char* msg_fmt = HAROLD::H_Message(215); // HESTR_0D7_S_HAS_BEEN_GIVEN_A_GIF
            snprintf(message_buffer, sizeof(message_buffer), msg_fmt, player->race_name, 1000);
            HAROLD::User_Box(&MOX::draw_main_screen_to_back, message_buffer, 0);
            Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 3);
            return;
        }

        if (key == get_tech_cheat) {
            s_player* player = &MOX::player[MOX::PLAYER_NUM];
            const char* tech_name = TECHDATA::technology_applications[(uint8_t)player->current_research_application].name;
            const char* msg_fmt = HAROLD::H_Message(216); // HESTR_0D8_S_WILL_BE_KNOWN
            snprintf(message_buffer, sizeof(message_buffer), msg_fmt, tech_name);
            player->research_accumulated = static_cast<int32_t>(menlo_cheat_points);
            HAROLD::User_Box(&MOX::draw_main_screen_to_back, message_buffer, 0);
            Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 4);
            return;
        }

        if (key == all_ai_cheat) {
            RUSS::Make_All_AI_Game();
            Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 7);
            return;
        }

        if (key == hi_score_cheat) {
            int16_t score = SCORE::Do_Calculate_Hi_Score(MOX::PLAYER_NUM);
            s_player* player = &MOX::player[MOX::PLAYER_NUM];
            snprintf(dialog_text, sizeof(dialog_text), "Current score for %s is %d", player->name, (int)score);
            HAROLD::User_Box(&MOX::draw_main_screen_to_back, dialog_text, 0);
        }
    }

    void __cdecl Add_Moveable_Box_Fields() {
        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();

        for (int16_t i = 0; i < 5; ++i) {
            int16_t order_id = MOX::moveable_box_order[i];
            MOVEBOX::Clear_Moveable_Box_Fields(order_id);

            if (order_id == 0) {
                if (MOVEBOX::Moveable_Box_Selected(0) != 0) {
                    MAINSCR::Wrapper_For_Normal_System_Fields();
                }
            }

            if (order_id == 2) {
                if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
                    int16_t node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
                    if (node_ptr == -1) {
                        MOVEBOX::Vanish_Fleet_Movement_Box();
                    } else {
                        int16_t flag = (MOX::moveable_box[2].active == 0) ? 1 : 0;
                        FLEETPOP::Add_Fleet_Movement_Box_Fields(node_ptr, flag);
                    }
                }
            }
        }

        graphics::Reset_Window();
        video::Clip_Off();
    }

    void __cdecl Do_Colony_Screen(int16_t colony_idx, int16_t* status_flag) {
        if (colony_idx >= 0) {
            s_colony* colony = &MOX::colony[colony_idx];

            if (colony->owner == MOX::PLAYER_NUM) {
                int16_t planet_idx = colony->planet;

                if (colony->outpost_flag != 0) {
                    char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
                    const char* format_msg = HAROLD::H_Message(217);
                    char buffer[80];

                    snprintf(buffer, sizeof(buffer), format_msg, planet_name);

                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, buffer, 0);
                } else {
                    MOX::current_screen = SCREEN_COLONY;
                    MOX::return_screen = SCREEN_MAIN;

                    s_planet_data* planet = &MOX::planet[planet_idx];
                    MOX::screen_data = planet->star_index;
                    COLONY::orbit_temp = static_cast<int16_t>(planet->orbit);

                    *status_flag = 1;
                }
            }
        }
    }

    void __cdecl Add_Cheats() {
        all_tech_cheat = fields::Add_Multi_Hot_Key_Field("EINSTEIN");
        more_money_cheat = fields::Add_Multi_Hot_Key_Field("MOOLA");
        get_tech_cheat = fields::Add_Multi_Hot_Key_Field("MENLO");
        omniscience_cheat = fields::Add_Multi_Hot_Key_Field("ISEEALL");
        reload_config_command = fields::Add_Multi_Hot_Key_Field("RE");
        reload_interface_command = fields::Add_Multi_Hot_Key_Field("RI");
        if (MOX::game_type < 2) {
            cycle_player_cheat = fields::Add_Multi_Hot_Key_Field("X");
        }
        leader_cheat = fields::Add_Multi_Hot_Key_Field("ALE");
        all_ai_cheat = fields::Add_Multi_Hot_Key_Field("ALLAI");
        hi_score_cheat = fields::Add_Multi_Hot_Key_Field("SCOR");
        last_field = fields::fields_count - 1;
    }

    void __cdecl Highlight_Star(int16_t star_idx) {
        if (MOX::star[star_idx].spectral_class != STAR_CLASS_BLACK_HOLE) {
            Print_Highlighted_Star_Name(star_idx);

            MOX::selected_star_color_offset += MOX::selected_star_color_mod;

            if (MOX::selected_star_color_offset == 3 || MOX::selected_star_color_offset == -1) {
                MOX::selected_star_color_mod = -MOX::selected_star_color_mod;
            }
        }
    }

    void __cdecl Print_Highlighted_Star_Name(int16_t star_idx) {
        int16_t window_status_cookie;
        int8_t is_highlighted;
        char star_name[32];
        s_animation_header* bitmap_ptr = nullptr;

        MISC::Save_Window_Status(&window_status_cookie);
        graphics::Set_Window(0x16 /* 22 */, 0x16 /* 22 */, 0x20f /* 527 */, 0x1a5 /* 421 */);
        video::Clip_On();

        MAINSCR::Get_Star_Name(star_idx, 1, &is_highlighted, star_name, sizeof(star_name), &bitmap_ptr);

        if (star_name[0] != '\0') {
            if (is_highlighted) {
                MISC::Tint_Bitmap_Colors(0, 0, 0, 0, 1, MAINSCR::star_name_highlight_value, bitmap_ptr);

                MAINSCR::star_name_highlight_value += MAINSCR::star_name_highlight_direction;

                if (MAINSCR::star_name_highlight_value == 0 || MAINSCR::star_name_highlight_value == 3) {
                    MAINSCR::star_name_highlight_direction = -MAINSCR::star_name_highlight_direction;
                }
            }

            MAINSCR::Print_Star_Name(star_idx, is_highlighted, star_name, bitmap_ptr);
        }

        MISC::Restore_Window_Status(window_status_cookie);
    }

    int16_t __cdecl Get_Mouse_Field(int16_t type, int16_t* out_field, int16_t* out_index) {
        bool is_box_selected = MOVEBOX::Moveable_Box_Selected(2);

        *out_field = -1;
        *out_index = -1;

        if (is_box_selected) {
            if (MOX::mouse_XY.star_idx > -1 && type == 0) {
                *out_index = MOX::mouse_XY.star_idx;
                return (int16_t)(MOX::mouse_XY.selected != 1);
            }
            if (MOX::mouse_XY.ship_icon_idx > -1 && type == 1) {
                *out_index = MOX::mouse_XY.ship_icon_idx;
                return (int16_t)(MOX::mouse_XY.selected != 1);
            }
            if (MOX::mouse_XY.hovering_star_idx > -1 && type == 0) {
                *out_field = MOX::mouse_XY.hovering_star_idx;
                return 4;
            }
            if (MOX::mouse_XY.hovering_ship_icon_idx > -1 && type == 1) {
                *out_field = MOX::mouse_XY.hovering_ship_icon_idx;
                return 4;
            }
        } else {
            if (MOX::mouse_XY.ship_icon_idx > -1 && type == 1) {
                *out_index = MOX::mouse_XY.ship_icon_idx;
                return (int16_t)(MOX::mouse_XY.selected != 1);
            }
            if (MOX::mouse_XY.star_idx > -1 && type == 0) {
                *out_index = MOX::mouse_XY.star_idx;
                return (int16_t)(MOX::mouse_XY.selected != 1);
            }
            if (MOX::mouse_XY.hovering_ship_icon_idx > -1 && type == 1) {
                *out_field = MOX::mouse_XY.hovering_ship_icon_idx;
                return 4;
            }
            if (MOX::mouse_XY.hovering_star_idx > -1 && type == 0) {
                *out_field = MOX::mouse_XY.hovering_star_idx;
                return 4;
            }
        }

        return -1;
    }

    void __cdecl Check_Stars_XY(int16_t x, int16_t y, int8_t selected) {
        static const int32_t thresholds[3] = {12, 10, 8};
        char local_name1[0x26] = "None";
        char local_name2[0x26] = "None";
        bool found = false;

        (void)local_name1;
        (void)local_name2;

        MOX::mouse_XY.star_idx = -1;
        MOX::mouse_XY.ship_icon_idx = -1;

        if (MOX::NUM_STARS <= 0) {
            return;
        }

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (found) {
                return;
            }

            int16_t screen_x, screen_y;
            Get_Star_Draw_Coords(star_idx, &screen_x, &screen_y);
            HAROLD::Map_Scale_To_Zoom_Level();

            const s_star_data& star = MOX::star[star_idx];

            if (star.spectral_class != STAR_CLASS_BLACK_HOLE) {
                HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx);
            }

            int32_t dx = (int32_t)screen_x - (int32_t)x;
            int32_t dy = (int32_t)screen_y - (int32_t)y;
            int32_t dist_sq = dx * dx + dy * dy;

            int32_t threshold = Scaled_Star_Map_Dimension(static_cast<int16_t>(thresholds[(int32_t)star.size]));
            if (dist_sq <= threshold * threshold) {
                MOX::mouse_XY.star_idx = star_idx;
                MOX::mouse_XY.selected = selected;
                ERICNET::Mox_Play_Sound(0x29);
                found = true;
            }
        }
    }

    void __cdecl Exit_Zooming_Mode(int16_t* zooming_state) {
        *zooming_state = 0;
        MOX::zoom_in = 0;
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 1);
        EVANHELP::Set_Main_Screen_Help_List();
    }

    void __cdecl Check_Ships_XY(int16_t mouse_x, int16_t mouse_y, int8_t selection_id) {
        MOX::mouse_XY.star_idx = -1;
        MOX::mouse_XY.ship_icon_idx = -1;

        if (MOX::ship_icon_count <= 0) {
            return;
        }

        for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
            s_ship_icon* icon = &MOX::ship_icon[i];
            int16_t ship_idx = MOX::ship_node[icon->node_idx].ship_idx;

            int16_t owner = HACCESS::Ship_Owner(ship_idx);
            s_animation_header* anim = SHIPS::Get_Ship_Icon_Pict_Seg(owner, 0);

            if (mouse_x >= icon->x) {
                int16_t width = animate::Get_Width(anim);
                if (mouse_x <= icon->x + width) {
                    if (mouse_y >= icon->y) {
                        int16_t height = animate::Get_Height(anim);
                        if (mouse_y <= icon->y + height) {
                            MOX::mouse_XY.ship_icon_idx = i;
                            MOX::mouse_XY.selected = selection_id;
                            ERICNET::Mox_Play_Sound(0x29);
                            return;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Do_Fleet_Popup(int16_t ship_icon_id, int8_t check_overlap) {
        int16_t zoom_level = HAROLD::Map_Scale_To_Zoom_Level();
        SYS::Clear_System_Display_Loaded_Flag();

        MOX::ship_icon_player_id = -1;
        MOX::fleet_ship_scanned = -1;
        MOX::eta_star_id = -1;

        HACCESS::Clear_Fleet_Box_Selected_Ship_Status();
        FLEETPOP::Clear_Ship_Node_Selection_Flags();

        MOX::first_time = 1;
        MOX::just_moved_stack = 0;

        if (check_overlap != 0) {
            if (SHIPS::Overlapped_Ship_Icon_Button(&ship_icon_id, 13 - zoom_level, 9 - zoom_level) != 0) {
                MOX::draw_main_screen_to_back = 1;
            }
        }

        int16_t old_stack_owner = -1;
        if (MOVEBOX::Moveable_Box_Selected(0)) {
            MOVEBOX::Vanish_System_Display_Box();
            MAINSCR::Draw_Mini_Main_Offscreen();
            MOX::first_node_ptr_in_stack_ = -1;
        } else if (MOVEBOX::Moveable_Box_Selected(2)) {
            old_stack_owner = HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[MOX::fleet_box_ship_stack]);
        }

        MOX::moveable_box[2].id = 1;
        MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Icon(ship_icon_id);

        if (MOX::fleet_box_ship_stack == -1) {
            MOVEBOX::Vanish_Fleet_Movement_Box();
            return;
        }

        MAINSCR::Set_Popup_Draw_XY(2, ship_icon_id, &MOX::moveable_box[2].x, &MOX::moveable_box[2].y);

        if (MOX::settings.expanding_help != 0) {
            MOX::draw_expanding_box = 1;
        }

        int16_t new_stack_owner = HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[MOX::fleet_box_ship_stack]);
        if (new_stack_owner != old_stack_owner || MOX::mainscr_palette != 2) {
            if (HAROLD::set_fade_for_first_fltpop == 0) {
                fields::Disable_Draw_All_Fields();
                MOVEBOX::Vanish_Fleet_Movement_Box();
                MAINSCR::Redraw_Main_Screen(0);
                fields::Enable_Draw_All_Fields();
            } else {
                ERIC::Set_Fade_Percent(0);
                HAROLD::set_fade_for_first_fltpop = 0;
                MAINSCR::Draw_Mini_Main_Screen();
            }
            MOX::moveable_box[2].id = 1;
            int16_t force_reload = (new_stack_owner != MOX::PLAYER_NUM);
            FLEETPOP::Reload_Fleet_Box_Palette(new_stack_owner, force_reload);
        }

        MOVEBOX::Move_Box_To_Top(2);
        MOX::add_help_fields = 0;
        MOX::fleet_movement_bar_value = 0x66;

        if (MOX::fleet_box_ship_stack > -1 && MOX::moveable_box[2].x < 32 && MOX::moveable_box[2].y < 32) {
            int16_t icon_x, icon_y;
            SHIPS::Get_Ship_Icon_Coords(ship_icon_id, &icon_x, &icon_y);

            int16_t box_x1 = MOX::moveable_box[2].x;
            int16_t box_y1 = MOX::moveable_box[2].y;
            int16_t box_x2 = box_x1 + (int16_t)MOX::moveable_box[2].width;
            int16_t box_y2 = box_y1 + (int16_t)MOX::moveable_box[2].height;

            if (MISC::Occluded(box_x1, box_y1, box_x2, box_y2, icon_x, icon_y, icon_x + 13, icon_y + 10)) {
                MOX::moveable_box[2].x = 619;
                MOX::moveable_box[2].y = 421;
            }
        }
    }

    void __cdecl Draw_Scanned_Ship_Seg() {
        int16_t icon_idx = main_screen_ship_icon_scanned;
        int16_t owner_idx = HACCESS::Ship_Icon_Owner(icon_idx);

        s_animation_header* anim = SHIPS::Get_Ship_Icon_Pict_Seg(owner_idx, 1);

        int16_t local_y, local_x;
        SHIPS::Get_Ship_Icon_Coords(icon_idx, &local_y, &local_x);

        int16_t final_x = MOX::ship_icon[icon_idx].x;
        int16_t final_y = MOX::ship_icon[icon_idx].y;

        animate::Draw(final_x, final_y, anim);
    }

    void __cdecl Draw_Fleet_Box_Selected_Ship_Seg() {
        int16_t node_ptr = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
        int16_t ship_icon_id = HACCESS::Ship_Icon_Id_From_Node_Ptr(node_ptr);

        int16_t owner = HACCESS::Ship_Icon_Owner(ship_icon_id);
        s_animation_header* anim = SHIPS::Get_Ship_Icon_Pict_Seg(owner, 1);

        int16_t local_x;
        int16_t local_y;
        SHIPS::Get_Ship_Icon_Coords(ship_icon_id, &local_x, &local_y);

        animate::Draw(MOX::ship_icon[ship_icon_id].x, MOX::ship_icon[ship_icon_id].y, anim);
    }

    void __cdecl Print_Galaxy_Map_Scanned_Star() {
        s_colors text_colors;
        char print_buffer[80];

        int16_t star_idx = MOX::galaxy_map_scanned_star;
        s_star_data* star = &MOX::star[star_idx];
        int16_t owner = (int16_t)star->owner;

        if (owner >= 0 && owner < MAX_PLAYERS) {
            HAROLD::Set_Player_Font_Style(owner, 0);
            char* race_name = HACCESS::Race_Name_String(owner);

            snprintf(print_buffer, sizeof(print_buffer), MOX::s_fmt_scanned_star_info, star->name, race_name);
        } else if (owner == -1) {
            for (int16_t i = 0; i < 8; ++i) {
                if (i < 2) {
                    text_colors.colors[i] = 0x08;
                } else {
                    text_colors.colors[i] = 0x0C;
                }
            }
            snprintf(print_buffer, sizeof(print_buffer), MOX::s_fmt_s, star->name);
        } else if (owner == -2) {
            for (int16_t i = 0; i < 8; ++i) {
                if (i < 2) {
                    text_colors.colors[i] = 0x14;
                } else {
                    text_colors.colors[i] = 0x1F;
                }
            }
            snprintf(print_buffer, sizeof(print_buffer), MOX::s_fmt_contested, star->name);
        }

        if (owner == -1 || owner == -2) {
            fonts::Set_Font_Style(2, &text_colors);
        }

        s_movable_box box = MOX::moveable_box[1];
        int16_t final_x = box.x + (int16_t)box.content_offset_x + 15;
        int16_t final_y = (int16_t)box.height + (box.y + (int16_t)box.content_offset_y) - 25;

        fonts::Print(final_x, final_y, print_buffer);
    }

    void Draw_Main_Screen_While_Moving_Boxes() {
        if (MOX::draw_main_screen_stencilled == 1) {
            Draw_Main_Screen_Before_Stencil();
            MOX::draw_main_screen_stencilled = 0;
            return;
        }

        graphics::Set_Window(0x16 /* 22 */, 0x16 /* 22 */, 0x20F /* 527 */, 0x1A5 /* 421 */);
        video::Clip_On();

        for (int16_t i = 4; i >= 0; --i) {
            int16_t box_id = MOX::moveable_box_order[i];
            if (MOVEBOX::Moveable_Box_Selected(box_id) != 0 && MOX::stencil_box_id != box_id) {
                MOVEBOX::Draw_Moveable_Box(box_id);
            }
        }

        if (MOX::stencil_box_id > -1) {
            if (MOX::stencil_box_id < 5) {
                int16_t button = mouse::Mouse_Button();
                MOVEBOX::Move_Moveable_Box(MOX::stencil_box_id, button);
            }
            if (MOX::stencil_box_id > -1 && MOX::stencil_box_id < 5) {
                MOVEBOX::Draw_Moveable_Box(MOX::stencil_box_id);
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();
        Print_Main_Screen_Data();

        if (MOX::stencil_box_id == -2) {
            video::Set_Page_Off();
            Draw_Main_Screen();
            MOX::stencil_box_id = -1;
        }
    }

    void __cdecl Check_Mouse_XY_Fields() {
        int16_t mouse_x = mouse::Pointer_X();
        int16_t mouse_y = mouse::Pointer_Y();

        MOX::mouse_XY.selected = 0;
        MOX::mouse_XY.hovering_star_idx = -1;
        MOX::mouse_XY.hovering_ship_icon_idx = -1;

        bool is_in_box = false;

        if (MOVEBOX::Moveable_Box_Selected(0)) {
            if (mouse_x > MOX::moveable_box[0].x &&
                mouse_x < (MOX::moveable_box[0].x + (int16_t)MOX::moveable_box[0].width) &&
                mouse_y > MOX::moveable_box[0].y &&
                mouse_y < (MOX::moveable_box[0].y + (int16_t)MOX::moveable_box[0].height)) {
                is_in_box = true;
            }
        } else if (MOVEBOX::Moveable_Box_Selected(2)) {
            if (mouse_x > MOX::moveable_box[2].x &&
                mouse_x < (MOX::moveable_box[2].x + (int16_t)MOX::moveable_box[2].width) &&
                mouse_y > MOX::moveable_box[2].y &&
                mouse_y < (MOX::moveable_box[2].y + (int16_t)MOX::moveable_box[2].height)) {
                is_in_box = true;
            }
        }

        if (mouse_x > 21 && mouse_x < 527 &&
            mouse_y > 21 && mouse_y < 421) {
            if (!is_in_box) {
                Scan_Stars_XY(mouse_x, mouse_y);
                Scan_Ships_XY(mouse_x, mouse_y);
            }
        }
    }

    /**
     * Updates parallax background positions based on map movement.
     * Returns the vertical wrap count for the far background layer.
     */
    int16_t __cdecl Get_New_Paralax_XYs(int16_t target_map_x, int16_t target_map_y, int16_t scale) {
        int16_t dx = (static_cast<int32_t>(target_map_x) - MOX::cur_map_x) / scale;
        int16_t dy = (static_cast<int32_t>(target_map_y) - MOX::cur_map_y) / scale;

        int16_t near_x = MOX::near_star_x;
        int16_t mid_x = MOX::mid_star_x;
        int16_t far_x = MOX::far_star_x;

        if (target_map_x != MOX::cur_map_x) {
            // sign_x logic calculates -abs(dx)
            int16_t sign_x = (dx > 0) ? -1 : (dx < 0 ? 1 : 0);
            int16_t step_x = sign_x * dx;

            far_x -= step_x;
            mid_x -= 2 * step_x;
            near_x -= 4 * step_x;
        }

        int16_t near_y = MOX::near_star_y;
        int16_t mid_y = MOX::mid_star_y;
        int16_t far_y = MOX::far_star_y;

        if (target_map_y != MOX::cur_map_y) {
            // sign_y logic calculates -abs(dy)
            int16_t sign_y = (dy > 0) ? -1 : (dy < 0 ? 1 : 0);
            int16_t step_y = sign_y * dy;

            far_y -= step_y;
            mid_y -= 2 * step_y;
            near_y -= 4 * step_y;
        }

        // Apply screen wrap (640x480)
        if (near_x < 0) near_x += 640;
        if (near_y < 0) near_y += 480;
        if (mid_x < 0) mid_x += 640;
        if (mid_y < 0) mid_y += 480;
        if (far_x < 0) far_x += 640;
        if (far_y < 0) far_y += 480;

        MOX::near_star_x = near_x % 640;
        MOX::near_star_y = near_y % 480;
        MOX::mid_star_x = mid_x % 640;
        MOX::mid_star_y = mid_y % 480;
        MOX::far_star_x = far_x % 640;

        int16_t wrap_count_y = far_y / 480;
        MOX::far_star_y = far_y % 480;

        return wrap_count_y;
    }

    int16_t __cdecl Scan_System_Display_Fields(int16_t mouse_input, int16_t input, int16_t* colony_btns, int16_t* m_planet_idx, int16_t* planet_idx, int16_t* m_ship_idx, int16_t* ship_idx, int16_t* selection) {
        int16_t ret_val = -1;
        int16_t gate_idx = -1;

        *planet_idx = -1;
        *m_planet_idx = -1;
        *ship_idx = -1;
        *m_ship_idx = -1;
        *selection = -1;

        // 1. Check for clicked ships
        for (int i = 0; i < 15; ++i) {
            if (MOX::system_display_ships[i].pic != NULL && MOX::system_display_ships[i].button == input) {
                *ship_idx = MOX::system_display_ships[i].ship_idx;
                ret_val = 0;
                break;
            }
        }

        // 2. Check for hovered ships if no click hit
        if (ret_val == -1) {
            for (int i = 0; i < 15; ++i) {
                if (MOX::system_display_ships[i].pic != NULL && MOX::system_display_ships[i].button == mouse_input) {
                    *m_ship_idx = MOX::system_display_ships[i].ship_idx;
                    ret_val = 4;
                    break;
                }
            }
        }

        // 3. Check for colony button interactions (Click, Right-Click popup, or Hover)
        if (ret_val == -1) {
            for (int i = 0; i < 5; ++i) {
                int16_t btn_id = colony_btns[i];
                if (input == btn_id) {
                    *planet_idx = ERIC::Get_Sint_Index_From_List(MOX::sys_disp[i].orbit, MOX::star[MOX::system_star_id].planet_index, 5);
                    ret_val = 0;
                    break;
                }
                if ((int)input + (int)btn_id == 0) { // Right-click or special interaction
                    if (MOX::planet[MOX::sys_disp[i].orbit].planet_type != PLANET_TYPE_GAS_GIANT) {
                        SYS::Potential_Colony_Info_Popup(MOX::sys_disp[i].orbit);
                        MOX::FULL_DRAW_SCREEN_FLAG = 1;
                    }
                    return -1;
                }
                if (mouse_input == btn_id) {
                    *m_planet_idx = ERIC::Get_Sint_Index_From_List(MOX::sys_disp[i].orbit, MOX::star[MOX::system_star_id].planet_index, 5);
                    ret_val = 4;
                }
            }
        }

        // 4. Check for system grid interactions if still no hit
        if (ret_val == -1) {
            int16_t center_x = MOX::system_display_base_x + 171; // 0xab
            int16_t center_y = MOX::system_display_base_y + 134; // 0x86
            int16_t ptr_x = mouse::Pointer_X();
            int16_t ptr_y = mouse::Pointer_Y();

            // Grid click scan
            if (input == MOX::system_display_grid_field) {
                int16_t orbit_id = GEO::Get_Orbit_From_X_Y(center_x - ptr_x, center_y - ptr_y);
                if (orbit_id >= 0) {
                    for (int i = 0; i < 5; ++i) {
                        int16_t p_idx = MOX::star[MOX::system_star_id].planet_index[i];
                        if (p_idx > -1 && MOX::planet[p_idx].orbit == orbit_id && MOX::planet[p_idx].planet_type == PLANET_TYPE_ASTEROID) {
                            *planet_idx = i;
                            ret_val = 0;
                            break;
                        }
                    }
                }
            }

            // Grid hover scan
            if (ret_val == -1) {
                int16_t orbit_id = -1;
                if (ptr_x > MOX::system_display_base_x &&
                    ptr_x < MOX::system_display_base_x + (int16_t)MOX::moveable_box[0].width &&
                    ptr_y > MOX::system_display_base_y &&
                    ptr_y < MOX::system_display_base_y + (int16_t)MOX::moveable_box[0].height) {
                    orbit_id = GEO::Get_Orbit_From_X_Y(center_x - ptr_x, center_y - ptr_y);
                }

                if (orbit_id >= 0) {
                    for (int i = 0; i < 5; ++i) {
                        int16_t p_idx = MOX::star[MOX::system_star_id].planet_index[i];
                        if (p_idx > -1 && MOX::planet[p_idx].orbit == orbit_id && MOX::planet[p_idx].planet_type == PLANET_TYPE_ASTEROID) {
                            *m_planet_idx = i;
                            ret_val = 4;
                            break;
                        }
                    }
                }
            }
        }

        // 5. Check for gate icon hover
        if (ret_val == -1) {
            for (int i = 0; i < 4; ++i) {
                if (MOX::gate_icon_field[i] == mouse_input) {
                    gate_idx = i;
                    ret_val = 4;
                    break;
                }
            }
        }

        // Determine final selection result for UI feedback
        *selection = -1;
        if (*m_planet_idx > -1) {
            *selection = *m_planet_idx;
        } else if (*m_ship_idx >= 0) {
            *selection = *m_ship_idx + 1000;
        } else if (gate_idx > -1) {
            *selection = gate_idx + 2000;
        }

        return ret_val;
    }

    void __cdecl Clear_Twinkling_Star_Data() {
        for (int16_t i = 0; i < MAX_STARS; ++i) {
            MOX::star[i].twinkling1 = -1;
            MOX::star[i].twinkling2 = -1;
        }
        n_twinkling_stars = 0;
    }

    void __cdecl Warn_About_Cheating_Players() {
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (i == MOX::PLAYER_NUM) continue;

            s_player* other_player = &MOX::player[i];
            s_player* local_player = &MOX::player[MOX::PLAYER_NUM];

            if (other_player->cheated_this_turn != 0 && !(local_player->warned_about_cheating_player & (1 << (i & 0x1F)))) {
                char name_buf[52];
                char race_buf[52];
                char lbx_msg[500];
                char output_msg[500];

                char* src = other_player->name;
                char* dst = name_buf;
                while ((*dst++ = *src++) != '\0');

                src = other_player->race_name;
                dst = race_buf;
                while ((*dst++ = *src++) != '\0');

                strings::String_To_Upper(name_buf);
                strings::String_To_Upper(race_buf);

                other_player->cheated = 1;

                MISC::Get_LBX_Message(22 /* LBX_MSG_CHEAT_WARNING */, lbx_msg);
                snprintf(output_msg, sizeof(output_msg), lbx_msg, name_buf, race_buf);

                HAROLD::User_Box(&MOX::draw_main_screen_to_back, output_msg, 3);
                video::Set_Page_Off();
                MAINSCR::Draw_Mini_Main_Screen();
                RUSS::Mox_Sync_Update();

                local_player->warned_about_cheating_player |= (1 << (i & 0x1F));
            }
        }
    }

    void __cdecl Change_Home_Star_Name() {
        int16_t player_idx = MOX::PLAYER_NUM;
        s_player* player = &MOX::player[player_idx];
        int16_t star_idx = MOX::planet[player->home_planet_id].star_index;

        int16_t center_y = namestar::Star_Name_Popup_Map_Center_Y();
        int16_t center_x = namestar::Star_Name_Popup_Map_Center_X();

        namestar::Change_Home_Star_Name_Popup(center_x, center_y, star_idx);

        fields::Clear_Fields();
        MAINSCR::Clear_Main_Screen_Fields();
        MAINSCR::Add_Cheats();
        MAINSCR::Add_Map_Fields();
        video::Set_Page_Off();
        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 1);
    }

    int16_t __cdecl Do_Reply_To_Diplomacy_Request(int16_t requester_id) {
        int16_t unused_field;
        int16_t first_reply_field;
        int16_t second_reply_field;
        int16_t third_reply_field;
        int16_t selected_reply = 0;
        bool has_selection = false;

        fields::Clear_Fields();
        MAINSCR::Clear_Main_Screen_Fields();
        video::Set_Page_Off();
        MAINSCR::Draw_Mini_Main_Screen();
        video::Copy_Off_To_Back();
        video::Set_Refresh_Stencil();
        RUSS::Mox_Sync_Update();

        MOX::temp_string[0] = '\0';
        MOX::screen_data = requester_id;

        fields::Clear_Fields();
        MAINSCR::Clear_Main_Screen_Fields();
        MAINSCR::Load_Reply_To_Diplomacy_Request_Popup();
        MAINSCR::Add_Reply_To_Diplomacy_Request_Fields(
            &first_reply_field,
            &second_reply_field,
            &third_reply_field,
            &unused_field);
        fields::Assign_Auto_Function(MAINSCR::Draw_Reply_To_Diplomacy_Request, 1);

        const int32_t popup_width =
            static_cast<int32_t>(animate::Get_Width(MOX::diplomacy_request_popup_seg));
        const int32_t popup_x = ((0x1FA - popup_width) / 2) + 0x15;

        const int32_t popup_height =
            static_cast<int32_t>(animate::Get_Height(MOX::diplomacy_request_popup_seg));
        const int32_t popup_y = ((0x190 - popup_height) / 2) + 0x15;

        EVANHELP::Set_Diplomacy_Request_Help_List(popup_x, popup_y);

        while (!has_selection) {
            const int16_t input_field = fields::Get_Input();

            if (first_reply_field == input_field) {
                selected_reply = 1;
                has_selection = true;
            } else if (second_reply_field == input_field) {
                selected_reply = 2;
                has_selection = true;
            } else if (third_reply_field == input_field) {
                selected_reply = 3;
                has_selection = true;
            }

            if (!has_selection) {
                video::Set_Page_Off();
                MAINSCR::Draw_Reply_To_Diplomacy_Request();
                RUSS::Mox_Sync_Update();
            }
        }

        allocate::Pop_Block(MOX::screen_seg);
        EVANHELP::Set_Main_Screen_Help_List();
        MAINSCR::already_faded_into_main_screen = 0;
        MOX::FULL_DRAW_SCREEN_FLAG = 1;

        return selected_reply;
    }

    int16_t __cdecl Get_Star_Id_For_Officers_Screen() {
        int16_t player_num = MOX::PLAYER_NUM;
        int16_t home_planet_id = MOX::player[player_num].home_planet_id;
        int16_t star_idx = -1;

        // Check if a specific system is selected in the UI
        if (MOVEBOX::Moveable_Box_Selected(0) != 0) {
            star_idx = MOX::moveable_box[0].id;
            if (star_idx != -1) {
                if (HAROLD::Player_Has_Colony(player_num, star_idx)) {
                    return star_idx;
                }
            }
        }

        // Search for the first star system where the player has a colony, starting from home
        int16_t home_star_idx = MOX::planet[home_planet_id].star_index;
        star_idx = home_star_idx;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (HAROLD::Player_Has_Colony(player_num, star_idx)) {
                return star_idx;
            }
            star_idx = (star_idx + 1) % MOX::NUM_STARS;
        }

        // Fallback to home star if no colony found or galaxy has no stars
        if (star_idx == -1) {
            star_idx = home_star_idx;
        }

        return star_idx;
    }

    /**
     * Iterates through the ship stack to find the first ship belonging to the current player.
     * If the first ship in the stack is already owned by the player, it returns that index.
     * Otherwise, it searches the entire stack.
     */
    int16_t __cdecl Get_Ship_Stack_For_Officers_Screen() {
        int16_t first_stack_idx = MOX::ship_stack_start[0];

        if (first_stack_idx > -1) {
            int16_t ship_idx = MOX::ship_node[first_stack_idx].ship_idx;

            if (MOX::ship[ship_idx].owner != MOX::PLAYER_NUM) {
                for (int16_t i = 0; i < MOX::ship_stack_count; ++i) {
                    int16_t current_stack_idx = MOX::ship_stack_start[i];
                    int16_t current_ship_idx = MOX::ship_node[current_stack_idx].ship_idx;

                    if (MOX::ship[current_ship_idx].owner == MOX::PLAYER_NUM) {
                        return current_stack_idx;
                    }
                }
                return -1;
            }
        }

        return first_stack_idx;
    }

    void __cdecl Tax_Rate_Popup() {
        int16_t scan_val;
        int16_t input_val;
        int16_t exit_flag;
        s_colors font_colors;

        font_colors.colors[0] = 0x9f;
        font_colors.colors[1] = 0xa0;
        font_colors.colors[2] = 0xa1;
        font_colors.colors[3] = 0xa2;
        input_val = 0;
        font_colors.colors[4] = 0;
        exit_flag = 0;
        font_colors.colors[5] = 0;
        font_colors.colors[6] = 0;
        font_colors.colors[7] = 0;

        MAINPUPS::g_temp_tax_rate = (int16_t)(uint8_t)MOX::player[MOX::PLAYER_NUM].tax_rate;

        MAINPUPS::last_scanned_field = -1;
        fonts::Set_Font_Style_Shadow_Down(1, &font_colors);
        MAINPUPS::Load_Tax_Rate_Popup();
        MAINPUPS::Do_Popup_Setup(-1, MAINPUPS::anims[0x24]);
        ERICHELP::Set_Tax_Rate_Popup_Help_List();
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(MAINPUPS::Draw_Tax_Rate_Popup, 1);
        MAINPUPS::Add_Tax_Rate_Fields();
        MAINPUPS::Draw_Tax_Rate_Popup();
        RUSS::Mox_Sync_Update();
        video::Set_Refresh_Differential();

        do {
            input_val = fields::Get_Input();
            scan_val = fields::Scan_Input();
            MAINPUPS::Evaluate_Tax_Rate_Input(&input_val, &scan_val, &exit_flag);
            MAINPUPS::Draw_Tax_Rate_Popup();
            RUSS::Mox_Sync_Update();
        } while (exit_flag == 0);

        MAINPUPS::Do_Popup_Cleanup();
    }

    int16_t __cdecl Check_For_Diplomacy_Request() {
        int16_t processed_any = 0;
        int16_t processed_count = 0;
        int16_t requester_id = (int16_t)NETMOX::Human_Requesting_Diplomacy();

        if (requester_id != -1) {
            processed_any = 1;
            do {
                if (processed_count > 0) {
                    Fade_Into_Main_Screen();
                }

                int16_t reply_type = Do_Reply_To_Diplomacy_Request(requester_id);

                if (reply_type == 1) {
                    MAINSCR2::force_main_screen_fade = 1;
                    NETMOX::Accept_Diplomacy_Request((int32_t)requester_id);
                } else if (reply_type == 2) {
                    NETMOX::Refuse_Diplomacy_Request((int32_t)requester_id);
                } else if (reply_type == 3) {
                    NETMOX::Tell_Diplomacy_Requester_To_Wait((int32_t)requester_id);
                }

                processed_count++;
                requester_id = (int16_t)NETMOX::Human_Requesting_Diplomacy();
            } while (requester_id != -1);
        }

        return processed_any;
    }

    void __cdecl Set_Reload_Fleet_Box_Palette_Flag() {
        MOX::mainscr_palette = 101;
        MOX::ship_icon_player_id = -1;
    }

    /**
     * Mirrors the fleet-box field selection state into the underlying ship-node selection flags.
     *
     * @param field_to_select Fleet movement field id to apply.
     */
    void __cdecl Set_Painted_Fleet_Fields(int16_t field_to_select) {
        int16_t box_ptr_idx = 0;
        int16_t icon_idx = MOX::first_fleet_movement_icon;
        int32_t limit = (int32_t)MOX::fleet_icon_buttons_added + (int32_t)icon_idx;

        if (limit > (int32_t)MOX::fleet_box.count) {
            limit = (int32_t)MOX::fleet_box.count;
        }

        while (icon_idx < (int16_t)limit) {
            if (MOX::fleet_movement_icon[icon_idx].field == field_to_select) {
                int16_t movement_icon_idx = MOX::fleet_movement_box_ptr[box_ptr_idx];
                int16_t node_idx = MOX::fleet_movement_icon[movement_icon_idx].ship_node_idx;

                if (MAINSCR::Ship_Can_Be_Selected(MOX::ship_node[node_idx].ship_idx)) {
                    MOX::ship_node[node_idx].selected = (int8_t)MAINSCR::fltpop_field_selection_status;
                }
            }
            box_ptr_idx++;
            icon_idx++;
        }
    }

    void __cdecl Center_Map(int16_t target_x, int16_t target_y) {
        int16_t old_x = MOX::cur_map_x;
        int16_t old_y = MOX::cur_map_y;

        // Calculate top-left corner based on centered target
        MOX::cur_map_x = MAP_SCALE::Clamp_Map_Offset_(target_x - HAROLD::Get_Up_Scaled_Value(253), MOX::MAP_MAX_X, HAROLD::Get_Up_Scaled_Value(506));
        MOX::cur_map_y = MAP_SCALE::Clamp_Map_Offset_(target_y - HAROLD::Get_Up_Scaled_Value(200), MOX::MAP_MAX_Y, HAROLD::Get_Up_Scaled_Value(400));

        // If viewport position changed, update parallax offsets
        if (old_x != MOX::cur_map_x || old_y != MOX::cur_map_y) {
            MAINSCR::Get_New_Paralax_XYs((int16_t)old_x, (int16_t)old_y, 1);
        }

        MOX::draw_main_screen_to_back = 1;
    }

    void __cdecl Add_Star_Fields() {
        int16_t zoom_radii[4] = {0x27, 0x21, 0x21, 0x18};

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            int16_t x_coord;
            int16_t y_coord;

            MAINSCR::Get_Star_Draw_Coords(star_idx, &x_coord, &y_coord);

            int16_t scale = HAROLD::Map_Scale_To_Zoom_Level();
            int16_t radius;

            if (MOX::star[star_idx].spectral_class == STAR_CLASS_BLACK_HOLE) {
                radius = Scaled_Star_Map_Dimension(zoom_radii[scale]);
            } else {
                radius = HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx);
            }

            int16_t adjusted_radius = (radius / 2) - 2;
            if (adjusted_radius < 2) {
                adjusted_radius = 2;
            }

            MOX::star_button[star_idx] = fields::Add_Clipped_Circle_Field(
                x_coord, y_coord, adjusted_radius, "", '\0');
        }
    }

    void __cdecl Redraw_Main_Screen(uint8_t redraw_full) {
        if (redraw_full != 0) {
            ERIC::Fast_Fade_Out();
            video::Set_Page_Off();
            graphics::Fill(0, 0, 649 /* 0x289 */, 479 /* 0x1df */, 0);
            RUSS::Mox_Sync_Update();
            video::Set_Refresh_Full();
            video::Set_Page_Off();
            Draw_Main_Screen_Filled(1);
            ERIC::Fast_Fade_In();
            video::Set_Refresh_Stencil();

            if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
                if (HACCESS::Get_Stack_Start_From_Saved_Ship_Array() != 0) {
                    int16_t ship_stack_idx = MOX::fleet_box_ship_stack;
                    int16_t ship_id = MOX::ship_stack_start[ship_stack_idx];
                    int16_t owner = HACCESS::Ship_Stack_Owner(ship_id);
                    FLEETPOP::Reload_Fleet_Box_Palette(owner, (owner != MOX::PLAYER_NUM));
                } else {
                    MOVEBOX::Vanish_Fleet_Movement_Box();
                }
            }
        } else {
            video::Set_Page_Off();
            Draw_Mini_Main_Screen();
            RUSS::Mox_Sync_Update();

            if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
                if (HACCESS::Get_Stack_Start_From_Saved_Ship_Array() != 0) {
                    int16_t ship_stack_idx = MOX::fleet_box_ship_stack;
                    int16_t ship_id = MOX::ship_stack_start[ship_stack_idx];
                    int16_t owner = HACCESS::Ship_Stack_Owner(ship_id);

                    video::Set_Page_Off();
                    FLEETPOP::Reload_Fleet_Box_Palette(owner, (owner != MOX::PLAYER_NUM));
                    Draw_Main_Screen();
                    RUSS::Mox_Sync_Update();
                } else {
                    MOVEBOX::Vanish_Fleet_Movement_Box();
                }
            }
        }
    }

    void __cdecl Scroll_Map(int16_t x, int16_t y) {
        int16_t target_x = x - HAROLD::Get_Up_Scaled_Value(253 /* 0xFD */);
        int16_t target_y = y - HAROLD::Get_Up_Scaled_Value(200 /* 0xC8 */);

        if (target_x < 0) {
            target_x = 0;
        }

        int16_t max_x_offset = HAROLD::Get_Up_Scaled_Value(506 /* 0x1FA */);
        if (MOX::MAP_MAX_X - max_x_offset < target_x) {
            target_x = MOX::MAP_MAX_X - max_x_offset;
        }

        if (target_y < 0) {
            target_y = 0;
        }

        int16_t max_y_offset = HAROLD::Get_Up_Scaled_Value(400 /* 0x190 */);
        if (MOX::MAP_MAX_Y - max_y_offset < target_y) {
            target_y = MOX::MAP_MAX_Y - max_y_offset;
        }

        int16_t old_x = MOX::cur_map_x;
        int16_t old_y = MOX::cur_map_y;
        int32_t distance = special::Range(old_x, old_y, target_x, target_y);

        // Check if movement is significant (over 50 pixels) and coordinates actually changed
        if ((int16_t)distance > 50 && (target_x != old_x || target_y != old_y)) {
            int16_t num_steps = (int16_t)distance / 50;
            video::Set_Refresh_Full();

            if (num_steps > 0) {
                int16_t count = num_steps;
                do {
                    MAINSCR::Get_New_Paralax_XYs(target_x, target_y, num_steps);
                    special::Interpolate_Line(&MOX::cur_map_x, &MOX::cur_map_y, target_x, target_y, 50);

                    MOX::animated_star = -1;
                    video::Set_Page_Off();
                    MAINSCR::Draw_Main_Screen_Filled(0);
                    MAINSCR::Draw_Main_Screen();
                    RUSS::Mox_Sync_Update();
                    count--;
                } while (count != 0);
            }

            MOX::animated_star = -1;
            video::Set_Page_Off();
            MAINSCR::Draw_Main_Screen_Filled(1);
            video::Set_Refresh_Stencil();
        } else {
            // Immediate update for small or no movement
            MOX::cur_map_x = target_x;
            MOX::draw_main_screen_to_back = 1;
            MOX::animated_star = -1;
            MOX::cur_map_y = target_y;
        }
    }

    void* __cdecl Allocate_First_Screen_Seg(int32_t unused_param) {
        (void)unused_param;
        // Allocate 76,000 bytes (0x128E0) for other ships
        MOX::other_ships_screen_seg = allocate::Allocate_First_Block(MOX::screen_seg, 76000 /* 0x128E0 */);

        // Update block metrics (return value is unused in assembly)
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        // Determine space required for a 60x60 bitmap and allocate it next
        uint32_t bitmap_size = bitmap::Get_Bitmap_Size(60 /* 0x3C */, 60 /* 0x3C */);
        MOX::global_bitmap_seg = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);

        return MOX::global_bitmap_seg;
    }

    bool __cdecl Ship_Can_Be_Selected(int16_t ship_idx) {
        return HACCESS::Player_Can_Order_Ship(MOX::PLAYER_NUM, &MOX::ship[ship_idx]);
    }

    void __cdecl Do_Next_Turn(int16_t* exit_flag, int8_t auto_mode) {
        MOX::at_least_one_report = 0;
        MOX::previous_report = 0;
        MOX::just_moved_stack = 0;
        MOX::animated_star = -1;

        warn_about_cheaters = 0xFF;

        if (MOX::game_type == 1 /* Hotseat */) {
            Save_Hotseat_Map_Info();
            if (MOX::game_type == 1) {
                MOX::player[MOX::PLAYER_NUM].player_done_flags = 1;
                MOX::current_screen = SCREEN_HOTSEAT_SELECT_PLAYER;
            } else {
                if (MOX::game_type == 2 || MOX::game_type == 3) {
                    MOX::current_screen = SCREEN_NET_NEXT_TURN;
                } else {
                    MOX::current_screen = SCREEN_MAIN_NEXT_TURN;
                }
                MOX::return_screen = SCREEN_REPORTS;
            }
        } else {
            if (MOX::game_type == 2 || MOX::game_type == 3) {
                MOX::current_screen = SCREEN_NET_NEXT_TURN;
            } else {
                MOX::current_screen = SCREEN_MAIN_NEXT_TURN;
            }
            MOX::return_screen = SCREEN_REPORTS;
        }

        Set_Reload_Fleet_Box_Palette_Flag();
        SYS::Clear_System_Display_Loaded_Flag();

        if (auto_mode == 0) {
            in_auto_next_turn_mode = 0;
            fields::Clear_Fields();
            Clear_Main_Screen_Fields();

            if (MOX::settings.end_of_turn_wait == 0) {
                MOX::global_exit_field = fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41);
            }
        } else {
            in_auto_next_turn_mode = 1;
            MISC::Increment_Mouse_Clock_Image();
            int16_t input = fields::Get_Input();
            MOX::auto_turn_exit = (uint8_t)input;

            if (MOX::auto_turn_exit != 0) {
                check_next_turn_exit_field = 0;
                mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                next_turn_hit = 0;
                fields::Clear_Fields();
                MOX::current_screen = SCREEN_MAIN;
            }
        }

        if (auto_mode == 0 || MOX::auto_turn_exit == 0) {
            BUILD_QUEUE::Apply_Custom_Autobuild_For_Player(MOX::PLAYER_NUM);
        }
        *exit_flag = 1;
    }

    void __cdecl Draw_Player_Done_Lights(void) {
        int8_t all_humans_ready_flag = 0;
        int16_t n_humans = HAROLD::Get_N_Remaining_Humans();
        int16_t done_count = 0;

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::game_type == 1 /* GAME_HOTSEAT */) {
                if ((int16_t)MULTPLAY::Hotseat_Player_Status(i) == 1) {
                    done_count++;
                }
            } else if (MOX::game_type == 2 || MOX::game_type == 3) {
                if (NETMOX::net.players_hit_next_turn[i] != 0) {
                    done_count++;
                }
            }
        }

        if ((n_humans - 1) <= done_count) {
            all_humans_ready_flag = 1;
        }

        int16_t draw_idx = 0;
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            bool should_draw = false;

            if (MOX::game_type == 2 || MOX::game_type == 3) {
                if (NETMOX::net.players_hit_next_turn[i] != 0) {
                    should_draw = true;
                }
            } else if (MOX::game_type == 1) {
                if ((int16_t)MULTPLAY::Hotseat_Player_Status(i) == 1) {
                    should_draw = true;
                }
            }

            if (should_draw) {
                int8_t color = MOX::player[i].color;
                int16_t anim_width = animate::Get_Width(MAINSCR::player_lights_seg[0]);
                int16_t x_pos = (anim_width * draw_idx) + 0x49;

                if (all_humans_ready_flag == 0) {
                    animate::Set_Animation_Frame(MAINSCR::player_lights_seg[color], 5);
                }

                animate::Draw(x_pos, 5, MAINSCR::player_lights_seg[color]);
                draw_idx++;
            }
        }
    }

    void __cdecl Draw_Game_Turn_Buttons() {
        animate::Reset_Animation_Frame(MOX::main_next_button_seg);
        animate::Draw(544, 441, MOX::main_next_button_seg);

        animate::Reset_Animation_Frame(MOX::main_game_button_seg);
        animate::Draw(249, 5, MOX::main_game_button_seg);
    }

    void __cdecl Draw_Main_Screen_Before_Stencil() {
        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        MAINSCR::Draw_Main_Main_Screen();
        video::Copy_Off_To_Back();

        for (int16_t i = 4; i >= 0; --i) {
            int16_t box_id = MOX::moveable_box_order[i];
            if (MOVEBOX::Moveable_Box_Selected(box_id) != 0) {
                if (MOX::stencil_box_id != box_id) {
                    int16_t mouse_btn = mouse::Mouse_Button();
                    MOVEBOX::Move_Moveable_Box(box_id, mouse_btn);
                    MOVEBOX::Draw_Moveable_Box(box_id);
                }
            }
        }

        MAINSCR::Draw_Game_Turn_Buttons();
        MOVEBOX::Draw_Moveable_Box(MOX::stencil_box_id);
    }

    void __cdecl Scan_Ships_XY(int16_t mouse_x, int16_t mouse_y) {
        bool found = false;
        char icon_label[80];

        for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
            if (found) {
                return;
            }

            int16_t ship_idx = MOX::ship_node[MOX::ship_icon[i].node_idx].ship_idx;
            int16_t owner = HACCESS::Ship_Owner(ship_idx);
            s_animation_header* pict_seg = SHIPS::Get_Ship_Icon_Pict_Seg(owner, 0);

            if (fields::field_box_mode == 1) {
                int16_t h = animate::Get_Height(pict_seg);
                int16_t w = animate::Get_Width(pict_seg);
                graphics::Box(MOX::ship_icon[i].x, MOX::ship_icon[i].y, w, h, 1, 10 /* 0xA */);
            }

            int16_t icon_x = MOX::ship_icon[i].x;
            int16_t icon_y = MOX::ship_icon[i].y;
            int16_t width = animate::Get_Width(pict_seg);
            int16_t height = animate::Get_Height(pict_seg);

            if (mouse_x >= icon_x && mouse_x <= (icon_x + width) &&
                mouse_y >= icon_y && mouse_y <= (icon_y + height)) {
                if (!MAINSCR::moving_a_stack || MOX::mouse_XY.hovering_star_idx == -1) {
                    MOX::mouse_XY.hovering_ship_icon_idx = i;
                    MOX::mouse_XY.selected = 0;
                    snprintf(icon_label, sizeof(icon_label), "ship icon %d", (int)i);
                    MOX::mouse_XY.hovering_star_idx = -1;
                }
                found = true;
            }
        }
    }

    /**
     * Cycles through stars to find the next or previous one where the player has a colony.
     * 
     * @param p_star_idx Pointer to the current star index; updated to the new star index if found.
     * @param player_idx The player whose colonies are being searched.
     * @param direction  Direction to cycle (0 for next, non-zero for previous).
     */
    void __cdecl Cycle_Star_Icons(int16_t* p_star_idx, int16_t player_idx, int16_t direction) {
        bool found = false;
        int16_t current_idx = *p_star_idx;

        // Search from current position to the edge of the star list
        while (true) {
            if (direction == 0 /* NEXT */) {
                current_idx++;
            } else {
                current_idx--;
            }

            bool in_bounds;
            if (direction == 0 /* NEXT */) {
                in_bounds = (int16_t)MOX::NUM_STARS > current_idx;
            } else {
                in_bounds = current_idx > -1;
            }

            if (!in_bounds) {
                break;
            }

            if (found) {
                return;
            }

            if (HAROLD::Player_Has_Colony(player_idx, current_idx)) {
                *p_star_idx = current_idx;
                found = true;
            }
        }

        if (found) {
            return;
        }

        // Wrap around to the other side of the list if nothing was found yet
        if (direction == 0 /* NEXT */) {
            current_idx = 0;
        } else {
            current_idx = (int16_t)MOX::NUM_STARS - 1;
        }

        while (true) {
            bool in_bounds;
            if (direction == 0 /* NEXT */) {
                in_bounds = (int16_t)MOX::NUM_STARS > current_idx;
            } else {
                // Backward wrap check ensures we don't pass the original star index
                in_bounds = (*p_star_idx - 1) < current_idx;
            }

            if (!in_bounds) {
                break;
            }

            if (found) {
                return;
            }

            if (HAROLD::Player_Has_Colony(player_idx, current_idx)) {
                *p_star_idx = current_idx;
                found = true;
            }

            if (direction == 0 /* NEXT */) {
                current_idx++;
            } else {
                current_idx--;
            }
        }

        if (!found) {
            *p_star_idx = -1;
        }
    }

    void __cdecl Scan_Stars_XY(int16_t mouse_x, int16_t mouse_y) {
        int32_t star_range_by_size[3];
        star_range_by_size[0] = 12;
        star_range_by_size[1] = 10;
        star_range_by_size[2] = 8;

        bool star_processed = false;
        moving_a_stack = 0;

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (star_processed) {
                return;
            }

            int16_t star_draw_x, star_draw_y;
            Get_Star_Draw_Coords(star_idx, &star_draw_x, &star_draw_y);

            int16_t box_size;

            if (MOX::star[star_idx].spectral_class == STAR_CLASS_BLACK_HOLE) {
                static const int16_t zoom_box_sizes[4] = {39, 33, 33, 24};
                box_size = Scaled_Star_Map_Dimension(zoom_box_sizes[HAROLD::Map_Scale_To_Zoom_Level()]);
            } else {
                box_size = HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx);
            }

            int32_t dx = (int32_t)star_draw_x - (int32_t)mouse_x;
            int32_t dy = (int32_t)star_draw_y - (int32_t)mouse_y;
            int32_t dist_sq = (dx * dx) + (dy * dy);

            int32_t range = Scaled_Star_Map_Dimension(
                static_cast<int16_t>(star_range_by_size[(int32_t)MOX::star[star_idx].size]));
            if (dist_sq <= (range * range) && MOX::mouse_XY.hovering_ship_icon_idx == -1) {
                s_colors font_colors;
                font_colors.colors[0] = 0;
                font_colors.colors[1] = 10;
                font_colors.colors[2] = 12;
                font_colors.colors[3] = 14;
                font_colors.colors[4] = 16;
                font_colors.colors[5] = 18;
                font_colors.colors[6] = 20;
                font_colors.colors[7] = 22;

                if (FLEETPOP::Some_Icon_Selected()) {
                    int16_t selected_ship_ids[SHIP_LIST_CAPACITY];
                    HACCESS::Get_Fleet_Box_Selected_Ship_Ids(selected_ship_ids, SHIP_LIST_CAPACITY, 0xFF);
                    SHIPMOVE::Ships_Try_To_Move_To(selected_ship_ids, star_idx, &MOX::g_ship_move_info);
                    moving_a_stack = 1;
                }

                MOX::mouse_XY.selected = 0;
                MOX::mouse_XY.hovering_ship_icon_idx = -1;
                MOX::mouse_XY.hovering_star_idx = star_idx;

                fonts::Set_Font_Style(2, &font_colors);
                fonts::Set_Font_Spacing(1);

                s_star_data* star_ptr = &MOX::star[star_idx];
                int16_t text_width = fonts::Get_String_Width(star_ptr->name);

                char buffer[80];

                snprintf(buffer, sizeof(buffer), "%s, len = %d", star_ptr->name, (int32_t)text_width);
                star_processed = true;
            }

            if (fields::field_box_mode == 1) {
                graphics::Box(star_draw_x - (box_size / 2), star_draw_y - (box_size / 2), box_size, box_size, 1, 10);
            }
        }
    }

    int16_t __cdecl Scan_Galaxy_Map_Fields(int16_t field_id_1, int16_t field_id_2, int16_t* star_idx_ptr_1, int16_t* star_idx_ptr_2) {
        int16_t result = -1;
        *star_idx_ptr_1 = -1;
        *star_idx_ptr_2 = -1;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (MOX::galaxy_map_star_field[i] == field_id_2) {
                result = 0;
                *star_idx_ptr_2 = i;
                break;
            }
        }

        if (*star_idx_ptr_2 == -1) {
            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                if (MOX::galaxy_map_star_field[i] == field_id_1 && MOX::star[i].spectral_class != STAR_CLASS_BLACK_HOLE) {
                    result = 4;
                    *star_idx_ptr_1 = i;
                    break;
                }
            }
        }

        if (result == -1 && MOX::galaxy_map_grid_field == field_id_2) {
            result = 2;
        }

        return result;
    }

    void __cdecl Check_Function_Keys(int16_t key, int16_t* flag) {
        int16_t colony_id = -1;
        char msg_buffer[200];

        if (key == -1110 /* 0xfbaa */) {
            return;
        }

        if (key > -1109 && key < -1100 /* 0xfbb3 */) {
            MAINSCR::Toggle_Game_Option(-1101 - key);
        }

        int32_t switch_idx = (int32_t)key + 1010;
        if (switch_idx > 9) {
            return;
        }

        switch (switch_idx) {
            case 0: /* DI = -1010 */
                if (MOX::disallow_saves_code == 0) {
                    FILEDEF::Save_Game(9);
                    strlcpy(msg_buffer, HAROLD::H_Message(211 /* HESTR_0D3_GAME_SAVED */), sizeof(msg_buffer));
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, msg_buffer, 0);
                } else {
                    strlcpy(msg_buffer, HAROLD::H_Message(212 /* HESTR_0D4_SAVE_GAME_FUNCTION_DIS */), sizeof(msg_buffer));
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, msg_buffer, 0);
                }
                break;

            case 1: /* DI = -1009 */
                MOX::current_screen = SCREEN_DISTANCE_BETWEEN_STARS;
                MOX::return_screen = SCREEN_MAIN;
                *flag = 1;
                break;

            case 4: /* DI = -1006 (Next Star) */
                MAINSCR::Cycle_Star_Icons(&MOX::system_star_id_for_auto_select, MOX::PLAYER_NUM, 0);
                if (MOX::system_star_id_for_auto_select > -1 && MOX::system_star_id_for_auto_select < MOX::NUM_STARS) {
                    if (MOX::settings.auto_select_colony != 0) {
                        colony_id = HACCESS::Last_Colony_Selected(MOX::system_star_id_for_auto_select, MOX::PLAYER_NUM);
                    }
                    if (colony_id > -1) {
                        MAINSCR::Do_Colony_Screen(colony_id, flag);
                    } else {
                        MOX::moveable_box[0].id = MOX::system_star_id_for_auto_select;
                        MAINSCR::Do_System_Popup(MOX::system_star_id_for_auto_select);
                    }
                }
                break;

            case 5: /* DI = -1005 (Prev Star) */
                MAINSCR::Cycle_Star_Icons(&MOX::system_star_id_for_auto_select, MOX::PLAYER_NUM, 1);
                if (MOX::system_star_id_for_auto_select > -1 && MOX::system_star_id_for_auto_select < MOX::NUM_STARS) {
                    if (MOX::settings.auto_select_colony != 0) {
                        colony_id = HACCESS::Last_Colony_Selected(MOX::system_star_id_for_auto_select, MOX::PLAYER_NUM);
                    }
                    if (colony_id > -1) {
                        MAINSCR::Do_Colony_Screen(colony_id, flag);
                    } else {
                        MOX::moveable_box[0].id = MOX::system_star_id_for_auto_select;
                        MAINSCR::Do_System_Popup(MOX::system_star_id_for_auto_select);
                    }
                }
                break;

            case 6: /* DI = -1004 (Relocations) */
            {
                int16_t reloc_count = HACCESS::N_Star_Relocations(MOX::PLAYER_NUM);
                if (reloc_count > 1) {
                    if (MAINSCR::merging_relocations != 0) {
                        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                        MAINSCR::merging_relocations = 0;
                    } else {
                        mouse::Set_Mouse_List(mouse_list_relocation, static_cast<int16_t>(std::size(mouse_list_relocation)));
                        MAINSCR::merging_relocations = 1;
                    }
                } else {
                    int16_t msg_id = (reloc_count == 1) ? 209 /* 0xD1 */ : 210 /* 0xD2 */;
                    strlcpy(msg_buffer, HAROLD::H_Message(msg_id), sizeof(msg_buffer));
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, msg_buffer, 0);
                }
                break;
            }

            case 8: /* DI = -1002 (Next Ship) */
            {
                int16_t ship_idx;
                MAINSCR::Cycle_Ship_Icons(&MOX::fleet_box_ship_stack, &ship_idx, 0);
                if (ship_idx > -1 && ship_idx < MOX::ship_icon_count) {
                    MAINSCR::Do_Fleet_Popup(ship_idx, 0);
                }
                break;
            }

            case 9: /* DI = -1001 (Prev Ship) */
            {
                int16_t ship_idx;
                MAINSCR::Cycle_Ship_Icons(&MOX::fleet_box_ship_stack, &ship_idx, 1);
                if (ship_idx > -1 && ship_idx < MOX::ship_icon_count) {
                    MAINSCR::Do_Fleet_Popup(ship_idx, 0);
                }
                break;
            }

            default:
                break;
        }
    }

    int32_t __cdecl Zoom_Box_Width(int16_t width, int16_t zoom_factor) {
        return (width * 506 /* 0x1fa */) / zoom_factor;
    }

    int32_t __cdecl Zoom_Box_Height(int16_t height, int16_t divisor) {
        return (height * 400) / divisor;
    }

    void __cdecl Draw_Main_Screen_For_Zooming() {
        s_colors color_table;
        uint32_t saved_seed;
        int16_t width;
        int16_t height;

        saved_seed = game_random::Get_Random_Seed();

        for (int16_t i = 0; i < 8; ++i) {
            if (i < 2 || i > 5) {
                color_table.colors[i] = 0x0C;
            } else if (i == 2 || i == 5) {
                color_table.colors[i] = 0x10;
            } else {
                color_table.colors[i] = 0x13;
            }
        }

        if (mouse::Pointer_X() <= 22) {
            zoom_box_x = 22;
        } else {
            zoom_box_x = mouse::Pointer_X();
        }

        if (mouse::Pointer_Y() <= 22) {
            zoom_box_y = 22;
        } else {
            zoom_box_y = mouse::Pointer_Y();
        }

        width = (int16_t)Zoom_Box_Width(HAROLD::Get_Map_Scale(MOX::zoom_count), MOX::cur_map_scale);
        height = (int16_t)Zoom_Box_Height(HAROLD::Get_Map_Scale(MOX::zoom_count), MOX::cur_map_scale);

        if (width + zoom_box_x >= 536) {
            zoom_box_x = 526 - width;
        }

        if (height + zoom_box_y >= 420) {
            zoom_box_y = 420 - height;
        }

        graphics::Set_Window(zoom_box_x, zoom_box_y, zoom_box_x + width, zoom_box_y + height);
        video::Clip_On();
        animate::Draw(22, 22, (s_animation_header*)temp_bitmap);

        HAROLD::Draw_Multi_Colored_Box(
            zoom_box_x,
            zoom_box_y,
            zoom_box_x + width,
            zoom_box_y + height,
            &color_table,
            static_cast<uint16_t>(zoom_box_color_increment));

        zoom_box_counter++;
        if ((zoom_box_counter % 3) == 0) {
            zoom_box_color_increment += zoom_box_increment_value;
        }

        if ((zoom_box_counter % 12) == 0) {
            zoom_box_counter = 0;
            zoom_box_increment_value = 0xFF; /* -1 */
        }

        video::Clip_Off();
        graphics::Reset_Window();
        game_random::Set_Random_Seed(saved_seed);
    }

    void __cdecl Toggle_Game_Option(int16_t option_idx) {
        if (option_idx < 0 || option_idx > 7 || option_idx == 3) {
            return;
        }

        char option_name[32];
        char message[100];
        int8_t status_flag;

        switch (option_idx) {
            case 0:
                MOX::settings.end_of_turn_summary = (MOX::settings.end_of_turn_summary == 0);
                strlcpy(option_name, HAROLD::H_Message(0xa6), sizeof(option_name));
                status_flag = MOX::settings.end_of_turn_summary;
                break;

            case 1:
                MOX::settings.end_of_turn_wait = (MOX::settings.end_of_turn_wait == 0);
                strlcpy(option_name, HAROLD::H_Message(0xa7), sizeof(option_name));
                status_flag = MOX::settings.end_of_turn_wait;
                break;

            case 2:
                MOX::settings.enemy_moves = (MOX::settings.enemy_moves == 0);
                strlcpy(option_name, HAROLD::H_Message(0xa8), sizeof(option_name));
                status_flag = MOX::settings.enemy_moves;
                break;

            case 4:
                MOX::settings.auto_select_ships = (MOX::settings.auto_select_ships == 0);
                strlcpy(option_name, HAROLD::H_Message(0xaa), sizeof(option_name));
                status_flag = MOX::settings.auto_select_ships;
                break;

            case 5:
                MOX::settings.animations_on = (MOX::settings.animations_on == 0);
                strlcpy(option_name, HAROLD::H_Message(0xab), sizeof(option_name));
                status_flag = MOX::settings.animations_on;
                break;

            case 6:
                MOX::settings.auto_select_colony = (MOX::settings.auto_select_colony == 0);
                strlcpy(option_name, HAROLD::H_Message(0xac), sizeof(option_name));
                status_flag = MOX::settings.auto_select_colony;
                if (status_flag != 0) {
                    HACCESS::Set_All_Colony_Auto_Select_Flags();
                }
                break;

            case 7:
                MOX::settings.show_relocation_lines = (MOX::settings.show_relocation_lines == 0);
                strlcpy(option_name, HAROLD::H_Message(0xad), sizeof(option_name));
                status_flag = MOX::settings.show_relocation_lines;
                break;

            default:
                return;
        }

        const char* status_text = HAROLD::H_Message(status_flag ? 0xe7 /* ON */ : 0xe8 /* OFF */);

        snprintf(message, sizeof(message), "%s %s", option_name, status_text);

        LOADSAVE::Save_Session_Related_Settings();
        HAROLD::User_Box(&MOX::draw_main_screen_to_back, message, 0);
    }

    int16_t __cdecl Scan_Fleet_Movement_Box_Fields(int16_t field_id_a, int16_t field_id_b, int16_t* out_chosen_offset, int16_t* out_scanned_icon_idx) {
        int16_t result = -1;
        bool found = false;
        int16_t counter = 0;

        *out_scanned_icon_idx = -1;
        *out_chosen_offset = -1;

        int limit = static_cast<int>(MOX::fleet_icon_buttons_added) + static_cast<int>(MOX::first_fleet_movement_icon);
        if (static_cast<int>(MOX::fleet_box.count) < limit) {
            limit = static_cast<int>(MOX::fleet_box.count);
        }

        int16_t i = MOX::first_fleet_movement_icon;
        while (i < static_cast<int16_t>(limit) && !found) {
            if (FLEETPOP::Can_Select_Fleet_Box_Ship(i)) {
                if (MOX::fleet_movement_icon[i].field == field_id_b) {
                    found = true;
                    result = 0;
                    *out_chosen_offset = counter;
                    *out_scanned_icon_idx = i;
                } else if (MOX::fleet_movement_icon[i].field + field_id_b == 0) {
                    *out_chosen_offset = counter;
                    found = true;
                    result = 1;
                }
            }
            counter++;
            i++;
        }

        if (result == -1) {
            // Redundantly recalculate limit as per assembly
            int limit2 = static_cast<int>(MOX::fleet_icon_buttons_added) + static_cast<int>(MOX::first_fleet_movement_icon);
            if (static_cast<int>(MOX::fleet_box.count) < limit2) {
                limit2 = static_cast<int>(MOX::fleet_box.count);
            }

            for (int16_t j = MOX::first_fleet_movement_icon; j < static_cast<int16_t>(limit2); ++j) {
                if (MOX::fleet_movement_icon[j].field == field_id_a) {
                    *out_scanned_icon_idx = j;
                    return 4;
                }
            }
        }

        return result;
    }

    void __cdecl Scan_Moveable_Box_Fields(int16_t mouse_input, int16_t input_field, int32_t scan_input, int16_t* out_exit_flag) {
        for (int16_t box_idx = 0; box_idx < 5; ++box_idx) {
            if (!MOVEBOX::Moveable_Box_Selected(box_idx)) {
                continue;
            }

            if (MOX::moveable_box[box_idx].close_btn == input_field) {
                if (mouse_input == 0) {
                    if (box_idx == 0) {
                        video::Set_Page_Off();
                        Draw_Main_Screen();
                        RUSS::Mox_Sync_Update();
                        MOVEBOX::Vanish_System_Display_Box();
                    } else if (box_idx == 2) {
                        HACCESS::Clear_Fleet_Box_Selected_Ship_Status();
                        FLEETPOP::Clear_Ship_Node_Selection_Flags();
                        MOX::draw_main_screen_to_back = 1;
                        MOVEBOX::Vanish_Fleet_Movement_Box();
                    } else {
                        MOX::moveable_box[box_idx].id = MOX::box_off_flag[box_idx];
                    }
                    continue;
                }
            } else {
                if (mouse_input == 0 && box_idx != MOX::moveable_box_order[0]) {
                    if (MOVEBOX::Box_Field_Selected(box_idx, input_field)) {
                        MOVEBOX::Move_Box_To_Top(MOX::moveable_box[box_idx].index);
                    }
                }
            }

            if (MOX::moveable_box[box_idx].index == 1) {
                int16_t scanned_star_id;
                int16_t center_star_id;
                int16_t scan_res = Scan_Galaxy_Map_Fields((int16_t)scan_input, input_field, &scanned_star_id, &center_star_id);
                MOX::galaxy_map_scanned_star = scanned_star_id;

                if (HAROLD::Map_Scale_To_Zoom_Count() != MOX::max_zoom_count) {
                    if (scan_res == 2) {
                        int16_t map_x = (int16_t)(((((int32_t)MOX::galaxy_map_grid_y * MOX::max_map_scale) / 10) * (int16_t)(5060 / MOX::galaxy_map_height)) / 10);
                        int16_t map_y = (int16_t)(((((int32_t)MOX::galaxy_map_grid_x * MOX::max_map_scale) / 10) * (int16_t)(4000 / MOX::galaxy_map_width)) / 10);
                        Center_Map(map_x, map_y);
                    } else if (scan_res == 0) {
                        int16_t map_x = MOX::star[center_star_id].x;
                        int16_t map_y = MOX::star[center_star_id].y;
                        Center_Map(map_x, map_y);
                    }
                }
            }

            if (MOVEBOX::Moveable_Box_Selected(2) && MOX::moveable_box[box_idx].index == 2) {
                if (input_field == MOX::fleet_box_scroll_up_button) {
                    SCROLL::Scroll_Up_One_Item(&MOX::fltpop_scroll_bar);
                } else if (input_field == MOX::fleet_box_scroll_dn_button) {
                    SCROLL::Scroll_Down_One_Item(&MOX::fltpop_scroll_bar);
                }

                if (input_field == MOX::fleet_movement_all_button) {
                    if (MOX::fleet_movement_icons_on == 1 || FLEETPOP::Fleet_Buttons_Chosen() == MOX::fleet_box.count) {
                        FLEETPOP::Set_Fleet_Movement_Icon_Selections(0, 0);
                        HACCESS::Clear_Fleet_Box_Selected_Ship_Status();
                        MOX::eta_star_id = -1;
                    } else {
                        FLEETPOP::Set_Fleet_Movement_Icon_Selections(1, 0);
                    }
                    continue;
                }

                int16_t scanned_ship_idx;
                int16_t scan_res = Scan_Fleet_Movement_Box_Fields(scan_input, input_field, &fleet_icon_chosen, &scanned_ship_idx);
                MOX::fleet_ship_scanned = -1;
                if (scanned_ship_idx > -1) {
                    MOX::eta_star_id = -1;
                    MOX::fleet_ship_scanned = scanned_ship_idx;
                }

                if (scan_res == 1) {
                    fields::Deactivate_Help_List();
                    int16_t ship_id = HACCESS::Fleet_Movement_Ship_Id(fleet_icon_chosen + MOX::first_fleet_movement_icon);
                    CMBTDRW1::Detailed_View_Ship(ship_id);
                    EVANHELP::Set_Main_Screen_Help_List();
                    MOX::draw_main_screen_to_back = 1;
                }

                if (MOX::fleet_box.colonize_btn_active != 0 && MOX::fleet_box_colonize_button == input_field) {
                    MOX::eta_star_id = -1;
                    MOX::current_screen = SCREEN_COLONIZATION_IN_MAIN;
                    MOX::return_screen = SCREEN_MAIN;
                    *out_exit_flag = 1;
                    continue;
                }

                if (MOX::fleet_box.transport_btn_active != 0 && MOX::fleet_box_transport_button == input_field) {
                    if (FLEETPOP::At_Least_One_Ship_Of_Type_Selected(SHIP_TYPE_TRANSPORT) != 0) {
                        MOX::current_screen = SCREEN_UNLOADING_TRANSPORTS_IN_MAIN;
                        MOX::return_screen = SCREEN_MAIN;
                        *out_exit_flag = 1;
                    } else {
                        char no_transport_ships_message[200];
                        strlcpy(no_transport_ships_message, HAROLD::H_Message(HESTR_0E3_NO_TRANSPORT_SHIPS_SEL), sizeof(no_transport_ships_message));
                        HAROLD::User_Box(&MOX::draw_main_screen_to_back, no_transport_ships_message, 0);
                    }
                    continue;
                }

                if (MOX::fleet_box.outpost_btn_active != 0 && MOX::fleet_box_outpost_button == input_field) {
                    MOX::current_screen = SCREEN_BUILDING_OUTPOSTS_IN_MAIN;
                    MOX::return_screen = SCREEN_MAIN;
                    *out_exit_flag = 1;
                    continue;
                }

                if (MOX::fleet_box.engage_btn_active != 0 && MOX::fleet_box_engage_button == input_field) {
                    int16_t star_id = HACCESS::Ship_Stack_Star_Id(MOX::ship_stack_start[MOX::fleet_box_ship_stack]);
                    if (star_id > -1 && star_id < MOX::NUM_STARS) {
                        MOX::star[star_id].ignore_combat_ships &= ~(1 << MOX::PLAYER_NUM);
                    }
                    continue;
                }

                if (MOX::fleet_box.attack_btn_active != 0 && MOX::fleet_box_attack_button == input_field && MOX::fleet_box_attack_button > 0) {
                    if (!FLEETPOP::Some_Icon_Selected()) {
                        char no_ships_selected_message[200];
                        strlcpy(no_ships_selected_message, HAROLD::H_Message(0xE6 /* HESTR_0E6_NO_SHIPS_SELECTED */), sizeof(no_ships_selected_message));
                        HAROLD::User_Box(&MOX::draw_main_screen_to_back, no_ships_selected_message, 3);
                    } else {
                        int16_t selected_ship_ids[MAX_SHIPS];
                        int16_t count = HACCESS::Get_Fleet_Box_Selected_Ship_Ids(selected_ship_ids, MAX_SHIPS, 255);
                        int16_t ship_id = HACCESS::Fleet_Movement_Ship_Id(0);

                        int16_t msg_idx = (count == 1) ? 0xE4 : 0xE5;
                        char confirmation_message[100];

                        snprintf(confirmation_message, sizeof(confirmation_message), HAROLD::H_Message(msg_idx), count);
                        if (HAROLD::User_Box(&MOX::draw_main_screen_to_back, confirmation_message, 1) != 0) {
                            int16_t num_deployed = SHIPS::Deploy_Ships_To_Attack_Antares();
                            MOX::first_node_ptr_in_stack_ = -1;
                            Update_Ship_Icons_In_Between_Turns();
                            FLEETPOP::Set_Fleet_Movement_Icon_Selections(0, 0);
                            HACCESS::Clear_Fleet_Box_Selected_Ship_Status();

                            if (MOX::fleet_box.count == num_deployed) {
                                MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Id(ship_id);
                                if (MOX::fleet_box_ship_stack == -1) {
                                    MOVEBOX::Vanish_Fleet_Movement_Box();
                                }
                            }
                        }
                    }
                    continue;
                }
            }
        }
    }

    void __cdecl Clear_Main_Screen_Fields() {
        zoom_in_button = -1000;
        zoom_out_button = -1000;
        game_button = -1000;
        races_button = -1000;
        fleets_button = -1000;
        colonies_button = -1000;
        design_button = -1000;
        info_button = -1000;
        next_button = -1000;
        MOX::system_display_grid_field = -1000;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            MOX::star_button[i] = -1000;
        }

        for (int16_t i = 0; i < 5; ++i) {
            MOX::planet_buttons[i] = -1000;
        }

        for (int16_t i = 0; i < 8; ++i) {
            MOX::system_display_ships[i].button = -1000;
        }

        MOX::main_screen_grid_field = -1000;
        debug_field = -1000;
        MOX::galaxy_map_grid_field = -1000;
        MOX::fleet_movement_all_button = -1000;
        MOX::fleet_box_colonize_button = -1000;
        MOX::fleet_box_outpost_button = -1000;
        MOX::fleet_box_transport_button = -1000;
        MOX::fleet_box_engage_button = -1000;
        MOX::fleet_box_attack_button = -1000;
        influence_hot_key = -1000;
        all_tech_cheat = -1000;
        leader_cheat = -1000;
        more_money_cheat = -1000;
        get_tech_cheat = -1000;
        omniscience_cheat = -1000;
        all_ai_cheat = -1000;
        cycle_player_cheat = -1000;
        reload_config_command = -1000;
        reload_interface_command = -1000;
    }

    void __cdecl Cycle_Ship_Icons(int16_t* current_stack_idx_ptr, int16_t* out_icon_idx, int16_t direction) {
        int16_t target_player_num;
        int16_t result_stack_idx = -1;
        int16_t result_icon_idx = -1;
        int16_t start_node_idx = -1;
        bool found_stack = false;
        bool found_icon = false;

        if (MOX::fleet_box_ship_stack > 0 && MOX::fleet_box_ship_stack < MAX_SHIPS && MOVEBOX::Moveable_Box_Selected(2)) {
            target_player_num = HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[MOX::fleet_box_ship_stack]);
        } else {
            target_player_num = MOX::PLAYER_NUM;
        }

        if (MOX::ship_icon_count > 0 && *current_stack_idx_ptr != -1) {
            int16_t stack_idx = *current_stack_idx_ptr;

            // Search from current position
            if (direction == 0 /* Forward */) {
                stack_idx++;
                while (stack_idx < MOX::ship_stack_count) {
                    start_node_idx = MOX::ship_stack_start[stack_idx];
                    int16_t ship_idx = MOX::ship_node[start_node_idx].ship_idx;
                    if (HACCESS::Ship_Owner(ship_idx) == target_player_num && MOX::ship[ship_idx].status < 3) {
                        found_stack = true;
                        result_stack_idx = stack_idx;
                        break;
                    }
                    stack_idx++;
                }
            } else /* Backward */ {
                stack_idx--;
                while (stack_idx > -1) {
                    start_node_idx = MOX::ship_stack_start[stack_idx];
                    int16_t ship_idx = MOX::ship_node[start_node_idx].ship_idx;
                    if (HACCESS::Ship_Owner(ship_idx) == target_player_num && MOX::ship[ship_idx].status < 3) {
                        found_stack = true;
                        result_stack_idx = stack_idx;
                        break;
                    }
                    stack_idx--;
                }
            }

            // Wrap around if not found
            if (!found_stack) {
                if (direction == 0 /* Forward */) {
                    stack_idx = 0;
                    while (stack_idx < MOX::ship_stack_count) {
                        start_node_idx = MOX::ship_stack_start[stack_idx];
                        int16_t ship_idx = MOX::ship_node[start_node_idx].ship_idx;
                        if (HACCESS::Ship_Owner(ship_idx) == target_player_num && MOX::ship[ship_idx].status < 3) {
                            found_stack = true;
                            result_stack_idx = stack_idx;
                            break;
                        }
                        stack_idx++;
                    }
                } else /* Backward */ {
                    stack_idx = MOX::ship_stack_count - 1;
                    while (stack_idx > (*current_stack_idx_ptr - 1)) {
                        start_node_idx = MOX::ship_stack_start[stack_idx];
                        int16_t ship_idx = MOX::ship_node[start_node_idx].ship_idx;
                        if (HACCESS::Ship_Owner(ship_idx) == target_player_num && MOX::ship[ship_idx].status < 3) {
                            found_stack = true;
                            result_stack_idx = stack_idx;
                            break;
                        }
                        stack_idx--;
                    }
                }
            }
        }

        if (start_node_idx != -1 && MOX::ship_icon_count > 0) {
            for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
                if (MOX::ship_icon[i].node_idx == start_node_idx) {
                    found_icon = true;
                    result_icon_idx = i;
                    break;
                }
            }
        }

        if (found_icon) {
            *out_icon_idx = result_icon_idx;
            *current_stack_idx_ptr = result_stack_idx;
        } else {
            *out_icon_idx = -1;
            *current_stack_idx_ptr = -1;
        }
    }

    void __cdecl Draw_Exploding_Plasma_Box(int16_t x_start, int16_t y_start, int16_t x_box, int16_t y_box, int16_t width, int16_t height, int16_t mode) {
        uint32_t random_seed = game_random::Get_Random_Seed();
        MOX::animated_star = -1;

        if (mode == 2) {
            int16_t node = HACCESS::Node_Ptr_From_Ship_Stack(MOX::fleet_box_ship_stack);
            int16_t n_icons = FLEETPOP::N_Fleet_Icons(node);
            FLEETPOP::Fleet_Box_Data(n_icons);
            width = MOX::fleet_box.width;
            height = MOX::fleet_box.height;
        }

        int16_t saved_window;
        MISC::Save_Window_Status(&saved_window);
        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();

        if (animate::clipping_flag == 1) {
            if (x_box < video::screen_window_x1) {
                x_box = video::screen_window_x1;
            }
            if (y_box < video::screen_window_y1) {
                y_box = video::screen_window_y1;
            }
            if (video::screen_window_x2 < width + x_box) {
                width = video::screen_window_x2 - x_box;
            }
            if (video::screen_window_y2 < height + y_box) {
                height = video::screen_window_y2 - y_box;
            }
        }

        int16_t original_x2 = x_box;
        int16_t original_y2 = y_box;

        fields::Disable_Draw_All_Fields();
        width -= 3;
        height -= 3;
        MOVEBOX::N_Moveable_Boxes_Selected(mode);
        video::Set_Page_Off();
        Draw_Main_Screen_Filled(1);
        graphics::Set_Window(0x16, 0x16, 0x20f, 0x1a5);
        video::Clip_On();
        Draw_Main_Main_Screen();
        video::Copy_Off_To_Back();

        uint8_t plasma_color_map[8];
        for (int i = 0; i < 8; ++i) {
            plasma_color_map[i] = static_cast<uint8_t>((i * 2) + 14);
        }

        int16_t unk_color = (int16_t)game_random::Random(8) - 1;
        int16_t color_anim = 0;

        int16_t dest_x = original_x2;
        int16_t dest_y = original_y2;

        if (x_start < dest_x) {
            if (y_start <= dest_y) {
                dest_x += 3;
                dest_y += 3;
            } else {
                if (dest_y + height <= 0x1a5) {
                    dest_y += height;
                }
                dest_y -= 3;
            }
        } else {
            if (y_start <= dest_y) {
                if (dest_x + width <= 0x20f) {
                    dest_x += width;
                }
                dest_x -= 3;
            } else {
                if (dest_x + width <= 0x20f) {
                    dest_x += width;
                }
                if (dest_y + height <= 0x1cb) {
                    dest_y += height;
                }
                dest_x -= 3;
                dest_y -= 3;
            }
        }

        int16_t step_x = (dest_x - x_start) / 10;
        int16_t step_y = (dest_y - y_start) / 10;
        int16_t accum_x = 0;
        int16_t accum_y = 0;

        for (int i = 0; i < 10; ++i) {
            int16_t w_offset;
            int16_t h_offset;

            if (original_x2 <= x_start) {
                if (y_start <= original_y2) {
                    w_offset = (width * i) / -10;
                    h_offset = (height * i) / 10;
                } else {
                    w_offset = (width * i) / -10;
                    h_offset = (height * i) / -10;
                }
            } else {
                if (y_start <= original_y2) {
                    w_offset = (width * i) / 10;
                    h_offset = (height * i) / 10;
                } else {
                    w_offset = (width * i) / 10;
                    h_offset = (height * i) / -10;
                }
            }

            int16_t box_x = x_start + accum_x + w_offset;
            int16_t box_y = y_start + accum_y + h_offset;

            if (box_x < 0x15) {
                box_x = 0x16;
            }
            if (box_x > 0x20f) {
                box_x = 0x20e;
            }

            if (box_y < 0x15) {
                box_y = 0x16;
            }
            if (box_y > 0x1a5) {
                box_y = 0x1a4;
            }

            int16_t draw_x = box_x - w_offset;
            int16_t draw_y = box_y - h_offset;

            if (i > 0) {
                video::Set_Page_Off();
            }

            line::Plasma_Box(draw_x, draw_y, (uint16_t)w_offset, h_offset, 3, color_anim, (uint8_t)unk_color, plasma_color_map);

            color_anim = (game_random::Random(10) + color_anim) & 0xFF;
            RUSS::Mox_Sync_Update();

            accum_y += step_y;
            accum_x += step_x;
        }

        if (animate::clipping_flag == 1) {
            if (original_x2 < video::screen_window_x1) {
                original_x2 = video::screen_window_x1;
            }
            if (original_y2 < video::screen_window_y1) {
                original_y2 = video::screen_window_y1;
            }
            if (video::screen_window_x2 < original_x2 + width) {
                width = video::screen_window_x2 - original_x2;
            }
            if (video::screen_window_y2 < original_y2 + height) {
                height = video::screen_window_y2 - original_y2;
            }
        }

        line::Plasma_Box(original_x2, original_y2, (uint16_t)width, height, 3, color_anim, (uint8_t)unk_color, plasma_color_map);
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();
        MOX::draw_main_screen_to_back = 1;
        game_random::Set_Random_Seed(random_seed);
        MISC::Restore_Window_Status(saved_window);
    }

    void __cdecl Get_Star_Name_Info(int16_t star_idx, int16_t* out_x, int16_t* out_y, int16_t* out_width) {
        int16_t saved_map_scale;
        int16_t zoom_star_size;
        int16_t string_width;
        int8_t is_visited;
        int16_t star_draw_x;
        int16_t star_draw_y;
        s_animation_header* unused_output = nullptr;
        char name_buffer[40];

        saved_map_scale = MOX::cur_map_scale;
        MOX::cur_map_scale = MOX::max_map_scale;

        HAROLD::Map_Scale_To_Zoom_Level();
        zoom_star_size = HAROLD::Map_Scale_Star_Size_To_Zoom_Level(star_idx);

        HAROLD::Set_Player_Font_Style(0, false);
        fonts::Set_Font_Spacing(1);

        MAINSCR::Get_Star_Name(star_idx, 0, &is_visited, name_buffer, sizeof(name_buffer), &unused_output);
        MAINSCR::Get_Star_Draw_Coords(star_idx, &star_draw_x, &star_draw_y);

        string_width = fonts::Get_String_Width(name_buffer);
        *out_width = string_width;

        // Calculate center-aligned X position
        *out_x = (star_draw_x - (string_width / 2)) + 1;

        // Calculate Y position relative to star size and zoom
        *out_y = (zoom_star_size / 2 - (int16_t)MOX::star[star_idx].size) + star_draw_y;

        MOX::cur_map_scale = saved_map_scale;
    }

    void __cdecl Main_Receive_Message(uint32_t from_player_id, int16_t sender_net_id_or_host_id, uint16_t msg_type, uint32_t payload_len, void* payload) {
        switch (msg_type) {
            case NET_PACKET_START_GAME: {
                if (payload == nullptr || payload_len < sizeof(s_net_pack_start_game)) {
                    exitm::Exit_With_Message("network start packet truncated");
                }
                s_net_pack_start_game pack;
                memcpy(&pack, payload, sizeof(pack));
                if (!Valid_Setup_Version(pack.version_string,
                        sizeof(pack.version_string))) {
                    exitm::Exit_With_Message("Incompatible network version");
                }
                if (orion2re::lua::Has_Pending_Lua_Network_Setup()) {
                    if (!Commit_Pending_Lua_Setup(sender_net_id_or_host_id,
                            orion2re::lua::LUA_NETWORK_SETUP_NEW_GAME,
                            payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "invalid Lua network new-game commit");
                    }
                } else {
                    if (orion2re::lua::Network_Lua_Enabled() ||
                        orion2re::lua::Has_Lua_Network_Setup_Commit(payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "Lua network setup offer is missing");
                    }
                    Apply_Setup_Config(payload, payload_len, sizeof(pack));
                }

                MOX::NUM_PLAYERS = pack.number_of_players;
                MOX::settings.random_events = pack.random_events;
                MOX::settings.game_difficulty = pack.game_difficulty;
                MOX::settings.number_of_players = pack.number_of_players;
                MOX::settings.galaxy_size = pack.galaxy_size;
                MOX::settings.galaxy_age = pack.galaxy_age;
                MOX::settings.strategic_combat_flag = pack.strategic_combat_flag;
                MOX::settings.starting_civilization_level =
                    pack.starting_civilization_level;
                MOX::settings.xenons_exist = pack.xenons_exist;

                NETMOX::net.host_election_id = (int16_t)pack.net_host_id;
                NETMOX::net.num_connected_players = (int8_t)pack.num_connected_players;
                MOX::PLAYER_NUM = -1;

                if (MOX::NUM_PLAYERS > 0) {
                    for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        MOX::player[i].network_player_id = pack.net_ids[i];
                        netcode::Net_Accept_Member(pack.net_ids[i]);
                        if (NETMOX::net.net_host_id == pack.net_ids[i]) {
                            MOX::PLAYER_NUM = i;
                        }
                    }
                }

                if (MOX::PLAYER_NUM != -1) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_SELECT_RACE;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_NO_AVAILABLE_SLOT;
                }
                break;
            }
            case NET_PACKET_PLAYER_SETUP: {
                NETMOX::net.player_connected_flags[from_player_id] = 1;
                memcpy(&MOX::player[from_player_id], payload, (uint16_t)payload_len);

                if (MOX::NUM_PLAYERS > 0) {
                    for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        if (from_player_id != i && MOX::player[i].color == MOX::player[from_player_id].color) {
                            MOX::player[from_player_id].color = 8;
                        }
                    }
                }
                break;
            }
            case NET_PACKET_GAME_DATA_PAYLOAD: {
                if (sender_net_id_or_host_id !=
                    NETMOX::net.host_election_id) {
                    break;
                }
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: GAME_DATA_PAYLOAD from_player=%u sender_net_id=%d bytes=%u sync=%u",
                    (unsigned int)from_player_id,
                    (int)sender_net_id_or_host_id,
                    (unsigned int)payload_len,
                    (unsigned int)NETMOX::net.sync_state);
                allocate::Push_Block(MOX::screen_seg);
                if (payload_len > k_max_game_data_payload_bytes) {
                    exitm::Exit_With_Message("game data payload too large");
                }
                NETMOX::net.network_data_ptr =
                    allocate::Allocate_Next_Block(MOX::screen_seg,
                        k_max_game_data_payload_bytes);
                memcpy(NETMOX::net.network_data_ptr, payload, payload_len);
                NETMOX::net.host_election_winner_id = sender_net_id_or_host_id;
                NETTURN::Net_Init_Host_Election();
                NETMOX::net.total_bytes_received = payload_len;
                break;
            }
            case NET_PACKET_GAME_ORDERS_PAYLOAD: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: GAME_ORDERS_PAYLOAD from_player=%u bytes=%u host_sync_counter_before=%d",
                    (unsigned int)from_player_id,
                    (unsigned int)payload_len,
                    (int)NETMOX::net.host_sync_counter);
                if (payload == nullptr || payload_len == 0 || payload_len > k_max_game_data_payload_bytes) {
                    exitm::Exit_With_Message("game orders payload invalid");
                }

                allocate::Push_Block(MOX::screen_seg);
                NETMOX::net.network_data_ptr = allocate::Allocate_Next_Block(MOX::screen_seg, payload_len);
                memcpy(NETMOX::net.network_data_ptr, payload, payload_len);
                NETMOX::net.total_bytes_received = payload_len;
                NETMOX::net.host_sync_counter++;
                if ((uint8_t)NETMOX::net.host_sync_counter == MOX::NUM_PLAYERS) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS;
                }
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: GAME_ORDERS_PAYLOAD complete host_sync_counter_after=%d next_sync=%u",
                    (int)NETMOX::net.host_sync_counter,
                    (unsigned int)NETMOX::net.sync_state);
                break;
            }
            case NET_PACKET_HOST_ELECTION_RANK: {
                int8_t rank = *(int8_t*)payload;
                int8_t hit_next = *(int8_t*)((uint8_t*)payload + 4);
                NETMOX::net.host_election_player_rank[from_player_id] = rank;
                NETMOX::net.players_hit_next_turn[from_player_id] = hit_next;

                int8_t cur_rank = NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM];
                if (cur_rank == -1 || rank < cur_rank) {
                    NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM] = rank;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_REQUEST: {
                int8_t remote_id = *(int8_t*)payload;
                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = remote_id;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_REQUESTED;

                int next_talker = NETMOX::Get_Next_AI_Talker(remote_id);
                if (next_talker == -1) {
                    NETMOX::net.ai_diplomacy_done_mask |= (1 << (from_player_id & 0x1F));
                } else {
                    NETMOX::net.ai_diplomacy_active_mask |= (1 << (from_player_id & 0x1F));
                    NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = remote_id;
                    NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_BEGIN: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED || NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_DONE) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_AI_DIPLOMACY_READY;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_DONE: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_AI_DIPLOMACY_DONE;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_CANCEL_REQUEST: {
                if (NETMOX::net.player_net_info[from_player_id].player_state != NET_PLAYER_STATE_AI_DIPLOMACY_REQUESTED) {
                    NETMOX::net.ai_diplomacy_active_mask &= ~(1 << (from_player_id & 0x1F));
                    int next_talker = NETMOX::Get_Next_AI_Talker(NETMOX::net.player_net_info[from_player_id].diplomacy_target_id);
                    if (next_talker != -1) {
                        NETMOX::net.ai_diplomacy_active_mask |= (1 << (next_talker & 0x1F));
                        NETMOX::net.player_net_info[next_talker].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                    }
                }
                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = -1;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.ai_diplomacy_cancel_mask |= (1 << (from_player_id & 0x1F));
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_CANCELLED: {
                NETMOX::net.diplomacy_target_id = -1;
                NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_END_STATE: {
                s_net_pack_end_ai_diplomacy* pack = (s_net_pack_end_ai_diplomacy*)payload;
                int8_t remote_id = NETMOX::net.player_net_info[from_player_id].diplomacy_target_id;

                memcpy(&MOX::player[remote_id], &pack->target_player, sizeof(s_player));
                memcpy(&MOX::player[from_player_id], &pack->player, sizeof(s_player));

                for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                    MOX::star[i].surrender_to[remote_id] = pack->surrender_to[i];
                }

                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = -1;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.players_sync_mask[remote_id] = 0xFF;
                NETMOX::net.diplomacy_sync_mask[remote_id] = from_player_id;

                int next_talker = NETMOX::Get_Next_AI_Talker(remote_id);
                if (next_talker != -1) {
                    NETMOX::net.ai_diplomacy_active_mask |= (1 << (next_talker & 0x1F));
                    NETMOX::net.player_net_info[next_talker].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                }
                break;
            }
            case NET_PACKET_TURN_END_REQUEST: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: TURN_END_REQUEST from_player=%u",
                    (unsigned int)from_player_id);
                NETMOX::net.player_is_active_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_SEND_GAME_ORDERS_REQUEST: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: SEND_GAME_ORDERS_REQUEST local_player=%d sync_before=%u",
                    (int)MOX::PLAYER_NUM,
                    (unsigned int)NETMOX::net.sync_state);
                NETMOX::net.sync_state = NET_SYNC_STATE_RECEIVING_GAME_ORDERS;
                break;
            }
            case NET_PACKET_COMBAT_SYNC_RESET: {
                memset(
                    &NETMOX::net.player_net_info,
                    0,
                    offsetof(s_network, sync_state) - offsetof(s_network, player_net_info));
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                break;
            }
            case NET_PACKET_PICK_TARGET_INFO: {
                NETMOX::net.sync_state = NET_SYNC_STATE_PICKING_COMBAT_TARGET;
                NETMOX::Decode_Pick_Target_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_COMBAT_TARGET_CHOSEN: {
                const s_net_pack_combat_target_chosen* packet = (const s_net_pack_combat_target_chosen*)payload;
                NETMOX::net.player_net_info[from_player_id].combat_defender_id = packet->combat_defender_id;
                NETMOX::net.player_net_info[from_player_id].combat_planet_id = packet->combat_planet_id;
                NETMOX::net.player_net_info[from_player_id].combat_target_player_id = packet->combat_target_player_id;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_COMBAT_TARGET_CHOSEN;
                break;
            }
            case NET_PACKET_YOU_ARE_TARGET_INFO: {
                NETMOX::Decode_You_Are_Target_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_COMBAT_EXECUTE: {
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER;
                break;
            }
            case NET_PACKET_COMBAT_RESOLUTION_BEGIN: {
                if (MOX::settings.strategic_combat_flag != 0) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_STRATEGIC_ATTACKER;
                } else if (NETMOX::net.combat_target_player_id < MAX_PLAYERS && MOX::player[NETMOX::net.combat_target_player_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER_VS_NONHUMAN;
                }
                break;
            }
            case NET_PACKET_BATTLE_RESULTS: {
                NETMOX::Decode_Battle_Results(payload, payload_len);
                break;
            }
            case NET_PACKET_STRATEGIC_BATTLE_RESULTS:
            case NET_PACKET_STRATEGIC_RESULTS_READY: {
                memcpy(NETMOX::net.combat_results_data, payload, sizeof(s_battle_results));
                if (msg_type == NET_PACKET_STRATEGIC_BATTLE_RESULTS) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_RESULTS_DISPLAY;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED;
                }
                break;
            }
            case NET_PACKET_COMBAT_RESULTS_ACK: {
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_COMBAT_READY;
                NETMOX::net.player_net_info[from_player_id].combat_attacker_id = -1;
                NETMOX::net.sync_phase_active_flag = 0;
                break;
            }
            case NET_PACKET_STRATEGIC_RESULTS_ACK: {
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                break;
            }
            case NET_PACKET_COMBAT_PHASE_COMPLETE: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: COMBAT_PHASE_COMPLETE local_player=%d",
                    (int)MOX::PLAYER_NUM);
                NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
                break;
            }
            case NET_PACKET_REQUEST_HUMAN_DIPLOMACY: {
                s_net_pack_req_human_diplomacy* pack = (s_net_pack_req_human_diplomacy*)payload;
                if (NETMOX::net.players_hit_next_turn[MOX::PLAYER_NUM] == 0) {
                    memcpy(&MOX::player[from_player_id], &pack->player, sizeof(s_player));

                    for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                        MOX::star[i].surrender_to[from_player_id] = pack->surrender_to[i];
                    }

                    if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id == from_player_id) {
                        uint8_t mask = ~(1 << (from_player_id & 0x1F));
                        NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
                        NETMOX::net.diplomacy_initiator_id = from_player_id;
                        NETMOX::net.players_active_mask &= mask;
                        NETMOX::net.human_diplomacy_request_mask &= mask;
                        NETMOX::net.is_diplomacy_initiator = (MOX::PLAYER_NUM < NETMOX::net.diplomacy_target_id);
                    } else if ((MOX::player[MOX::PLAYER_NUM].ignoring & (1 << (from_player_id & 0x1F))) != 0) {
                        NETMOX::net.human_diplomacy_ignored_mask |= (1 << (from_player_id & 0x1F));
                    } else {
                        uint8_t mask = (1 << (from_player_id & 0x1F));
                        NETMOX::net.players_active_mask |= mask;
                        NETMOX::net.human_diplomacy_request_mask |= mask;
                    }
                }
                break;
            }
            case NET_PACKET_ACCEPT_HUMAN_DIPLOMACY: {
                memcpy(&MOX::player[from_player_id], payload, sizeof(s_player));

                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id == from_player_id) {
                    uint8_t mask = ~(1 << (from_player_id & 0x1F));
                    NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
                    NETMOX::net.diplomacy_initiator_id = from_player_id;
                    NETMOX::net.players_active_mask &= mask;
                    NETMOX::net.human_diplomacy_request_mask &= mask;
                    NETMOX::net.is_diplomacy_initiator = 1;
                } else if ((MOX::player[MOX::PLAYER_NUM].ignoring & (1 << (from_player_id & 0x1F))) != 0) {
                    NETMOX::net.human_diplomacy_ignored_mask |= (1 << (from_player_id & 0x1F));
                } else {
                    uint8_t mask = (1 << (from_player_id & 0x1F));
                    NETMOX::net.players_active_mask |= mask;
                    NETMOX::net.human_diplomacy_request_mask |= mask;
                }
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMACY_IGNORED: {
                NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_REJECTED;
                break;
            }
            case NET_PACKET_CANCEL_HUMAN_DIPLOMACY: {
                uint8_t mask = ~(1 << (from_player_id & 0x1F));
                NETMOX::net.players_active_mask &= mask;
                NETMOX::net.human_diplomacy_request_mask &= mask;

                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_READY && NETMOX::net.diplomacy_target_id == from_player_id) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                    NETMOX::net.diplomacy_target_id = -1;
                    NETMOX::net.diplomacy_active_flag = 0;
                }
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING && NETMOX::net.diplomacy_target_id == from_player_id) {
                    if (NETMOX::net.diplomacy_active_flag == 0) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED;
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                    }
                    NETMOX::net.diplomacy_active_flag = 0;
                    NETMOX::net.diplomacy_initiator_id = -1;
                }
                break;
            }
            case NET_PACKET_PICK_YOUR_POSITION: {
                NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_PICK_POSITION;
                NETMOX::net.host_election_id = sender_net_id_or_host_id;
                NETMOX::Decode_Pick_Your_Position((s_net_pack_pick_your_position*)payload, payload_len);
                break;
            }
            case NET_PACKET_PICK_YOUR_POSITION_RESULT: {
                int32_t target_id;
                memcpy(&target_id, payload, sizeof(target_id));
                MOX::player[target_id].network_player_id = sender_net_id_or_host_id;
                NETMOX::net.joining_net_id = -1;
                break;
            }
            case NET_PACKET_SYNC_PLAYER_STATE: {
                s_net_pack_end_ai_diplomacy* pack = (s_net_pack_end_ai_diplomacy*)payload;
                int target_id = pack->target_player_id;

                memcpy(&MOX::player[target_id], &pack->target_player, sizeof(s_player));
                memcpy(&MOX::player[pack->player_num], &pack->player, sizeof(s_player));

                for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                    pack->surrender_to[i] = MOX::star[i].surrender_to[target_id];
                }
                break;
            }
            case NET_PACKET_DRAGON_INFO: {
                NETMOX::net.sync_state = NET_SYNC_STATE_DRAGON_BRIBE_PROMPT;
                NETMOX::Decode_Dragon_Info((int16_t*)payload, (uint16_t)payload_len);
                break;
            }
            case NET_PACKET_DRAGON_NO_BRIBE: {
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_HUMAN_DIPLOMACY_IGNORED;
                break;
            }
            case NET_PACKET_DRAGON_BRIBE_DESTINATION: {
                if (payload_len != sizeof(int16_t)) {
                    break;
                }
                memcpy(&NETMOX::net.dragon_bribe_destination_star_id, payload, sizeof(int16_t));
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_DRAGON_BRIBE_DESTINATION;
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMAT_WAIT: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id == from_player_id) {
                    NETMOX::net.human_diplomat_wait_flag = 1;
                }
                break;
            }
            case NET_PACKET_CHAT: {
                if (CHAT::chat_info != nullptr) {
                    CHAT::Receive_Chat_Msg(from_player_id, (char*)payload);
                }
                break;
            }
            case NET_PACKET_LOAD_GAME_START: {
                if (payload == nullptr || payload_len < sizeof(s_load_net_screen_packet)) {
                    exitm::Exit_With_Message("network load packet truncated");
                }
                s_load_net_screen_packet pack;
                memcpy(&pack, payload, sizeof(pack));
                if (!Valid_Setup_Version(pack.version_string,
                        sizeof(pack.version_string))) {
                    exitm::Exit_With_Message("Incompatible network version");
                }
                if (orion2re::lua::Has_Pending_Lua_Network_Setup()) {
                    if (!Commit_Pending_Lua_Setup(sender_net_id_or_host_id,
                            orion2re::lua::LUA_NETWORK_SETUP_LOAD_GAME,
                            payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "invalid Lua network load-game commit");
                    }
                } else {
                    if (orion2re::lua::Network_Lua_Enabled() ||
                        orion2re::lua::Has_Lua_Network_Setup_Commit(payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "Lua network setup offer is missing");
                    }
                    Apply_Setup_Config(payload, payload_len, sizeof(pack));
                }

                int cur_players = pack.number_of_players;
                while (cur_players > 0) {
                    cur_players--;
                    MOX::player[cur_players].network_player_id =
                        pack.network_ids[cur_players];
                    netcode::Net_Accept_Member(
                        pack.network_ids[cur_players]);
                }
                break;
            }
            case NET_PACKET_PLAYER_COLOR_SELECTED: {
                uint8_t color_idx = *(uint8_t*)payload;
                uint8_t player_idx = *(uint8_t*)((uint8_t*)payload + 1);
                RACESEL::color_flag[color_idx] = 1;
                MOX::player[player_idx].color = color_idx;
                break;
            }
            case NET_PACKET_REQUEST_PLAYER_COLOR: {
                uint8_t color_idx = *(uint8_t*)payload;
                if (RACESEL::color_flag[color_idx] == 0) {
                    RACESEL::color_flag[color_idx] = 1;
                    MOX::player[from_player_id].color = color_idx;
                    NETMOX::net.player_color_selected_flags[from_player_id] = 1;
                }
                break;
            }
            case NET_PACKET_GNN_CHAT: {
                if (CHAT::chat_info != nullptr) {
                    CHAT::Receive_Chat_Msg(8, (char*)payload);
                }
                break;
            }
            case NET_PACKET_PLAYER_PHASE_READY: {
                NETMOX::net.player_race_selected_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_PLAYER_ABDICATED:
            case NET_PACKET_PLAYER_SURRENDERED: {
                NETMOX::Decode_Drop_Out_Info(from_player_id, (s_net_pack_drop_out_info*)payload);
                break;
            }
            case NET_PACKET_PLAYER_DISCONNECTED: {
                NETMOX::Decode_Player_Disconnected_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_PLAYER_RACE_SELECTED: {
                if (NETMOX::net.host_election_id == NETMOX::net.net_host_id) {
                    exitm::Exit_With_Message(NETMOX::only_1_host_allowed_error);
                }
                uint8_t race_idx = *(uint8_t*)payload;
                uint8_t player_idx = *(uint8_t*)((uint8_t*)payload + 1);
                RACESEL::race_flag[race_idx] = 1;
                MOX::player[player_idx].race = race_idx;
                break;
            }
            case NET_PACKET_REQUEST_PLAYER_RACE: {
                uint8_t race_idx = *(uint8_t*)payload;
                if (RACESEL::race_flag[race_idx] == 0) {
                    RACESEL::race_flag[race_idx] = 1;
                    MOX::player[from_player_id].race = race_idx;
                    NETMOX::net.player_race_selected_alt_flags[from_player_id] = 1;
                }
                break;
            }
            case NET_PACKET_FORCE_PLAYER_ELIMINATED: {
                MOX::player[MOX::PLAYER_NUM].eliminated = 1;
                break;
            }
            case NET_PACKET_MONSTER_UPDATE: {
                if (payload_len != sizeof(s_net_pack_monster_update)) {
                    break;
                }
                s_net_pack_monster_update monster_update;
                memcpy(&monster_update, payload, sizeof(monster_update));
                EVENTS::Event_Monster_Update((int16_t)monster_update.monster_type, monster_update.target_planet_id, (int16_t)monster_update.player_idx_or_amount);
                break;
            }
            case NET_PACKET_PLAYER_RACE_DESELECTED: {
                uint8_t race_idx = *(uint8_t*)payload;
                RACESEL::race_flag[race_idx] = 0;
                break;
            }
            case NET_PACKET_PLAYER_RACE_DESELECT_REQUEST: {
                uint8_t race_idx = *(uint8_t*)payload;
                RACESEL::race_flag[race_idx] = 0;
                NETMOX::net.player_race_deselected_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_RACE_SELECTION_SYNCED: {
                NETMOX::net.race_selection_changed = 0;
                break;
            }
            case NET_PACKET_TACTICAL_COMBAT_START: {
                NETMOX::net.tactical_combat_sync_state = NET_TACTICAL_COMBAT_SYNC_STARTED;
                break;
            }
            case NET_PACKET_TACTICAL_COMBAT_END: {
                NETMOX::net.tactical_combat_sync_state = NET_TACTICAL_COMBAT_SYNC_FINISHED;
                break;
            }
            case NET_PACKET_NETWORK_SYNC_STATUS: {
                if (*(int32_t*)payload == 0) {
                    NETMOX::net.network_sync_complete_flag = 0;
                }
                break;
            }
            case NET_PACKET_CHANGE_RELATIONS: {
                int16_t relation_params[6];
                memcpy(relation_params, payload, sizeof(relation_params));
                diplomac::Change_Relations(relation_params[0], relation_params[1], relation_params[2], relation_params[3], relation_params[4], relation_params[5]);
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMACY_READY_DATA: {
                memcpy(&MOX::player[from_player_id], payload, sizeof(s_player));
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING && NETMOX::net.diplomacy_target_id == from_player_id) {
                    uint8_t mask = ~(1 << (from_player_id & 0x1F));
                    NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_READY;
                    NETMOX::net.players_active_mask &= mask;
                    NETMOX::net.human_diplomacy_request_mask &= mask;
                }
                break;
            }
            case NET_PACKET_DECLARE_WAR: {
                int32_t target_player_id;
                memcpy(&target_player_id, payload, sizeof(target_player_id));
                diplomac::Declare_War((int16_t)from_player_id, (int16_t)target_player_id);
                break;
            }
        }
    }

    void __cdecl Wrapper_For_Normal_System_Fields() {
        s_movable_box& box = MOX::moveable_box[0];

        SYS::Init_System_Display(box.id);
        SYS::Add_System_Display_Popup_Fields(
            box.id,
            box.x,
            box.y,
            box.content_offset_x,
            box.content_offset_y,
            0x0D,
            0x2F,
            &box.drag_field,
            &box.close_btn,
            &box.window_field_id);
    }

    void __cdecl Mini_Main_Screen_Text_Box(char* text_1, char* text_2) {
        video::Set_Refresh_Full();
        TEXTBOX::Text_Box(text_1, text_2);
        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
        Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        video::Set_Refresh_Stencil();
        MOX::draw_main_screen_to_back = 1;
    }

    void __cdecl Load_Reply_To_Diplomacy_Request_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        MOX::diplomacy_request_popup_seg = (s_animation_header*)farload::Far_Reload_Next("BUFFER0.LBX", 0x0f, MOX::screen_seg);

        MOX::diplomacy_request_accept_seg = (s_animation_header*)farload::Far_Reload_Next("BUFFER0.LBX", 0x10, MOX::screen_seg);

        MOX::diplomacy_request_invoke_seg = (s_animation_header*)farload::Far_Reload_Next("BUFFER0.LBX", 0x11, MOX::screen_seg);

        MOX::diplomacy_request_wait_seg = (s_animation_header*)farload::Far_Reload_Next("BUFFER0.LBX", 0x12, MOX::screen_seg);
    }

    void __cdecl Add_Reply_To_Diplomacy_Request_Fields(
        int16_t* accept_field,
        int16_t* invoke_field,
        int16_t* wait_field,
        int16_t* dismiss_field) {
        const int16_t popup_left = static_cast<int16_t>(
            ((0x1FA - animate::Get_Width(MOX::diplomacy_request_popup_seg)) / 2) + 0x15);
        const int16_t popup_top = static_cast<int16_t>(
            ((0x190 - animate::Get_Height(MOX::diplomacy_request_popup_seg)) / 2) + 0x15);
        const int16_t button_top = static_cast<int16_t>(popup_top + 0x5A);

        *accept_field = fields::Add_Button_Field(
            static_cast<int16_t>(popup_left + 0x40),
            button_top,
            "",
            MOX::diplomacy_request_accept_seg,
            "A",
            0x28);

        *invoke_field = fields::Add_Button_Field(
            static_cast<int16_t>(popup_left + 0xBC),
            button_top,
            "",
            MOX::diplomacy_request_invoke_seg,
            "I",
            0x28);

        *wait_field = fields::Add_Button_Field(
            static_cast<int16_t>(popup_left + 0x13B),
            button_top,
            "",
            MOX::diplomacy_request_wait_seg,
            "W",
            0x28);

        const int16_t popup_bottom = static_cast<int16_t>(
            animate::Get_Height(MOX::diplomacy_request_popup_seg) + popup_top);
        const int16_t popup_right = static_cast<int16_t>(
            animate::Get_Width(MOX::diplomacy_request_popup_seg) + popup_left);

        fields::Add_Hidden_Field(
            popup_left,
            popup_top,
            popup_right,
            popup_bottom,
            "",
            0);

        *dismiss_field = fields::Add_Hidden_Field(
            0,
            0,
            0x27F,
            0x1DF,
            "\x1B",
            0x29);
    }

    void __cdecl Draw_Reply_To_Diplomacy_Request() {
        int32_t popup_x = (0x1FA - animate::Get_Width(MOX::diplomacy_request_popup_seg)) / 2 + 0x15;
        int32_t popup_y = (0x190 - animate::Get_Height(MOX::diplomacy_request_popup_seg)) / 2 + 0x15;
        int32_t player_index = static_cast<int32_t>(MOX::screen_data);
        uint32_t player_color =
            MOX::main_palette_player_colors[static_cast<uint8_t>(MOX::player[player_index].color)];
        s_colors font_colors;

        Draw_Mini_Main_Screen();
        Draw_Main_Main_Screen();
        animate::Draw(static_cast<int16_t>(popup_x), static_cast<int16_t>(popup_y), MOX::diplomacy_request_popup_seg);

        MISC::Get_Mox_Font_Colors(4, static_cast<uint8_t>(player_color), static_cast<uint8_t>(player_color + 3), &font_colors);
        fonts::Set_Font_Style(4, &font_colors);

        s_player* player = &MOX::player[player_index];
        const char* message_format = HAROLD::H_Message(0xDB);

        snprintf(MOX::temp_string, sizeof(MOX::temp_string), message_format, player->name, player->race_name);
        fonts::Print(static_cast<int16_t>(popup_x + 0x27), static_cast<int16_t>(popup_y + 0x3C), MOX::temp_string);
    }

    void __cdecl Save_Hotseat_Map_Info() {
        const int16_t saved_player_num = MOX::PLAYER_NUM;
        const int32_t saved_player_index = saved_player_num;

        hotseat_system_box[saved_player_index].x = MOX::moveable_box[0].x;
        hotseat_system_box[saved_player_index].y = MOX::moveable_box[0].y;
        hotseat_system_box[saved_player_index].system_id = MOX::moveable_box[0].id;

        hotseat_fleet_box[saved_player_index].x = MOX::moveable_box[2].x;
        hotseat_fleet_box[saved_player_index].y = MOX::moveable_box[2].y;
        hotseat_fleet_box[saved_player_index].box_id = MOX::moveable_box[2].id;

        HACCESS::Save_Hotseat_Fleet_Box_Ship(saved_player_num);

        const int32_t current_player_index = MOX::PLAYER_NUM;
        hotseat_map_scale[current_player_index] = static_cast<uint8_t>(MOX::cur_map_scale);
    }

} // namespace MAINSCR