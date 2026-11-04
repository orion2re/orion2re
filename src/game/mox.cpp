#include "pch.h"

namespace MOX {
    const char ship_type_names[SHIP_TYPE_COUNT][6][16] = {
        {
            "combat ships",
            "not used",
            "",
            "",
            "",
            "",
        },
        {
            "Colony Ship",
            "Kolonieschiff",
            "Colonial",
            "Nave Colonia",
            "Nave colonia",
            "Colony Ship",
        },
        {
            "Transport",
            "Transporter",
            "Transport",
            "Transporte",
            "Nave truppa",
            "Transport",
        },
        {
            "tanks",
            "not used",
            "",
            "",
            "",
            "",
        },
        {
            "Outpost Ship",
            "Au|enposten",
            "Avante-poste",
            "Puesto Avanzado",
            "Nave avamposto",
            "Outpost Ship",
        },
    };

    int8_t tech_categories[51][2] = {
        {50, 1},
        {50, 1},
        {50, 1},
        {50, 1},
        {50, 1},
        {20, 1},
        {20, 1},
        {20, 1},
        {10, 1},
        {50, 1},
        {20, 0},
        {5, 1},
        {10, 1},
        {50, 0},
        {50, 0},
        {10, 1},
        {10, 0},
        {50, 1},
        {20, 0},
        {20, 0},
        {5, 0},
        {20, 0},
        {50, 1},
        {10, 0},
        {10, 0},
        {10, 0},
        {20, 0},
        {5, 1},
        {20, 0},
        {5, 1},
        {10, 0},
        {10, 1},
        {50, 0},
        {50, 0},
        {10, 1},
        {5, 1},
        {10, 0},
        {5, 0},
        {5, 0},
        {50, 0},
        {50, 1},
        {5, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
    };

    const char s_combat_lbx[11] = "combat.lbx";

    int16_t first_field_in_group[10] = {0, 18, 55, 57, 29, 7, 22, 28, 10, 74};

    s_animation_header* fighter_seg[15][64];
    uint8_t draw_officer_to_back;
    int16_t ordnance_bonus[15];
    int16_t n_fltscrn_big_icons_added;
    int16_t n_fltscrn_big_icons;
    int8_t n_fltscrn_rows;
    int16_t old_small_ship_stack_ptr;
    int16_t old_first_fltscrn_icon;
    int16_t n_big_icons_selected;
    uint8_t galaxy_star_anim_count;
    int16_t fltscrn_bar_value;
    int8_t big_icon_display_columns;
    int8_t big_icon_display_rows;
    uint16_t ken_trace;
    int8_t big_icon_display_max_icons;
    int16_t big_icon_x_offset;
    int16_t big_icon_y_offset;
    int16_t offpop_big_icon_selected;
    uint8_t drawing_officer_popup;
    int16_t* fltscrn_ship_icon_selection_list;
    uint8_t look_at_colonies;
    s_big_icon* fltscrn_big_icon;
    uint8_t add_help_fields;
    uint8_t first_time;
    int16_t fleet_movement_bar_value;
    uint8_t draw_expanding_box;
    uint8_t mainscr_palette;
    s_scroll_bar* scroll_bar;
    int16_t fltscrn_stack_owner;
    uint8_t add_scrap_button;
    uint8_t skip_draw_ships;
    uint8_t fix_palette;
    s_animation_header* scrap_button_seg;
    s_animation_header* colony_marker_seg[MAX_PLAYERS];
    s_animation_header* outpost_marker_seg[MAX_PLAYERS];
    s_animation_header* o_highlight_seg;
    uint8_t add_all_button;
    s_animation_header* fleet_all_button_seg;
    s_animation_header* fleet_return_button_seg;
    int16_t old_fltscrn_stack_owner;
    s_animation_header* up_scroll_button_seg;
    s_animation_header* down_scroll_button_seg;
    s_animation_header* prev_fleet_button_seg;
    s_animation_header* next_fleet_button_seg;
    s_animation_header* relocate_fleet_button_seg;
    s_animation_header* officers_button_seg;
    uint8_t add_officers_button;
    int16_t fltscrn_support_button_status;
    int16_t fltscrn_combat_button_status;
    s_mp_screen* mp_screen;
    int8_t player_mode[15];
    int16_t cur_ship;
    int16_t net_destination;
    int16_t done_button;
    int16_t msl_impact_warning_button;
    int16_t draw_legal_boxes_button;
    int16_t msl_warning_button;
    int16_t wait_button;
    int16_t scan_button;
    int16_t missile_field[MAX_COMBAT_FLYERS];
    int16_t combat_grid_width = ORIGINAL_COMBAT_GRID_WIDTH;
    int16_t combat_grid_height = ORIGINAL_COMBAT_GRID_HEIGHT;
    int16_t combat_grid[MAX_COMBAT_GRID_WIDTH][MAX_COMBAT_GRID_HEIGHT];
    int16_t weapon_mod_field_status[WEAPON_MOD_COUNT];
    int16_t weapon_button[8];
    int16_t ship_turn_order[MAX_COMBAT_SHIPS];
    int16_t repl_fields[10];
    int16_t side_end_of_turn[15];
    int16_t unused_ship_turn_order_marker;
    int16_t design_choice_items[40];
    int16_t weapon_mod_fields[WEAPON_MOD_COUNT];
    int16_t weapon_arc_field[5];
    int16_t weapon_rack_field[5];
    int16_t multi_colored_box_color_start;
    int16_t beam_filter_button;
    int16_t missile_filter_button;
    int16_t special_filter_button;
    int16_t replacement_box_accept_button;
    int16_t bomb_filter_button;
    const char s__00551100[1] = "";

    uint16_t fleet_box_map_height;
    s_current_design* design;

    int16_t scanned_big_ship;
    int16_t small_ship_stack_ptr;
    int16_t scanned_small_ship;
    int16_t n_fltscrn_officers;
    int16_t* officer_id_list;
    s_animation_header* star_colors_seg[11];
    s_animation_header* fleet_galaxy_star_seg[11];
    s_animation_header* colony_galaxy_star_seg[10];
    s_animation_header* officer_skill_icon_seg[27];
    s_animation_header* hl_antaran_ship_icon_seg[24];
    s_animation_header* antaran_ship_icons[24];
    s_animation_header* o_ring_seg[5];
    s_animation_header* planet_mask_seg[5];
    s_animation_header* fltscrn_small_ship_icon_seg[8];
    s_animation_header* monster_ship_seg[4];
    s_animation_header* hl_monster_ship_icon_seg[4];
    s_animation_header* ship_icon_seg[32];
    s_animation_header* hl_ship_icon_seg[32];
    s_animation_header* fleet_background_seg;
    void* officer_pictures_screen_seg;
    s_animation_header* officer_main_seg;
    s_animation_header* officer_colony_box_seg;
    s_animation_header* officer_colony_ldrs_button_seg;
    s_animation_header* officer_ship_ldrs_button_seg;
    s_animation_header* officer_up_button_seg;
    s_animation_header* officer_dn_button_seg;
    s_animation_header* officer_prev_button_seg;
    s_animation_header* officer_next_button_seg;
    s_animation_header* officer_hire_button_seg;
    s_animation_header* officer_return_button_seg;
    s_animation_header* officer_cancel_hire_button_seg;
    s_animation_header* selected_box_seg;
    s_animation_header* scanned_box_seg;
    s_animation_header* fleet_scroll_anim_seg;
    s_animation_header* officer_normal_window_seg;
    s_animation_header* officer_bright_window_seg;
    s_animation_header** temp_officers_pict;
    s_animation_header* officer_dismiss_button_seg;
    s_animation_header* officer_fleet_box_seg;
    s_animation_header* dull_officer_dismiss_button_seg;
    s_animation_header* dull_officer_pool_button_seg;
    s_animation_header* officer_pool_button_seg;
    s_animation_header* dull_officer_hire_button_seg;
    s_animation_header* officer_hire_replace_seg;
    int8_t officer_scanned;
    int8_t officer_scrn_type;

    int8_t officer_selected;
    int16_t special_scroll_dn_button;
    int8_t n_weapons_loaded;
    uint8_t n_special_systems;
    int8_t* fltscrn_icon_selection_status;
    uint8_t star_class_table[STAR_CLASS_COUNT][GALAXY_AGE_COUNT] = {
        {20, 10, 5},
        {25, 15, 5},
        {10, 16, 30},
        {10, 16, 21},
        {32, 37, 30},
        {1, 2, 3},
        {2, 4, 6},
    };
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT] = {0, 0, 0, 2, 2, 4, 4, 2, 4, 6};
    int16_t ranged_to_hit_penalty[9] = {0, 0, -10, -20, -30, -40, -55, -70, -85};
    int16_t ranged_damage_penalty[9] = {0, 0, -10, -20, -30, -40, -50, -60, -65};
    int16_t cur_missile_count;
    int16_t NUM_STARS;
    s_scroll_bar fltpop_scroll_bar;
    int16_t cur_map_scale;
    int16_t cur_map_x;
    int16_t cur_map_y;
    uint16_t global_esc;
    int16_t return_screen;
    char* save_game_dates;
    char* save_game_stardates;
    char* star_name_loaded_string;
    int16_t scanned_field;
    int8_t previous_screen;
    int16_t max_debug_saves;
    int16_t first_debug_save;
    int16_t debug_saves_on;
    uint8_t turns_between_debug_saves;
    void* sound_buffer;
    int16_t hot_seat_flag;
    int16_t current_screen;
    int16_t design_screen_replacement_type;
    int16_t design_screen_replacement_slot;
    int16_t weapons_field[8];
    s_animation_header* asteroid_seg;
    s_animation_header* screen_seg;
    s_animation_header* diplomacy_request_popup_seg;
    s_animation_header* combat_bitmap;
    s_animation_header* combat_map_planet_seg;
    s_animation_header* combat_planet_seg;
    s_animation_header* combat_planet_seg2;
    uint8_t ship_darken_map[128][16];
    uint8_t given_chance_to_rename_home_star[MAX_PLAYERS];
    uint8_t player_has_ship_in_system;
    s_animation_header* moveable_box_pict[25];
    int8_t game_type;
    s_antaran antarans;
    int8_t maps_to_generate;
    uint8_t auto_turn_exit;
    int16_t special_scroll_up_button[2];
    uint8_t blue_color2;
    int16_t auto_msl_warning_off;
    s_animation_header* draw_ship_bitmap;
    int16_t multi_colored_line_start;
    int16_t design_choice_item_selected[40];
    int8_t first_special_system_ctr;
    int16_t special_system_field[8];
    int16_t planet_buttons[5];
    uint8_t first_design_screen_call;
    int16_t design_size;
    uint8_t refresh_mode;
    uint8_t mox_font_colors_offset;
    int16_t star_width;
    int16_t star_height;
    char temp_string[512];
    char temp_string_buf[64];
    int16_t take_control_button;
    int16_t auto_cmbt_button_value;
    int16_t PLAYER_NUM;
    uint8_t active_player[9];
    int16_t active_count;
    int16_t ai_retreat_flag;
    int16_t bio_weapon_flag;
    int16_t last_ship_status;
    int16_t ship_status_display;
    int16_t dont_draw_ship;
    int16_t draw_active_missiles;
    int16_t order_info_flag;
    int16_t super_fast_flag;
    int8_t real_attack_flag;
    int16_t shields_flag;
    int16_t msl_impact_warning_flag;
    int16_t draw_legal_boxes_flag;
    int16_t take_control_flag;
    int16_t combat_far_x;
    int16_t combat_mid_x;
    int16_t combat_far_y;
    int16_t combat_mid_y;
    int16_t net_flag;
    int16_t in_nebula_flag;
    int16_t skan_flag;
    uint8_t draw_design_screen_to_back;
    uint8_t refit_ship;
    int32_t original_cost;
    int16_t design_name_field;
    int16_t design_cancel_button;
    int16_t design_clear_button;
    int16_t design_build_button;
    s_player* player;
    s_player* cur_plyr_ptr;
    char* personality_labels[7];
    char* objective_labels[6];
    char* treaty_labels[7];
    char* help_category_labels[17];
    int16_t old_system_index;
    int8_t selected_star_color_mod;
    int8_t selected_star_color_offset;
    int32_t stardate;
    s_settings settings;
    char* hyper_field_title;
    const char s_Harbinger[12] = "Harbinger";
    uint8_t at_least_one_tactical_combat;
    s_animation_header* planet_summary_galaxy_star_seg[11];
    int8_t first_weapon_ctr;
    int16_t fleet_box_colonize_button;
    uint8_t reloaded_ship_palette;
    uint8_t g_galaxy_age;
    int16_t ship_data_id;
    int16_t cur_combat_x;
    int16_t cur_ptr;
    int16_t wait_ptr;
    int16_t board_flag;
    int16_t tactical_net_choice;
    int16_t auto_button;
    int16_t auto_combat;
    int16_t speedx2_button;
    int16_t grid_button;
    int16_t shields_button;
    int16_t super_fast_button;
    int16_t self_destruct_button;
    int16_t weapons_button;
    int16_t specials_button;
    int16_t cmbt_info_button;
    int16_t flee_button;
    int16_t board_button;
    int16_t map_field;
    int16_t map_y;
    int16_t map_x;
    int16_t center_button;
    int16_t space_field;
    int16_t ypos;
    int16_t xpos;
    int16_t saved_msl_warn;
    uint8_t red_color_pulse_value;
    int8_t red_color_pulse_incr;
    int16_t fleet_ship_scanned;
    int16_t fleet_movement_all_button;
    int16_t galaxy_map_scanned_star;
    int8_t draw_system_display_without_button;
    uint8_t need_to_restore_help_list;
    int16_t ship_icon_player_id;
    int8_t at_least_one_report;
    uint8_t previous_report;
    int16_t first_node_ptr_in_stack_;
    int16_t system_display_ring_radius[5];
    uint8_t rot_plan_dim[5];
    int16_t system_star_id;
    int16_t system_star_id_for_auto_select;
    uint8_t draw_fleet_screen_to_back;
    s_mouse_xy mouse_XY;
    int16_t system_display_ship_scanned;
    int16_t system_icon_scanned;
    uint16_t global_exit_field;
    int16_t eta_star_id;
    int16_t stencil_box_id;
    int16_t draw_main_screen_stencilled;
    int16_t animated_star;
    int8_t just_moved_stack;
    int16_t first_fleet_movement_icon;
    int16_t next_free_node;
    int16_t mid_star_x;
    int16_t mid_star_y;
    int16_t design_screen_replacement_item;
    int16_t far_star_x;
    int16_t far_star_y;
    int16_t near_star_y;
    int16_t near_star_x;
    const char s_fmt_contested[15] = "%s (contested)";
    void* special_names_seg;
    void* global_data_seg;
    s_nebula* nebula;
    uint8_t NUM_NEBULAS;
    int16_t NUM_COLONIES;
    int16_t planet_count;
    int8_t climate_modifier_table[MINERAL_RESOURCE_COUNT] = {0, 0, 0, -10, -20};
    s_planet_data* planet;
    const char* roman_literals[21] = {
        nullptr,
        "I",
        "II",
        "III",
        "IV",
        "V",
        "VI",
        "VII",
        "VIII",
        "IX",
        "X",
        "XI",
        "XII",
        "XIII",
        "XIV",
        "XV",
        "XVI",
        "XVII",
        "XVIII",
        "XIX",
        "XX",
    };
    char* planet_special_string[SYSTEM_SPECIAL_COUNT];
    char* planet_climate_string[PLANET_CLIMATE_COUNT];
    char* mineral_class_string[MINERAL_RESOURCE_COUNT];
    uint8_t red_color2;
    char* planet_size_string[PLANET_SIZE_COUNT];
    char* planet_type_string[10];
    char* planet_group_string[PLANET_GROUP_COUNT];
    char* planet_atmosphere_string[3];
    char* spectral_class_string[STAR_CLASS_COUNT];
    char* admin_skill_names[9];
    char* planet_gravity_string[PLANET_GRAVITY_COUNT];
    int16_t NUM_SHIPS;
    void* global_bitmap_seg;
    uint8_t first_time_drawn;
    uint8_t draw_main_screen_to_back;
    uint8_t dont_draw_main_screen_to_back;
    int16_t zoom_count;
    int16_t max_zoom_count;
    uint8_t star_bg_index;
    s_animation_header* star_bg_seg;
    void* scan_ship_seg;
    s_animation_header* detail_fleet_icon_seg;
    uint16_t fleet_icon_button_count;
    int16_t weapons_count_plus_button[8];
    int16_t weapons_count_minus_button[8];
    int16_t design_ship_size_field[6];
    void* techname_seg;
    void* tech_desc_seg;
    s_animation_header* main_screen_seg;
    s_animation_header* design_screen_seg;
    s_animation_header* design_icon_lf_arrow_seg;
    s_animation_header* design_icon_rt_arrow_seg;
    s_animation_header* clear_design_button_seg;
    s_animation_header* cancel_design_button_seg;
    s_animation_header* build_design_button_seg;
    s_animation_header* plus_button_seg;
    s_animation_header* minus_button_seg;
    s_animation_header* d_scroll_up_button_seg;
    s_animation_header* d_scroll_dn_button_seg;
    s_animation_header* dull_d_scroll_up_button_seg;
    s_animation_header* dull_d_scroll_dn_button_seg;
    s_animation_header* beam_filter_button_seg;
    s_animation_header* missile_filter_button_seg;
    s_animation_header* bomb_filter_button_seg;
    s_animation_header* special_filter_button_seg;
    s_animation_header* weapons_cancel_button_seg;
    s_animation_header* weapons_accept_button_seg;
    s_animation_header* weapons_top_box_seg;
    s_animation_header* weapons_mid_box_seg;
    s_animation_header* weapons_bot_box_seg;
    s_animation_header* weapons_selected_seg;
    s_animation_header* weapons_blank_mods_seg;
    s_animation_header* system_repl_scroll_bar_seg;
    s_animation_header* system_repl_box_top_seg;
    s_animation_header* system_repl_box_mid_seg;
    s_animation_header* system_repl_box_btm_seg;
    s_animation_header* system_repl_box_cancel_btn;
    int16_t weapons_clear_button[8];
    int16_t special_system_clear_button[8];
    s_animation_header* design_size_button_seg[6];
    s_animation_header* darkened_size_button_seg[6];
    s_animation_header* weapons_arc_pict_seg[5];
    s_animation_header* weapons_arc_text_seg[5];
    s_animation_header* weapons_rack_text_seg[5];
    s_animation_header* weapons_special_seg[18];
    s_weapon_filter_item* weapon_filter;
    char* special_desc_string[SPECIAL_COUNT - 1];
    char* weapon_notes_string[40];
    s_animation_header* d_scroll_bar_seg;
    s_animation_header* weapons_rack_seg;
    s_animation_header* mainscrn_diplo_lights_seg;
    s_animation_header* diplomacy_request_wait_seg;
    s_animation_header* diplomacy_request_invoke_seg;
    s_animation_header* diplomacy_request_accept_seg;
    void* global_screen_seg;
    void* global_cache_seg;
    s_animation_header* mid_star_seg;
    s_animation_header* far_star_seg;
    uint8_t fleet_icon_selection_status[BITMAP(MAX_SHIPS)];
    s_animation_header* star_seg[40];
    s_animation_header* sun_seg[STAR_CLASS_COUNT];
    uint16_t n_stats_to_generate;
    uint32_t global_cache_size;
    uint32_t cmd_line_seed;
    int16_t ship_icon_lf_button;
    int16_t ship_icon_rt_button;
    uint8_t zoom_in;
    s_animation_header* main_zoom_in_button_seg;
    s_animation_header* main_zoom_out_button_seg;
    s_animation_header* main_game_button_seg;
    s_animation_header* main_next_button_seg;
    s_animation_header* main_colonies_button_seg;
    s_animation_header* main_planets_button_seg;
    s_animation_header* main_fleets_button_seg;
    s_animation_header* main_leaders_button_seg;
    s_animation_header* main_races_button_seg;
    s_animation_header* main_info_button_seg;
    int16_t star_button[MAX_STARS];
    int16_t main_screen_grid_field;
    int16_t main_screen_grid_x;
    int16_t main_screen_grid_y;
    int16_t NUM_PLAYERS;
    uint8_t red_color1;
    char* government_labels[GOVERNMENT_COUNT];
    char* special_ability_names[32];
    int32_t stardate_at_beg_of_turn;
    int8_t global_var;
    uint16_t FULL_DRAW_SCREEN_FLAG;
    uint16_t combat_init;
    void* warp_core_remap_colors;
    s_mouse_list* combat_mouse_grid;
    void* stasis_bubble_remap_colors;
    s_animation_header* combat_background_seg;
    s_animation_header* combat_far_star;
    s_animation_header* combat_mid_star;
    s_animation_header* combat_near_star;
    uint16_t apply_damage_to_planet;
    uint16_t missile_warning;
    void* cmbt_option_seg;
    s_animation_header* warning_missile_seg;
    void* self_destruct_seg;
    uint16_t saved_msl_warning;
    void* auto_button_seg;
    s_animation_header* combat_options_background_seg;
    int16_t cur_combat_y;
    int16_t grid_flag;
    int16_t speedx2_flag;
    int16_t auto_combat_flag[2];
    int16_t combat_clock_frame;
    int16_t combat_near_x;
    int16_t combat_near_y;
    void* cmbt_done_button_seg;
    void* level_symbol_seg;
    void* scan_button_seg;
    void* board_button_seg;
    void* flee_button_seg;
    void* wait_button_seg;
    void* cmbt_info_button_seg;
    void* weapons_button_seg;
    void* specials_button_seg;
    s_animation_header* msl_warning_on_seg;
    void* warning_fighter_seg;
    s_animation_header* ship_bitmap;
    s_animation_header* darken_plasma_map;
    void* shield_remap_colors;
    void* pulsar_remap_colors;
    s_animation_header* small_tractor_seg;
    void* tractor_remap_colors;
    void* bhg_remap_colors;
    void* plasma_web_remap_colors;
    void* stellar_converter_remap_colors;
    s_animation_header* msl_seg[112];
    s_animation_header* ship_cursor[4];
    s_animation_header* shield_div_bitmap[4];
    s_animation_header* fltscrn_monster_ship_icon[7];
    s_animation_header* shield_mask_bitmap[16];
    int16_t fighter_beam_type[9];
    int16_t fighter_bomb_type[9];
    s_combat_data* combat_data;
    s_missile* missile;
    int16_t missile_hit_radius[6] = {12, 20, 26, 28, 30, 30};
    s_net_tactical_combat_message new_message;

