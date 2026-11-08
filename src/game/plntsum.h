#pragma once

struct s_animation_header;

typedef struct s_plntsum_screen s_plntsum_screen, *Ps_plntsum_screen;

struct s_plntsum_screen {
    struct s_animation_header* pics[14];
    int16_t buttons[10];
    int16_t fields[8];
    int16_t colony_icons[8];
    int16_t output_icons[8];
    int16_t up_button;
    int16_t dn_button;
    int16_t esc_button;
    int16_t planet_icons[MAX_STARS];
    bool refresh_fields;
    int16_t scanned_input;
    int16_t fields_first_row;
    int16_t fields_total_rows;
    int16_t fields_sort_choice;
};

namespace PLNTSUM {
    extern int16_t plntscrn_delta_y[9];
    extern int16_t sort_choice;
    extern int16_t filter_out_adverse_gravity_planets;
    extern int16_t filter_out_hostile_environment_planets;
    extern int16_t filter_out_mineral_scarcity_planets;
    extern int16_t filter_out_enemy_controlled_planets;
    extern int16_t filter_out_planets_out_of_range;

    extern s_plntsum_screen* screen;

    extern s_colors hi_neutral_colors;
    extern s_colors red_colors;
    extern s_colors green_colors;

    extern s_plntsum_planet* planet_summary;

    extern s_plntsum_ship* ship_summary;

    extern int16_t n_ships_in_summary;
    extern int8_t can_reach_star[MAX_STARS];

    extern uint8_t explored_bitmap[BITMAP(MAX_PLANETS)];

    void __cdecl Reset_Explored_Planets();

    extern uint8_t found_outpost_ship;

    extern uint8_t draw_colony_button_status;

    extern int16_t send_choice;

    extern uint8_t found_colony_ship;

    extern uint8_t draw_outpost_button_status;

    extern int16_t plntsum_scroll_bar;

    extern uint32_t planet_summary_reserved_counter;

    extern uint8_t planet_summary_refresh_marker;

    void __cdecl Add_Planet_Summary_Bar_Field();

    void __cdecl Add_Colony_Ship_Icon_Field(int16_t* field_id, int16_t row_index);

    void __cdecl Add_Outpost_Ship_Icon_Field(int16_t* field_id, int16_t row_index);

    void __cdecl Add_PlntSum_Galaxy_Map_Fields(int16_t* fields_array);

    void __cdecl Add_Plntsum_Fields();

    void __cdecl Refresh_Plntsum_Fields();

    void __cdecl Handle_Send_Button(int16_t input_id);

    void __cdecl Planet_Summary_Screen();

    bool __cdecl Send_Colony_Button_Selectable();
    bool __cdecl Send_Outpost_Button_Selectable();
    bool __cdecl Summary_Found_Colony_Ship(int16_t ship_summary_idx);
    bool __cdecl Summary_Found_Outpost_Ship(int16_t ship_summary_idx);
    bool __cdecl Planet_Is_Outpost_Planet(int16_t planet_idx);
    bool __cdecl Planet_Has_Players_Colony(int16_t planet_idx);
    bool __cdecl Planet_Has_Been_Visited(int16_t planet_index);
    bool __cdecl Planet_In_Range_Of_A_Colony(int16_t planet_idx);

    int16_t __cdecl Productivity_Penalty(int16_t player_idx, int16_t planet_gravity);

    int16_t __cdecl First_Planet_In_Summary_List(int16_t star_index);

    int16_t __cdecl Summary_Planet_Index(int16_t star_index);

    int16_t __cdecl Get_ETA_To_Star(int16_t star_idx);

    void __cdecl Set_Planet_Explored_Flags();

    bool __cdecl Planet_Has_Adverse_Gravity(int16_t planet_idx, int16_t player_idx);
    bool __cdecl Planet_Is_Enemy_Controlled(int16_t planet_idx, int16_t player_idx);
    bool __cdecl Planet_Has_Hostile_Environment(int16_t planet_idx);

