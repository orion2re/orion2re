#pragma once

#include "colony_product.h"
#include <cstdint>

struct s_animation_header;
struct s_colors;
struct s_planet_data;
struct s_colony;
struct s_colony_bldg;

namespace COLONY {
    extern uint8_t allow_native_annihilation;
    extern uint8_t max_buildings_scrapped_per_turn;

    bool Can_Scrap_Building(const s_colony& colony, uint16_t stardate);
    void Record_Building_Scrap(s_colony* colony, uint16_t stardate);

    extern int32_t building_cr[170];

    extern int16_t colony_fields[21];

    extern int16_t military_fields[2];

    extern int16_t job_fields[3];

    extern int16_t pop_fields_count[8];

    extern int16_t sys_disp_summ_fields[5];

    extern int16_t sys_disp_fields[3];

    extern int16_t sys_disp_planet_fields[5];

    extern int16_t pop_fields[42];

    extern s_colony_bldg colony_bldgs[6];

    extern int16_t colony_satellites[10];

    extern char FIREPTS_LBX[12];
    extern const char RACEICON_LBX[13];
    extern const char PLANETS_LBX[12];
    extern const char AMEBAFIN_LBX[13];
    extern const char COLBLDG_LBX[12];
    extern const char COLSUM_LBX[11];
    extern const char COLSYSDI_LBX[13];
    extern const char DIMTVFIN_LBX[13];
    extern const char NEWGAME_LBX[12];
    extern const char PLNTDFIN_LBX[13];
    extern const char MAINPUPS_LBX[13];
    extern const char BEAMS_LBX[10];
    extern const char CMBTSFX_LBX[12];
    extern const char SPHERSFX_LBX[13];
    extern char COLROADS_LBX[13];

    extern int16_t step_squish;
    extern int16_t scanned_pop;
    extern int16_t scanned_military;
    extern int16_t scanned_satellite;

    void __cdecl Init_Colonies();

    /**
     * Shows the colony screen for the star/orbit currently stored in the screen globals.
     */
    void __cdecl Colony_Screen();

    bool __cdecl Colonize_Planet(game::id_planet planet_idx);

    extern int16_t colony_satellite_fields[10];

    extern int16_t colony_saved_screen;
    extern int16_t g_scanned_star;
    extern game::id_planet g_scanned_planet;
    extern game::id_planet planet_display_handle;

    extern uint16_t colonists_count;

    extern int16_t orbit_temp;
    extern game::id_colony temp_colony_idx;

    extern int16_t ship_bitm_n;
    extern int16_t bldg_bitm_n;
    extern int16_t g_ship_pic_n;
    extern int32_t g_ship_player_n;
    extern s_animation_header* ship_pic_bitm;
    extern s_animation_header* ship_bitm;
    extern s_animation_header* swap_bitm;

    extern s_animation_header* mouse_bitm;

    extern const char COLONY_LBX[11];
    extern const char COLVEGGIE_LBX[13];
    extern const char COLGCBT_LBX[12];
    extern const char COLONY2_LBX[12];
    extern const char COLPUPS_LBX[12];

    extern s_animation_header* c_anims[36];

    /**
     * Caches the active colony, planet, star, and orbit handles for colony-screen helpers.
     * @param colony_idx Index of the colony to expose through the colony globals.
     */
    void __cdecl Set_Colony_Globals(game::id_colony colony_idx);

    extern uint8_t debug_flag;
    extern uint8_t using_colony_screen_palette;
    extern uint8_t drawing_display;
    extern uint8_t valid_colony;
    extern uint8_t field_mode;
    extern int16_t move_colonist;

    void __cdecl Draw_Bldg_CR_Dark(int16_t col, int16_t row);

    extern int16_t scanned_bldg_C;
    extern int16_t scanned_bldg_R;
    extern uint8_t full_draw;
    extern game::id_colony colony_handle;
    extern int16_t building_cr_xy[7][7][4];

    extern int16_t colony_bldg_fields[36];

    extern int16_t pop_fields_coord_var[8];

    extern int16_t pop_fields_ref[3][42];

    extern game::id_planet planet_handle;
    extern game::id_star star_handle;
    extern int32_t orbit_n;
    extern uint8_t cheats;
    extern game::id_star sys_disp_star_handle;

    void __cdecl Load_Colony_Anims();
    /** Loads the colony-screen animation and bitmap resources into the screen segment. */
    void __cdecl Load_Colony_Screen_Seg();
    void __cdecl Load_Planet_Screen();
    s_animation_header* __cdecl Cache_Load_Planet(int16_t planet_id);
    s_animation_header* __cdecl C_Anims(int16_t anim_id);