    int16_t gate_icon_field[4];
    int16_t system_display_base_x;
    int16_t system_display_base_y;
    s_sys_disp* sys_disp;
    s_system_display_ships system_display_ships[15];
    int16_t system_display_grid_y[6];
    int16_t system_display_grid_x[6];
    int16_t system_display_grid_field;
    char* race_names[20];
    int8_t class_to_num_satellites[10][6] = {
        {0, 0, 1, 2, 0, 0},
        {1, 1, 2, 2, 1, 0},
        {1, 1, 2, 2, 1, 0},
        {2, 1, 2, 3, 1, 0},
        {3, 2, 3, 3, 2, 0},
        {3, 2, 3, 4, 2, 0},
        {4, 3, 4, 4, 2, 0},
        {4, 3, 4, 5, 3, 1},
        {5, 4, 5, 5, 3, 1},
        {5, 4, 5, 5, 4, 1},
    };
    uint16_t star_fields_dim[6];

    s_ship_data* ship;

    int16_t combat_defender_count;
    int16_t combat_attacker_count;
    s_qship* qcombat_data;
    int16_t combat_total_ship_count;
    uint8_t combat_done_waiting;
    s_colors combat_green = {{0x81, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89}};
    int16_t combat_attacker_player;
    int16_t combat_turn;
    uint8_t colony_defense_flags;
    int16_t cmbt_colony_id;
    int16_t combat_defender_player;
    s_combat_bomb_result combat_bomb_result;

