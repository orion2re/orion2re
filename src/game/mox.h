#pragma once

#include <cstddef>
#include <vector>

#include "game_state.h"

typedef struct s_skill_data s_skill_data, *Ps_skill_data;

struct s_skill_data {
    uint8_t skill_id;
    uint32_t skill_mask;
    uint8_t skill_type;
    int16_t strength;
    int8_t level_up;
    uint8_t cost;
    const char* name;
    const char* format_str;
};

namespace MOX {
    extern const char ship_type_names[SHIP_TYPE_COUNT][6][16];
    extern int8_t tech_categories[51][2];
    extern const char s_combat_lbx[11];
    extern int16_t first_field_in_group[10];
    extern s_animation_header* fighter_seg[15][64];
    extern uint8_t draw_officer_to_back;
    extern int16_t ordnance_bonus[15];
    extern int16_t n_fltscrn_big_icons_added;
    extern int16_t n_fltscrn_big_icons;
    extern int8_t n_fltscrn_rows;
    extern int16_t old_small_ship_stack_ptr;
    extern int16_t old_first_fltscrn_icon;
    extern int16_t n_big_icons_selected;
    extern uint8_t galaxy_star_anim_count;
    extern int16_t fltscrn_bar_value;
    extern int8_t big_icon_display_columns;
    extern int8_t big_icon_display_rows;
    extern uint16_t ken_trace;
    extern int8_t big_icon_display_max_icons;
    extern int16_t big_icon_x_offset;
    extern int16_t big_icon_y_offset;
    extern int16_t offpop_big_icon_selected;
    extern uint8_t drawing_officer_popup;
    extern int16_t* fltscrn_ship_icon_selection_list;
    extern uint8_t look_at_colonies;
    extern s_big_icon* fltscrn_big_icon;
    extern uint8_t add_help_fields;
    extern uint8_t first_time;
    extern int16_t fleet_movement_bar_value;
    extern uint8_t draw_expanding_box;
    extern uint8_t mainscr_palette;
    extern s_scroll_bar* scroll_bar;
    extern int16_t fltscrn_stack_owner;
    extern uint8_t add_scrap_button;
    extern uint8_t skip_draw_ships;
    extern uint8_t fix_palette;
    extern s_animation_header* scrap_button_seg;
    extern s_animation_header* colony_marker_seg[MAX_PLAYERS];
    extern s_animation_header* outpost_marker_seg[MAX_PLAYERS];
    extern s_animation_header* o_highlight_seg;
    extern uint8_t add_all_button;
    extern s_animation_header* fleet_all_button_seg;
    extern s_animation_header* fleet_return_button_seg;
    extern int16_t old_fltscrn_stack_owner;
    extern s_animation_header* up_scroll_button_seg;
    extern s_animation_header* down_scroll_button_seg;
    extern s_animation_header* prev_fleet_button_seg;
    extern s_animation_header* next_fleet_button_seg;
    extern s_animation_header* relocate_fleet_button_seg;
    extern s_animation_header* officers_button_seg;
    extern uint8_t add_officers_button;
    extern int16_t fltscrn_support_button_status;
    extern int16_t fltscrn_combat_button_status;
    extern s_mp_screen* mp_screen;
    extern int8_t player_mode[15];
    extern int16_t cur_ship;
    extern int16_t net_destination;
    extern int16_t done_button;
    extern int16_t msl_impact_warning_button;
    extern int16_t draw_legal_boxes_button;
    extern int16_t msl_warning_button;
    extern int16_t wait_button;
    extern int16_t scan_button;
    extern int16_t missile_field[MAX_COMBAT_FLYERS];
    extern int16_t combat_grid_width;
    extern int16_t combat_grid_height;
    extern int16_t combat_grid[MAX_COMBAT_GRID_WIDTH][MAX_COMBAT_GRID_HEIGHT];
    extern int16_t weapon_mod_field_status[WEAPON_MOD_COUNT];
    extern int16_t weapon_button[8];
    extern int16_t ship_turn_order[MAX_COMBAT_SHIPS];
    extern int16_t repl_fields[10];
    extern int16_t side_end_of_turn[15];
    extern int16_t unused_ship_turn_order_marker;
    extern int16_t design_choice_items[40];
    extern int16_t weapon_mod_fields[WEAPON_MOD_COUNT];
    extern int16_t weapon_arc_field[5];
    extern int16_t weapon_rack_field[5];
    extern int16_t multi_colored_box_color_start;
    extern int16_t beam_filter_button;
    extern int16_t missile_filter_button;
    extern int16_t special_filter_button;
    extern int16_t replacement_box_accept_button;
    extern int16_t bomb_filter_button;
    extern const char s__00551100[1];

