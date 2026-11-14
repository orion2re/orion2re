#include <cstddef>
#include "unaligned.h"

#pragma pack(push, 1)

typedef struct s_adv_civ_player s_adv_civ_player, *Ps_adv_civ_player;

typedef struct s_adv_civ_player_star s_adv_civ_player_star, *Ps_adv_civ_player_star;

typedef struct s_adv_civ_player_planet s_adv_civ_player_planet, *Ps_adv_civ_player_planet;

struct s_adv_civ_player_planet {
    int8_t is_homeworld;
    int16_t proximity_bonus;
    int32_t worth;
    int16_t planet_idx;
    int8_t is_settled;
};

struct s_adv_civ_player_star {
    int16_t star_idx;
    int16_t min_dist_to_colony;
};

struct s_adv_civ_player {
    int16_t candidate_star_count;
    int16_t candidate_planet_count;
    int16_t settled_planet_count;
    int16_t furthest_star_dist_from_home;
    int8_t has_planet[MAX_PLANETS];
    struct s_adv_civ_player_star stars[MAX_STARS];
    struct s_adv_civ_player_planet planets[MAX_PLANETS];
};

typedef struct s_adv_planet_worth s_adv_planet_worth, *Ps_adv_planet_worth;

typedef struct s_adv_planet_worth_s1 s_adv_planet_worth_s1, *Ps_adv_planet_worth_s1;

struct s_adv_planet_worth_s1 {
    int16_t planet_idx;
    int32_t worth;
};

struct s_adv_planet_worth {
    int8_t num_planets;
    struct s_adv_planet_worth_s1 planets[20];
};

typedef struct s_alloc_header s_alloc_header, *Ps_alloc_header;

struct s_alloc_header {
    uint32_t current;
    uint32_t size;
    uint32_t reserved;
};

typedef struct s_animation_header s_animation_header, *Ps_animation_header;

struct s_animation_header {
    int16_t width;
    int16_t height;
    int16_t current_frame;
    int16_t frame_count;
    int8_t loop_frame;
    int8_t keyframe_interval;
    uint8_t unknown_0xa;
    undefined1 flags;
};

// The frame offset table follows the header as uint32_t entries, but
// animations are loaded at arbitrary addresses, so the table has no
// alignment guarantee. Access entries only through these helpers.
inline uint8_t* Animation_Frame_Table_(s_animation_header* header) {
    return reinterpret_cast<uint8_t*>(header) + sizeof(s_animation_header);
}

inline const uint8_t* Animation_Frame_Table_(const s_animation_header* header) {
    return reinterpret_cast<const uint8_t*>(header) + sizeof(s_animation_header);
}

inline uint32_t Animation_Frame_Offset_(const s_animation_header* header, int32_t frame) {
    return unaligned::Load<uint32_t>(Animation_Frame_Table_(header) + frame * sizeof(uint32_t));
}

inline void Set_Animation_Frame_Offset_(s_animation_header* header, int32_t frame, uint32_t offset) {
    unaligned::Store<uint32_t>(Animation_Frame_Table_(header) + frame * sizeof(uint32_t), offset);
}

inline uint8_t* Animation_Frame_Data_(s_animation_header* header, int32_t frame) {
    return reinterpret_cast<uint8_t*>(header) + Animation_Frame_Offset_(header, frame);
}

inline const uint8_t* Animation_Frame_Data_(const s_animation_header* header, int32_t frame) {
    return reinterpret_cast<const uint8_t*>(header) + Animation_Frame_Offset_(header, frame);
}

typedef struct s_antaran s_antaran, *Ps_antaran;

struct s_antaran {
    int16_t attack_pressure;
    int16_t resource_offensive;
    int16_t resource_defensive;
    int16_t defensive_ships[HULL_CLASS_COUNT];
    int16_t offensive_ships[HULL_CLASS_COUNT];
    int16_t offensive_deployed[HULL_CLASS_COUNT];
    undefined1 field6_0x3c;
    int32_t invasion_stardate;
    uint8_t attack_notified_mask;
    uint8_t offensive_titan_built;
    uint8_t offensive_limit_exhausted_mask;
};

typedef struct s_battle_results s_battle_results, *Ps_battle_results;

typedef struct s_combat_bomb_result s_combat_bomb_result, *Ps_combat_bomb_result;

typedef struct s_battle_results_info s_battle_results_info, *Ps_battle_results_info;

typedef struct s_net_player_info s_net_player_info, *Ps_net_player_info;

typedef struct s_invasion_side_info s_invasion_side_info, *Ps_invasion_side_info;

typedef struct s_invasion_player_bonuses s_invasion_player_bonuses, *Ps_invasion_player_bonuses;

struct s_invasion_player_bonuses {
    uint8_t antigrav_bonus;
    uint8_t battleoids_bonus;
    int8_t has_battleoids;
    uint8_t powered_armor_bonus;
    int8_t has_powered_armor;
    uint8_t best_armor_tech_app_id;
    uint8_t troop_bonus;
    uint8_t personal_shield_tech_app_id;
    uint8_t personal_shield_bonus;
    uint8_t rifle_tech_app_id;
    uint8_t rifle_bonus;
    int8_t ground_combat_bonus;
    int8_t heavy_g_bonus;
    int8_t low_g_penalty;
    int8_t subterranean_bonus;
    int8_t difficulty_bonus;
    int16_t officer_bonus;
    int8_t officer_idx;
};

struct s_invasion_side_info {
    int16_t player_id;
    int16_t unit_strengths[UNIT_COUNT];
    int16_t unit_counts[UNIT_COUNT];
    int8_t unit_hps[UNIT_COUNT];
    int8_t current_troop_type_idx;
    int8_t current_unit_damage;
    int8_t round_hit_unit_type;
    int8_t round_killed_unit_type;
    int16_t colony_id;
    int16_t* ship_list;
    int16_t n_ships;
    int16_t opponent_player_id;
    struct s_invasion_player_bonuses combat_modifiers;
};

struct s_battle_results_info {
    struct s_invasion_side_info attacker_invasion;
    struct s_invasion_side_info defender_invasion;
    int32_t ground_defense_strength;
    int16_t captured_techs_data[TECH_APP_COUNT];
    int16_t captured_tech_id;
    int16_t captured_tech_planet_id;
    int8_t planet_destroyed_flag;
    int16_t orion_techs[7];
    int16_t rescued_hero_id;
    char summary[100];
    char details[1000];
    int8_t is_mind_controlled;
};

struct s_combat_bomb_result {
    int8_t attacker_idx;
    int16_t colony_idx;
    int8_t is_planetary_hit;
    int16_t bomb_hits_remaining;
    int8_t is_bio_weapon;
    int16_t bio_hits_remaining;
    int16_t infantry_killed;
    int16_t tanks_killed;
    int16_t population_killed;
    int8_t buildings_destroyed[BUILDING_COUNT];
    int16_t bio_pop_killed;
    int8_t damage_occurred;
    int8_t colony_wiped_out;
    int16_t production_lost;
    int32_t total_hits_processed;
};

struct s_net_player_info {
    int8_t player_state;
    int16_t combat_star_id;
    int16_t combat_defender_id;
    int16_t combat_planet_id;
    int16_t combat_target_player_id;
    uint8_t combat_type;
    int8_t combat_attacker_id;
    int8_t combat_result;
    int8_t diplomacy_target_id;
};

struct s_battle_results {
    struct s_net_player_info player_info;
    int8_t outpost_destroyed_flag;
    int16_t planet_id;
    struct s_combat_bomb_result bomb;
    struct s_battle_results_info info;
};

typedef struct s_beam_dmg_msg s_beam_dmg_msg, *Ps_beam_dmg_msg;

struct s_beam_dmg_msg {
    int16_t x;
    int16_t y;
    uint16_t total_dmg;
    uint16_t hull_dmg;
    uint16_t shield_dmg;
    uint16_t anim_timer;
    uint8_t side;
};

typedef struct s_beam_fx_entry s_beam_fx_entry, *Ps_beam_fx_entry;

struct s_beam_fx_entry {
    int16_t weapon_id;
    int16_t shot_count;
    uint16_t result_flags;
    uint16_t special_flag;
    int16_t accum_hit_x;
    int16_t accum_hit_y;
    uint16_t specials;
    int8_t is_damage_entry;
};

typedef struct s_big_icon s_big_icon, *Ps_big_icon;

struct s_big_icon {
    int16_t grid_x;
    int16_t grid_y;
    int8_t selected;
    int16_t screen_x;
    int16_t screen_y;
    int16_t button_id;
    int16_t ship_idx;
};

typedef struct s_buffer_cached_segment s_buffer_cached_segment, *Ps_buffer_cached_segment;

struct s_buffer_cached_segment {
    uint8_t is_loaded;
    int16_t buffer_seg_id;
    int32_t last_use_counter;
    void* data_ptr;
};

typedef struct s_build_ship_icon_item s_build_ship_icon_item, *Ps_build_ship_icon_item;

struct s_build_ship_icon_item {
    int8_t player_idx;
    int16_t ship_idx;
    int16_t icon_idx;
};

typedef struct s_class_group s_class_group, *Ps_class_group;

struct s_class_group {
    int8_t planet_group_by_orbit[5];
};

typedef struct s_coldraw_road s_coldraw_road, *Ps_coldraw_road;

struct s_coldraw_road {
    undefined field0_0x0;
    undefined field1_0x1;
    undefined field2_0x2;
    undefined field3_0x3;
    undefined field4_0x4;
    undefined field5_0x5;
    undefined field6_0x6;
    undefined field7_0x7;
    undefined field8_0x8;
    undefined field9_0x9;
    undefined field10_0xa;
    undefined field11_0xb;
    undefined field12_0xc;
    undefined field13_0xd;
    undefined field14_0xe;
    undefined field15_0xf;
    int16_t anims[4];
    uint8_t connections[4];
    undefined field18_0x1c;
};

typedef struct s_coldraw_veggie_cell s_coldraw_veggie_cell, *Ps_coldraw_veggie_cell;

typedef struct s_coldraw_veggie_sprite s_coldraw_veggie_sprite, *Ps_coldraw_veggie_sprite;

struct s_coldraw_veggie_sprite {
    int16_t screen_x;
    int16_t screen_y;
    uint16_t draw_flag;
};

struct s_coldraw_veggie_cell {
    undefined field0_0x0;
    undefined field1_0x1;
    undefined field2_0x2;
    undefined field3_0x3;
    undefined field4_0x4;
    undefined field5_0x5;
    undefined field6_0x6;
    undefined field7_0x7;
    int16_t anim_entries[2];
    struct s_coldraw_veggie_sprite sprites[2];
};

typedef struct s_colony s_colony, *Ps_colony;

struct s_colony {
    int8_t owner;
    int8_t allocated_to;
    int16_t planet;
    int16_t officer;
    uint8_t outpost_flag;
    int8_t morale;
    int16_t pollution;
    uint8_t n_pops;
    int8_t specialty;
    uint32_t pop[MAX_COLONY_POPULATION];
    int16_t pop_roundoff[10];
    int16_t pop_growth[10];
    int8_t n_turns_existed;
    uint8_t food2_per_farmer;
    uint8_t industry_per_worker;
    int8_t research_per_scientist;
    uint8_t max_farms;
    int8_t max_population;
    uint8_t climate;
    int16_t ground_strength;
    int16_t space_strength;
    int16_t production[ECON_COUNT];
    uint8_t maintenance[ECON_COUNT];
    int16_t imports[ECON_COUNT];
    int8_t n_industry_recyclers;
    uint8_t food2_needed_for_our_empire;
    uint8_t food2_needed_for_assimilated;
    uint8_t food2_needed_for_conquered;
    uint8_t food2_needed_for_natives;
    uint8_t industry2_needed_for_our_empire;
    uint8_t industry2_needed_for_androids;
    uint8_t industry2_needed_for_assimilated;
    uint8_t industry2_needed_for_conquered;
    int8_t food2_needed_for_empire[8];
    int8_t industry2_needed_for_empire[8];
    int8_t n_food_replicated;
    int16_t producing[7];
    int16_t just_produced;
    int16_t production_spent;
    int16_t n_industry_taxed;
    uint8_t auto_building;
    int16_t production_surplus;
    int16_t bought_outright;
    int8_t occupation_points;
    int8_t occupation_policy;
    int16_t military[2];
    int8_t tank_roundoff;
    int8_t infantry_roundoff;
    uint8_t buildings[BUILDING_COUNT];
    uint16_t last_turn_building_destroyed;
    uint8_t buildings_scrapped_this_turn;
};