    s_combat_data global_combat_data;

    int16_t* officers_original_star_id;

    int16_t* officers_original_ship_id;

    void* dimmed_all_button_seg;
    void* dimmed_relocate_button_seg;
    void* dimmed_scrap_button_seg;
    void* dimmed_officers_button_seg;

    s_leader_data* leaders;

    char* officer_level_names[6];

    char* star_officer_level_names[6];

    s_skill_data skill_data[54] = {
        {0, 0x1, 0, 20, 1, 2, nullptr, "%d%%"},
        {1, 0x2, 0, 30, 1, 4, nullptr, "%d%%"},
        {2, 0x4, 0, 20, 1, 0, nullptr, "+%d"},
        {3, 0x8, 0, 30, 1, 0, nullptr, "+%d"},
        {4, 0x10, 0, 100, 1, 1, nullptr, "+%d%"},
        {5, 0x20, 0, 150, 1, 2, nullptr, "+%d%"},
        {6, 0x1, 1, 20, 1, 3, nullptr, "%d%%"},
        {7, 0x2, 1, 30, 1, 6, nullptr, "%d%%"},
        {8, 0x1, 2, 100, 1, 2, nullptr, "-%d%%"},
        {9, 0x2, 2, 150, 1, 4, nullptr, "-%d%%"},
        {10, 0x40, 0, 600, 1, 2, nullptr, "-%dBC"},
        {11, 0x80, 0, 900, 1, 4, nullptr, "-%dBC"},
        {12, 0x4, 2, 100, 1, 3, nullptr, "+%d%%"},
        {13, 0x8, 2, 150, 1, 6, nullptr, "+%d%%"},
        {14, 0x4, 1, 50, 1, 3, nullptr, "+%d"},
        {15, 0x8, 1, 75, 1, 6, nullptr, "+%d"},
        {16, 0x10, 2, 100, 1, 3, nullptr, "+%d%%"},
        {17, 0x20, 2, 150, 1, 6, nullptr, "+%d%%"},
        {18, 0x10, 1, 50, 1, 2, nullptr, "+%d"},
        {19, 0x20, 1, 75, 1, 4, nullptr, "+%d"},
        {20, 0x40, 1, 50, 1, 4, nullptr, "+%d"},
        {21, 0x80, 1, 75, 1, 8, nullptr, "+%d"},
        {22, 0x40, 2, 10, 1, 1, nullptr, "+%d"},
        {23, 0x80, 2, 15, 1, 2, nullptr, "+%d"},
        {24, 0x100, 2, 100, 1, 3, nullptr, "+%d%%"},
        {25, 0x200, 2, 150, 1, 6, nullptr, "+%d%%"},
        {26, 0x400, 2, 100, 1, 2, nullptr, "+%d%%"},
        {27, 0x800, 2, 150, 1, 4, nullptr, "+%d%%"},
        {28, 0x100, 0, 100, 1, 1, nullptr, "+10BC"},
        {29, 0x200, 0, 100, 1, 2, nullptr, "+15BC"},
        {30, 0x100, 1, 10, 4, 1, nullptr, "+%d"},
        {31, 0x200, 1, 10, 3, 2, nullptr, "+%d"},
        {32, 0x400, 0, 20, 1, 2, nullptr, "+%d"},
        {33, 0x800, 0, 30, 1, 4, nullptr, "+%d"},
        {34, 0x400, 1, 50, 1, 3, nullptr, "+%d"},
        {35, 0x800, 1, 75, 1, 6, nullptr, "+%d"},
        {36, 0x1000, 0, 50, 1, 2, nullptr, "+%d"},
        {37, 0x2000, 0, 75, 1, 4, nullptr, "+%d"},
        {38, 0x1000, 2, 100, 1, 3, nullptr, "+%d%%"},
        {39, 0x2000, 2, 150, 1, 6, nullptr, "+%d%%"},
        {40, 0x1000, 1, 20, 1, 1, nullptr, "+%d"},
        {41, 0x2000, 1, 30, 1, 2, nullptr, "+%d"},
        {42, 0x4000, 2, 50, 1, 4, nullptr, "+%d%%"},
        {43, 0x8000, 2, 75, 1, 8, nullptr, "+%d%%"},
        {44, 0x4000, 0, 20, 1, 3, nullptr, "+%d%%"},
        {45, 0x8000, 0, 30, 1, 6, nullptr, "+%d%%"},
        {46, 0x10000, 2, 20, 1, 2, nullptr, "+%d"},
        {47, 0x20000, 2, 30, 1, 4, nullptr, "+%d"},
        {48, 0x10000, 0, 20, 1, 2, nullptr, "+%d%%"},
        {49, 0x20000, 0, 30, 1, 4, nullptr, "+%d%%"},
        {50, 0x40000, 0, 100, 1, 2, nullptr, "+%d%%"},
        {51, 0x80000, 0, 150, 1, 4, nullptr, "+%d%%"},
        {52, 0x4000, 1, 50, 1, 3, nullptr, "+%d"},
        {53, 0x8000, 1, 75, 1, 6, nullptr, "+%d"},
    };