    /**
     * Loads the requested government animation frame from COLONY2.LBX.
     * @param govt_anim_id Government animation entry offset.
     */
    s_animation_header* __cdecl Load_Govt_Anim(int16_t govt_anim_id);

    s_animation_header* __cdecl Govt_Anim(int16_t govt_anim_id);

    s_animation_header* __cdecl Colony_Pop_Anim(const s_colony& colony, int16_t pop_idx);

    int16_t __cdecl Get_Effective_Pop_Player(const s_colony& colony, int16_t pop_idx);

    s_animation_header* __cdecl Colony_Pop_Icon(int16_t race_index);

    s_animation_header* __cdecl Military_Anims(int16_t animation_variant, int16_t frame);
    s_animation_header* __cdecl Military_Anim(int16_t animation_type, int16_t frame);

    /**
     * Maps a colony pop record to the colony-screen portrait state.
     * @param colony_idx Colony that owns the pop record.
     * @param pop_idx Index of the pop entry within the colony.
     */
    int16_t __cdecl Pop_To_Pop_State(uint32_t pop_value);

    char* __cdecl Pop_State_String(int16_t state);

    char* __cdecl Conquered_String(bool is_conquered);

    char* __cdecl Pop_Race_String(game::id_colony colony_idx, int16_t pop_idx);

    char* __cdecl Job_String(int16_t job_id);

    void __cdecl Get_Bldg_Box(int16_t c, int16_t r, int16_t* x_arr, int16_t* y_arr);

    bool __cdecl XY_In_Bldg_Poly(int16_t x, int16_t y, int16_t bldg_type, int32_t bldg_id);

    void __cdecl Update_Colony_Palette();
    void __cdecl Make_Bldg_Array_For_Colony(game::id_colony colony_handle, uint8_t flag);
    void __cdecl Reset_Mouse_Picture();
    void __cdecl Clear_Mouse_Picture();
    void __cdecl Picture_To_Mouse(s_animation_header* anim_header);

    void __cdecl Load_Ship_Bitm();

    void __cdecl Load_Bldg_Bitm();

    /** Loads the stationed officer portrait for the active colony star, if any. */
    void __cdecl Load_Officer_Anim();

    void __cdecl Load_Galaxy_Map_Anims();

    char* __cdecl CR_Building_Name(int16_t building_id);

    char* __cdecl Real_Building_Name(int16_t building_id);

    char* __cdecl Real_Building_Name2(int16_t building_id);

    s_animation_header* __cdecl Player_Troop_Anim(int16_t player_idx, int16_t anim_type, int16_t frame);

    char* __cdecl Player_Troop_Name(int16_t player_idx, int16_t troop_type, int16_t race);

    void __cdecl Remap_Draw_Ship_Seg_N_Centered(int16_t x, int16_t y, int16_t ship_picture_num);

    void __cdecl Do_Load_Colony_Anims();

    s_animation_header* __cdecl Spy_Anim(int16_t race_index);

    /**
     * Returns the colony-screen population animation for a job/state/race combination.
     * @param job_type Farmer/worker/scientist animation group.
     * @param pop_state Colony-screen state variant for the pop.
     * @param race_idx Race index whose portrait set should be used.
     */
    s_animation_header* __cdecl People_Anim(int16_t job_type, int16_t pop_state, int16_t race_idx);

    s_animation_header* __cdecl Prod_Anims(int16_t production_type, int16_t variant, int8_t frame);

    s_animation_header* __cdecl Import_Anims(int16_t type, int16_t is_tens, int8_t frame);

    s_animation_header* __cdecl Short_Anims(int16_t type, int16_t is_tens, uint8_t frame);

    int16_t __cdecl Satellite_Anim_Pic(int16_t id);

    s_animation_header* __cdecl Satellite_Anim(int16_t satellite_id);

    void __cdecl Draw_Satellite_Dark(int16_t x, int16_t y, int16_t satellite_id);

    s_animation_header* __cdecl Buffer_Reload_Planets(int16_t entry_num);
    s_animation_header* __cdecl Buffer_Reload_Colony(int16_t index);
    bool __cdecl Sat_Should_Be_Replaced(int16_t building_id);
    void __cdecl Insert_Satellite_Into_Array(int16_t satellite_id, int16_t* count);

    void __cdecl Remap_Draw_Ship_D_Centered(int16_t x, int16_t y, int16_t design_index);

    void __cdecl Draw_Building_With_Bottom_Centered(int16_t x, int16_t y, int16_t type, int16_t col, int16_t row);

    void __cdecl Remap_Draw_Ship_Centered(int16_t x, int16_t y, int16_t ship_index);

    int16_t __cdecl Calculate_Colony_Turn_Count_From_Scrap_For_Prod(game::id_colony colony_idx, game::colony_product product_id);