typedef struct s_colony_bldg s_colony_bldg, *Ps_colony_bldg;

struct s_colony_bldg {
    int32_t buildings[6];
};

typedef struct s_colony_job_production s_colony_job_production, *Ps_colony_job_production;

struct s_colony_job_production {
    int32_t base_prod;
    int16_t racial_bonuses[COLONY_JOB_RACIAL_BONUS_COUNT];
    int16_t tech_apps[TECH_APP_COUNT];
    int16_t buildings_prod[BUILDING_COUNT];
    int32_t food_bonus;
    int32_t artifacts_bonus;
    int32_t intelligence_bonus;
    int32_t base_prod_scaled;
    int32_t gravity_penalty;
    int32_t conquered_penalty;
    int32_t pollution_penalty;
    int32_t morale_bonus;
    int32_t govt_bonus;
    int32_t blockaded_penalty;
    int32_t officer_bonus;
    int32_t difficulty_bonus;
    int32_t final_prod;
};

typedef struct s_colors s_colors, *Ps_colors;

struct s_colors {
    uint8_t colors[8];
};

typedef struct s_colsum_col_pop_ref s_colsum_col_pop_ref, *Ps_colsum_col_pop_ref;

struct s_colsum_col_pop_ref {
    int16_t refs[MAX_COLONY_POPULATION];
};

typedef struct s_combat_data s_combat_data, *Ps_combat_data;

typedef struct s_combat_weapon_slot s_combat_weapon_slot, *Ps_combat_weapon_slot;

struct s_combat_weapon_slot {
    int16_t weapon_id;
    uint8_t count;
    int8_t arc;
    uint16_t specials;
    int16_t ammo;
    int8_t active;
    uint8_t shots_left;
    uint8_t status;
};

struct s_combat_data {
    char name[30];
    int16_t ship_idx;
    int8_t owner;
    int16_t x;
    int16_t y;
    int8_t facing_dir;
    int8_t unit_status;
    int8_t size_class;
    int8_t shield_type;
    int16_t shield_arc_max;
    int16_t shield_arc_current[4];
    int8_t ftl_type;
    int8_t computer_type;
    int8_t armor_type;
    int16_t tac_val_attack;
    int16_t tac_val_defense;
    int16_t tac_val_evade;
    int8_t combat_speed_base;
    int16_t current_speed;
    int16_t movement_left;
    int8_t target_type_flag;
    int8_t special_status_flag;
    uint8_t special_status_timer;
    uint16_t special_cooldown;
    int16_t plasma_web_damage;
    int16_t reflected_damage_pool;
    int8_t field40_0x47;
    uint8_t combat_status_flags;
    undefined field42_0x49;
    uint16_t stasis_source_idx;
    int8_t is_retreating;
    uint8_t special_device_flags[5];
    undefined field46_0x51;
    struct s_combat_weapon_slot weapons[8];
    int16_t structure_max;
    int8_t crew_quality;
    int16_t officer_idx;
    uint8_t marine_count;
    int8_t is_captured;
    int8_t ai_range_tactic;
    uint8_t special_device_damage_flags[5];
    undefined field55_0xb7;
    uint8_t weapon_ready_flags[8];
    int16_t structure_damage;
    int16_t armor_remaining;
    int16_t picture_num;
    struct s_animation_header* ship_image_seg;
    uint16_t drive_max_hits;
    uint16_t computer_max_hits;
    int16_t weapon_total_hits[8];
    int16_t weapon_unit_hits[8];
    int16_t special_device_hits[8];
    uint16_t shield_max_hits;
    uint16_t drive_current_hits;
    uint16_t computer_current_hits;
    int16_t weapon_hits[8];
    int16_t weapon_hits_per_unit[8];
    int16_t special_hits[8];
    uint16_t shield_current_hits;
    int8_t black_hole_flag;
    uint16_t black_hole_source_idx;
    uint8_t previous_owner;
};

typedef struct s_combfind_star_has_player s_combfind_star_has_player, *Ps_combfind_star_has_player;

struct s_combfind_star_has_player {
    int8_t ship_sizes[5];
    int8_t has_transport;
    int8_t has_player;
    int8_t colony_count;
    int16_t transport_count;
    int8_t transport_unk;
};

typedef struct s_combfind_target s_combfind_target, *Ps_combfind_target;

struct s_combfind_target {
    int16_t colony_targets[5];
    int16_t num_colony_targets;
    int16_t player_targets[15];
    int16_t num_player_targets;
};

typedef struct s_comcore_open_req s_comcore_open_req, *Ps_comcore_open_req;

struct s_comcore_open_req {
    undefined field0_0x0;
    undefined field1_0x1;
    undefined field2_0x2;
    undefined field3_0x3;
    char name[10];
    uint32_t msg_size;
};

typedef struct s_crew_data s_crew_data, *Ps_crew_data;

struct s_crew_data {
    uint16_t experience;
    int16_t attack;
    int16_t defense;
    int16_t ground;
};

typedef struct s_current_design s_current_design, *Ps_current_design;

struct s_current_design {
    char name[16];
    undefined1 field1_0x10;
    int16_t shield;
    int16_t shield_cost;
    int16_t shield_space;
    undefined1 field5_0x17[4];
    int16_t ftl_type;
    int16_t drive_speed;
    int16_t drive_cost;
    int16_t base_space;
    undefined1 field13_0x23[2];
    int16_t base_combat_speed;
    undefined1 field16_0x27[2];
    int16_t field18_0x29;
    int16_t computer_type;
    int16_t computer_cost;
    int16_t fuel_type;
    int16_t armor_type;
    int16_t armor_cost;
    int16_t weapon_type[8];
    int16_t weapon_count[8];
    int8_t weapon_firing_arc[8];
    uint16_t weapon_specials[8];
    int32_t weapon_cost[8];
    int32_t weapon_space[8];
    int8_t weapon_ammo[8];
    int16_t special_devices[8];
    int16_t special_device_cost[8];
    int16_t special_device_space[8];
    int16_t ship_size;
    int16_t picture_type;
    int32_t hull_space;
    int32_t space_used;
    int32_t total_cost;
    int8_t show_help;
    int16_t num_weapon_slots;
    int16_t num_special_slots;
    int8_t replacement_item_unlocked[40];
    int8_t special_system_scroll_first_row;
    int16_t special_system_scroll_thumb_y;
};

typedef struct s_dipscrn_net_message s_dipscrn_net_message, *Ps_dipscrn_net_message;

struct s_dipscrn_net_message {
    uint8_t sender_player_id;
    uint8_t target_player_id;
    int16_t arg1;
    int16_t msg_type;
    int16_t arg2;
};

typedef struct s_drop_area s_drop_area, *Ps_drop_area;

struct s_drop_area {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    uint8_t colors_main[3];
    uint8_t colors_left[3];
    uint8_t colors_right[3];
};

typedef struct s_event_data s_event_data, *Ps_event_data;

struct s_event_data {
    int8_t player_idx;
    int8_t status;
    int8_t turn_count;
    int16_t target_id;
    int16_t amount;
    int16_t max_amount;
};

typedef struct s_file_buffer_entry s_file_buffer_entry, *Ps_file_buffer_entry;

struct s_file_buffer_entry {
    char file_name[16];
    struct s_buffer_cached_segment segments[410];
    int32_t segment_count;
};

typedef struct s_first_field_in_group s_first_field_in_group, *Ps_first_field_in_group;

struct s_first_field_in_group {
    uint8_t biology_field1;
    uint8_t power_field1;
    uint8_t physics_field1;
    uint8_t construction_field1;
    uint8_t force_fields_field1;
    uint8_t chemistry_field1;
    uint8_t computers_field1;
    uint8_t sociology_field1;
    uint8_t xenon_field1;
};

typedef struct s_fleet_box s_fleet_box, *Ps_fleet_box;

struct s_fleet_box {
    int16_t x;
    int16_t y;
    int16_t height;
    int16_t width;
    int16_t map_height;
    undefined field5_0xa;
    undefined field6_0xb;
    int16_t count;
    int16_t field8_0xe;
    int16_t field9_0x10;
    struct s_animation_header* name_seg;
    struct s_animation_header* bottom_seg;
    int8_t is_small_frame;
    int8_t mouse_pressed;
    int8_t attack_btn_active;
    int8_t engage_btn_active;
    int8_t colonize_btn_active;
    int8_t transport_btn_active;
    int8_t outpost_btn_active;
};

typedef struct s_fleet_movement_icon s_fleet_movement_icon, *Ps_fleet_movement_icon;

struct s_fleet_movement_icon {
    int8_t field0_0x0;
    int16_t x;
    int16_t y;
    int16_t field;
    int16_t ship_node_idx;
};

typedef struct s_fmtpara_char s_fmtpara_char, *Ps_fmtpara_char;

struct s_fmtpara_char {
    int16_t x_advance;
    int16_t y;
    uint8_t font_id;
    uint8_t fg_and_attr;
    uint8_t bg_color;
    int8_t ch;
};

typedef struct s_fmtpara_line s_fmtpara_line, *Ps_fmtpara_line;

struct s_fmtpara_line {
    int16_t x;
    int16_t left_limit_x;
    int16_t right_limit_x;
    int16_t exclusion_left_x;
    int16_t exclusion_right_x;
    int16_t x_candidate;
    int16_t char_count;
    int16_t space_count;
    int16_t line_y_offset;
    int16_t prev_x;
    int16_t left_offset_abs;
    int16_t font_spacing;
    bool line_done;
    int8_t line_started;
};

typedef struct s_font s_font, *Ps_font;

struct s_font {
    struct s_colors selected_colors;
    int16_t font_style_height;
    int16_t font_style;
    struct s_colors normal_colors;
    struct s_colors highlight_colors;
    struct s_colors special_colors;
    int16_t current_height;
    uint16_t line_height;
    uint16_t leading_offset;
    int16_t font_style_spacing;
    int8_t font_style_widths[256];
    uint32_t font_style_offsets[256];
    int16_t font_heights[8];
    int16_t font_spacing[8];
    int16_t leading_extra[8];
    int8_t font_glyph_widths[6][256];
    uint32_t font_glyph_offsets[6][256];
};

typedef struct s_font_color s_font_color, *Ps_font_color;

struct s_font_color {
    uint8_t field1;
    uint32_t field2;
    uint16_t field6;
    uint8_t field7;
};

typedef struct s_game_anims s_game_anims, *Ps_game_anims;

struct s_game_anims {
    struct s_animation_header* pictures[25];
    int8_t animating;
};

typedef struct s_game_popup_fields s_game_popup_fields, *Ps_game_popup_fields;

typedef struct s_save_game_description s_save_game_description, *Ps_save_game_description;

struct s_save_game_description {
    char description[37];
};