    s_colony* colony;

    s_animation_header* darkened_turn_button_seg;

    s_animation_header* fleet_box_selection_seg;
    int16_t ship_frame;
    s_animation_header* mainscrn_plyr_done_lights_seg;
    uint16_t skip_fields;

    uint8_t ship_icon_width[4];
    uint8_t ship_icon_height[4];

    s_star_data* star;

    uint8_t orbit_to_satellite_type[10][5] = {
        {MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_ASTEROID},
        {MAPGEN_SATELLITE_COMPANION_STAR, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_GAS_GIANT},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_GAS_GIANT, MAPGEN_SATELLITE_ASTEROID, MAPGEN_SATELLITE_GAS_GIANT, MAPGEN_SATELLITE_GAS_GIANT},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_GAS_GIANT, MAPGEN_SATELLITE_GAS_GIANT, MAPGEN_SATELLITE_GAS_GIANT},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_GAS_GIANT, MAPGEN_SATELLITE_GAS_GIANT, MAPGEN_SATELLITE_GAS_GIANT},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_GAS_GIANT},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET},
        {MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET, MAPGEN_SATELLITE_PLANET},
    };

    s_ship_node ship_node[MAX_SHIPS];

    int16_t ship_stack_count;

    int16_t ship_stack_start[MAX_SHIPS];

    uint8_t fleet_box_ship_ids[BITMAP(MAX_SHIPS)];

    void* other_ships_screen_seg;
    void* fleet_box_grid_sm_seg4;
    int16_t icon_overlay_y;
    int16_t ship_icon_count;
    int16_t icon_overlay_x;
    int16_t fleet_box_ship_stack;
    s_ship_icon ship_icon[SHIP_ICON_COUNT];
    int8_t black_hole_anim_count[MAX_STARS];
    int16_t bill_savegame[6] = {2, 0, 0, 0, 0, 0};
    uint16_t antaran_resource_level;
    s_crew_data crew_data[CREW_QUALITY_COUNT] = {
        {0, 0, 0, 0},
        {50, 15, 15, 5},
        {150, 30, 30, 10},
        {500, 50, 50, 15},
        {1000, 75, 75, 20},
    };
    uint16_t chance_for_antaran_invasion;
    uint16_t disallow_saves_code;
    int16_t max_map_scale;
    int16_t MAP_MAX_X;
    int16_t MAP_MAX_Y;
    uint8_t g_galaxy_size;
    void* nebula_screen_seg;
    s_animation_header* nebula_pict_seg[12][4];
    uint8_t planet_max_farms[PLANET_SIZE_COUNT] = {2, 4, 5, 7, 10};
    uint8_t planet_size_table[PLANET_SIZE_COUNT] = {1, 3, 7, 9, 10};
    uint8_t climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT] = {
        {15, 15, 10, 20},
        {55, 50, 15, 0},
        {25, 25, 10, 70},
        {5, 10, 10, 0},
        {0, 5, 10, 8},
        {0, 0, 10, 2},
        {0, 0, 11, 0},
        {0, 0, 11, 0},
        {0, 0, 11, 0},
        {0, 0, 2, 0},
    };
    uint8_t normal_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT] = {
        {15, 15, 10, 20},
        {55, 50, 15, 0},
        {25, 25, 10, 70},
        {5, 10, 10, 0},
        {0, 5, 10, 8},
        {0, 0, 10, 2},
        {0, 0, 11, 0},
        {0, 0, 11, 0},
        {0, 0, 11, 0},
        {0, 0, 2, 0},
    };
    uint8_t old_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT] = {
        {15, 5, 5, 20},
        {40, 30, 8, 0},
        {20, 20, 8, 50},
        {25, 25, 13, 0},
        {0, 20, 13, 30},
        {0, 0, 13, 0},
        {0, 0, 13, 0},
        {0, 0, 13, 0},
        {10, 0, 0, 0},
        {4, 0, 0, 0},
    };
    s_movable_box moveable_box[5];
    uint8_t fleet_movement_icons_on;
    int16_t fleet_icon_buttons_added;
    int16_t fleet_box_outpost_button;
    int16_t fleet_box_attack_button;
    int16_t fleet_box_scroll_dn_button;
    int16_t fleet_box_transport_button;
    int16_t fleet_box_engage_button;
    int16_t fleet_box_scroll_up_button;
    s_animation_header* fleet_box_name_lg_seg;
    s_animation_header* fleet_box_name_sm_seg;
    s_animation_header* fleet_box_bottom_lg_seg;
    s_animation_header* fleet_box_close_lg_button_seg;
    s_animation_header* fleet_box_close_sm_button_seg;
    s_animation_header* fleet_box_engage_lg_button_seg;
    s_animation_header* fleet_box_engage_sm_button_seg;
    s_animation_header* fleet_box_colonize_lg_button_seg;
    s_animation_header* fleet_box_colonize_sm_button_seg;
    s_animation_header* fleet_box_transport_lg_button_seg;
    s_animation_header* fleet_box_transport_sm_button_seg;
    s_animation_header* fleet_box_outpost_lg_button_seg;
    s_animation_header* fleet_box_outpost_sm_button_seg;
    s_animation_header* fleet_box_att_xenons_lg_button_seg;
    s_animation_header* fleet_box_att_xenons_sm_button_seg;
    s_animation_header* fleet_box_bottom_sm_seg;
    s_animation_header* fleet_box_grid_lg_seg;
    int16_t fleet_movement_box_ptr[9];
    s_fleet_box fleet_box;
    s_animation_header* fleet_box_scroll_dn_button_seg;
    int16_t fleet_movement_bar_field;
    s_animation_header* fleet_box_scroll_up_button_seg;
    s_animation_header* fleet_box_all_button_seg;
    int8_t mouse_button_pressed;
    s_save_game_description save_game_description[10];
    int16_t popup_base_x;
    int16_t popup_base_y;
    int16_t screen_data;
    int16_t screen_mouse_list_type;
    int16_t temp_star_handle;
    s_animation_header* gate_icon_seg[16];
    int16_t moveable_box_reorder_fallback;
    int16_t moveable_box_order[5];
    int16_t box_off_flag[5];
    s_saved_box saved_boxes[5];
    uint8_t fleet_box_scroll_first_ship;
    int16_t galaxy_map_grid_field;
    int16_t galaxy_map_height;
    int16_t galaxy_map_grid_y;
    int16_t galaxy_map_width;
    int16_t galaxy_map_grid_x;
    int16_t galaxy_map_star_field[MAX_STARS];
    s_animation_header* fltscrn_flashing_ship_pict_seg[8];
    s_animation_header* fltscrn_flashing_monster_ship_icon[7];

    s_mouse_list mouse_list_zoom_mode[5] = {
        {6, 0, 22, 22, 526, 420},
        {1, 0, 0, 0, 639, 21},
        {1, 0, 0, 0, 21, 479},
        {1, 0, 527, 0, 639, 479},
        {1, 0, 0, 421, 639, 479},
    };
    uint8_t main_palette_player_colors[MAX_PLAYERS] = {73, 98, 110, 32, 62, 148, 45, 85};
    size_t h_string_ix[397];
    std::vector<char> h_strings;
    s_help_box help_entry_list[50];
    uint8_t planet_special = 64;
    uint8_t planet_special_weighted_chance[SYSTEM_SPECIAL_COUNT] = {64, 5, 3, 2, 3, 2, 9, 4, 3, 0, 5, 0};
    s_mouse_list mouse_list_arrow = {1, 0, 0, 0, 639, 479};
    s_mouse_list cmbt_mouse_list_clock = {1, 0, 0, 0, 639, 479};
    s_mouse_list mouse_list_variable = {20, 0, 0, 0, 639, 479};
    s_mouse_list mouse_list_none = {0, 0, 0, 0, 639, 479};
    s_animation_header* near_star_seg;
    const char s_ATZ_0055AA7C[4] = "ATZ";
    const char s_AT_K0_M1_D2_0055AA6C[14] = "AT &K0 M1 &D2";
    const char s_AT_H0_S0_0_0055AA60[11] = "AT H0 S0=0";
    const char s_5551212_0055AA58[8] = "5551212";
    const char s_fmt_s[3] = "%s";
    const char s_fmt_scanned_star_info[8] = "%s (%s)";

    s_ship_move_info g_ship_move_info;

    s_fleet_movement_icon fleet_movement_icon[MAX_SHIPS];

    void __cdecl Apply_Save_Settings(s_settings* settings) {
        MOX::settings.xenons_exist = settings->xenons_exist;
        MOX::settings.game_difficulty = settings->game_difficulty;
        // The saved setup preference is separate from the loaded game's player count.
        MOX::settings.number_of_players = (int8_t)INITGAME::Normalize_New_Game_Player_Count(
            settings->number_of_players, MOX::NUM_PLAYERS);
        MOX::settings.galaxy_size = settings->galaxy_size;
        MOX::settings.galaxy_age = settings->galaxy_age;
        MOX::settings.strategic_combat_flag = settings->strategic_combat_flag;
        MOX::settings.starting_civilization_level = settings->starting_civilization_level;
        MOX::settings.start_of_game_seed = settings->start_of_game_seed;
        MOX::settings.random_events = settings->random_events;
    }
} // namespace MOX