    bool __cdecl Planet_Is_Mineral_Scarce(int16_t planet_idx);
    int16_t __cdecl Colony_Ship_Auto_Colonization(s_ship_data* ship);
    void __cdecl Set_Auto_Colonize_Flag(s_ship_data* ship, int16_t flag);
    bool __cdecl Ship_Is_Not_Colonizing(s_ship_data* ship);

    int16_t __cdecl Planet_Summary_List_Id(int16_t planet_idx);

    void __cdecl Set_Colony_Ship_To_Star_Distances(int16_t summary_idx, int16_t ship_idx);

    void __cdecl Set_Outpost_Ship_To_Star_Distances(int16_t summary_idx, int16_t ship_idx);

    void __cdecl Update_Ship_Etas();

    extern int16_t last_ship_moved;

    extern const char* PTR_s_PLNTSUM_LBX_00556AC0;

    extern s_animation_header* send_colony_ship_button_seg;

    extern s_animation_header* send_outpost_ship_button_seg;

    void __cdecl Load_Player_Icon();

    void __cdecl Load_Planet_Summary_Pictures();

    void __cdecl Load_Planet_Pictures();

    void __cdecl Allocate_Space_For_Summary_Screen();

    void __cdecl Check_For_Colony_And_Outpost_Ships();

    void __cdecl Determine_Button_Status();

    void __cdecl Build_Ship_Summary();

    int __cdecl qsort_by_climate(const void* a, const void* b);
    int __cdecl qsort_by_minerals(const void* a, const void* b);
    int __cdecl qsort_by_max_population(const void* a, const void* b);

    void __cdecl Sort_Planet_Summary();
    void __cdecl Filter_Explored_Planets(int16_t* summary_count);
    int16_t __cdecl Planet_Summary_Id(int16_t explored_id);

    int16_t __cdecl Nearest_Ship_Id(int16_t index);

    void __cdecl Send_Nearest_Ship(int16_t explored_idx);

    void __cdecl Reset_Ship_To_Star_Distances();

    void __cdecl Undo_Send_Order(int16_t planet_idx, int16_t order_type);

    extern bool can_reach_scanned_planet;

    extern int8_t box_field_scanned;

    void __cdecl Scan_Summary_Box_Fields(int16_t input_field_id, int16_t clicked_field_id, int16_t* last_input_field_id_ptr);

    void __cdecl Wrapper_For_Planet_Summary_Galaxy_Map();

    void __cdecl Draw_Nearest_Ship_Directional_Line(int16_t star_idx);

    void __cdecl Get_Colony_Outpost_Marker_XY(int16_t summary_idx, int16_t* out_x, int16_t* out_y);

    void __cdecl Print_Colony_Outpost_Marker(int16_t x, int16_t y, int16_t ship_summary_idx, int8_t colony, int8_t outpost);

    extern bool recalc_first_planet;

    extern uint8_t fade_flag;

    extern uint8_t planet_summary_state_marker;

    extern uint8_t skip_draw_planets;

    void __cdecl Fill_Summary_Screen_Scroll_Bar();

    extern s_animation_header** p_rotating_planet;

    void __cdecl Draw_Rotating_Planets();

    s_animation_header* __cdecl Load_Planet_Segment(int16_t planet_anim_idx);

    void __cdecl Summary_Screen_Print_Data();

    void __cdecl Draw_Colony_ETA(int16_t planet_sum_idx, int16_t scroll_idx);
    void __cdecl Draw_Outpost_ETA(int16_t planet_sum_idx, int16_t scroll_idx);

    void __cdecl Player_Colors(int16_t player_idx, s_colors* colors_out, int8_t type, uint8_t* base_color_out);

    void __cdecl Colony_Owner_Colors(int16_t colony_idx, s_colors* colors, int8_t highlight_type);

    void __cdecl Summary_Screen_Print_Scanned();

    void __cdecl Draw_Mini_Planet_Summary_Screen();
} // namespace PLNTSUM