struct s_game_popup_fields {
    int32_t slot_occupied_flags[10];
    int16_t btn_main_load_id;
    int16_t btn_main_save_id;
    int16_t btn_main_quit_id;
    int16_t btn_main_new_id;
    int16_t btn_main_options_id;
    int16_t btn_main_close_id;
    undefined field7_0x34;
    undefined field8_0x35;
    undefined field9_0x36;
    undefined field10_0x37;
    int16_t saved_game_fields[10];
    int16_t debug_hotspot_id;
    undefined field13_0x4e;
    undefined field14_0x4f;
    undefined field15_0x50;
    undefined field16_0x51;
    int16_t option_names[13];
    int16_t btn_settings_accept_id;
    int16_t btn_popup_close_id;
    int16_t screen_cancel_field_id;
    int16_t field_space;
    int16_t music_level;
    int16_t sound_fx_level;
    uint16_t music_on;
    uint16_t sound_fx_on;
    int16_t save_slot_fields[10];
    uint16_t options[13];
    int8_t slot_status[10];
    struct s_save_game_description descriptions[10];
    int8_t saved_game_types[10];
    int16_t btn_popup_load_id;
    int16_t btn_popup_save_id;
    struct s_colors color_normal;
    struct s_colors color_active;
    int16_t option_toggles[13];
    uint16_t language;
    undefined field37_0x260;
    undefined field38_0x261;
    undefined field39_0x262;
    undefined field40_0x263;
    undefined field41_0x264;
    undefined field42_0x265;
    undefined field43_0x266;
    undefined field44_0x267;
    undefined field45_0x268;
    undefined field46_0x269;
    undefined field47_0x26a;
    undefined field48_0x26b;
    undefined field49_0x26c;
    undefined field50_0x26d;
    undefined field51_0x26e;
    undefined field52_0x26f;
    undefined field53_0x270;
    undefined field54_0x271;
    undefined field55_0x272;
    undefined field56_0x273;
    undefined field57_0x274;
    undefined field58_0x275;
    undefined field59_0x276;
    undefined field60_0x277;
};

typedef struct s_glass_color s_glass_color, *Ps_glass_color;

