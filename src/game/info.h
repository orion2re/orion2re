#pragma once
#include <cstdint>

struct s_animation_header;
struct s_colors;
struct s_drop_area;
struct s_help_box;
struct s_help_topic_record;
struct s_list_item;
struct s_list_state;
struct s_tech_review_achievement;

typedef struct s_tech_review_achievement s_tech_review_achievement, *Ps_tech_review_achievement;

typedef enum TECH_REVIEW_GROUP {
    TECH_REVIEW_GROUP_MISCELLANEOUS = 0,
    TECH_REVIEW_GROUP_NEW_CONSTRUCTION_TYPES = 1,
    TECH_REVIEW_GROUP_SPIES = 2,
    TECH_REVIEW_GROUP_COLONY = 3,
    TECH_REVIEW_GROUP_GROUND_COMBAT = 4,
    TECH_REVIEW_GROUP_SHIP_EQUIPMENT = 5,
    TECH_REVIEW_GROUP_FOOD = 6,
    TECH_REVIEW_GROUP_POLLUTION = 7,
    TECH_REVIEW_GROUP_MORALE = 8,
    TECH_REVIEW_GROUP_PRODUCTION = 9,
    TECH_REVIEW_GROUP_COLONY_DEFENSE = 10,
    TECH_REVIEW_GROUP_RESEARCH = 11,
    TECH_REVIEW_GROUP_MONEY = 12,
    TECH_REVIEW_GROUP_BEAMS = 13,
    TECH_REVIEW_GROUP_MISSILES_TORPEDOES = 14,
    TECH_REVIEW_GROUP_BOMBS_BIOLOGICAL = 15,
    TECH_REVIEW_GROUP_FIGHTERS = 16,
    TECH_REVIEW_GROUP_SPECIAL = 17,
    TECH_REVIEW_GROUP_SHIELDS = 18,
    TECH_REVIEW_GROUP_ARMOR = 19,
    TECH_REVIEW_GROUP_DRIVES = 20,
    TECH_REVIEW_GROUP_COMPUTERS = 21,
    TECH_REVIEW_GROUP_FUELS_RANGE = 22,
    TECH_REVIEW_GROUP_EQUIPMENT_DEFENSE = 23,
    TECH_REVIEW_GROUP_SCANNERS = 24,
    TECH_REVIEW_GROUP_OFFENSE = 25,
    TECH_REVIEW_GROUP_COUNT = 26,
    TECH_REVIEW_GROUP_NONE = 255,
} TECH_REVIEW_GROUP;

struct s_tech_review_achievement {
    uint16_t application;
    uint8_t group;
};

namespace INFO {
    enum {
        TECH_REVIEW_ACHIEVEMENT_COUNT = 256,
        TECH_REVIEW_COLONY_COUNT = 256,
        TECH_REVIEW_WEAPON_COUNT = 256,
        TECH_REVIEW_EQUIPMENT_COUNT = 256,
    };

    extern s_tech_review_achievement review_achievements[TECH_REVIEW_ACHIEVEMENT_COUNT];
    extern s_tech_review_achievement review_colony[TECH_REVIEW_COLONY_COUNT];
    extern s_tech_review_achievement review_weapons[TECH_REVIEW_WEAPON_COUNT];
    extern s_tech_review_achievement review_equipment[TECH_REVIEW_EQUIPMENT_COUNT];

    extern s_tech_review_achievement* review_list[4];

    extern s_colors info_normal_color;

    extern s_colors info_high_color;

    extern int16_t tech_rev_field[8];

    extern s_drop_area tech_review_drops[4];

    extern uint16_t list_sizes[4];

    extern s_list_state tech_review_list_header;

    extern int16_t tech_review_tab_index;

    extern void* g_tech_image_seg;

    extern int16_t g_last_list_field;

    extern uint16_t g_selected_app_id;

    extern s_list_state* g_header_p;

    extern s_animation_header* g_bitmap1_p;

    extern s_animation_header* g_bitmap2_p;

    extern int16_t g_prior_choice;

    extern s_list_item* g_selected_list_p;

    extern uint8_t g_list_update_needed;

    extern int16_t g_first_list_field;

    extern s_colors info_red_color;

    enum e_info_menu_layout : uint8_t {
        INFO_MENU_LAYOUT_CLASSIC,
        INFO_MENU_LAYOUT_ICE,
    };

    extern e_info_menu_layout info_menu_layout;

    extern uint8_t g_reference_screen_type;

    extern uint8_t g_turn_message_ix;

    extern int16_t g_reference_ix;

    extern char (*g_how_to_str_)[48];

    extern int16_t g_first_how_to_field;

    extern int16_t g_first_category_field;

    extern char (*g_category_str_p)[48];

    extern uint8_t race_statistics_page;

    extern s_drop_area turn_summary_drop;

    extern s_drop_area reference_drops[4];

    extern s_list_state g_turn_header;

    extern s_list_state g_category_header;

    extern uint16_t g_exit_field;

    extern s_list_item* g_selected_msg_p;