    extern uint16_t fleet_box_map_height;
    extern s_current_design* design;

    extern int16_t scanned_big_ship;
    extern int16_t small_ship_stack_ptr;
    extern int16_t scanned_small_ship;
    extern int16_t n_fltscrn_officers;
    extern int16_t* officer_id_list;
    // Suggested name: MOX::star_colors_seg
    extern s_animation_header* star_colors_seg[11];
    extern s_animation_header* fleet_galaxy_star_seg[11];
    extern s_animation_header* colony_galaxy_star_seg[10];
    extern s_animation_header* officer_skill_icon_seg[27];
    extern s_animation_header* hl_antaran_ship_icon_seg[24];
    extern s_animation_header* antaran_ship_icons[24];
    extern s_animation_header* o_ring_seg[5];
    extern s_animation_header* planet_mask_seg[PLANET_SIZE_COUNT];
    extern s_animation_header* fltscrn_small_ship_icon_seg[8];
    extern s_animation_header* monster_ship_seg[4];
    extern s_animation_header* hl_monster_ship_icon_seg[4];
    extern s_animation_header* ship_icon_seg[32];
    extern s_animation_header* hl_ship_icon_seg[32];
    extern s_animation_header* fleet_background_seg;
    extern void* officer_pictures_screen_seg;
    extern s_animation_header* officer_main_seg;
    extern s_animation_header* officer_colony_box_seg;
    extern s_animation_header* officer_colony_ldrs_button_seg;
    extern s_animation_header* officer_ship_ldrs_button_seg;
    extern s_animation_header* officer_up_button_seg;
    extern s_animation_header* officer_dn_button_seg;
    extern s_animation_header* officer_prev_button_seg;
    extern s_animation_header* officer_next_button_seg;
    extern s_animation_header* officer_hire_button_seg;
    extern s_animation_header* officer_return_button_seg;
    extern s_animation_header* officer_cancel_hire_button_seg;
    extern s_animation_header* selected_box_seg;
    extern s_animation_header* scanned_box_seg;
    extern s_animation_header* fleet_scroll_anim_seg;
    extern s_animation_header* officer_normal_window_seg;
    extern s_animation_header* officer_bright_window_seg;
    extern s_animation_header** temp_officers_pict;
    extern s_animation_header* officer_dismiss_button_seg;
    extern s_animation_header* officer_fleet_box_seg;
    extern s_animation_header* dull_officer_dismiss_button_seg;
    extern s_animation_header* dull_officer_pool_button_seg;
    extern s_animation_header* officer_pool_button_seg;
    extern s_animation_header* dull_officer_hire_button_seg;
    extern s_animation_header* officer_hire_replace_seg;
    extern int8_t officer_scanned;
    extern int8_t officer_scrn_type;
    extern int8_t officer_selected;

    extern int16_t special_scroll_dn_button;
    extern int8_t n_weapons_loaded;
    extern uint8_t n_special_systems;

    extern int8_t* fltscrn_icon_selection_status;