    int16_t __cdecl Calculate_Current_Production_Turn_Count(game::id_colony colony_idx);

    void __cdecl Clear_Colony_Buidlings();
    void __cdecl Clear_Satellite_Array();

    void __cdecl Remove_Satellite_From_Array(int16_t building_id);

    int16_t __cdecl N_Houses_In_Bldg_Array();

    void __cdecl Find_Replacement_Slot_For_Building(int16_t bldg_id, int16_t* out_row, int16_t* out_col);

    int16_t __cdecl CR_To_XY(int16_t col, int16_t row, int16_t axis);

    void __cdecl Insert_Bldg_Into_Array(game::id_colony colony, int16_t bldg_id, int16_t bldg_type, int8_t use_back_to_front);

    void __cdecl Sort_Bldg_Array_Columns();

    bool __cdecl Get_Bldg_CR(int16_t building_id, int16_t* row_out, int16_t* col_out);

    void __cdecl Turn_To_Rubble_CR_Bldg(int16_t building_id);

    void __cdecl Bldg_Back_To_Front(int16_t building_type, int16_t* out_val1, int16_t* out_val2);

    void __cdecl Draw_Colony_Bldg(int16_t col, int16_t row);

    void __cdecl Draw_Colony_Bldgs();

    void __cdecl Draw_Colony_Satellites();

    void __cdecl Draw_Colony_Info_Background();

    void __cdecl Box_Galaxy_Map_Star(int16_t star_idx, int16_t pulse_style);

    void __cdecl Print_Sys_Disp_Scanned_String(char* text);

    void __cdecl Print_Scanned_String(char* text);

    void __cdecl Print_Building_Name(int16_t row, int16_t col);

    void __cdecl Print_Scanned_Military();

    void __cdecl Print_Scanned_Satellite();

    void __cdecl Draw_Colony_Info();

    void __cdecl Draw_Colony_Info_Military();

    int32_t __cdecl Draw_Colony_Info_Military_For(int32_t military_index, int32_t start_x, int32_t drawn_count);

    void __cdecl Draw_Colony_Info_Production_For(int32_t selection);

    int32_t __cdecl Do_Colony_Info_Military_Stuff_For(int32_t military_index, int32_t start_x, int32_t drawn_count, int32_t mode);

    void __cdecl Set_Colony_Font_To(int32_t style);

    void __cdecl Draw_Colony_Info_Officer();

    void __cdecl Draw_Colony_Info_Production();

    void __cdecl Draw_Colony_Info_Pop_For(int32_t type);

    void __cdecl Draw_Colony_Info_Pop_Scanned();

    void __cdecl Draw_Colony_Info_Pop();

    void __cdecl Draw_Colony_Info_Morale();

    void __cdecl Draw_Info_Name_And_Pop();

    void __cdecl Draw_Info_Build();

    void __cdecl Draw_Current_Production_Graph();

    void __cdecl Draw_Turn_Count();

    void __cdecl Draw_Sys_Disp_Scanned();

    void __cdecl Draw_Colony_Info_Scanned();

    void __cdecl No_Op1();

    void __cdecl Allocate_Mini_Planet_Cache();

    void __cdecl Draw_Colony_Screen();

    void __cdecl Add_Job_Field_For(int16_t index);

    void __cdecl Add_Move_Colonist_Job_Field_For(int16_t index);

    int32_t __cdecl Do_Colony_Info_Military_Stuff_For_Aux(int32_t military_index, int32_t prev_ret, int32_t drawn_count);

    void __cdecl Add_Job_Fields();

    void __cdecl Add_Military_Fields();

    void __cdecl Add_Screen_Fields();

    void __cdecl Add_Stacking_Fields();

    void __cdecl Add_Satellite_Fields();

    void __cdecl Add_Bldg_Fields();

    int32_t __cdecl Bldg_Coords_To_Centered_Screen_Coord(int32_t grid_y, int32_t grid_x, int32_t axis);

    void __cdecl Update_Fields();

    bool __cdecl Prompt_For_Autobuild_Queue(game::id_colony colony_idx);

    void __cdecl Do_Informational_And_Decision_Popups();

    bool __cdecl Colony_Has_Natives(game::id_colony colony_idx);

    /** Returns whether a pending occupation policy has eligible populations. */
    bool __cdecl Colony_Needs_Occupation_Policy(game::id_colony colony_idx);

    /** Returns whether an occupation policy may annihilate a population unit. */
    bool __cdecl Colony_Pop_Is_Annihilation_Target(const s_colony* colony, uint32_t pop);

    int8_t __cdecl Evaluate_Input(int16_t* input_field, int16_t* scan_field);