    extern s_drop_area how_to_drops[2];
    extern s_drop_area category_drops[3];
    extern s_drop_area race_stats_drops[4];
    extern int16_t race_bitmap_x[4];
    extern int16_t race_bitmap_y[4];
    extern int16_t race_statistics_specials_help_count;
    extern s_help_box race_statistics_specials_help[50];

    /** Returns the currently selected player-info tab index. */
    int16_t __cdecl Get_Plyr_Info_Btns();

    /** Fills the output array with normalized maintenance percentages for the active player. */
    int16_t __cdecl Initialize_Maintenance_Values(int32_t* values);

    /** Builds the maintenance chart bar and label bitmaps from the supplied values. */
    void __cdecl Build_Maint_Chart_Bitmaps(s_animation_header** sprites, s_animation_header** bars, s_animation_header** labels, int32_t* values);

    extern s_colors info_dark_color;

    extern s_colors* maintenance_bar_colors[7];

    extern s_colors* PTR__orange_text_colors[8];

    extern s_colors hist_palettes[MAX_PLAYERS];

    extern s_colors* hist_graph_palettes[MAX_PLAYERS];

    /** Recolors bitmap pixels whose indices fall within the supplied palette range. */
    void __cdecl Colorize_Bitmap(s_animation_header* anim, s_colors* color_table, uint8_t range_start, uint8_t range_end);

    extern s_drop_area history_graph_drops[2];
    extern int16_t graph_button_x[4];

    extern uint8_t graph_offsets[10];
    extern bool history_graph_smoothing;

    /** Draws the maintenance income chart and summary text for the active player. */
    void __cdecl Draw_Maint_Income_Chart(s_animation_header** chart_bars, s_animation_header** chart_labels, int16_t visible_bar_count);

    /** Runs the history-graph tab until the caller's exit threshold is reached. */
    int16_t __cdecl History_Subscreen(s_animation_header** lbx_images, int16_t exit_threshold);

    void __cdecl Set_Up_Player_Colors(s_animation_header** anim_ptrs);

    int32_t __cdecl Smooth_History_Data(int32_t* data, int16_t start_idx, int16_t end_idx);

    void __cdecl Get_Plyr_Hist_Btns(int16_t* btns);

    void __cdecl Set_Plyr_Hist_Btns(int16_t* btns_state);

    void __cdecl Draw_History_Subscreen();

    void __cdecl Draw_Drop_Area(s_drop_area* area);

    void __cdecl Draw_Multiple_Drop_Areas(s_drop_area* areas, uint16_t count);

    void __cdecl Display_Graphed_Players_Names();

    int16_t __cdecl Draw_The_History_Graph();

    void __cdecl Draw_Histories(int16_t offset_x);

    void __cdecl Introduce_Subscreen();

    void __cdecl Draw_Tech_Review_Subscreen();

    void Init_Tech_Review_List(s_list_state* state, int16_t review_idx, const s_player& subject);

    /** Runs the tech-review tab for the supplied player until the exit threshold is reached. */
    int16_t __cdecl _Tech_Review_Subscreen(s_animation_header** anims, int16_t exit_threshold, game::id_player player);

    int16_t __cdecl Tech_Review_Subscreen(s_animation_header** headers, int16_t exit_threshold);

    /** Runs the race-statistics tab until the caller's exit threshold is reached. */
    int16_t __cdecl Race_Stats_Subscreen(s_animation_header** anims, int16_t exit_threshold);

    void __cdecl Print_Player_Specials_To_Bitmap(int16_t x, int16_t y, game::id_player player, s_animation_header* bitmap);

    void __cdecl Race_Statistics_Specials_Help(int16_t x, int16_t y, game::id_player player);

    void __cdecl Draw_Race_Stats_Subscreen();

    void __cdecl Draw_Dummy_Turn_Summary_Subscreen();

    void __cdecl Draw_Turn_Summary_Subscreen();

    void __cdecl _Init_Turn_Summary_List(s_list_state* state, int16_t num_items, int16_t font_style, int16_t font_lf);

    void __cdecl Init_Turn_Summary_List(s_list_state* state, int16_t val);

    int16_t __cdecl Turn_Summary_Subscreen(s_animation_header** anim_data, int16_t min_field);

    /** Displays the reference index view and records which sub-view should open next. */
    int16_t __cdecl Reference_Main_Subscreen(s_animation_header** unused_anims, int16_t exit_code);

    void __cdecl Draw_Reference_Main_Subscreen();

    int16_t __cdecl Reference_How_To_Subscreen(s_animation_header** anims, int16_t exit_code);

    void __cdecl Draw_Reference_Category_Subscreen();

    int16_t __cdecl Reference_Category_Subscreen(s_animation_header** anims, int16_t unused_exit_code);

    /** Dispatches to the active reference sub-view. */
    int16_t __cdecl Reference_Subscreen(s_animation_header** anims, int16_t exit_threshold);

    void __cdecl Info_Screen();
} // namespace INFO