    // References
    extern uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT];
    extern uint8_t star_class_table[STAR_CLASS_COUNT][GALAXY_AGE_COUNT];
    extern int16_t ranged_to_hit_penalty[9];
    extern int16_t ranged_damage_penalty[9];
    extern int16_t cur_missile_count;
    extern int16_t NUM_STARS;
    extern s_scroll_bar fltpop_scroll_bar;
    extern int16_t cur_map_scale;
    extern int16_t cur_map_x;
    extern int16_t cur_map_y;
    extern uint16_t global_esc;
    extern int16_t return_screen;
    extern char* save_game_dates;
    extern char* save_game_stardates;
    extern char* star_name_loaded_string;
    extern int16_t scanned_field;
    extern int8_t previous_screen;
    extern int16_t max_debug_saves;
    extern int16_t first_debug_save;
    extern int16_t debug_saves_on;
    extern uint8_t turns_between_debug_saves;
    extern void* sound_buffer;
    extern int16_t hot_seat_flag;
    extern int16_t current_screen;
    extern int16_t design_screen_replacement_type;
    extern int16_t design_screen_replacement_slot;
    extern int16_t weapons_field[8];
    extern s_animation_header* asteroid_seg;
    extern s_animation_header* screen_seg;
    extern s_animation_header* diplomacy_request_popup_seg;
    extern s_animation_header* combat_bitmap;
    extern s_animation_header* combat_map_planet_seg;
    extern s_animation_header* combat_planet_seg;
    extern s_animation_header* combat_planet_seg2;
    extern uint8_t ship_darken_map[128][16];
    extern uint8_t given_chance_to_rename_home_star[MAX_PLAYERS];
    extern uint8_t player_has_ship_in_system;
    extern s_animation_header* moveable_box_pict[25];
    extern int8_t game_type;
    extern s_antaran antarans;
    extern int8_t maps_to_generate;
    extern uint8_t auto_turn_exit;
    extern int16_t special_scroll_up_button[2];
    extern uint8_t blue_color2;
    extern int16_t auto_msl_warning_off;
    extern s_animation_header* draw_ship_bitmap;
    extern int16_t multi_colored_line_start;
    extern int16_t design_choice_item_selected[40];
    extern int8_t first_special_system_ctr;
    extern int16_t special_system_field[8];
    extern int16_t planet_buttons[5];
    extern uint8_t first_design_screen_call;
    extern int16_t design_size;
    extern uint8_t refresh_mode;
    extern uint8_t mox_font_colors_offset;
    extern int16_t star_width;
    extern int16_t star_height;
    extern char temp_string[512];
    extern char temp_string_buf[64];
    extern int16_t take_control_button;
    extern int16_t auto_cmbt_button_value;
    extern int16_t PLAYER_NUM;
    extern uint8_t active_player[9];
    extern int16_t active_count;
    extern int16_t ai_retreat_flag;
    extern int16_t bio_weapon_flag;
    extern int16_t last_ship_status;
    extern int16_t ship_status_display;
    extern int16_t dont_draw_ship;
    extern int16_t draw_active_missiles;
    extern int16_t order_info_flag;
    extern int16_t super_fast_flag;
    extern int8_t real_attack_flag;
    extern int16_t shields_flag;
    extern int16_t msl_impact_warning_flag;
    extern int16_t draw_legal_boxes_flag;
    extern int16_t take_control_flag;
    extern int16_t combat_far_x;
    extern int16_t combat_mid_x;
    extern int16_t combat_far_y;
    extern int16_t combat_mid_y;
    extern int16_t net_flag;
    extern int16_t in_nebula_flag;
    extern int16_t skan_flag;
    extern uint8_t draw_design_screen_to_back;
    extern uint8_t refit_ship;
    extern int32_t original_cost;
    extern int16_t design_name_field;
    extern int16_t design_cancel_button;
    extern int16_t design_clear_button;
    extern int16_t design_build_button;
    extern s_player* player;
    extern s_player* cur_plyr_ptr;
    extern char* personality_labels[7];
    extern char* objective_labels[6];
    extern char* treaty_labels[7];
    extern char* help_category_labels[17];
    extern int16_t old_system_index;
    extern int8_t selected_star_color_mod;
    extern int8_t selected_star_color_offset;
    extern int32_t stardate;
    extern s_settings settings;
    extern char* hyper_field_title;
    extern const char s_Harbinger[12];
    extern uint8_t at_least_one_tactical_combat;
    extern s_animation_header* planet_summary_galaxy_star_seg[11];
    extern int8_t first_weapon_ctr;
    extern int16_t fleet_box_colonize_button;
    extern uint8_t reloaded_ship_palette;
    extern uint8_t g_galaxy_age;
    extern int16_t ship_data_id;
    extern int16_t cur_combat_x;
    extern int16_t cur_ptr;
    extern int16_t wait_ptr;
    extern int16_t board_flag;
    extern int16_t tactical_net_choice;
    extern int16_t auto_button;
    extern int16_t auto_combat;
    extern int16_t speedx2_button;
    extern int16_t grid_button;
    extern int16_t shields_button;
    extern int16_t super_fast_button;
    extern int16_t self_destruct_button;
    extern int16_t weapons_button;
    extern int16_t specials_button;
    extern int16_t cmbt_info_button;
    extern int16_t flee_button;
    extern int16_t board_button;
    extern int16_t map_field;
    extern int16_t map_y;
    extern int16_t map_x;
    extern int16_t center_button;
    extern int16_t space_field;
    extern int16_t ypos;
    extern int16_t xpos;
    extern int16_t saved_msl_warn;
    extern uint8_t red_color_pulse_value;
    extern int8_t red_color_pulse_incr;
    extern int16_t fleet_ship_scanned;
    extern int16_t fleet_movement_all_button;
    extern int16_t galaxy_map_scanned_star;
    extern int8_t draw_system_display_without_button;
    extern uint8_t need_to_restore_help_list;
    extern int16_t ship_icon_player_id;
    extern int8_t at_least_one_report;
    extern uint8_t previous_report;
    extern int16_t first_node_ptr_in_stack_;
    extern int16_t system_display_ring_radius[5];
    extern uint8_t rot_plan_dim[PLANET_SIZE_COUNT];
    extern int16_t system_star_id;
    extern int16_t system_star_id_for_auto_select;
    extern uint8_t draw_fleet_screen_to_back;
    extern s_mouse_xy mouse_XY;
    extern int16_t system_display_ship_scanned;
    extern int16_t system_icon_scanned;
    extern uint16_t global_exit_field;
    extern int16_t eta_star_id;
    extern int16_t stencil_box_id;
    extern int16_t draw_main_screen_stencilled;
    extern int16_t animated_star;
    extern s_mouse_list mouse_list_zoom_mode[5];
    extern int8_t just_moved_stack;
    extern int16_t first_fleet_movement_icon;

    extern int16_t next_free_node;

    extern uint8_t main_palette_player_colors[MAX_PLAYERS];

    extern size_t h_string_ix[397];
    extern std::vector<char> h_strings;

    extern s_help_box help_entry_list[50];

    extern uint8_t planet_special;

    extern uint8_t planet_special_weighted_chance[SYSTEM_SPECIAL_COUNT];

    extern s_mouse_list mouse_list_arrow;
    extern s_mouse_list cmbt_mouse_list_clock;
    extern s_mouse_list mouse_list_variable;
    extern s_mouse_list mouse_list_none;

    extern int16_t mid_star_x;
    extern int16_t mid_star_y;
    extern int16_t design_screen_replacement_item;
    extern int16_t far_star_x;
    extern int16_t far_star_y;
    extern int16_t near_star_y;
    extern int16_t near_star_x;
    extern s_animation_header* near_star_seg;

    extern const char s_ATZ_0055AA7C[4];

    extern const char s_AT_K0_M1_D2_0055AA6C[14];

    extern const char s_AT_H0_S0_0_0055AA60[11];

    extern const char s_5551212_0055AA58[8];

    extern const char s_fmt_s[3];

    extern const char s_fmt_scanned_star_info[8];

    extern const char s_fmt_contested[15];

    extern void* special_names_seg;

    extern void* global_data_seg;

    extern s_nebula* nebula;

    extern uint8_t NUM_NEBULAS;

    extern int16_t NUM_COLONIES;

    extern int16_t planet_count;

    // Suggested name: MOX::climate_modifier_table
    extern int8_t climate_modifier_table[MINERAL_RESOURCE_COUNT];

    extern s_planet_data* planet;

    extern const char* roman_literals[21];

    extern char* planet_special_string[SYSTEM_SPECIAL_COUNT];
    extern char* planet_climate_string[PLANET_CLIMATE_COUNT];
    extern char* mineral_class_string[MINERAL_RESOURCE_COUNT];
    extern uint8_t red_color2;
    extern char* planet_size_string[PLANET_SIZE_COUNT];
    extern char* planet_type_string[10];
    extern char* planet_group_string[PLANET_GROUP_COUNT];
    extern char* planet_atmosphere_string[3];
    extern char* spectral_class_string[STAR_CLASS_COUNT];
    extern char* admin_skill_names[9];

    extern char* planet_gravity_string[PLANET_GRAVITY_COUNT];

    extern int16_t NUM_SHIPS;
    extern void* global_bitmap_seg;
    extern uint8_t first_time_drawn;
    extern uint8_t draw_main_screen_to_back;
    extern uint8_t dont_draw_main_screen_to_back;
    extern int16_t zoom_count;
    extern int16_t max_zoom_count;
    extern uint8_t star_bg_index;
    extern s_animation_header* star_bg_seg;
    extern void* scan_ship_seg;
    extern s_animation_header* detail_fleet_icon_seg;
    extern uint16_t fleet_icon_button_count;
    extern int16_t weapons_count_plus_button[8];
    extern int16_t weapons_count_minus_button[8];
    extern int16_t design_ship_size_field[6];
    extern void* techname_seg;
    extern void* tech_desc_seg;
    extern s_animation_header* main_screen_seg;
    extern s_animation_header* design_screen_seg;
    extern s_animation_header* design_icon_lf_arrow_seg;
    extern s_animation_header* design_icon_rt_arrow_seg;
    extern s_animation_header* clear_design_button_seg;
    extern s_animation_header* cancel_design_button_seg;
    extern s_animation_header* build_design_button_seg;
    extern s_animation_header* plus_button_seg;
    extern s_animation_header* minus_button_seg;
    extern s_animation_header* d_scroll_up_button_seg;
    extern s_animation_header* d_scroll_dn_button_seg;
    extern s_animation_header* dull_d_scroll_up_button_seg;
    extern s_animation_header* dull_d_scroll_dn_button_seg;
    extern s_animation_header* beam_filter_button_seg;
    extern s_animation_header* missile_filter_button_seg;
    extern s_animation_header* bomb_filter_button_seg;
    extern s_animation_header* special_filter_button_seg;
    extern s_animation_header* weapons_cancel_button_seg;
    extern s_animation_header* weapons_accept_button_seg;
    extern s_animation_header* weapons_top_box_seg;
    extern s_animation_header* weapons_mid_box_seg;
    extern s_animation_header* weapons_bot_box_seg;
    extern s_animation_header* weapons_selected_seg;
    extern s_animation_header* weapons_blank_mods_seg;
    extern s_animation_header* system_repl_scroll_bar_seg;
    extern s_animation_header* system_repl_box_top_seg;
    extern s_animation_header* system_repl_box_mid_seg;
    extern s_animation_header* system_repl_box_btm_seg;
    extern s_animation_header* system_repl_box_cancel_btn;
    extern int16_t weapons_clear_button[8];
    extern int16_t special_system_clear_button[8];
    extern s_animation_header* design_size_button_seg[6];
    extern s_animation_header* darkened_size_button_seg[6];
    extern s_animation_header* weapons_arc_pict_seg[5];
    extern s_animation_header* weapons_arc_text_seg[5];
    extern s_animation_header* weapons_rack_text_seg[5];
    extern s_animation_header* weapons_special_seg[18];
    extern s_weapon_filter_item* weapon_filter;
    extern char* special_desc_string[39];
    extern char* weapon_notes_string[40];
    extern s_animation_header* d_scroll_bar_seg;
    extern s_animation_header* weapons_rack_seg;

    extern s_animation_header* mainscrn_diplo_lights_seg;

    extern s_animation_header* diplomacy_request_wait_seg;
    extern s_animation_header* diplomacy_request_invoke_seg;
    extern s_animation_header* diplomacy_request_accept_seg;

    extern void* global_screen_seg;

    extern void* global_cache_seg;

    extern s_animation_header* mid_star_seg;
    extern s_animation_header* far_star_seg;
    extern uint8_t fleet_icon_selection_status[BITMAP(MAX_SHIPS)];

    // Suggested name: MOX::star_seg
    extern s_animation_header* star_seg[40];
    extern s_animation_header* sun_seg[STAR_CLASS_COUNT];

    extern uint16_t n_stats_to_generate;

    extern uint32_t global_cache_size;

    extern uint32_t cmd_line_seed;

    extern int16_t ship_icon_lf_button;
    extern int16_t ship_icon_rt_button;

    extern uint8_t zoom_in;
    extern s_animation_header* main_zoom_in_button_seg;
    extern s_animation_header* main_zoom_out_button_seg;
    extern s_animation_header* main_game_button_seg;
    extern s_animation_header* main_next_button_seg;
    extern s_animation_header* main_colonies_button_seg;
    extern s_animation_header* main_planets_button_seg;
    extern s_animation_header* main_fleets_button_seg;
    extern s_animation_header* main_leaders_button_seg;
    extern s_animation_header* main_races_button_seg;
    extern s_animation_header* main_info_button_seg;
    extern int16_t star_button[MAX_STARS];
    extern int16_t main_screen_grid_field;
    extern int16_t main_screen_grid_x;
    extern int16_t main_screen_grid_y;

    extern int16_t NUM_PLAYERS;
    extern uint8_t red_color1;
    extern char* government_labels[GOVERNMENT_COUNT];
    extern char* special_ability_names[32];
    extern int32_t stardate_at_beg_of_turn;
    extern int8_t global_var;
    extern uint16_t FULL_DRAW_SCREEN_FLAG;
    extern uint16_t combat_init;
    extern void* warp_core_remap_colors;
    extern s_mouse_list* combat_mouse_grid;
    extern void* stasis_bubble_remap_colors;
    extern s_animation_header* combat_background_seg;
    extern s_animation_header* combat_far_star;
    extern s_animation_header* combat_mid_star;
    extern s_animation_header* combat_near_star;
    extern uint16_t apply_damage_to_planet;
    extern uint16_t missile_warning;
    extern void* cmbt_option_seg;
    extern s_animation_header* warning_missile_seg;
    extern void* self_destruct_seg;
    extern uint16_t saved_msl_warning;
    extern void* auto_button_seg;
    extern s_animation_header* combat_options_background_seg;
    extern int16_t cur_combat_y;
    extern int16_t grid_flag;
    extern int16_t speedx2_flag;
    extern int16_t auto_combat_flag[2];
    extern int16_t combat_clock_frame;
    extern int16_t combat_near_x;
    extern int16_t combat_near_y;
    extern void* cmbt_done_button_seg;
    extern void* level_symbol_seg;
    extern void* scan_button_seg;
    extern void* board_button_seg;
    extern void* flee_button_seg;
    extern void* wait_button_seg;
    extern void* cmbt_info_button_seg;
    extern void* weapons_button_seg;
    extern void* specials_button_seg;
    extern s_animation_header* msl_warning_on_seg;
    extern void* warning_fighter_seg;
    extern s_animation_header* ship_bitmap;
    extern s_animation_header* darken_plasma_map;
    extern void* shield_remap_colors;
    extern void* pulsar_remap_colors;
    extern s_animation_header* small_tractor_seg;
    extern void* tractor_remap_colors;
    extern void* bhg_remap_colors;
    extern void* plasma_web_remap_colors;
    extern void* stellar_converter_remap_colors;
    extern s_animation_header* msl_seg[112];
    extern s_animation_header* ship_cursor[4];
    extern s_animation_header* shield_div_bitmap[4];
    extern s_animation_header* fltscrn_monster_ship_icon[7];
    extern s_animation_header* shield_mask_bitmap[16];
    extern int16_t fighter_beam_type[9];
    extern int16_t fighter_bomb_type[9];
    extern s_combat_data* combat_data;
    extern s_missile* missile;
    extern int16_t missile_hit_radius[6];
    extern s_net_tactical_combat_message new_message;
    extern int16_t gate_icon_field[4];
    extern int16_t system_display_base_x;
    extern int16_t system_display_base_y;
    extern s_sys_disp* sys_disp;
    extern s_system_display_ships system_display_ships[15];
    extern int16_t system_display_grid_y[6];
    extern int16_t system_display_grid_x[6];
    extern int16_t system_display_grid_field;
    extern char* race_names[20];
    extern int8_t class_to_num_satellites[10][6];
    extern uint16_t star_fields_dim[6];

    extern s_ship_data* ship;

    extern int16_t combat_defender_count;
    extern int16_t combat_attacker_count;
    extern s_qship* qcombat_data;
    extern int16_t combat_total_ship_count;
    extern uint8_t combat_done_waiting;
    extern s_colors combat_green;
    extern int16_t combat_attacker_player;
    extern int16_t combat_turn;
    extern uint8_t colony_defense_flags;
    extern int16_t cmbt_colony_id;
    extern int16_t combat_defender_player;
    extern s_combat_bomb_result combat_bomb_result;

    extern s_combat_data global_combat_data;

    extern int16_t* officers_original_star_id;

    extern int16_t* officers_original_ship_id;

    extern void* dimmed_all_button_seg;
    extern void* dimmed_relocate_button_seg;
    extern void* dimmed_scrap_button_seg;
    extern void* dimmed_officers_button_seg;


    extern char* officer_level_names[6];

    extern char* star_officer_level_names[6];

    extern s_skill_data skill_data[54];

    extern s_colony* colony;

    extern s_animation_header* darkened_turn_button_seg;

    extern s_animation_header* fleet_box_selection_seg;
    extern int16_t ship_frame;
    extern s_animation_header* mainscrn_plyr_done_lights_seg;
    extern uint16_t skip_fields;

    extern uint8_t ship_icon_width[4];
    extern uint8_t ship_icon_height[4];
    extern s_ship_move_info g_ship_move_info;

    extern s_star_data* star;

    // Suggested name: MOX::orbit_to_satellite_type
    extern uint8_t orbit_to_satellite_type[10][5];

    // Suggested name: MOX::ship_node
    extern s_ship_node ship_node[MAX_SHIPS];

    extern int16_t ship_stack_count;

    extern int16_t ship_stack_start[MAX_SHIPS];

    extern uint8_t fleet_box_ship_ids[BITMAP(MAX_SHIPS)];

    extern void* other_ships_screen_seg;

    extern void* fleet_box_grid_sm_seg4;

    extern int16_t icon_overlay_y;

    extern int16_t ship_icon_count;

    extern int16_t icon_overlay_x;

    extern int16_t fleet_box_ship_stack;

    extern s_ship_icon ship_icon[SHIP_ICON_COUNT];

    extern int8_t black_hole_anim_count[MAX_STARS];

    extern int16_t bill_savegame[6];
    extern uint16_t antaran_resource_level;
    extern s_crew_data crew_data[CREW_QUALITY_COUNT];
    extern uint16_t chance_for_antaran_invasion;
    extern uint16_t disallow_saves_code;

    extern int16_t max_map_scale;
    extern int16_t MAP_MAX_X;
    extern int16_t MAP_MAX_Y;
    extern uint8_t g_galaxy_size;

    extern void* nebula_screen_seg;

    // Suggested name: MOX::nebula_pict_segments
    // This represents a 2D array of sprite pointers, dimensions [12][4].
    extern s_animation_header* nebula_pict_seg[12][4];

    // Suggested name: planet_max_farms
    extern uint8_t planet_max_farms[PLANET_SIZE_COUNT];

    extern uint8_t planet_size_table[PLANET_SIZE_COUNT];

    // 4 groups per climate
    extern uint8_t climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT];

    // Suggested name: MOX::normal_galaxy_climate_roll_table
    extern uint8_t normal_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT];

    // Suggested name: MOX::old_galaxy_climate_roll_table
    extern uint8_t old_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT];

    // Array of movable box structures.
    extern s_movable_box moveable_box[5];

    extern uint8_t fleet_movement_icons_on;

    extern int16_t fleet_icon_buttons_added;
    extern int16_t fleet_box_outpost_button;
    extern int16_t fleet_box_attack_button;
    extern int16_t fleet_box_scroll_dn_button;
    extern int16_t fleet_box_transport_button;
    extern int16_t fleet_box_engage_button;
    extern int16_t fleet_box_scroll_up_button;

    extern s_animation_header* fleet_box_name_lg_seg;

    extern s_animation_header* fleet_box_name_sm_seg;

    extern s_animation_header* fleet_box_bottom_lg_seg;

    extern s_animation_header* fleet_box_close_lg_button_seg;
    extern s_animation_header* fleet_box_close_sm_button_seg;
    extern s_animation_header* fleet_box_engage_lg_button_seg;
    extern s_animation_header* fleet_box_engage_sm_button_seg;
    extern s_animation_header* fleet_box_colonize_lg_button_seg;
    extern s_animation_header* fleet_box_colonize_sm_button_seg;
    extern s_animation_header* fleet_box_transport_lg_button_seg;
    extern s_animation_header* fleet_box_transport_sm_button_seg;
    extern s_animation_header* fleet_box_outpost_lg_button_seg;
    extern s_animation_header* fleet_box_outpost_sm_button_seg;
    extern s_animation_header* fleet_box_att_xenons_lg_button_seg;
    extern s_animation_header* fleet_box_att_xenons_sm_button_seg;

    extern s_animation_header* fleet_box_bottom_sm_seg;

    extern s_animation_header* fleet_box_grid_lg_seg;

    extern int16_t fleet_movement_box_ptr[9];

    extern s_fleet_box fleet_box;

    extern s_animation_header* fleet_box_scroll_dn_button_seg;
    extern int16_t fleet_movement_bar_field;
    extern s_animation_header* fleet_box_scroll_up_button_seg;
    extern s_animation_header* fleet_box_all_button_seg;

    extern int8_t mouse_button_pressed;

    extern s_fleet_movement_icon fleet_movement_icon[MAX_SHIPS];

    extern s_save_game_description save_game_description[10];

    extern int16_t popup_base_x;
    extern int16_t popup_base_y;
    extern int16_t screen_data;
    extern int16_t screen_mouse_list_type;
    extern int16_t temp_star_handle;

    // Suggested name: MOX::gate_icon_seg
    extern s_animation_header* gate_icon_seg[16];

    // Cached movable-box id preserved while reordering the z-order list.
    extern int16_t moveable_box_reorder_fallback;

    // Suggested name: MOX::moveable_box_display_order
    extern int16_t moveable_box_order[5];

    // Suggested name: MOX::box_visibility_flags
    extern int16_t box_off_flag[5];

    extern s_saved_box saved_boxes[5];

    extern uint8_t fleet_box_scroll_first_ship;

    extern int16_t galaxy_map_grid_field;

    extern int16_t galaxy_map_height;

    extern int16_t galaxy_map_grid_y;

    extern int16_t galaxy_map_width;
    extern int16_t galaxy_map_grid_x;

    extern int16_t galaxy_map_star_field[MAX_STARS];

    extern s_animation_header* fltscrn_flashing_ship_pict_seg[8];

    extern s_animation_header* fltscrn_flashing_monster_ship_icon[7];

    /**
     * Copies persisted game-setup values into the active global settings.
     * @param settings Source settings record loaded from save data.
     */
    void __cdecl Apply_Save_Settings(s_settings* settings);
} // namespace MOX