    uint32_t __cdecl Check_System_Display_Fields(int16_t* input_field, int16_t* scan_field);

    void __cdecl Do_Cheats();

    void __cdecl Show_Morale(game::id_colony colony_idx);

    void __cdecl Show_BC_Production(game::id_colony colony_idx);

    void __cdecl Show_Food_Production(game::id_colony colony_idx);

    void __cdecl Set_Colony_Screen_To_New_Colony(game::id_colony colony_idx);

    int8_t __cdecl Evaluate_Screen_Input(int16_t* input_field);

    void __cdecl Check_Job_Fields(int16_t* p_clicked_id, int16_t* p_target_id);

    void __cdecl Pickup_Colonists(int16_t colonists);

    int16_t __cdecl Get_Scanned_Pop(int16_t field_idx);

    int16_t __cdecl Get_Selected_Pop(int16_t job_type);

    void __cdecl Check_Military_Fields(int16_t* input_field, int16_t* scan_field);

    void __cdecl Check_Satellite_Fields(int16_t* input_field, int16_t* scan_field);

    void __cdecl Check_Bldg_Fields(int16_t* input_field, int16_t* scan_field);

    void __cdecl Fix_Colony_Queue(game::id_colony colony_idx);

    void __cdecl Squish_Out_Colony_Queue_Slot(game::id_colony colony_idx, int16_t slot_idx);

    void __cdecl Defragment_Colony_Queue(game::id_colony colony_idx);

    void __cdecl Cleanup_Colony_Queue(game::id_colony colony_idx);

    void __cdecl Col_Calc_Wrapper(game::id_colony colony_idx);

    void __cdecl Reset_Field_Arrays();

    void __cdecl Clear_Sys_Disp_Fields();

    int16_t __cdecl Sum_Colonists(game::id_colony colony_idx, int16_t job, int16_t state, int16_t race, int16_t moved_flag, int16_t conquered_flag);

    void __cdecl Reset_Field_Mode();

    void __cdecl Drop_Military();

    void __cdecl Clear_Move_Military();

    void __cdecl Show_Colony_Screen();

    void __cdecl Restore_Control();

    void __cdecl Draw_Current_Building2(int16_t x, int16_t y, int16_t building_id);
    void __cdecl Draw_Current_Building(int16_t building_id);
    void __cdecl Draw_Current_Ship(int16_t ship_id);

    extern game::id_colony g_colony_n;
    extern s_colors bright_blue_font_color_array;
    extern s_colors font_color_array;
    extern s_colors font_dim_color_array;
    extern s_colors font_bright_color_array;
    extern s_colors red_font_color_array[3];

    void __cdecl Set_Colony_Font_To_Blue(int16_t style, int8_t is_bright);

    void __cdecl Set_Colony_Font();

    void __cdecl Set_Colony_Font_To_Reg_Blue(uint8_t font_idx, uint8_t state);

    extern int16_t galaxy_map_name_field;

    void __cdecl Clear_Galaxy_Map_Fields();

    extern int16_t g_count;

    void __cdecl Draw_Planet_Summary(int16_t x, int16_t y, game::id_planet planet_id);

    bool __cdecl Colony_Can_Buy_Product_0(game::id_colony colony_idx);

    void __cdecl Tested_Colony_Buys_Outright(game::id_colony colony_idx);

    s_animation_header* __cdecl Mini_Planet_Anim(game::id_planet planet_id);

    int16_t __cdecl Bldg_Coords_To_Effective_Frame(int16_t x, int32_t y);

    char* __cdecl Colony_Type(game::id_colony colony_idx);

    void __cdecl Set_Colony_Font_To_Red(uint8_t style, uint8_t color_idx);

    /**
     * Finds the next non-outpost colony owned by the same player.
     * @param current_idx Current colony index.
     * @param direction 0 searches forward, 1 searches backward.
     */
    game::id_colony __cdecl Get_Next_Colony(game::id_colony current_idx, int16_t direction);

    void __cdecl Set_Last_Colony_Selected(game::id_colony colony_idx, game::id_player player_idx);

    void __cdecl Reset_Step_Squish();

    int16_t __cdecl N_Bldgs(game::id_colony colony_idx);

    void __cdecl Draw_Pop_Dark(int16_t x, int16_t y, const s_colony& colony, int16_t pop_idx);

    void __cdecl Create_Shadow_Farmer(int16_t unused);

    s_animation_header* __cdecl Shadow_Farmer_Bitm();

    void __cdecl Set_Colony_Font_To_Player(uint8_t font_style, game::id_player player_idx);

    extern s_colors player_font_color_array[MAX_PLAYERS];
} // namespace COLONY