struct s_glass_color {
    uint8_t intensity;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

typedef struct s_good_star s_good_star, *Ps_good_star;

struct s_good_star {
    uint8_t star_bitmap[BITMAP(MAX_STARS)];
    uint32_t star_distances[MAX_STARS];
    uint16_t star_count;
};

typedef struct s_help_box s_help_box, *Ps_help_box;

struct s_help_box {
    int16_t help_id;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
};

typedef struct s_help_record s_help_record, *Ps_help_record;

#pragma pack(push, 1)
struct s_help_record {
    char title[80];
    char anim_lbx[14];
    uint32_t anim_info;
    uint8_t unknown_0x62;
    uint32_t next_help_idx;
    char body[1300];
};
#pragma pack(pop)

typedef struct s_help_topic_record s_help_topic_record, *Ps_help_topic_record;

struct s_help_topic_record {
    char text[80];
    uint32_t id;
};

typedef struct s_hiscore_pop s_hiscore_pop, *Ps_hiscore_pop;

struct s_hiscore_pop {
    int16_t race_id;
    int16_t icon_id;
};

typedef struct s_hiscore_tmp_pop s_hiscore_tmp_pop, *Ps_hiscore_tmp_pop;

struct s_hiscore_tmp_pop {
    int16_t colony_idx;
    uint8_t race_idx;
    uint8_t pop_idx;
    int8_t is_used;
};

typedef struct s_hof s_hof, *Ps_hof;

struct s_hof {
    int16_t version;
    char player_names[10][20];
    int16_t scores[10];
    int16_t race_ids[10];
    int8_t difficulties[10];
    char race_names[10][20];
};

typedef struct s_hotseat_fleet_box_state s_hotseat_fleet_box_state, *Ps_hotseat_fleet_box_state;

struct s_hotseat_fleet_box_state {
    int16_t x;
    int16_t y;
    int16_t box_id;
    int16_t stack_ship_id;
};

typedef struct s_hotseat_system_box_state s_hotseat_system_box_state, *Ps_hotseat_system_box_state;

struct s_hotseat_system_box_state {
    int16_t x;
    int16_t y;
    int16_t system_id;
    int16_t field3_0x6;
};

typedef struct s_lbx_header s_lbx_header, *Ps_lbx_header;

struct s_lbx_header {
    uint16_t entry_count;
    uint16_t magic;
    uint32_t unknown4;
    uint32_t offsets[510];
};

struct _iobuf_copy {
    char* _ptr;
    int32_t _cnt;
    char* _base;
    int32_t _flag;
    int32_t _file;
    int32_t _charbuf;
    int32_t _bufsiz;
    char* _tmpfname;
};

typedef struct s_leader_data s_leader_data, *Ps_leader_data;

struct s_leader_data {
    char name[15];
    char title[20];
    uint8_t type;
    int16_t xp;
    uint32_t general_skills;
    uint32_t special_skills;
    uint8_t tech_application[3];
    uint8_t pict_num;
    int16_t skill_value;
    uint8_t level;
    int16_t location;
    sbyte eta;
    char display_level_popup;
    sbyte status;
    int8_t player_index;
};

typedef struct s_main_menu s_main_menu, *Ps_main_menu;

typedef struct s_main_menu_button s_main_menu_button, *Ps_main_menu_button;

struct s_main_menu_button {
    struct s_animation_header* field0_0x0;
    struct s_animation_header* field1_0x4;
    struct s_animation_header* field2_0x8;
    int16_t field;
};

struct s_main_menu {
    struct s_main_menu_button fields[6];
    undefined field1_0x54;
    undefined field2_0x55;
    undefined field3_0x56;
    undefined field4_0x57;
    undefined field5_0x58;
    undefined field6_0x59;
    undefined field7_0x5a;
    undefined field8_0x5b;
    undefined field9_0x5c;
    undefined field10_0x5d;
    undefined field11_0x5e;
    undefined field12_0x5f;
    undefined field13_0x60;
    undefined field14_0x61;
    undefined field15_0x62;
    undefined field16_0x63;
    undefined field17_0x64;
    undefined field18_0x65;
    undefined field19_0x66;
    undefined field20_0x67;
    undefined field21_0x68;
    undefined field22_0x69;
    undefined field23_0x6a;
    undefined field24_0x6b;
    undefined field25_0x6c;
    undefined field26_0x6d;
    undefined field27_0x6e;
    undefined field28_0x6f;
    undefined field29_0x70;
    undefined field30_0x71;
    undefined field31_0x72;
    undefined field32_0x73;
    undefined field33_0x74;
    undefined field34_0x75;
    undefined field35_0x76;
    undefined field36_0x77;
    undefined field37_0x78;
    undefined field38_0x79;
    undefined field39_0x7a;
    undefined field40_0x7b;
    undefined field41_0x7c;
    undefined field42_0x7d;
    undefined field43_0x7e;
    undefined field44_0x7f;
    undefined field45_0x80;
    undefined field46_0x81;
    undefined field47_0x82;
    undefined field48_0x83;
    undefined field49_0x84;
    undefined field50_0x85;
    undefined field51_0x86;
    undefined field52_0x87;
    undefined field53_0x88;
    undefined field54_0x89;
    undefined field55_0x8a;
    undefined field56_0x8b;
    undefined field57_0x8c;
    undefined field58_0x8d;
    undefined field59_0x8e;
    undefined field60_0x8f;
    undefined field61_0x90;
    undefined field62_0x91;
    undefined field63_0x92;
    undefined field64_0x93;
    undefined field65_0x94;
    undefined field66_0x95;
    undefined field67_0x96;
    undefined field68_0x97;
    undefined field69_0x98;
    undefined field70_0x99;
    undefined field71_0x9a;
    undefined field72_0x9b;
    undefined field73_0x9c;
    undefined field74_0x9d;
    undefined field75_0x9e;
    undefined field76_0x9f;
    undefined field77_0xa0;
    undefined field78_0xa1;
    undefined field79_0xa2;
    undefined field80_0xa3;
    undefined field81_0xa4;
    undefined field82_0xa5;
    undefined field83_0xa6;
    undefined field84_0xa7;
    undefined field85_0xa8;
    undefined field86_0xa9;
    undefined field87_0xaa;
    undefined field88_0xab;
    undefined field89_0xac;
    undefined field90_0xad;
    undefined field91_0xae;
    undefined field92_0xaf;
    undefined field93_0xb0;
    undefined field94_0xb1;
    undefined field95_0xb2;
    undefined field96_0xb3;
    undefined field97_0xb4;
    undefined field98_0xb5;
    undefined field99_0xb6;
    undefined field100_0xb7;
    undefined field101_0xb8;
    undefined field102_0xb9;
    undefined field103_0xba;
    undefined field104_0xbb;
    undefined field105_0xbc;
    undefined field106_0xbd;
    undefined field107_0xbe;
    undefined field108_0xbf;
    undefined field109_0xc0;
    undefined field110_0xc1;
    undefined field111_0xc2;
    undefined field112_0xc3;
    undefined field113_0xc4;
    undefined field114_0xc5;
    undefined field115_0xc6;
    undefined field116_0xc7;
    undefined field117_0xc8;
    undefined field118_0xc9;
    undefined field119_0xca;
    undefined field120_0xcb;
    undefined field121_0xcc;
    undefined field122_0xcd;
    undefined field123_0xce;
    undefined field124_0xcf;
    undefined field125_0xd0;
    undefined field126_0xd1;
};

typedef struct s_missile s_missile, *Ps_missile;

struct s_missile {
    int16_t type;
    int16_t owner;
    int16_t source_unit_idx;
    int16_t target_unit_idx;
    sbyte target_is_missile;
    int16_t x;
    int16_t y;
    int16_t hits_taken;
    int16_t quantity;
    uint16_t specials;
    int16_t travel_dist;
    int8_t facing_dir;
    uint8_t speed;
    int8_t fighter_count;
    undefined field14_0x18;
    int8_t is_active;
};

typedef struct s_online_settings s_online_settings, *Ps_online_settings;

struct s_online_settings {
    uint8_t reserved_0x00[17];
    char endpoint[30];
    uint8_t reserved_0x2f[84];
};

typedef struct s_mouse_list s_mouse_list, *Ps_mouse_list;

struct s_mouse_list {
    int16_t icon;
    int16_t frame;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
};

typedef struct s_mouse_xy s_mouse_xy, *Ps_mouse_xy;

struct s_mouse_xy {
    int16_t ship_icon_idx;
    int16_t hovering_ship_icon_idx;
    int16_t star_idx;
    int16_t hovering_star_idx;
    int8_t selected;
};

typedef struct s_movable_box s_movable_box, *Ps_movable_box;

struct s_movable_box {
    int16_t index;
    int16_t id;
    int16_t height;
    int16_t width;
    int16_t x;
    int16_t y;
    int16_t active;
    int16_t drag_field;
    int16_t close_btn;
    int16_t window_field_id;
    int16_t unselected_width;
    int16_t unselected_height;
    int16_t content_offset_x;
    int16_t content_offset_y;
};

typedef struct s_mp_screen s_mp_screen, *Ps_mp_screen;

struct s_mp_screen {
    char signature[13];
    undefined field1_0xd;
    undefined field2_0xe;
    struct s_animation_header* pic;
    struct s_animation_header* res_setup_bg;
    undefined field5_0x17;
    undefined field6_0x18;
    undefined field7_0x19;
    undefined field8_0x1a;
    struct s_animation_header* anim_btn_unk_4;
    struct s_animation_header* anim_chat_msg_bg;
    struct s_animation_header* anim_turn_screen_top;
    struct s_animation_header* anim_turn_screen_mid;
    struct s_animation_header* anim_turn_screen_bot;
    struct s_animation_header* bg_seg;
    struct s_animation_header* player_banner_anims[MAX_PLAYERS];
    struct s_animation_header* btn_accept_off;
    struct s_animation_header* btn_accept_on;
    undefined field18_0x5b;
    undefined field19_0x5c;
    undefined field20_0x5d;
    undefined field21_0x5e;
    struct s_animation_header* anim_network;
    struct s_animation_header* anim_online;
    struct s_animation_header* anim_unused_connection;
    struct s_animation_header* anim_hotseat;
    struct s_animation_header* anim_btn_unk_3;
    struct s_animation_header* anim_btn_cancel;
    undefined field28_0x77;
    undefined field29_0x78;
    undefined field30_0x79;
    undefined field31_0x7a;
    struct s_animation_header* anim_btn_start_game;
    struct s_animation_header* anim_btn_load_game;
    struct s_animation_header* anim_btn_join_game;
    struct s_animation_header* anim_btn_setup;
    struct s_animation_header* anim_btn_begin;
    struct s_animation_header* anim_unk_3;
    undefined field39_0x97;
    undefined field40_0x98;
    undefined field41_0x99;
    undefined field42_0x9a;
    undefined field43_0x9b;
    undefined field44_0x9c;
    undefined field45_0x9d;
    undefined field46_0x9e;
    undefined field47_0x9f;
    undefined field48_0xa0;
    undefined field49_0xa1;
    undefined field50_0xa2;
    undefined field51_0xa3;
    undefined field52_0xa4;
    undefined field53_0xa5;
    undefined field54_0xa6;
    int16_t game_list_field_ids[10];
    int16_t player_field_ids[MAX_PLAYERS];
    int8_t save_slot_valid[10];
    int16_t field_id_close_dropdown;
    int16_t field_id_network;
    int16_t field_id_online;
    int16_t field_id_unused_connection;
    int16_t field_id_hotseat;
    int16_t field_id_cancel;
    int16_t field_id_ok;
    int16_t field_id_start_game;
    int16_t field_id_load_game;
    int16_t field_id_join_game;
    int16_t field_id_setup;
    int16_t field_id_begin;
    undefined field71_0xef;
    undefined field72_0xf0;
    int16_t lbx_entry_num;
    int16_t screen_x;
    int16_t screen_y;
    int16_t flag_network_disabled;
    int16_t flag_online_disabled;
    int16_t flag_unused_connection;
    int16_t flag_hotseat_disabled;
    int16_t setup_screen_state;
    int16_t last_scan_id;
    uint8_t is_host_flag;
    uint8_t player_connected_flags[MAX_PLAYERS];
    int16_t bg_height;
    int8_t subscreen_mode;
    int8_t msg_layout_alt;
    int8_t player_button_status[MAX_PLAYERS];
    struct s_colors player_font_colors_1[8];
    struct s_colors player_font_colors_2[8];
    struct s_colors player_font_colors_3[8];
    struct s_colors sel_glow_colors;
    int8_t needs_full_redraw;
    int16_t num_remaining_humans;
    int16_t force_player_selection;
    int16_t forced_player_idx;
    int8_t exit_screen_flag;
    int8_t sel_anim_frame;
    int8_t sel_anim_active;
    uint8_t anim_frame_timer;
};

typedef struct s_MSG s_MSG, *Ps_MSG;

typedef union s_MSG_union s_MSG_union, *Ps_MSG_union;

typedef struct s_MSG_colony s_MSG_colony, *Ps_MSG_colony;

typedef struct s_MSG_players s_MSG_players, *Ps_MSG_players;

typedef struct s_MSG_financial s_MSG_financial, *Ps_MSG_financial;

typedef struct s_MSG_lost_ship s_MSG_lost_ship, *Ps_MSG_lost_ship;

typedef struct s_MSG_scrap_ship s_MSG_scrap_ship, *Ps_MSG_scrap_ship;

typedef struct s_MSG_lost_colony s_MSG_lost_colony, *Ps_MSG_lost_colony;

typedef struct s_MSG_hyperspace_beast s_MSG_hyperspace_beast, *Ps_MSG_hyperspace_beast;

typedef struct s_MSG_lost_attacking_spy s_MSG_lost_attacking_spy, *Ps_MSG_lost_attacking_spy;

typedef struct s_MSG_lost_defending_spy s_MSG_lost_defending_spy, *Ps_MSG_lost_defending_spy;

typedef struct s_MSG_artemis s_MSG_artemis, *Ps_MSG_artemis;

typedef struct s_MSG_artemis_leader s_MSG_artemis_leader, *Ps_MSG_artemis_leader;

typedef struct s_MSG_failed_retreat s_MSG_failed_retreat, *Ps_MSG_failed_retreat;

typedef struct s_MSG_spy_assasination s_MSG_spy_assasination, *Ps_MSG_spy_assasination;

typedef struct s_MSG_anomaly s_MSG_anomaly, *Ps_MSG_anomaly;

typedef struct s_MSG_diplomacy s_MSG_diplomacy, *Ps_MSG_diplomacy;

typedef struct s_MSG_eel s_MSG_eel, *Ps_MSG_eel;

typedef struct s_MSG_artifacts s_MSG_artifacts, *Ps_MSG_artifacts;

typedef struct s_MSG_scrap_leader s_MSG_scrap_leader, *Ps_MSG_scrap_leader;

typedef union s_MSG_colony_union s_MSG_colony_union, *Ps_MSG_colony_union;

typedef struct s_MSG_colony_scrap_building s_MSG_colony_scrap_building, *Ps_MSG_colony_scrap_building;

typedef struct s_MSG_colony_build_info s_MSG_colony_build_info, *Ps_MSG_colony_build_info;

typedef struct s_MSG_colony_assimilation s_MSG_colony_assimilation, *Ps_MSG_colony_assimilation;

typedef struct s_MSG_colony_starvation s_MSG_colony_starvation, *Ps_MSG_colony_starvation;

typedef struct s_MSG_colony_settlers s_MSG_colony_settlers, *Ps_MSG_colony_settlers;

typedef struct s_MSG_colony_rebellion s_MSG_colony_rebellion, *Ps_MSG_colony_rebellion;

typedef struct s_MSG_colony_android_killed s_MSG_colony_android_killed, *Ps_MSG_colony_android_killed;

struct s_MSG_financial {
    int16_t a;
    int16_t b;
};

struct s_MSG_colony_scrap_building {
    int16_t building_id;
    int16_t bc;
};

struct s_MSG_lost_attacking_spy {
    int16_t player_idx;
};

struct s_MSG_colony_settlers {
    int8_t settlers;
    int8_t over_limit;
    bool blockaded;
};

struct s_MSG_players {
    int16_t player_a_idx;
    int16_t player_b_idx;
};

struct s_MSG_artemis {
    int16_t star_idx;
};

struct s_MSG_artifacts {
    int16_t star_idx;
    int8_t tech1;
    int8_t tech2;
};

struct s_MSG_scrap_leader {
    int8_t leader_idx;
};

struct s_MSG_failed_retreat {
    int16_t star_idx;
    int8_t reason;
    int16_t ships_lost;
    int8_t leader_count;
};

struct s_MSG_artemis_leader {
    int16_t star_idx;
    int8_t leader_idx;
    int8_t fate;
};

struct s_MSG_eel {
    int16_t star_idx;
};

struct s_MSG_scrap_ship {
    int16_t where;
    int16_t bc;
    int16_t ship_idx;
};

struct s_MSG_hyperspace_beast {
    int16_t ship_idx;
    int16_t star_idx;
    int8_t officer_idx;
    int8_t leader_status;
    int8_t fate;
    int8_t your_ship;
};

struct s_MSG_lost_ship {
    int16_t star_idx;
    int16_t ship_idx;
    int8_t leader_idx;
    int8_t fate;
};

struct s_MSG_colony_android_killed {
    int8_t race_idx;
};

struct s_MSG_colony_starvation {
    int8_t a;
    int8_t b;
};

struct s_MSG_colony_assimilation {
    uint8_t val;
};

struct s_MSG_colony_rebellion {
    int8_t owner_player;
    int8_t rebels;
    int8_t lost;
};

struct s_MSG_colony_build_info {
    int16_t build_id;
    int16_t product_id;
    bool is_refit;
    int8_t append_fmt;
};

union s_MSG_colony_union {
    struct s_MSG_colony_scrap_building scrap;
    struct s_MSG_colony_build_info build;
    struct s_MSG_colony_assimilation assimilation;
    struct s_MSG_colony_starvation starvation;
    struct s_MSG_colony_settlers settlers;
    struct s_MSG_colony_rebellion rebels;
    struct s_MSG_colony_android_killed android_killed;
};

struct s_MSG_colony {
    int16_t planet_idx;
    MSG_COLONY_SUBTYPE subtype;
    union s_MSG_colony_union c;
};

struct s_MSG_lost_colony {
    int16_t planet_idx;
    int16_t reason;
    int16_t leader_idx;
    int8_t colony_or_outpost;
};

struct s_MSG_diplomacy {
    int8_t diplomacy_idx;
    int8_t plr_a;
    int8_t plr_b;
};

struct s_MSG_lost_defending_spy {
    int16_t count;
};

struct s_MSG_spy_assasination {
    int8_t plr_a;
    int8_t plr_b;
    int8_t leader_id;
    int8_t victim_id;
};

struct s_MSG_anomaly {
    int16_t star_idx;
};

union s_MSG_union {
    struct s_MSG_colony colony;
    struct s_MSG_players players;
    struct s_MSG_financial financial;
    struct s_MSG_lost_ship lost_ship;
    struct s_MSG_scrap_ship scrap_ship;
    struct s_MSG_lost_colony lost_colony;
    struct s_MSG_hyperspace_beast hyperspace_beast;
    struct s_MSG_lost_attacking_spy lost_attacking_spy;
    struct s_MSG_lost_defending_spy lost_defending_spy;
    struct s_MSG_artemis artemis;
    struct s_MSG_artemis_leader artemis_leader;
    struct s_MSG_failed_retreat retreat;
    struct s_MSG_spy_assasination spy_assasination;
    struct s_MSG_anomaly anomaly;
    struct s_MSG_diplomacy diplomacy;
    struct s_MSG_eel eel;
    struct s_MSG_artifacts artifacts;
    struct s_MSG_scrap_leader scrap_leader;
};

struct s_MSG {
    int8_t msg_type;
    uint8_t priority;
    union s_MSG_union u;
    uint8_t reserved[8];
};

typedef struct s_nebula s_nebula, *Ps_nebula;

struct s_nebula {
    int16_t x;
    int16_t y;
    int8_t type;
};

typedef struct s_net_chat s_net_chat, *Ps_net_chat;

typedef struct s_net_chat_msg s_net_chat_msg, *Ps_net_chat_msg;

struct s_net_chat_msg {
    uint8_t player_id;
    char text[81];
};

struct s_net_chat {
    struct s_net_chat_msg messages[14];
    int32_t count;
};

typedef struct s_net_tactical_combat_message s_net_tactical_combat_message, *Ps_net_tactical_combat_message;

struct s_net_tactical_combat_message {
    int16_t viewport_x;
    int16_t viewport_y;
    int32_t random_seed;
    int16_t message_code;
    int16_t arg1;
    int16_t arg2;
    int16_t arg3;
};

typedef struct s_net_pack_change_relation s_net_pack_change_relation, *Ps_net_pack_change_relation;

struct s_net_pack_change_relation {
    int16_t change;
    int16_t attacker;
    int16_t defender;
    int16_t reason;
    int16_t star_idx;
    int16_t incident_value;
};

typedef struct s_net_pack_create_player s_net_pack_create_player, *Ps_net_pack_create_player;

struct s_net_pack_create_player {
    uint32_t msg_type;
    uint32_t player_type;
    uint32_t player_id;
    char event_str[52];
    char player_name[20];
};

typedef struct s_net_pack_destroy_player s_net_pack_destroy_player, *Ps_net_pack_destroy_player;

struct s_net_pack_destroy_player {
    uint32_t msg_type;
    uint32_t player_id;
};

typedef struct s_net_pack_drop_out_info s_net_pack_drop_out_info, *Ps_net_pack_drop_out_info;

typedef struct s_player s_player, *Ps_player;

typedef struct s_settler_info s_settler_info, *Ps_settler_info;

typedef struct s_ship_design s_ship_design, *Ps_ship_design;

typedef struct s_ship_weapons s_ship_weapons, *Ps_ship_weapons;

struct s_ship_weapons {
    int16_t type;
    int8_t count;
    int8_t current_count;
    int8_t firing_arc;
    uint16_t specials;
    int8_t ammo;
};

struct s_ship_design {
    char name[16];
    int8_t size;
    int8_t ship_type;
    int8_t shield_type;
    int8_t ftl_type;
    uint8_t speed;
    int8_t computer_type;
    int8_t armor_type;
    int8_t special_device_flags[5];
    struct s_ship_weapons ship_weapon[8];
    sbyte picture_num;
    uint8_t previous_owner;
    int16_t cost;
    uint8_t combat_speed;
    uint16_t date_of_design;
};

struct s_settler_info {
    // bits 0-15 source_colony, 16-31 dest_planet, 32-35 race, 36-39 eta, 40-41 job.
    SETTLER::value_type val;
};

struct s_player {
    int8_t picture;
    char name[20];
    char race_name[15];
    uint8_t eliminated;
    uint8_t race;
    uint8_t color;
    uint8_t personality;
    uint8_t objectives;
    int16_t home_planet_id;
    int16_t network_player_id;
    int8_t player_done_flags;
    int16_t dead_field;
    int8_t research_breakthrough;
    uint8_t tax_rate;
    int32_t bc;
    int16_t n_freighters;
    int16_t surplus_freighters;
    int16_t command_points;
    int16_t command_points_used;
    int16_t food_freighted;
    int16_t settlers_freighted;
    struct s_settler_info settlers[MAX_FREIGHTED_SETTLERS];
    int16_t total_pop;
    int16_t food_produced;
    int16_t industry_produced;
    int16_t research_produced;
    int16_t bc_produced;
    int16_t surplus_food;
    int16_t surplus_bc;
    int32_t total_maintenance;
    int16_t maintenance[6];
    uint8_t tech_fields[TECH_FIELD_COUNT];
    uint8_t tech_applications[TECH_APP_COUNT];
    int32_t research_accumulated;
    int8_t captured_orion;
    int8_t captured_antares;
    int8_t won_council_vote;
    uint16_t player_defeated[MAX_PLAYERS];
    int16_t captured_population;
    int8_t last_attacker;
    uint8_t ship_design_theme;
    int8_t ship_special_theme;
    int8_t tech_apps_learned[4];
    int8_t tech_modifiers[17];
    uint8_t hyper_advanced_tech[MAX_PLAYERS];
    int32_t total_known_tech_cost;
    int8_t knows_racial_stats;
    int32_t last_tech_report_date[MAX_PLAYERS];
    int8_t player_tech_applications[MAX_PLAYERS][27];
    int8_t current_research_field;
    int8_t current_research_application;
    int8_t got_new_app;
    int16_t ship_range;
    struct s_ship_design ship_designs[6];
    int16_t ships_built[6];
    sbyte contact[MAX_PLAYERS];
    int8_t just_established_contact;
    int8_t n_times_established_contact[MAX_PLAYERS];
    sbyte within_range[MAX_PLAYERS];
    int8_t do_not_research;
    int16_t offensive_stagepoint;
    int8_t best_ftl_drive;
    uint8_t ship_speed;
    int8_t officer_for_hire;
    int16_t n_trade_pops;
    int16_t current_trade_agreement_level[MAX_PLAYERS];
    int16_t trade_agreement_goal[MAX_PLAYERS];
    int16_t n_research_pops;
    int16_t current_research_agreement_level[MAX_PLAYERS];
    int16_t research_agreement_goal[MAX_PLAYERS];
    int8_t fighter_beam_type;
    int8_t fighter_bomb_type;
    int16_t total_research;
    int16_t total_ships;
    int32_t total_ship_strength[MAX_PLAYERS];
    int16_t total_colonies;
    sbyte failed_to_expand;
    int8_t first_contact[MAX_PLAYERS];
    sbyte relations[MAX_PLAYERS];
    int8_t base_relations[MAX_PLAYERS];
    sbyte treaty[MAX_PLAYERS];
    int8_t trade_treaty[MAX_PLAYERS];
    int8_t research_treaty[MAX_PLAYERS];
    int16_t tribute_treaty[MAX_PLAYERS];
    uint8_t diplomacy_incident[MAX_PLAYERS];
    int8_t diplomacy_message[MAX_PLAYERS];
    int16_t diplomacy_severity[MAX_PLAYERS];
    int16_t diplomacy_value[MAX_PLAYERS];
    int16_t diplomacy_system[MAX_PLAYERS];
    int16_t treaty_modifier[MAX_PLAYERS];
    int16_t trade_modifier[MAX_PLAYERS];
    int16_t tech_exchange_modifier[MAX_PLAYERS];
    int16_t peace_modifier[MAX_PLAYERS];
    sbyte last_bad_diplomatic_incident[MAX_PLAYERS];
    int8_t broken_treaties_modifier[MAX_PLAYERS];
    int8_t last_broken_treaty[MAX_PLAYERS];
    int8_t last_gift[MAX_PLAYERS];
    int16_t ship_war_losses[MAX_PLAYERS];
    int16_t colony_war_losses[MAX_PLAYERS];
    int8_t biological_weapons_flag[MAX_PLAYERS];
    int8_t time_since_last_attack[MAX_PLAYERS];
    int8_t threats[MAX_PLAYERS];
    int8_t dishonored_flag[MAX_PLAYERS];
    int8_t peace_duration[MAX_PLAYERS];
    int16_t last_turn_diplomacy_severity[MAX_PLAYERS];
    int8_t last_broken_treaty_type[MAX_PLAYERS];
    sbyte last_turn_treaty[MAX_PLAYERS];
    int8_t diplomacy_proposal_request[MAX_PLAYERS];
    int8_t diplomacy_proposal_tribute_bribe[MAX_PLAYERS];
    uint8_t diplomacy_proposal_tech_bribe1[MAX_PLAYERS];
    uint8_t diplomacy_proposal_tech_bribe2[MAX_PLAYERS];
    int16_t diplomacy_proposal_gold_bribe[MAX_PLAYERS];
    int8_t diplomacy_proposal_war_player[MAX_PLAYERS];
    int32_t diplomacy_proposal_exchange_max_value[MAX_PLAYERS];
    uint8_t diplomacy_proposal_exchange_tech[MAX_PLAYERS];
    int16_t diplomacy_proposal_system_bribe[MAX_PLAYERS];
    int16_t sneak_attack_planet;
    int8_t sneak_attack_message;
    int8_t sneak_attack_player;
    int8_t council_threat;
    int16_t reward_amount[MAX_PLAYERS];
    uint8_t reward_tech[MAX_PLAYERS];
    int8_t reward_attack_player[MAX_PLAYERS];
    int16_t food_shortage_duration;
    int8_t trust_worthiness[MAX_PLAYERS];
    int8_t trust_breaker_player[MAX_PLAYERS];
    int8_t trust_breaker_treaty[MAX_PLAYERS];
    int8_t stop_spying_duration[MAX_PLAYERS];
    int8_t stop_blockading_duration[MAX_PLAYERS];
    int8_t war_buildup_duration;
    int16_t last_diplomacy_proposal_turn[MAX_PLAYERS];
    int16_t diplomacy_proposal_rejection[MAX_PLAYERS];
    int16_t opportunity_planet_num[MAX_PLAYERS];
    int16_t opportunity_planet_value[MAX_PLAYERS];
    int32_t opportunity_actual_attacker_ratio[MAX_PLAYERS];
    int16_t temp_diplomacy_dummy1[MAX_PLAYERS];
    int8_t diplomacy_demand_rejection_message[MAX_PLAYERS];
    uint8_t last_diplomacy_threat_turn[MAX_PLAYERS];
    int8_t delayed_diplomacy_orders[MAX_PLAYERS];
    int8_t traits[TRAIT_COUNT];
    int8_t evolutionary_upgrade_traits[TRAIT_COUNT];
    int8_t picked_evolutionary_upgrade;
    int8_t applied_evolutionary_upgrade;
    int8_t fleet_history[350];
    int8_t tech_history[350];
    int8_t population_history[350];
    int8_t production_history[350];
    uint8_t spies[MAX_PLAYERS];
    int8_t history_btns;
    uint8_t ignoring;
    int8_t peace_flags[MAX_PLAYERS];
    int8_t cheated;
    uint16_t stardate_last_offered_hero;
    int8_t current_zoom_level;
    int16_t cur_map_x;
    int16_t cur_map_y;
    int8_t council_voted_for;
    int8_t surrender_to;
    int16_t time_since_last_lucky_event;
    int8_t has_galactic_lore;
    int8_t warned_about_cheating_player;
    int8_t cheated_this_turn;
    int32_t tribute_income;
    int8_t filler[45];
};

// Reads a player's research status for a technology field. Application 125
// has field TECH_FIELD_INVALID (-1), and the original game indexed
// tech_fields with it unchecked, reading the preceding byte of s_player (the
// high byte of maintenance[5]). Reproduce that read without indexing the
// array out of bounds.
inline uint8_t Player_Tech_Field_Status_(const s_player* player, int16_t tech_field_id) {
    return reinterpret_cast<const uint8_t*>(player)[offsetof(s_player, tech_fields) + tech_field_id];
}

struct s_net_pack_drop_out_info {
    struct s_player player_data;
    int8_t star_surrender_to[MAX_STARS];
};

typedef struct s_net_pack_end_ai_diplomacy s_net_pack_end_ai_diplomacy, *Ps_net_pack_end_ai_diplomacy;

/* WARNING! conflicting data type names: /stdint.h/uint8_t - /orion2.h/uint8_t */

struct s_net_pack_end_ai_diplomacy {
    int32_t target_player_id;
    struct s_player target_player;
    uint8_t surrender_to[MAX_STARS];
    int32_t player_num;
    struct s_player player;
};

typedef struct s_net_pack_monster_update s_net_pack_monster_update, *Ps_net_pack_monster_update;

struct s_net_pack_monster_update {
    int32_t monster_type;
    int16_t target_planet_id;
    // Used only when target_planet_id is -1; updates both event player and amount.
    int32_t player_idx_or_amount;
};

typedef struct s_net_pack_pick_your_position s_net_pack_pick_your_position, *Ps_net_pack_pick_your_position;

struct s_net_pack_pick_your_position {
    int16_t num_players;
    struct s_player players[MAX_PLAYERS];
};

struct s_net_pack_combat_target_chosen {
    int16_t combat_defender_id;
    int16_t combat_planet_id;
    int16_t combat_target_player_id;
};

typedef struct s_net_pack_req_human_diplomacy s_net_pack_req_human_diplomacy, *Ps_net_pack_req_human_diplomacy;

struct s_net_pack_req_human_diplomacy {
    struct s_player player;
    int8_t surrender_to[MAX_STARS];
};

typedef struct s_net_pack_star_order s_net_pack_star_order, *Ps_net_pack_star_order;

struct s_net_pack_star_order {
    uint32_t flags;
    char name[15];
    int32_t officer_index;
    int8_t surrender_to;
};

typedef struct s_net_pack_start_game s_net_pack_start_game, *Ps_net_pack_start_game;

struct s_net_pack_start_game {
    int32_t net_host_id;
    int16_t num_connected_players;
    int32_t net_ids[8];
    uint8_t random_events;
    uint8_t game_difficulty;
    int8_t number_of_players;
    uint8_t galaxy_size;
    uint8_t galaxy_age;
    uint8_t strategic_combat_flag;
    uint8_t starting_civilization_level;
    uint8_t xenons_exist;
    char version_string[50];
};

typedef struct s_net_player s_net_player, *Ps_net_player;

struct s_net_player {
    uint32_t player_id;
    char event_string[52];
    char player_name[20];
    uint32_t join_time;
};

typedef struct s_network s_network, *Ps_network;

struct s_network {
    struct s_online_settings online_settings;
    char online_init_str[11];
    undefined field2_0x8e;
    undefined field3_0x8f;
    undefined field4_0x90;
    undefined field5_0x91;
    undefined field6_0x92;
    int8_t online_initialized;
    uint8_t mode;
    int32_t joining_net_id;
    int8_t num_connected_players;
    int16_t net_host_id;
    int8_t player_connected_flags[MAX_PLAYERS];
    int16_t host_election_id;
    uint32_t total_bytes_received;
    // Layout-preserving legacy fields from removed game-layer chunk assembly.
    int16_t received_chunk_count;
    int16_t total_chunk_count;
    void* network_data_ptr;
    int8_t host_election_player_rank[MAX_PLAYERS];
    int8_t players_hit_next_turn[MAX_PLAYERS];
    int16_t local_net_id;
    int8_t host_active_player_id;
    int8_t host_sync_counter;
    int8_t field23_0xc6;
    struct s_net_player_info player_net_info[15];
    uint8_t sync_state;
    int16_t combat_star_id;
    int16_t combat_defender_id;
    int16_t combat_planet_id;
    int16_t combat_target_player_id;
    uint8_t combat_type;
    int8_t combat_attacker_id;
    int8_t combat_result;
    int8_t diplomacy_target_id;
    int8_t sync_phase_active_flag;
    struct s_battle_results* combat_results_data;
    uint8_t ai_diplomacy_done_mask;
    uint8_t ai_diplomacy_active_mask;
    uint8_t ai_diplomacy_cancel_mask;
    uint8_t human_diplomacy_ignored_mask;
    uint8_t players_active_mask;
    uint8_t players_sync_mask[MAX_PLAYERS];
    uint8_t diplomacy_sync_mask[MAX_PLAYERS];
    undefined field127_0x1a1;
    int8_t is_diplomacy_initiator;
    int8_t human_diplomat_wait_flag;
    undefined field130_0x1a4;
    uint8_t human_diplomacy_request_mask;
    uint8_t host_orders_received_flags[MAX_PLAYERS];
    int8_t player_is_active_flags[MAX_PLAYERS];
    int8_t host_election_rank_sync_cache[MAX_PLAYERS];
    char cur_game_name[9];
    int8_t player_color_selected_flags[MAX_PLAYERS];
    undefined all_ai_game_sync_flag;
    int8_t player_race_selected_flags[MAX_PLAYERS];
    int16_t host_election_winner_id;
    uint8_t is_network_active;
    uint8_t player_phase_ready_sent_flags[MAX_PLAYERS];
    uint8_t player_race_selected_alt_flags[MAX_PLAYERS];
    uint8_t player_race_deselected_flags[MAX_PLAYERS];
    uint8_t player_race_selection_sync_pending[MAX_PLAYERS];
    uint8_t race_selection_changed;
    uint8_t tactical_combat_sync_state;
    uint8_t network_sync_complete_flag;
    undefined field198_0x1fe;
    int8_t connection_timeout_flag;
    int8_t connection_state;
    undefined field160_0x201;
    undefined field161_0x202;
    int8_t diplomacy_partner_id;
    int8_t diplomacy_handshake_state;
    undefined field164_0x205;
    undefined field165_0x206;
    int8_t diplomacy_initiator_id;
    int8_t diplomacy_active_flag;
    int16_t dragon_bribe_destination_star_id;
    uint32_t cur_session_id;
};

typedef struct s_npcdiplo_tech s_npcdiplo_tech, *Ps_npcdiplo_tech;

struct s_npcdiplo_tech {
    int32_t value;
    uint8_t index;
};

typedef struct s_officer_skill_help s_officer_skill_help, *Ps_officer_skill_help;

struct s_officer_skill_help {
    int16_t skill_help_x[5];
    int16_t skill_id[5];
};

typedef struct s_orion_candidate s_orion_candidate, *Ps_orion_candidate;

struct s_orion_candidate {
    int32_t dist;
    int16_t star_idx;
};

typedef struct s_palette_cube s_palette_cube, *Ps_palette_cube;

struct s_palette_cube {
    uint16_t first_hist_index;
    uint16_t last_hist_index;
    uint16_t level;
    uint8_t r_min;
    uint8_t r_max;
    uint8_t g_min;
    uint8_t g_max;
    uint8_t b_min;
    uint8_t b_max;
};

typedef struct s_palette_entry s_palette_entry, *Ps_palette_entry;

struct s_palette_entry {
    uint8_t changed;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

typedef struct s_parsec s_parsec, *Ps_parsec;

struct s_parsec {
    int32_t dist[MAX_STARS];
};

typedef struct s_planet_data s_planet_data, *Ps_planet_data;

struct s_planet_data {
    int16_t colony_index;
    int16_t star_index;
    int8_t orbit;
    int8_t planet_type;
    int8_t size;
    int8_t gravity_class;
    int8_t group;
    int8_t climate;
    int8_t climate_bg_type;
    int8_t mineral_class;
    uint8_t food_per_farmer;
    uint8_t n_terraforms;
    uint8_t max_farms;
    uint8_t max_population;
    int8_t planet_special;
    int8_t environmental_alterations;
};

typedef struct s_planet_max_farms s_planet_max_farms, *Ps_planet_max_farms;

struct s_planet_max_farms {
    undefined field0_0x0;
    undefined field1_0x1;
    undefined field2_0x2;
    undefined field3_0x3;
    undefined field4_0x4;
    undefined field5_0x5;
    undefined field6_0x6;
    undefined field7_0x7;
    undefined field8_0x8;
    undefined field9_0x9;
    undefined field10_0xa;
    undefined field11_0xb;
    undefined field12_0xc;
    undefined field13_0xd;
    undefined field14_0xe;
    undefined field15_0xf;
};

typedef struct s_player_info s_player_info, *Ps_player_info;

struct s_player_info {
    int8_t current_government;
    int8_t pop_growth_bonus;
    int8_t food_bonus;
    int8_t production_bonus;
    int8_t research_bonus;
    int8_t tax_bonus;
    int8_t ship_defense;
    int8_t ship_attack;
    int8_t ground_combat_bonus;
    int8_t spying_bonus;
    int8_t low_g_world;
    int8_t heavy_g_world;
    sbyte aquatic;
    sbyte subterranean;
    int8_t large_home_world;
    int8_t rich_home_world;
    int8_t ancient_home_world;
    sbyte cybernetic;
    int8_t eats_minerals;
    int8_t repulsive;
    int8_t charismatic;
    int8_t uncreative;
    int8_t creative;
    sbyte environment_immune;
    int8_t fantastic_traders;
    sbyte telepathic;
    sbyte lucky;
    int8_t omniscience;
    int8_t stealthy_ships;
    sbyte trans_dimensional;
    int8_t warlord;
};

typedef struct s_plntsum_planet s_plntsum_planet, *Ps_plntsum_planet;

struct s_plntsum_planet {
    int16_t max_pop;
    int16_t food_per_farmer;
    int16_t minerals;
    int16_t mineral_class;
    int16_t climate;
    int16_t explored;
    int16_t best_colony_eta;
    int16_t best_outpost_eta;
    int16_t best_colony_ship_idx;
    int16_t best_outpost_ship_idx;
    int8_t blocked;
    int16_t ship_summary_idx;
    int16_t scan_highlight_pulse;
    int8_t scan_highlight_pulse_step;
    int8_t colony_eta;
    int8_t outpost_eta;
};

typedef struct s_plntsum_ship s_plntsum_ship, *Ps_plntsum_ship;

struct s_plntsum_ship {
    int16_t colony_ship_idx;
    int16_t outpost_ship_idx;
    int16_t ship_icon_id;
    bool colony_available;
    bool output_available;
};

typedef struct s_potential_colony_info s_potential_colony_info, *Ps_potential_colony_info;

struct s_potential_colony_info {
    int16_t pop_growth_bonus;
    int16_t maintenance;
    int16_t field2_0x4;
    int16_t worker_bonus;
    int16_t food;
    int16_t industry;
    int16_t research;
};

typedef struct s_prev_leader_data s_prev_leader_data, *Ps_prev_leader_data;

struct s_prev_leader_data {
    int8_t status;
    int16_t location;
    int16_t eta;
};

typedef struct s_qship s_qship, *Ps_qship;

struct s_qship {
    int8_t player;
    uint8_t size;
    int16_t ship_design_idx;
    int16_t field3_0x4;
    int8_t status;
    int16_t officer_idx;
    int16_t shield_id;
    int16_t missile_weapon_id;
    int16_t beam_weapon_id;
    int16_t bomb_weapon_id;
    int16_t special_weapon_id;
    int16_t missile_defense_bonus;
    int16_t beam_defense_bonus;
    int16_t base_ocv;
    int16_t beam_damage_modifier;
    int16_t missile_damage_modifier;
    int16_t max_hits;
    int16_t structural_damage;
};

typedef struct s_racecust_race_customize s_racecust_race_customize, *Ps_racecust_race_customize;

typedef struct s_racecust_trait_mod s_racecust_trait_mod, *Ps_racecust_trait_mod;

struct s_racecust_trait_mod {
    uint8_t trait_idx;
    int8_t trait_val;
};

struct s_racecust_race_customize {
    uint8_t count;
    struct s_racecust_trait_mod items[8];
};

typedef struct s_racecust_race_evolution s_racecust_race_evolution, *Ps_racecust_race_evolution;

struct s_racecust_race_evolution {
    uint8_t count;
    struct s_racecust_trait_mod items[2];
};

typedef struct s_report s_report, *Ps_report;

typedef union s_report_data s_report_data, *Ps_report_data;

typedef struct s_report_steal s_report_steal, *Ps_report_steal;

typedef struct s_report_sabotage s_report_sabotage, *Ps_report_sabotage;

typedef struct s_report_enemy_steal s_report_enemy_steal, *Ps_report_enemy_steal;

typedef struct s_report_enemy_sabotage s_report_enemy_sabotage, *Ps_report_enemy_sabotage;

typedef struct s_report_artifacts s_report_artifacts, *Ps_report_artifacts;

struct s_report_artifacts {
    int16_t planet_idx;
    int16_t tech_app_id;
};

struct s_report_enemy_sabotage {
    int16_t enemy_player_idx;
    int16_t planet_idx;
    uint8_t building_id;
};

struct s_report_sabotage {
    int8_t framed_player_idx;
    uint8_t victim_player_idx;
    int16_t planet_idx;
    uint8_t building_id;
};

struct s_report_enemy_steal {
    int16_t enemy_player_idx;
    int16_t tech_app_id;
};

struct s_report_steal {
    int8_t framed_player_idx;
    uint8_t victim_player_idx;
    int16_t tech_app_id;
};

union s_report_data {
    struct s_report_steal steal;
    struct s_report_sabotage sabotage;
    struct s_report_enemy_steal enemy_steal;
    struct s_report_enemy_sabotage enemy_sabotage;
    struct s_report_artifacts artifacts;
};

struct s_report {
    uint8_t type;
    int16_t player_idx;
    uint8_t action;
    union s_report_data d;
    uint8_t reserved[11];
};

typedef struct s_retreat_report s_retreat_report, *Ps_retreat_report;

struct s_retreat_report {
    int8_t reason;
    int16_t count;
    int8_t killed_officers;
};

typedef struct s_saved_box s_saved_box, *Ps_saved_box;

struct s_saved_box {
    int16_t field_id;
    int8_t is_open;
};

typedef struct s_scan_list_entry s_scan_list_entry, *Ps_scan_list_entry;

struct s_scan_list_entry {
    int16_t min_x;
    int16_t max_x;
};

typedef struct s_scroll_bar s_scroll_bar, *Ps_scroll_bar;

struct s_scroll_bar {
    int16_t thumb_center_y;
    int16_t last_thumb_center_y;
    int16_t thumb_center_y_copy;
    int16_t thumb_height_px;
    int16_t total_rows;
    int16_t visible_rows;
    int16_t first_visible_row;
    int16_t track_len_px;
    int16_t up_btn;
    int16_t dn_btn;
    undefined field10_0x14;
    undefined field11_0x15;
    undefined field12_0x16;
    undefined field13_0x17;
    int8_t type;
    undefined mouse_button_down;
    uint8_t row_step;
    int16_t thumb_y_snapshot;
};

typedef struct s_settings s_settings, *Ps_settings;

struct s_settings {
    uint8_t end_of_turn_summary;
    uint8_t end_of_turn_wait;
    uint8_t random_events;
    uint8_t enemy_moves;
    uint8_t expanding_help;
    uint8_t auto_select_ships;
    uint8_t animations_on;
    uint8_t auto_select_colony;
    uint8_t show_relocation_lines;
    uint8_t show_gnn_report;
    uint8_t auto_delete_tg_housing;
    uint8_t auto_saves;
    uint8_t only_show_serious_turn_summary_reports;
    uint8_t ship_initiative;
    int8_t tactical_combat_speed;
    int8_t message_delay;
    undefined field16_0x10;
    uint8_t sound_fx_on;
    sbyte sound_fx_level;
    uint8_t music_on;
    int8_t music_level;
    int8_t active_save_slot;
    uint8_t multi_player_game_type;
    char net_game_name[9];
    struct s_online_settings online_settings;
    uint8_t online_init_string[11];
    undefined field26_0xae;
    undefined field27_0xaf;
    undefined field28_0xb0;
    undefined field29_0xb1;
    undefined field30_0xb2;
    int8_t online_initialized;
    int8_t combat_speedx2_flag;
    int8_t combat_legal_moves_flag;
    int8_t combat_missile_impact_flag;
    int8_t combat_shields_arc_flag;
    int8_t combat_grid_flag;
    undefined field37_0xb9;
    undefined field38_0xba;
    undefined field39_0xbb;
    undefined field40_0xbc;
    undefined field41_0xbd;
    undefined field42_0xbe;
    undefined field43_0xbf;
    undefined field44_0xc0;
    undefined field45_0xc1;
    undefined field46_0xc2;
    undefined field47_0xc3;
    undefined field48_0xc4;
    undefined field49_0xc5;
    undefined field50_0xc6;
    undefined field51_0xc7;
    undefined field52_0xc8;
    undefined field53_0xc9;
    undefined field54_0xca;
    undefined field55_0xcb;
    undefined field56_0xcc;
    undefined field57_0xcd;
    undefined field58_0xce;
    undefined field59_0xcf;
    undefined field60_0xd0;
    undefined field61_0xd1;
    uint8_t language;
    uint8_t xenons_exist;
    int8_t game_difficulty;
    int8_t number_of_players;
    int8_t galaxy_size;
    int8_t galaxy_age;
    uint8_t strategic_combat_flag;
    int8_t starting_civilization_level;
    uint32_t start_of_game_seed;
    int16_t sound_driver_id;
    int16_t music_driver_id;
    uint16_t version;
    undefined field74_0xe4;
    undefined field75_0xe5;
    undefined field76_0xe6;
    undefined field77_0xe7;
    undefined field78_0xe8;
    undefined field79_0xe9;
    undefined field80_0xea;
    undefined field81_0xeb;
    undefined field82_0xec;
    undefined field83_0xed;
    undefined field84_0xee;
    undefined field85_0xef;
    undefined field86_0xf0;
    undefined field87_0xf1;
    undefined field88_0xf2;
    undefined field89_0xf3;
    undefined field90_0xf4;
    undefined field91_0xf5;
    undefined field92_0xf6;
    undefined field93_0xf7;
    undefined field94_0xf8;
    undefined field95_0xf9;
    undefined field96_0xfa;
    undefined field97_0xfb;
    undefined field98_0xfc;
    undefined field99_0xfd;
    undefined field100_0xfe;
    undefined field101_0xff;
    undefined field102_0x100;
    undefined field103_0x101;
    undefined field104_0x102;
    undefined field105_0x103;
    undefined field106_0x104;
    undefined field107_0x105;
    undefined field108_0x106;
    undefined field109_0x107;
    undefined field110_0x108;
    undefined field111_0x109;
    undefined field112_0x10a;
    undefined field113_0x10b;
    undefined field114_0x10c;
    undefined field115_0x10d;
    undefined field116_0x10e;
    undefined field117_0x10f;
    undefined field118_0x110;
    undefined field119_0x111;
    undefined field120_0x112;
    undefined field121_0x113;
    undefined field122_0x114;
    undefined field123_0x115;
    undefined field124_0x116;
    undefined field125_0x117;
    undefined field126_0x118;
    undefined field127_0x119;
    undefined field128_0x11a;
    undefined field129_0x11b;
    undefined field130_0x11c;
    undefined field131_0x11d;
    undefined field132_0x11e;
    undefined field133_0x11f;
    undefined field134_0x120;
    undefined field135_0x121;
    undefined field136_0x122;
    undefined field137_0x123;
    undefined field138_0x124;
    undefined field139_0x125;
    undefined field140_0x126;
    undefined field141_0x127;
    undefined field142_0x128;
    undefined field143_0x129;
    undefined field144_0x12a;
    undefined field145_0x12b;
    undefined field146_0x12c;
    undefined field147_0x12d;
    undefined field148_0x12e;
    undefined field149_0x12f;
    undefined field150_0x130;
    undefined field151_0x131;
    undefined field152_0x132;
    undefined field153_0x133;
    undefined field154_0x134;
    undefined field155_0x135;
    undefined field156_0x136;
    undefined field157_0x137;
    undefined field158_0x138;
    undefined field159_0x139;
    undefined field160_0x13a;
    undefined field161_0x13b;
    undefined field162_0x13c;
    undefined field163_0x13d;
    undefined field164_0x13e;
    undefined field165_0x13f;
    undefined field166_0x140;
    undefined field167_0x141;
    undefined field168_0x142;
    undefined field169_0x143;
    undefined field170_0x144;
    undefined field171_0x145;
    undefined field172_0x146;
    undefined field173_0x147;
    undefined field174_0x148;
    undefined field175_0x149;
    undefined field176_0x14a;
    undefined field177_0x14b;
    undefined field178_0x14c;
    undefined field179_0x14d;
    undefined field180_0x14e;
    undefined field181_0x14f;
    undefined field182_0x150;
    undefined field183_0x151;
    undefined field184_0x152;
    undefined field185_0x153;
    undefined field186_0x154;
    undefined field187_0x155;
    undefined field188_0x156;
    undefined field189_0x157;
    undefined field190_0x158;
    undefined field191_0x159;
    undefined field192_0x15a;
    undefined field193_0x15b;
    undefined field194_0x15c;
    undefined field195_0x15d;
    undefined field196_0x15e;
    undefined field197_0x15f;
    undefined field198_0x160;
    undefined field199_0x161;
    undefined field200_0x162;
    undefined field201_0x163;
    undefined field202_0x164;
    undefined field203_0x165;
    undefined field204_0x166;
    undefined field205_0x167;
    undefined field206_0x168;
    undefined field207_0x169;
    undefined field208_0x16a;
    undefined field209_0x16b;
    undefined field210_0x16c;
    undefined field211_0x16d;
    undefined field212_0x16e;
    undefined field213_0x16f;
    undefined field214_0x170;
    undefined field215_0x171;
    undefined field216_0x172;
    undefined field217_0x173;
    undefined field218_0x174;
    undefined field219_0x175;
    undefined field220_0x176;
    undefined field221_0x177;
    undefined field222_0x178;
    undefined field223_0x179;
    undefined field224_0x17a;
    undefined field225_0x17b;
    undefined field226_0x17c;
    undefined field227_0x17d;
    undefined field228_0x17e;
    undefined field229_0x17f;
    undefined field230_0x180;
    undefined field231_0x181;
    undefined field232_0x182;
    undefined field233_0x183;
    undefined field234_0x184;
    undefined field235_0x185;
    undefined field236_0x186;
    undefined field237_0x187;
    undefined field238_0x188;
    undefined field239_0x189;
    undefined field240_0x18a;
    undefined field241_0x18b;
    undefined field242_0x18c;
    undefined field243_0x18d;
    undefined field244_0x18e;
    undefined field245_0x18f;
    undefined field246_0x190;
    undefined field247_0x191;
    undefined field248_0x192;
    undefined field249_0x193;
    undefined field250_0x194;
    undefined field251_0x195;
    undefined field252_0x196;
    undefined field253_0x197;
    undefined field254_0x198;
    undefined field255_0x199;
    undefined field256_0x19a;
    undefined field257_0x19b;
    undefined field258_0x19c;
    undefined field259_0x19d;
    undefined field260_0x19e;
    undefined field261_0x19f;
    undefined field262_0x1a0;
    undefined field263_0x1a1;
    undefined field264_0x1a2;
    undefined field265_0x1a3;
    undefined field266_0x1a4;
    undefined field267_0x1a5;
    undefined field268_0x1a6;
    undefined field269_0x1a7;
    undefined field270_0x1a8;
    undefined field271_0x1a9;
    undefined field272_0x1aa;
    undefined field273_0x1ab;
    undefined field274_0x1ac;
    undefined field275_0x1ad;
    undefined field276_0x1ae;
    undefined field277_0x1af;
    undefined field278_0x1b0;
    undefined field279_0x1b1;
    undefined field280_0x1b2;
    undefined field281_0x1b3;
    undefined field282_0x1b4;
    undefined field283_0x1b5;
    undefined field284_0x1b6;
    undefined field285_0x1b7;
    undefined field286_0x1b8;
    undefined field287_0x1b9;
    undefined field288_0x1ba;
    undefined field289_0x1bb;
    undefined field290_0x1bc;
    undefined field291_0x1bd;
    undefined field292_0x1be;
    undefined field293_0x1bf;
    undefined field294_0x1c0;
    undefined field295_0x1c1;
    undefined field296_0x1c2;
    undefined field297_0x1c3;
    undefined field298_0x1c4;
    undefined field299_0x1c5;
    undefined field300_0x1c6;
    undefined field301_0x1c7;
    undefined field302_0x1c8;
    undefined field303_0x1c9;
    undefined field304_0x1ca;
    undefined field305_0x1cb;
    undefined field306_0x1cc;
    undefined field307_0x1cd;
    undefined field308_0x1ce;
    undefined field309_0x1cf;
    undefined field310_0x1d0;
    undefined field311_0x1d1;
    undefined field312_0x1d2;
    undefined field313_0x1d3;
    undefined field314_0x1d4;
    undefined field315_0x1d5;
    undefined field316_0x1d6;
    undefined field317_0x1d7;
    undefined field318_0x1d8;
    undefined field319_0x1d9;
    undefined field320_0x1da;
    undefined field321_0x1db;
    undefined field322_0x1dc;
    undefined field323_0x1dd;
    undefined field324_0x1de;
    undefined field325_0x1df;
    undefined field326_0x1e0;
    undefined field327_0x1e1;
    undefined field328_0x1e2;
    undefined field329_0x1e3;
    undefined field330_0x1e4;
    undefined field331_0x1e5;
    undefined field332_0x1e6;
    undefined field333_0x1e7;
    undefined field334_0x1e8;
    undefined field335_0x1e9;
    undefined field336_0x1ea;
    undefined field337_0x1eb;
    undefined field338_0x1ec;
    undefined field339_0x1ed;
    undefined field340_0x1ee;
    undefined field341_0x1ef;
    undefined field342_0x1f0;
    undefined field343_0x1f1;
    undefined field344_0x1f2;
    undefined field345_0x1f3;
    undefined field346_0x1f4;
    undefined field347_0x1f5;
    undefined field348_0x1f6;
    undefined field349_0x1f7;
    undefined field350_0x1f8;
    undefined field351_0x1f9;
    undefined field352_0x1fa;
    undefined field353_0x1fb;
    undefined field354_0x1fc;
    undefined field355_0x1fd;
    undefined field356_0x1fe;
    undefined field357_0x1ff;
    undefined field358_0x200;
    undefined field359_0x201;
    undefined field360_0x202;
    undefined field361_0x203;
    undefined field362_0x204;
    undefined field363_0x205;
    undefined field364_0x206;
    undefined field365_0x207;
    undefined field366_0x208;
    undefined field367_0x209;
    undefined field368_0x20a;
    undefined field369_0x20b;
    undefined field370_0x20c;
    undefined field371_0x20d;
    undefined field372_0x20e;
    undefined field373_0x20f;
    undefined field374_0x210;
    undefined field375_0x211;
    undefined field376_0x212;
    undefined field377_0x213;
    undefined field378_0x214;
    undefined field379_0x215;
    undefined field380_0x216;
    undefined field381_0x217;
    undefined field382_0x218;
    undefined field383_0x219;
    undefined field384_0x21a;
    undefined field385_0x21b;
    undefined field386_0x21c;
    undefined field387_0x21d;
    undefined field388_0x21e;
    undefined field389_0x21f;
    undefined field390_0x220;
    undefined field391_0x221;
    undefined field392_0x222;
    undefined field393_0x223;
    undefined field394_0x224;
    undefined field395_0x225;
    undefined field396_0x226;
    undefined field397_0x227;
    undefined field398_0x228;
};

typedef struct s_settler_settle_result s_settler_settle_result, *Ps_settler_settle_result;

struct s_settler_settle_result {
    uint8_t can_settle;
    uint8_t dest_owner_mismatch;
    uint8_t source_blockaded;
    uint8_t source_space_anomaly;
    uint8_t dest_is_outpost;
    uint8_t only_one_pop_left;
    uint8_t insufficient_freighters;
    uint8_t max_settlers_in_transit;
    uint8_t source_plague;
    uint8_t no_room_at_destination;
    uint8_t field10_0xa;
    uint8_t dest_blockaded;
    uint8_t dest_space_anomaly;
    uint8_t eta_turns;
    uint8_t selected_pop_flag_0x400;
};

typedef struct s_ship_data s_ship_data, *Ps_ship_data;

struct s_ship_data {
    struct s_ship_design d;
    int8_t owner;
    sbyte status;
    int16_t location;
    int16_t x;
    int16_t y;
    int8_t group_has_navigator;
    uint8_t travelling_speed;
    uint8_t turns_left;
    int8_t shield_damage_percent;
    int8_t drive_damage_percent;
    int8_t computer_damage;
    int8_t crew_quality;
    uint16_t crew_experience;
    int16_t officer_index;
    uint8_t special_device_damage_flags[5];
    int16_t armor_damage;
    int16_t structural_damage;
    int8_t mission;
    int8_t just_built;
    int16_t prior_jump_gate_destination;
    int32_t prior_jump_gate_stardate;
};

typedef struct s_ship_drive_data s_ship_drive_data, *Ps_ship_drive_data;

struct s_ship_drive_data {
    undefined field0_0x0;
    undefined field1_0x1;
    undefined field2_0x2;
    undefined field3_0x3;
    undefined field4_0x4;
    undefined field5_0x5;
    int16_t tech_application_idx;
    undefined field7_0x8;
    undefined field8_0x9;
    undefined field9_0xa;
    undefined field10_0xb;
    undefined field11_0xc;
    undefined field12_0xd;
    undefined field13_0xe;
    undefined field14_0xf;
    undefined field15_0x10;
    undefined field16_0x11;
    undefined field17_0x12;
    undefined field18_0x13;
    undefined field19_0x14;
    undefined field20_0x15;
    undefined field21_0x16;
    undefined field22_0x17;
    undefined field23_0x18;
    undefined field24_0x19;
    undefined field25_0x1a;
    undefined field26_0x1b;
    undefined field27_0x1c;
    undefined field28_0x1d;
    undefined field29_0x1e;
    undefined field30_0x1f;
    uint8_t combat_speed_modifier[12];
    int16_t ftl_speed;
};

typedef struct s_ship_icon s_ship_icon, *Ps_ship_icon;

struct s_ship_icon {
    int16_t stack_id;
    int16_t node_idx;
    int16_t star_idx;
    int16_t stack_slot;
    int16_t x;
    int16_t y;
};

typedef struct s_ship_move_info s_ship_move_info, *Ps_ship_move_info;

struct s_ship_move_info {
    int8_t blackhole_blocks;
    int8_t insufficient_fuel;
    int8_t immobile;
    int8_t pad;
    uint8_t turns_left;
    int8_t moving;
    int8_t out_of_range;
    int8_t has_navigator;
    uint8_t traveling_speed;
    uint8_t parsecs;
    int8_t uses_wormhole;
    int8_t has_jumpgate_route;
    int8_t has_stargate_route;
    int8_t hyperspace_flux;
};

typedef struct s_ship_node s_ship_node, *Ps_ship_node;

struct s_ship_node {
    int16_t ship_idx;
    int16_t next_node;
    int8_t selected;
};

typedef struct s_smack_video s_smack_video, *Ps_smack_video;

struct s_smack_video {
    uint32_t version;
    int32_t width;
    int32_t height;
    uint32_t frame_count;
    undefined unk2[88];
    uint32_t has_palette;
    undefined unk3[30];
    uint8_t palette[236][3];
    undefined unk4[34];
    uint32_t current_frame;
    int32_t rect_left;
    int32_t rect_top;
    uint32_t rect_width;
    uint32_t rect_height;
};

typedef uint16_t WORD;

typedef struct s_spcl s_spcl, *Ps_spcl;

struct s_spcl {
    int16_t id;
    int16_t type;
};

typedef struct s_star_data s_star_data, *Ps_star_data;

struct s_star_data {
    char name[15];
    int16_t x;
    int16_t y;
    int8_t size;
    int8_t owner;
    int8_t pict_type;
    int8_t spectral_class;
    int8_t last_planet_selected[MAX_PLAYERS];
    int8_t black_hole_blocks[BITMAP(MAX_STARS)];
    int8_t system_special;
    int16_t wormhole_star_id;
    uint8_t blockaded;
    uint8_t blockaded_by[MAX_PLAYERS];
    uint8_t visited;
    uint8_t just_visited;
    uint8_t ignore_colony_ships;
    uint8_t ignore_combat_ships;
    int8_t colonize_player;
    uint8_t has_colony;
    uint8_t has_warp_field_interdictor;
    int16_t next_wfi_in_list;
    uint8_t has_tachyon;
    uint8_t has_subspace;
    uint8_t has_stargate;
    uint8_t has_jumpgate;
    uint8_t has_artemis_net;
    uint8_t has_dimensional_portal;
    uint8_t is_stagepoint;
    int8_t officer_index[MAX_PLAYERS];
    int16_t planet_index[5];
    int16_t relocate_ship_to[MAX_PLAYERS];
    int8_t twinkling1;
    int8_t twinkling2;
    uint8_t not_used3;
    int8_t surrender_to[MAX_PLAYERS];
    uint8_t in_nebula;
    uint8_t artifacts_gave_app;
};

typedef struct s_star_weight s_star_weight, *Ps_star_weight;

struct s_star_weight {
    int16_t star_idx;
    uint8_t weight;
};

typedef struct s_sys_disp s_sys_disp, *Ps_sys_disp;

struct s_sys_disp {
    int16_t orbit;
    int16_t x;
    int16_t y;
    int8_t oribit_pos;
    int8_t orbit_idx;
    int16_t current_frame;
    struct s_animation_header* pic;
};

typedef struct s_system_display_ships s_system_display_ships, *Ps_system_display_ships;

struct s_system_display_ships {
    int16_t x;
    int16_t y;
    int16_t button;
    int16_t ship_idx;
    struct s_animation_header* pic;
};

typedef struct s_tech_data_personal_shield s_tech_data_personal_shield, *Ps_tech_data_personal_shield;

struct s_tech_data_personal_shield {
    int16_t tech_app_id;
    int8_t bonus;
};

typedef struct s_tech_data_rifle s_tech_data_rifle, *Ps_tech_data_rifle;

struct s_tech_data_rifle {
    int16_t tech_app_id;
    uint8_t bonus;
};

typedef struct s_techdata_planetary_shield s_techdata_planetary_shield, *Ps_techdata_planetary_shield;

struct s_techdata_planetary_shield {
    uint16_t tech_app_id;
    uint8_t absorption;
};

typedef struct s_transport_pack_ack s_transport_pack_ack, *Ps_transport_pack_ack;

struct s_transport_pack_ack {
    uint32_t message_stamp;
    uint16_t send_data_length;
    uint8_t packet_index;
    uint8_t total_packets;
    uint8_t host;
    uint16_t current_user_type;
    uint8_t burst_index;
    uint8_t message_is_processed;
    uint32_t send_immediate_burst;
    uint16_t pad;
};

typedef struct s_transport_packet s_transport_packet, *Ps_transport_packet;

#define TRANSPORT_PAYLOAD_SIZE 331

struct s_transport_packet {
    uint32_t message_stamp;
    uint16_t send_data_length;
    uint8_t packet_index;
    uint8_t total_packets;
    uint8_t sender_host_id;
    uint16_t current_user_type;
    uint8_t burst_index;
    uint8_t message_is_processed;
    uint32_t send_immediate_burst;
    uint16_t pad;
    uint8_t payload[TRANSPORT_PAYLOAD_SIZE];
};

typedef struct s_weapon_filter_item s_weapon_filter_item, *Ps_weapon_filter_item;

struct s_weapon_filter_item {
    int16_t weapon_idx;
    int32_t tech_cost;
};

typedef struct s_weapon_visual_fx s_weapon_visual_fx, *Ps_weapon_visual_fx;

struct s_weapon_visual_fx {
    int32_t r1;
    int32_t g1;
    int32_t b1;
    int32_t r2;
    int32_t g2;
    int32_t b2;
    int32_t thickness;
    int32_t anim_style;
    int32_t sfx_sprite_id;
    int32_t duration;
    int32_t wide_bolt_size_bonus;
};

typedef struct s_winnet_provider s_winnet_provider, *Ps_winnet_provider;

struct s_winnet_provider {
    GUID guid;
    char name[100];
    uint32_t major_version;
    uint32_t minor_version;
};

typedef struct s_wormhole_data s_wormhole_data, *Ps_wormhole_data;

struct s_wormhole_data {
    uint8_t success;
    int16_t dest_star_idx;
    int16_t count;
};

struct s_palette_header {
    int16_t start_index;
    int16_t count;
};

inline s_palette_header* Animation_Palette_Header_(s_animation_header* header) {
    return reinterpret_cast<s_palette_header*>(Animation_Frame_Table_(header) + (header->frame_count + 1) * sizeof(uint32_t));
}

inline const s_palette_header* Animation_Palette_Header_(const s_animation_header* header) {
    return reinterpret_cast<const s_palette_header*>(Animation_Frame_Table_(header) + (header->frame_count + 1) * sizeof(uint32_t));
}

struct s_packed_frame_header {
    uint16_t unknown_0;
    uint16_t start_y;
};

struct s_packed_frame_run {
    int16_t pixel_count;
    int16_t skip_count;
};

struct s_packed_screen_background_run32 {
    uint32_t skip_count;
    uint32_t pixel_count;
};

struct s_packed_screen_background_run8 {
    int8_t skip_count;
    int8_t pixel_count;
};

#pragma pack(pop)
