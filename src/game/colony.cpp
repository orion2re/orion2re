#include "pch.h"

namespace COLONY {
    int32_t building_cr[170] = {
        316, 492, 225, 461, 153, 428, 96, 401, 50, 379, 10, 361, -21, 348, 406, 461, 316, 430, 242, 404, 182, 381, 130, 363, 88, 346, 53, 333, 480, 430, 391, 403, 317, 381, 255, 362, 204, 348, 158, 334, 121, 322, 539, 403, 454, 382, 380, 363, 319, 348, 264, 335, 219, 322, 180, 312, 587, 382, 505, 363, 434, 348, 372, 334, 318, 323, 271, 312, 232, 303, 629, 364, 549, 349, 479, 335, 419, 323, 367, 314, 318, 303, 277, 295, 666, 351, 586, 336, 517, 324, 459, 313, 409, 304, 360, 295, 320, 288, 316, 430, 242, 404, 182, 381, 130, 363, 88, 346, 53, 333, 391, 403, 317, 381, 255, 362, 204, 348, 158, 334, 121, 322, 454, 382, 380, 363, 319, 348, 264, 335, 219, 322, 180, 312, 505, 363, 434, 348, 372, 334, 318, 323, 271, 312, 232, 303, 549, 349, 479, 335, 419, 323, 367, 314, 318, 323, 277, 295, 586, 336, 517, 324, 517, 313, 409, 304, 360, 295, 320, 288};

    int16_t colony_fields[21] = {
        -1000, -1000, -1000, -1000, -1000, -1000, -1000, -1000};

    int16_t military_fields[2] = {
        -1000, -1000};

    int16_t job_fields[3];

    int16_t pop_fields_count[8];

    int16_t sys_disp_summ_fields[5];

    int16_t sys_disp_fields[3];

    int16_t sys_disp_planet_fields[5];

    int16_t pop_fields[42];

    s_colony_bldg colony_bldgs[6];

    int16_t colony_satellites[10];

    char FIREPTS_LBX[12] = "FIREPTS.LBX";
    const char PLANETS_LBX[12] = "PLANETS.LBX";
    const char AMEBAFIN_LBX[13] = "AMEBAFIN.LBX";
    const char COLBLDG_LBX[12] = "COLBLDG.LBX";
    const char COLSUM_LBX[11] = "COLSUM.LBX";
    const char COLSYSDI_LBX[13] = "COLSYSDI.LBX";
    const char DIMTVFIN_LBX[13] = "DIMTVFIN.LBX";
    const char NEWGAME_LBX[12] = "NEWGAME.LBX";
    const char PLNTDFIN_LBX[13] = "PLNTDFIN.LBX";
    const char MAINPUPS_LBX[13] = "MAINPUPS.LBX";
    const char BEAMS_LBX[10] = "BEAMS.LBX";
    const char CMBTSFX_LBX[12] = "CMBTSFX.LBX";
    const char SPHERSFX_LBX[13] = "SPHERSFX.LBX";
    char COLROADS_LBX[13] = "COLROADS.LBX";

    int16_t step_squish = 0;
    int16_t scanned_pop = -1;
    int16_t scanned_military = -1;
    int16_t scanned_satellite = -1;

    int16_t colony_satellite_fields[10];

    int16_t colony_saved_screen;
    int16_t g_scanned_star;
    int16_t g_scanned_planet;
    int16_t planet_display_handle = -1;

    uint16_t colonists_count;

    int16_t orbit_temp;
    int16_t temp_colony_idx;

    int16_t ship_bitm_n = -1;
    int16_t bldg_bitm_n = -1;
    int16_t g_ship_pic_n = -1;
    int32_t g_ship_player_n = -1;
    s_animation_header* ship_pic_bitm;
    s_animation_header* ship_bitm;
    s_animation_header* swap_bitm;

    s_animation_header* mouse_bitm;

    const char COLONY_LBX[11] = "COLONY.LBX";
    const char COLVEGGIE_LBX[13] = "COLVEGGI.LBX";
    const char COLGCBT_LBX[12] = "COLGCBT.LBX";
    const char COLONY2_LBX[12] = "COLONY2.LBX";
    const char COLPUPS_LBX[12] = "COLPUPS.LBX";

    s_animation_header* c_anims[36];

    uint8_t debug_flag;
    uint8_t using_colony_screen_palette;
    uint8_t drawing_display;
    uint8_t valid_colony = 1;
    uint8_t field_mode;
    int16_t move_colonist = -1;

    int16_t scanned_bldg_C = -1;
    int16_t scanned_bldg_R = -1;
    uint8_t full_draw = 1;
    int16_t colony_handle;
    int16_t building_cr_xy[7][7][4] = {
        316, 0, 492, 0, 225, 0, 461, 0, 153, 0, 428, 0, 96, 0, 401, 0, 50, 0, 379, 0, 10, 0, 361, 0, -21, -1, 348, 0, 406, 0, 461, 0, 316, 0, 430, 0, 242, 0, 404, 0, 182, 0, 381, 0, 130, 0, 363, 0, 88, 0, 346, 0, 53, 0, 333, 0, 480, 0, 430, 0, 391, 0, 403, 0, 317, 0, 381, 0, 255, 0, 362, 0, 204, 0, 348, 0, 158, 0, 334, 0, 121, 0, 322, 0, 539, 0, 403, 0, 454, 0, 382, 0, 380, 0, 363, 0, 319, 0, 348, 0, 264, 0, 335, 0, 219, 0, 322, 0, 180, 0, 312, 0, 587, 0, 382, 0, 505, 0, 363, 0, 434, 0, 348, 0, 372, 0, 334, 0, 318, 0, 323, 0, 271, 0, 312, 0, 232, 0, 303, 0, 629, 0, 364, 0, 549, 0, 349, 0, 479, 0, 335, 0, 419, 0, 323, 0, 367, 0, 314, 0, 318, 0, 303, 0, 277, 0, 295, 0, 666, 0, 351, 0, 586, 0, 336, 0, 517, 0, 324, 0, 459, 0, 313, 0, 409, 0, 304, 0, 360, 0, 295, 0, 320, 0, 288, 0};

    int16_t colony_bldg_fields[36];

    int16_t pop_fields_coord_var[8];

    int16_t pop_fields_ref[3][42];

    int16_t planet_handle;
    int16_t star_handle;
    int32_t orbit_n;
    uint8_t cheats;
    int16_t sys_disp_star_handle;
    int16_t sys_disp_scanned_star_handle;

    int16_t g_colony_n = -1;
    s_colors bright_blue_font_color_array = {{228, 229, 229, 0, 0, 0, 0, 0}};
    s_colors font_color_array = {{245, 253, 252, 0, 0, 0, 0, 0}};
    s_colors font_dim_color_array = {{244, 249, 248, 0, 0, 0, 0, 0}};
    s_colors font_bright_color_array = {{244, 133, 132, 0, 0, 0, 0, 0}};

    s_colors red_font_color_array[3] = {
        {{236, 238, 237, 0, 0, 0, 0, 0}},
        {{236, 239, 238, 0, 0, 0, 0, 0}},
        {{236, 238, 237, 0, 0, 0, 0, 0}},
    };

    int16_t galaxy_map_name_field;

    int16_t g_count;

    s_colors player_font_color_array[MAX_PLAYERS] = {
        {{236, 238, 237, 0, 0, 0, 0, 0}},
        {{183, 187, 185, 0, 0, 0, 0, 0}},
        {{232, 234, 233, 0, 0, 0, 0, 0}},
        {{83, 102, 100, 0, 0, 0, 0, 0}},
        {{245, 251, 249, 0, 0, 0, 0, 0}},
        {{154, 156, 155, 0, 0, 0, 0, 0}},
        {{240, 242, 241, 0, 0, 0, 0, 0}},
        {{154, 156, 155, 0, 0, 0, 0, 0}}};

    void __cdecl Init_Colonies() {
        return;
    }

    void __cdecl No_Op1() {
        return;
    }

    char* __cdecl CR_Building_Name(int16_t building_id) {
        if (building_id == BUILDING_ARTEMIS_SYSTEM_NET || building_id == BUILDING_DIMENSIONAL_PORTAL ||
            building_id == BUILDING_STAR_BASE || building_id == BUILDING_STAR_FORTRESS) {
            return ESTRINGS::E_Strings(ESTR_HOUSING);
        }

        if (building_id == BUILDING_BATTLESTATION) {
            return ESTRINGS::E_Strings(ESTR_RUBBLE);
        }

        return Real_Building_Name(building_id);
    }

    char* __cdecl Real_Building_Name(int16_t building_id) {
        return TECHDATA::buildings[building_id].name;
    }

    char* __cdecl Real_Building_Name2(int16_t building_id) {
        return TECHDATA::buildings[building_id].name;
    }

    s_animation_header* __cdecl Buffer_Reload_Planets(int16_t entry_num) {
        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
            COLONY::PLANETS_LBX,
            static_cast<int>(entry_num),
            MOX::global_cache_seg));
    }

    s_animation_header* __cdecl Cache_Load_Planet(int16_t planet_id) {
        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(PLANETS_LBX, planet_id, MOX::global_cache_seg));
    }

    s_animation_header* __cdecl Buffer_Reload_Colony(int16_t index) {
        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
            COLONY_LBX,
            static_cast<int32_t>(index) + 9,
            MOX::global_cache_seg));
    }

    int16_t __cdecl Satellite_Anim_Pic(int16_t id) {
        switch (id) {
            case BUILDING_ARTEMIS_SYSTEM_NET:
                return 7;
            case BUILDING_BATTLESTATION:
                return 1;
            case BUILDING_DIMENSIONAL_PORTAL:
                return 3;
            case BUILDING_STAR_BASE:
                return 2;
            case BUILDING_STAR_FORTRESS:
                return 0;
            default:
                return -1;
        }
    }

    void __cdecl Update_Colony_Palette() {
        fonts::Load_Palette(1, 0, 255);
        s_animation_header* colony_anims = C_Anims(0);
        animate::Draw_Palette(colony_anims);
        remap::Update_Glass_Remap_Colors();
    }

    s_animation_header* __cdecl Spy_Anim(int16_t race_index) {
        int16_t entry_index = race_index * 13 + 11;

        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
            RACEICON_LBX,
            static_cast<int32_t>(entry_index),
            MOX::global_cache_seg));
    }

    bool __cdecl Sat_Should_Be_Replaced(int16_t building_id) {
        const s_colony& colony = MOX::colony[COLONY::colony_handle];

        if (building_id == BUILDING_STAR_BASE) {
            // A Star Base is redundant if either a Battlestation or a Star Fortress exists.
            if (colony.buildings[BUILDING_BATTLESTATION] != 0 ||
                colony.buildings[BUILDING_STAR_FORTRESS] != 0) {
                return true;
            }
        } else if (building_id == BUILDING_BATTLESTATION) {
            // A Battlestation is redundant if a Star Fortress exists.
            if (colony.buildings[BUILDING_STAR_FORTRESS] != 0) {
                return true;
            }
        }

        return false;
    }

    void __cdecl Insert_Satellite_Into_Array(int16_t satellite_id, int16_t* count) {
        if (!Sat_Should_Be_Replaced(satellite_id)) {
            colony_satellites[*count] = satellite_id;
            (*count)++;
        }
    }

    void __cdecl Clear_Colony_Buidlings() {
        memset(colony_bldgs, 0, sizeof(colony_bldgs));
    }

    void __cdecl Clear_Satellite_Array() {
        memset(colony_satellites, 0xff, sizeof(colony_satellites));
    }

    void __cdecl Find_Replacement_Slot_For_Building(int16_t bldg_id, int16_t* out_row, int16_t* out_col) {
        int16_t min_cost = 1000;

        if (bldg_id >= 0) {
            *out_row = 0;
            *out_col = 0;

            for (int16_t i = 0; i < 6; ++i) {
                for (int16_t j = 0; j < 6; ++j) {
                    int16_t current_bldg_id = static_cast<int16_t>(COLONY::colony_bldgs[i].buildings[j]);

                    if (current_bldg_id == BUILDING_ARTEMIS_SYSTEM_NET ||
                        current_bldg_id == BUILDING_DIMENSIONAL_PORTAL ||
                        current_bldg_id == BUILDING_STAR_BASE ||
                        current_bldg_id == BUILDING_STAR_FORTRESS) {
                        min_cost = 0;
                        *out_row = i;
                        *out_col = j;
                    } else {
                        int32_t current_cost = TECHDATA::buildings[current_bldg_id].cost;

                        if (current_cost < static_cast<int32_t>(min_cost)) {
                            *out_row = i;
                            *out_col = j;
                            min_cost = static_cast<int16_t>(current_cost);
                        }
                    }
                }
            }
        }
    }

    void __cdecl Load_Bldg_Bitm() {
        COLBLDG::bldg_bitm = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x3860);
        bldg_bitm_n = -1;
    }

    void __cdecl Load_Ship_Bitm() {
        ship_bitm = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0xE30);
        ship_pic_bitm = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0xE30);

        ship_bitm_n = -1;
        g_ship_pic_n = -1;
        g_ship_player_n = -1;
    }

    void __cdecl Load_Galaxy_Map_Anims() {
        int entry_num = 23;
        void* screen_seg = MOX::screen_seg;

        for (int i = 0; i < 10; ++i) {
            MOX::colony_galaxy_star_seg[i] = (s_animation_header*)farload::Far_Reload_Next("gstar.lbx", entry_num, screen_seg);
            entry_num++;

            screen_seg = MOX::screen_seg;
        }
    }

    void __cdecl Load_Planet_Screen() {
        return;
    }

    s_animation_header* __cdecl Prod_Anims(int16_t production_type, int16_t variant, int8_t frame) {
        int32_t index = (int32_t)production_type + (int32_t)variant * 4;

        if (frame != 0) {
            index += 27;
        }

        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(COLONY2_LBX, index, MOX::global_cache_seg));
    }

    void __cdecl Reset_Mouse_Picture() {
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        fields::Enable_Draw_All_Fields();
    }

    void __cdecl Clear_Mouse_Picture() {
        bitmap::Clear_Mouse_Image(0x13);
        mouse::Set_Mouse_List(&MOX::mouse_list_variable, 1);

        if (COLMOVE::cluster_colony_n != -1) {
            int16_t first_unemployed = COLMOVE::Get_First_Unemployed(COLMOVE::cluster_colony_n);
            if (first_unemployed != -1) {
                fields::Disable_Draw_All_Fields();
            }
        }
    }

    void __cdecl Remap_Draw_Ship_D_Centered(int16_t x, int16_t y, int16_t design_index) {
        int16_t picture_num = static_cast<int16_t>(MOX::player[MOX::PLAYER_NUM].ship_designs[design_index].picture_num);

        Remap_Draw_Ship_Seg_N_Centered(x, y, picture_num);
    }

    bool __cdecl Get_Bldg_CR(int16_t building_id, int16_t* row_out, int16_t* col_out) {
        int16_t empty_count = 0;
        *row_out = 0;
        *col_out = 0;

        for (int16_t i = 0; i < 6; ++i) {
            for (int16_t j = 0; j < 6; ++j) {
                int32_t current_id = colony_bldgs[i].buildings[j];

                if (current_id == (int32_t)building_id) {
                    *row_out = i;
                    *col_out = j;
                    return true;
                }

                if (current_id == 0) {
                    empty_count++;
                    if (game_random::Random((int32_t)empty_count) == 1) {
                        *row_out = i;
                        *col_out = j;
                    }
                }
            }
        }

        return building_id == -1;
    }

    /**
     * @brief Marks a specific colony building as rubble.
     * @param building_id The identifier of the building to destroy.
     */
    void __cdecl Turn_To_Rubble_CR_Bldg(int16_t building_id) {
        int16_t row;
        int16_t col;

        if (Get_Bldg_CR(building_id, &row, &col)) {
            full_draw = 1;
            // Value 8 represents the 'Rubble' state for a building
            colony_bldgs[row].buildings[col] = 8;
        }
    }

    int16_t __cdecl Calculate_Colony_Turn_Count_From_Scrap_For_Prod(int16_t colony_idx, int16_t product_id) {
        s_colony* colony = &MOX::colony[colony_idx];
        int16_t remaining_cost = COLCALC::Colony_Product_Cost(colony, product_id);
        int16_t net_production = colony->production[ECON_INDUSTRY];

        if (net_production >= remaining_cost) {
            return 1;
        }

        if (net_production <= 0) {
            return 10000;
        }

        if (net_production < 2) {
            net_production = 1;
        }

        return (remaining_cost / net_production) + 1;
    }

    int16_t __cdecl Calculate_Current_Production_Turn_Count(int16_t colony_idx) {
        int16_t turns = COLCALC::Colony_N_Turns_To_Produce(&MOX::colony[colony_idx]);

        if (MOX::colony[colony_idx].bought_outright != 0) {
            return 1;
        }

        if (turns == -1) {
            return 10000;
        }

        return turns;
    }

    void __cdecl Remap_Draw_Ship_Centered(int16_t x, int16_t y, int16_t ship_index) {
        uint8_t palette_buffer[31];

        for (int i = 0; i < 31; ++i) {
            palette_buffer[i] = 0x51 + i;
        }

        s_ship_data* ship = &MOX::ship[ship_index];
        int16_t picture_num = (int16_t)ship->d.picture_num;
        int16_t previous_owner = (int16_t)(int8_t)ship->d.previous_owner;

        if (picture_num != ship_bitm_n || (int32_t)previous_owner != g_ship_player_n) {
            ship_bitm_n = picture_num;
            g_ship_player_n = (int32_t)previous_owner;

            s_animation_header* picture_seg = KEN::Get_Ship_Picture_Seg(previous_owner, picture_num);
            bitmap::Draw_Picture_To_Bitmap(ship_bitm, picture_seg);

            replace::Replace_Color_Range(ship_bitm, palette_buffer, 1, 31);

            for (int16_t i = 0; i < 16; ++i) {
                uint8_t replacement_color = palette_buffer[i * 2];
                replace::Replace_Color(ship_bitm, i + 0xC0, (int32_t)replacement_color);
                replace::Replace_Color(ship_bitm, i + 0xD0, (int32_t)replacement_color);
            }
        }

        ERIC::Draw_Centered(x, y, ship_bitm);
    }

    int16_t __cdecl N_Houses_In_Bldg_Array() {
        int16_t house_count = 0;

        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                int32_t building_id = colony_bldgs[i].buildings[j];

                if (building_id == BUILDING_ARTEMIS_SYSTEM_NET ||
                    building_id == BUILDING_DIMENSIONAL_PORTAL ||
                    building_id == BUILDING_STAR_BASE ||
                    building_id == BUILDING_STAR_FORTRESS) {
                    house_count++;
                }
            }
        }

        return house_count;
    }

    void __cdecl Bldg_Back_To_Front(int16_t building_type, int16_t* out_val1, int16_t* out_val2) {
        const uint8_t lookup_table[72] = {
            5, 5, 5, 4, 4, 5, 5, 3, 3, 5, 4, 4, 5, 2, 2, 5, 4, 3, 3, 4, 5, 1, 1, 5, 4, 2, 2, 4, 3, 3, 5, 0, 0, 5, 4, 1, 1, 4, 2, 3, 3, 2, 4, 0, 0, 4, 3, 1, 1, 3, 2, 2, 3, 0, 0, 3, 2, 1, 1, 2, 2, 0, 0, 2, 1, 1, 1, 0, 0, 1, 0, 0};

        *out_val1 = lookup_table[building_type * 2];
        *out_val2 = lookup_table[building_type * 2 + 1];
    }

    void __cdecl Allocate_Mini_Planet_Cache() {
        return;
    }

    void __cdecl Clear_Galaxy_Map_Fields() {
        galaxy_map_name_field = -1000;
        MOX::galaxy_map_grid_field = -1000;

        for (int i = 0; i < MAX_STARS; ++i) {
            MOX::galaxy_map_star_field[i] = -1000;
        }
    }

    void __cdecl Set_Colony_Font_To_Blue(int16_t style, int8_t is_bright) {
        fonts::Set_Outline_Color(0x50, 0x50);

        s_colors* primary_colors;
        if (is_bright == 0) {
            primary_colors = &font_color_array;
        } else {
            primary_colors = &font_bright_color_array;
        }

        fonts::Set_Font_Style_Shadow_Down(style & 0xFF, primary_colors);
        fonts::Set_Font_Highlight_Colors(&bright_blue_font_color_array);
        fonts::Set_Font_Special_Colors(&red_font_color_array[(uint8_t)is_bright]);
    }

    void __cdecl Set_Colony_Font() {
        fonts::Set_Outline_Color(0x50, 0x50);
        Set_Colony_Font_To(3);
    }

    void __cdecl Clear_Sys_Disp_Fields() {
        for (int i = 0; i < 5; ++i) {
            sys_disp_planet_fields[i] = -1000;
        }

        for (int i = 0; i < 3; ++i) {
            sys_disp_fields[i] = -1000;
        }
    }

    void __cdecl Reset_Field_Arrays() {
        COLONY::Clear_Sys_Disp_Fields();

        for (int i = 0; i < 5; ++i) {
            COLONY::sys_disp_summ_fields[i] = -1000;
        }

        for (int i = 0; i < 21; ++i) {
            COLONY::colony_fields[i] = -1000;
        }

        for (int i = 0; i < 2; ++i) {
            COLONY::military_fields[i] = -1000;
        }

        for (int i = 0; i < 42; ++i) {
            COLONY::pop_fields[i] = -1000;
        }

        for (int i = 0; i < 3; ++i) {
            COLONY::job_fields[i] = -1000;
        }
    }

    bool __cdecl Colonize_Planet(int16_t planet_idx) {
        if (MOX::planet[planet_idx].colony_index == -1) {
            int16_t colony_idx = MOX::NUM_COLONIES;
            MOX::NUM_COLONIES++;

            INITGAME::Init_Colony(colony_idx, planet_idx, (uint8_t)MOX::PLAYER_NUM);

            return true;
        }

        return false;
    }

    bool __cdecl Colony_Can_Buy_Product_0(int16_t colony_idx) {
        if (COLCALC::Colony_Can_Afford_To_Buy_Product(colony_idx)) {
            s_colony& colony = MOX::colony[colony_idx];

            if (colony.bought_outright == 0) {
                int16_t planet_idx = colony.planet;
                int16_t star_idx = MOX::planet[planet_idx].star_index;
                s_star_data& star = MOX::star[star_idx];

                if (!struct_::Test_Bit_Field(&star.blockaded, MOX::PLAYER_NUM)) {
                    int32_t cost = COLCALC::Colony_Cost_To_Buy_Producing_Product(&colony);
                    if (cost > 0) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void __cdecl Draw_Colony_Info_Background() {
        if (full_draw != 0) {
            graphics::Darken_Fill(0, 0, 640, 150);

            int16_t unemployed_idx = COLMOVE::Get_First_Unemployed(colony_handle);

            if (unemployed_idx == -1) {
                s_animation_header* anim = C_Anims(3);
                animate::Draw(0, 0, anim);
            } else {
                s_animation_header* anim = C_Anims(4);
                animate::Draw(0, 0, anim);
            }
        }
    }

    void __cdecl Draw_Colony_Satellites() {
        if (HAROLD::Was_Mouse_List_Saved()) {
            return;
        }

        for (int16_t i = 0; i < 10; ++i) {
            int16_t satellite_id = colony_satellites[i];
            if (satellite_id == -1) {
                continue;
            }

            // The X coordinate oscillates around center (295) based on index parity
            // i=0: 295, i=1: 245, i=2: 395, i=3: 145, i=4: 495, etc.
            int16_t multiplier = (i % 2 == 0) ? 1 : -1;
            int16_t x = (multiplier * i * 50) + 295;
            int16_t y = 162;

            if (satellite_id == scanned_satellite && ERIC::Global_Cycler(30, 2) != 0) {
                Draw_Satellite_Dark(x, y, satellite_id);
            } else {
                s_animation_header* anim = Satellite_Anim(satellite_id);
                animate::Draw(x, y, anim);
            }
        }
    }

    void __cdecl Box_Galaxy_Map_Star(int16_t star_idx, int16_t pulse_style) {
        (void)pulse_style;

        int16_t x;
        int16_t y;

        MOVEBOX::Get_Galaxy_Map_Star_XY(
            star_idx,
            7,
            24,
            MOX::galaxy_map_height,
            MOX::galaxy_map_width,
            11,
            31,
            0,
            &x,
            &y);

        ERIC::Pulsing_Box(x, y, 5, 5, 1, 1, 2);
        timer::Release_Time(2);
    }

    int32_t __cdecl Draw_Colony_Info_Military_For(int32_t military_index, int32_t start_x, int32_t drawn_count) {
        return COLONY::Do_Colony_Info_Military_Stuff_For(military_index, start_x, drawn_count, 0);
    }

    void __cdecl Draw_Colony_Info_Military() {
        int32_t last_result = 0;
        int32_t counter = 0;
        int32_t max_val = 0;
        int32_t i = 0;

        do {
            last_result = Draw_Colony_Info_Military_For(i, last_result, counter);
            if (max_val < last_result) {
                max_val = last_result;
                counter++;
            }
            i++;
        } while (i < 2);
    }

    void Draw_Colony_Info_Officer() {
        int8_t officer_idx = MOX::star[COLONY::star_handle].officer_index[MOX::PLAYER_NUM];

        if (officer_idx != -1) {
            MOX::main_screen_seg = reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload("BUFFER0.LBX", 0, MOX::global_cache_seg));
            remap::Calculate_Picture_Remap_Colors(MOX::main_screen_seg);

            s_animation_header* anim1 = C_Anims(33);
            animate::Draw(544, 351, anim1);

            s_animation_header* anim2 = C_Anims(30);
            animate::Draw(550, 357, anim2);

            Set_Colony_Font_To(2);

            s_leader_data& leader = MOX::leaders[officer_idx];

            if (leader.eta > 0) {
                char local_buffer[500];
                char* format = ESTRINGS::E_Strings(285);
                snprintf(local_buffer, sizeof(local_buffer), format, (int)leader.eta);
                fonts::Print_Centered(586, 433, local_buffer);
                return;
            } else {
                graphics::Darken_Fill(550, 433, 623, 442);
                fonts::Print_Centered(586, 433, leader.name);
            }
        }
    }

    void __cdecl Draw_Colony_Info_Production() {
        Draw_Colony_Info_Production_For(ECON_BC);
        Draw_Colony_Info_Production_For(ECON_FOOD);
        Draw_Colony_Info_Production_For(ECON_INDUSTRY);
        Draw_Colony_Info_Production_For(ECON_RESEARCH);
    }

    void __cdecl Draw_Colony_Info_Production_For(int32_t selection) {
        int16_t row_index;
        int16_t row_adjust;

        if (selection == ECON_BC) {
            row_index = -1;
            row_adjust = -3;
        } else {
            row_index = (int16_t)selection;
            row_adjust = -1;
        }

        int16_t y_offset = (row_index * 0x1e) + row_adjust + 0x41;

        COLDRAW::Draw_Colony_Prod(
            colony_handle,
            selection,
            128,
            y_offset,
            301,
            20);
    }

    void __cdecl Draw_Colony_Info_Pop() {
        Draw_Colony_Info_Pop_For(0);
        Draw_Colony_Info_Pop_For(1);
        Draw_Colony_Info_Pop_For(2);
        Draw_Colony_Info_Pop_Scanned();
    }

    void __cdecl Draw_Current_Building(int16_t building_id) {
        Draw_Current_Building2(0x242, 0x94, building_id);
    }

    void __cdecl Draw_Current_Ship(int16_t ship_id) {
        graphics::Set_Window(517, 17, 639, 158);
        video::Clip_On();
        Remap_Draw_Ship_Centered(578, 77, ship_id);
        video::Clip_Off();
    }

    void __cdecl Draw_Sys_Disp_Scanned() {
        if (drawing_display == 0) {
            COLSYSDI::Draw_Col_Sys_Disp_Last_Scanned(7, 24);
        } else if (drawing_display == 1) {
            if (g_scanned_star != -1) {
                s_star_data* star = &MOX::star[g_scanned_star];
                Print_Sys_Disp_Scanned_String(star->name);

                Box_Galaxy_Map_Star(g_scanned_star, 1);
            }
        }
    }

    void __cdecl Print_Building_Name(int16_t row, int16_t col) {
        if (row != -1 && col != -1) {
            int32_t building_id = colony_bldgs[row].buildings[col];

            char* name = CR_Building_Name(static_cast<int16_t>(building_id));
            Print_Scanned_String(name);
        }
    }

    void __cdecl Print_Scanned_String(char* text) {
        Set_Colony_Font();

        int16_t width = BILL::_Get_Paragraph_Max_Width(0x280, text, 0);
        int16_t half_width = width / 2;

        graphics::Darken_Fill(0x135 - half_width, 0x1cf, half_width + 0x149, 0x1de);

        fonts::Print_Centered(0x13f, 0x1d3, text);
    }

    void __cdecl Print_Scanned_Satellite() {
        if (scanned_satellite != -1) {
            char* building_name = Real_Building_Name2(scanned_satellite);
            Print_Scanned_String(building_name);
        }
    }

    void __cdecl Draw_Colony_Info_Scanned() {
        Print_Scanned_Military();
        Print_Scanned_Satellite();
        Print_Building_Name(scanned_bldg_C, scanned_bldg_R);
    }

    void __cdecl Draw_Satellite_Dark(int16_t x, int16_t y, int16_t satellite_id) {
        s_animation_header* satellite_anim = Satellite_Anim(satellite_id);

        bitmap::Create_Blank_Bitmap(120, 120, COLBLDG::bldg_bitm);
        bitmap::Add_Picture_To_Bitmap(0, 0, satellite_anim, (s_animation_header*)COLBLDG::bldg_bitm);
        bitmap::Mask_Out_Colors(COLBLDG::bldg_bitm, 240, 255);
        bitmap::Tint_Bitmap(COLBLDG::bldg_bitm, COLBLDG::bldg_bitm, 2);

        graphics::Set_Window(0, 0, 639, 479);
        video::Clip_On();
        animate::Draw(x, y, (s_animation_header*)COLBLDG::bldg_bitm);
        video::Clip_Off();
    }

    void __cdecl Draw_Bldg_CR_Dark(int16_t col, int16_t row) {
        int32_t bldg_id = colony_bldgs[col].buildings[row];
        s_animation_header* bldg_anim = (s_animation_header*)COLBCACH::Cache_Load_Bldg(col, row, (int16_t)bldg_id);

        int16_t x = (int16_t)CR_To_XY(col, row, 0) - 60;
        int16_t y = (int16_t)CR_To_XY(col, row, 1) - 120;

        bitmap::Create_Blank_Bitmap(120, 120, COLBLDG::bldg_bitm);
        bitmap::Add_Picture_To_Bitmap(-x, -y, bldg_anim, (s_animation_header*)COLBLDG::bldg_bitm);
        bitmap::Mask_Out_Colors(COLBLDG::bldg_bitm, 240, 255);
        bitmap::Tint_Bitmap(COLBLDG::bldg_bitm, COLBLDG::bldg_bitm, 2);

        graphics::Set_Window(0, 0, 639, 479);
        video::Clip_On();
        animate::Draw(x, y, COLBLDG::bldg_bitm);
        video::Clip_Off();
    }

    void __cdecl Print_Scanned_Military() {
        char buffer[500];

        if (COLONY::scanned_military != (int16_t)-1) {
            // Units are indexed with +1 for scanned military data
            char* unit_name = TECHDATA::units[COLONY::scanned_military + 1].name;
            int unit_count = (int)MOX::colony[COLONY::colony_handle].military[COLONY::scanned_military];

            char* format = ESTRINGS::E_Strings(14 /* ESTR_MILITARY_COUNT_FORMAT */);
            snprintf(buffer, sizeof(buffer), format, unit_count, unit_name);

            COLONY::Print_Scanned_String(buffer);
        }
    }

    char* __cdecl Job_String(int16_t job_id) {
        if (job_id == 0) {
            return ESTRINGS::E_Strings(ESTR_FARMER);
        }

        if (job_id == 1) {
            return ESTRINGS::E_Strings(ESTR_WORKER);
        }

        return ESTRINGS::E_Strings(ESTR_SCIENTIST);
    }

    char* __cdecl Conquered_String(bool is_conquered) {
        if (is_conquered) {
            return ESTRINGS::E_Strings(ESTR_CONQUERED);
        }
        return ESTRINGS::E_Strings(12);
    }

    char* __cdecl Pop_State_String(int16_t state) {
        switch (state) {
            case 0:
                return ESTRINGS::E_Strings(ESTR_EXPERT);
            case 2:
            case 3:
            case 4:
                return ESTRINGS::E_Strings(12);
            default:
                // Case 1 and any value not explicitly handled above return the default string
                return ESTRINGS::E_Strings(12);
        }
    }

    void __cdecl Draw_Colony_Info_Pop_Scanned() {
        char buffer[500];

        if (COLONY::scanned_pop != -1) {
            int16_t handle_s = COLONY::colony_handle;
            uint32_t pop_data = MOX::colony[COLONY::colony_handle].pop[COLONY::scanned_pop];

            char* str_state;
            char* str_conquered;
            char* str_race;
            char* str_job;
            char* format_str;

            // Branching logic based on language affects the order of components in the final string
            if (MOX::settings.language == 4 /* Localized variant */) {
                str_state = Pop_State_String(Pop_To_Pop_State(handle_s, COLONY::scanned_pop));
                str_conquered = Conquered_String((bool)((pop_data >> 10) & 1));
                str_race = Pop_Race_String(handle_s, COLONY::scanned_pop);
                str_job = Job_String((int16_t)((pop_data >> 7) & 3));

                format_str = ESTRINGS::E_Strings(76);
                snprintf(buffer, sizeof(buffer), format_str, str_job, str_race, str_conquered, str_state);
            } else {
                str_job = Job_String((int16_t)((pop_data >> 7) & 3));
                str_state = Pop_State_String(Pop_To_Pop_State(handle_s, COLONY::scanned_pop));
                str_race = Pop_Race_String(handle_s, COLONY::scanned_pop);
                str_conquered = Conquered_String((bool)((pop_data >> 10) & 1));

                format_str = ESTRINGS::E_Strings(76);
                snprintf(buffer, sizeof(buffer), format_str, str_conquered, str_race, str_state, str_job);
            }

            Set_Colony_Font();
            Print_Scanned_String(buffer);
        }
    }

    char* __cdecl Pop_Race_String(int16_t colony_idx, int16_t pop_idx) {
        // Each population unit's race index is stored in the lower 4 bits of its data
        uint32_t race_index = MOX::colony[colony_idx].pop[pop_idx] & 0x0F;

        if (race_index == 8) {
            return ESTRINGS::E_Strings(static_cast<int16_t>(0xB3));
        }

        if (race_index == 9) {
            return ESTRINGS::E_Strings(static_cast<int16_t>(0x255));
        }

        return MOX::player[race_index].race_name;
    }

    void __cdecl Draw_Colony_Info_Morale() {
        COLDRAW::Draw_Info_Morale(COLONY::colony_handle, 310, 33, 510);
    }

    void __cdecl Draw_Current_Production_Graph() {
        s_colony* colony = &MOX::colony[COLONY::colony_handle];

        int cost = COLCALC::Colony_Producing_Product_Cost(colony);
        int spent = (int)colony->production_spent;

        if (colony->bought_outright != 0) {
            spent = cost;
        }

        COLDRAW::Draw_Colony_PC_Bar(606, 43, cost, spent);
    }

    void __cdecl Draw_Turn_Count() {
        s_colony& colony = MOX::colony[colony_handle];
        const int32_t turns = Calculate_Current_Production_Turn_Count(colony_handle);
        const int32_t product_cost = COLCALC::Colony_Producing_Product_Cost(&colony);
        const int32_t total_production =
            (int32_t)colony.production_spent + colony.bought_outright;
        const STRBUILD::s_named_argument arguments[] = {
            STRBUILD::Named_Argument("TURNS", turns),
            STRBUILD::Named_Argument("STORED_PROD", colony.production_spent),
            STRBUILD::Named_Argument("BOUGHT_PROD", colony.bought_outright),
            STRBUILD::Named_Argument("TOTAL_PROD", total_production),
            STRBUILD::Named_Argument("PRODUCT_COST", product_cost),
        };

        fonts::Set_Font_Style_Shadow_Down(0, &font_color_array);
        TEXT_FORMAT::Print(
            TEXT_FORMAT::format[TEXT_FORMAT::ROW_PROD],
            arguments,
            sizeof(arguments) / sizeof(arguments[0]));
        TEXT_FORMAT::Print(
            TEXT_FORMAT::format[TEXT_FORMAT::ROW_PROD2],
            arguments,
            sizeof(arguments) / sizeof(arguments[0]));
    }

    void __cdecl Do_Informational_And_Decision_Popups() {
        char buffer[500];
        s_colony* colony = &MOX::colony[COLONY::colony_handle];

        if (Colony_Needs_Occupation_Policy(COLONY::colony_handle)) {
            OCPOLPUP::Occupation_Policy_Popup();
            Col_Calc_Wrapper(COLONY::colony_handle);
            Show_Colony_Screen();
        }

        int16_t production_id = colony->just_produced;
        if (production_id == COLONY_PRODUCTION_NONE) {
            return;
        }

        if (production_id == COLONY_PRODUCTION_JUST_COLONIZED) {
            char* planet_name = HACCESS::Get_Planet_Name(colony->planet);
            char* format = ESTRINGS::E_Strings(ESTR_S_JUST_COLONIZED_PLACE_YOUR_ORDERS_FOR_FUTURE_PRODUCTION);
            snprintf(buffer, sizeof(buffer), format, planet_name);
            GENDRAW::Help(buffer);
            Show_Colony_Screen();
            colony->just_produced = COLONY_PRODUCTION_NONE;
            return;
        }

        if (production_id == COLONY_PRODUCTION_JUST_CONQUERED) {
            char* planet_name = HACCESS::Get_Planet_Name(colony->planet);
            char* format = ESTRINGS::E_Strings(ESTR_S_JUST_CONQUERED_PLACE_YOUR_ORDERS_FOR_FUTURE_PRODUCTION);
            snprintf(buffer, sizeof(buffer), format, planet_name);
            GENDRAW::Help(buffer);
            Show_Colony_Screen();
            colony->just_produced = COLONY_PRODUCTION_NONE;
            return;
        }

        if (production_id == COLONY_PRODUCTION_JUST_LIBERATED) {
            char* planet_name = HACCESS::Get_Planet_Name(colony->planet);
            char* format = ESTRINGS::E_Strings(ESTR_S_JUST_LIBERATED_PLACE_YOUR_ORDERS_FOR_FUTURE_PRODUCTION);
            snprintf(buffer, sizeof(buffer), format, planet_name);
            GENDRAW::Help(buffer);
            Show_Colony_Screen();
            colony->just_produced = COLONY_PRODUCTION_NONE;
            return;
        }

        char* selection_name = COLBLDG::Selection_Name(production_id);
        char* planet_name = HACCESS::Get_Planet_Name(colony->planet);
        char* format = ESTRINGS::E_Strings(ESTR_S_FINISHED_BUILDING_S_PLACE_YOUR_ORDERS_FOR_FUTURE_PRODUCTION);
        snprintf(buffer, sizeof(buffer), format, planet_name, selection_name);
        GENDRAW::Help(buffer);
        Show_Colony_Screen();
        colony->just_produced = COLONY_PRODUCTION_NONE;
    }

    void __cdecl Fix_Colony_Queue(int16_t colony_idx) {
        Defragment_Colony_Queue(colony_idx);

        while (MOX::colony[colony_idx].producing[0] == COLONY_PRODUCTION_REPEAT) {
            Squish_Out_Colony_Queue_Slot(colony_idx, 0);
        }
    }

    void __cdecl Defragment_Colony_Queue(int16_t colony_idx) {
        // Compact with a write index: squishing in place and advancing
        // skipped a NONE that shifted into the slot just checked
        // ([NONE, NONE, SHIP, ...] kept its leading NONE), stalling the
        // queue at producing[0] == NONE.
        int16_t write = 0;

        for (int16_t read = 0; read < 7; ++read) {
            int16_t production = MOX::colony[colony_idx].producing[read];

            if (production != COLONY_PRODUCTION_NONE) {
                MOX::colony[colony_idx].producing[write++] = production;
            }
        }

        while (write < 7) {
            MOX::colony[colony_idx].producing[write++] = COLONY_PRODUCTION_NONE;
        }
    }

    void __cdecl Cleanup_Colony_Queue(int16_t colony_idx) {
        COLONY::Fix_Colony_Queue(colony_idx);

        if (MOX::colony[colony_idx].producing[0] == COLONY_PRODUCTION_NONE) {
            MOX::colony[colony_idx].producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
        }
    }

    void __cdecl Squish_Out_Colony_Queue_Slot(int16_t colony_idx, int16_t slot_idx) {
        if (slot_idx < 6) {
            for (int i = (int)slot_idx; i < 6; i++) {
                MOX::colony[colony_idx].producing[i] = MOX::colony[colony_idx].producing[i + 1];
            }
        }
        MOX::colony[colony_idx].producing[6] = COLONY_PRODUCTION_NONE;
    }

    void __cdecl Col_Calc_Wrapper(int16_t colony_idx) {
        COLCALC::Colony_Calculation(colony_idx);

        if (COLONY::move_colonist != -1 && COLONY::field_mode == 1) {
            s_colony* colony = &MOX::colony[colony_idx];

            if (colony->n_pops != 0) {
                // Iterates through colony pops to find the last one without flag 0x200
                for (int16_t i = 0; i < (int16_t)(uint8_t)colony->n_pops; ++i) {
                    if ((colony->pop[i] & 0x200 /* FLAG_COLONIST_MOVING */) == 0) {
                        COLONY::move_colonist = i;
                    }
                }
            }
        }
    }

    void __cdecl Reset_Field_Mode() {
        int16_t unemployed_index = COLMOVE::Get_First_Unemployed(COLONY::colony_handle);

        if (unemployed_index != -1) {
            if (COLONY::field_mode == 1) {
                COLMOVE::Clear_Cluster();
            } else if (COLONY::field_mode == 2) {
                COLONY::Drop_Military();
            }

            COLONY::Col_Calc_Wrapper(COLONY::colony_handle);
        }

        COLONY::field_mode = 0;
    }

    void __cdecl Drop_Military() {
        char* help_text;

        help_text = ESTRINGS::E_Strings(ESTR_NO_SHIPS_IN_THIS_HALF_FINISHED_GAME_AND_NO_INTERFACE_FOR_PUTTING_THIS_GUY_ON_ONE);
        GENDRAW::Help(help_text);

        Show_Colony_Screen();

        MOX::colony[COLONY::colony_handle].military[COLONY::move_colonist]++;

        Clear_Move_Military();
        Col_Calc_Wrapper(COLONY::colony_handle);
        Reset_Field_Mode();
    }

    void __cdecl Show_Food_Production(int16_t colony_idx) {
        char buffer[3000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        COLCALC::Colony_Food_Production(&MOX::colony[colony_idx], &builder);
        TEXTBOX::Text_Box(COLCALC::food_summary_msg, buffer);
    }

    char* __cdecl Colony_Type(int16_t colony_idx) {
        int8_t specialty = MOX::colony[colony_idx].specialty;

        switch (specialty) {
            case -1:
                return ESTRINGS::E_Strings(12);

            case 0:
                return ESTRINGS::E_Strings(ESTR_AGRICULTURAL);

            case 1:
                return ESTRINGS::E_Strings(ESTR_INDUSTRIAL);

            case 2:
                return ESTRINGS::E_Strings(ESTR_RESEARCH);

            default:
                return ESTRINGS::E_Strings(ESTR_RESEARCH);
        }
    }

    void __cdecl Set_Colony_Font_To_Red(uint8_t style, uint8_t color_idx) {
        fonts::Set_Outline_Color(0x50, 0x50);

        s_colors* colors_ptr = &red_font_color_array[(uint8_t)color_idx];

        fonts::Set_Font_Style_Shadow_Down(style & 0xFF, colors_ptr);
        fonts::Set_Font_Highlight_Colors(&bright_blue_font_color_array);
        fonts::Set_Font_Special_Colors(colors_ptr);
    }

    void __cdecl Set_Colony_Font_To(int32_t style) {
        fonts::Set_Outline_Color(0x50, 0x50);

        // style & 0xff extracts the lower byte as seen in the disassembly (MOV AL, byte ptr [ESP + 0xc])
        fonts::Set_Font_Style_Shadow_Down(style & 0xFF, &COLONY::font_color_array);

        fonts::Set_Font_Highlight_Colors(&COLONY::bright_blue_font_color_array);

        // red_font_color_array is the special-colors table passed by pointer
        fonts::Set_Font_Special_Colors(COLONY::red_font_color_array);
    }

    void __cdecl Show_BC_Production(int16_t colony_idx) {
        char local_buffer[3000];
        STRBUILD::s_builder builder = STRBUILD::Init(local_buffer, sizeof(local_buffer));

        COLCALC::Colony_BC_Production(&MOX::colony[colony_idx], &builder);

        TEXTBOX::Text_Box(COLCALC::bc_summary_msg, local_buffer);
    }

    void __cdecl Show_Morale(int16_t colony_idx) {
        char buffer[2000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        s_colony* colony = &MOX::colony[colony_idx];

        COLCALC::Colony_Morale(colony, &builder);

        TEXTBOX::Text_Box(COLCALC::morale_summary_msg, buffer);
    }

    void __cdecl Remap_Draw_Ship_Seg_N_Centered(int16_t x, int16_t y, int16_t ship_picture_num) {
        uint8_t ship_palette_map[31] = {
            0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f};

        if (COLONY::ship_bitm_n != ship_picture_num) {
            COLONY::ship_bitm_n = ship_picture_num;
            COLONY::g_ship_player_n = (int32_t)MOX::PLAYER_NUM;

            s_animation_header* ship_seg = KEN::Get_Ship_Picture_Seg(MOX::PLAYER_NUM, ship_picture_num);
            bitmap::Draw_Picture_To_Bitmap(COLONY::ship_bitm, ship_seg);

            replace::Replace_Color_Range(COLONY::ship_bitm, ship_palette_map, 1, 31);
            for (int16_t i = 0; i < 16; ++i) {
                uint8_t palette_index = ship_palette_map[i * 2];
                replace::Replace_Color(COLONY::ship_bitm, i + 0xc0, palette_index);
                replace::Replace_Color(COLONY::ship_bitm, i + 0xd0, palette_index);
            }
        }

        ERIC::Draw_Centered(x, y, COLONY::ship_bitm);
    }

    int16_t __cdecl Pop_To_Pop_State(int16_t colony_idx, int16_t pop_idx) {
        // Extract the lower nibble of the population data to determine status
        uint32_t pop_status = MOX::colony[colony_idx].pop[pop_idx] & 0xf;

        if (pop_status == 9) {
            return 3;
        }

        // Logic based on assembly: if status is 8, return 4, otherwise return 2
        // Assembly uses SUB EAX, 8; CMP EAX, 1; SBB EAX, EAX; which evaluates to CF=1 only if EAX=0 (status=8)
        if (pop_status == 8) {
            return 4;
        }

        return 2;
    }

    int16_t __cdecl Get_Effective_Pop_Player(int16_t colony_idx, int16_t pop_idx) {
        uint32_t pop_val = MOX::colony[colony_idx].pop[pop_idx];
        uint32_t pop_type = pop_val & 0x0F;

        if (pop_type == 8 || pop_type == 9) {
            return static_cast<int16_t>(MOX::colony[colony_idx].owner);
        }

        return static_cast<int16_t>(pop_type);
    }

    s_animation_header* __cdecl Colony_Pop_Anim(int16_t colony_idx, int16_t pop_idx) {
        uint32_t pop_data = MOX::colony[colony_idx].pop[pop_idx];
        int16_t pop_state = static_cast<int16_t>(Pop_To_Pop_State(colony_idx, pop_idx));

        uint32_t type = (pop_data >> 7) & 0x3;

        int16_t player_idx = Get_Effective_Pop_Player(colony_idx, pop_idx);
        uint32_t race = static_cast<uint32_t>(static_cast<uint8_t>(MOX::player[player_idx].race));

        // Check for specific flag in pop data (bit 10 / 0x400)
        if ((pop_data & 0x400) != 0) {
            return Colony_Pop_Icon(race);
        }

        return People_Anim(static_cast<int16_t>(type), pop_state, static_cast<int16_t>(race));
    }

    s_animation_header* __cdecl Colony_Pop_Icon(int16_t race_index) {
        // Entry calculation based on race index: each race has a set of icons starting at offset 12 with stride 13
        int16_t entry_index = (int16_t)(race_index * 13 + 12);

        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(COLONY::RACEICON_LBX, (int)entry_index, MOX::global_cache_seg));
    }

    void __cdecl Print_Sys_Disp_Scanned_String(char* text) {
        Set_Colony_Font();
        fonts::Print_Centered(61, 135, text);
    }

    void __cdecl Draw_Colony_Info_Pop_For(int32_t type) {
        // Vertical offset calculation: 0x3E (62) is base Y, 0x1E (30) is row height.
        int16_t y_coord = static_cast<int16_t>(type * 30 + 62);

        COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            COLONY::colony_handle,
            static_cast<int16_t>(type),
            0,
            0x136,
            y_coord,
            0x1FE,
            1,
            COLONY::pop_fields_ref[type],
            &COLONY::pop_fields_count[type],
            &COLONY::pop_fields_coord_var[type],
            &COLONY::job_fields[type]);
    }

    int8_t __cdecl Evaluate_Input(int16_t* input_field, int16_t* scan_field) {
        uint32_t system_fields = Check_System_Display_Fields(input_field, scan_field);

        // Mask: 0xFFFF00FF clears bits 8-15 (AH register)
        uint32_t masked_fields = system_fields & 0xFFFF00FF;
        int8_t result = (int8_t)masked_fields;

        // TEST AX, AX checks bits 0-15 of EAX. Since bits 8-15 are 0, this checks AL.
        if (COLONY::drawing_display != 2 && (uint16_t)masked_fields != 0) {
            result = Evaluate_Screen_Input(input_field);
            Check_Job_Fields(input_field, scan_field);
            Check_Military_Fields(input_field, scan_field);
            Check_Satellite_Fields(input_field, scan_field);
            Check_Bldg_Fields(input_field, scan_field);
        }

        return result;
    }

    s_animation_header* __cdecl Mini_Planet_Anim(int16_t planet_idx) {
        s_planet_data* planet = &MOX::planet[planet_idx];
        uint8_t climate;

        if (planet->colony_index == -1) {
            climate = (uint8_t)planet->climate;
        } else {
            climate = (uint8_t)MOX::colony[planet->colony_index].climate;
        }

        int32_t anim_index = static_cast<int32_t>(planet->size) + static_cast<int32_t>(climate) * PLANET_SIZE_COUNT;

        char* resource_name = ESTRINGS::E_Strings(ESTR_CMBTPLNTLBX);

        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
            resource_name,
            anim_index,
            MOX::global_cache_seg));
    }

    void __cdecl Update_Fields() {
        fields::Clear_Fields();
        FAKEBUTS::Clear_Fake_Buttons();
        COLONY::Clear_Sys_Disp_Fields();
        COLONY::Clear_Galaxy_Map_Fields();

        if (COLONY::valid_colony != 0) {
            COLONY::Add_Job_Fields();
            COLONY::Add_Military_Fields();
        }

        if (COLONY::drawing_display == 0) {
            COLSYSDI::Add_Col_Sys_Disp_Fields(7, 24, 115);
        } else if (COLONY::drawing_display == 1) {
            MOVEBOX::Add_Galaxy_Map_Fields_2(
                7,
                24,
                MOX::galaxy_map_height,
                MOX::galaxy_map_width,
                11,
                31,
                0,
                0,
                0,
                &COLONY::galaxy_map_name_field,
                MOX::galaxy_map_star_field,
                &MOX::galaxy_map_grid_field);
        }

        COLONY::Add_Screen_Fields();
    }

    int16_t __cdecl Get_Scanned_Pop(int16_t field_idx) {
        g_colony_n = colony_handle;

        return COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            colony_handle,
            field_idx,
            4,
            310,
            field_idx * 30 + 62,
            510,
            1,
            pop_fields_ref[field_idx],
            &pop_fields_count[field_idx],
            &pop_fields_coord_var[field_idx],
            &job_fields[field_idx]);
    }

    int16_t __cdecl Get_Selected_Pop(int16_t job_type) {
        int32_t index = (int32_t)job_type;

        // Note: The assembly calculates (short)(job_type * 30 + 62) as the 5th argument.
        return COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            colony_handle,
            job_type,
            3,
            310,
            static_cast<int16_t>(job_type * 30 + 62),
            510,
            1,
            pop_fields_ref[index],
            &pop_fields_count[index],
            &pop_fields_coord_var[index],
            &job_fields[index]);
    }

    void __cdecl Add_Job_Field_For(int16_t index) {
        int32_t arg5_calc = (int32_t)((int16_t)(index * 30 + 62));

        COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(
            colony_handle,
            index,
            1,
            310,
            arg5_calc,
            510,
            1,
            pop_fields_ref[index],
            &pop_fields_count[index],
            &pop_fields_coord_var[index],
            &job_fields[index]);
    }

    void __cdecl Add_Job_Fields() {
        uint8_t mode = field_mode;
        for (int16_t i = 0; i < 3; ++i) {
            if (mode == 0) {
                Add_Job_Field_For(i);
                mode = field_mode;
            } else if (mode == 1) {
                Add_Move_Colonist_Job_Field_For(i);
                mode = field_mode;
            }
        }
    }

    void __cdecl Check_Job_Fields(int16_t* p_clicked_id, int16_t* p_target_id) {
        Set_Colony_Font();
        scanned_pop = -1;

        if (field_mode == 0) {
            for (int16_t i = 0; i < 3; ++i) {
                if (*p_clicked_id == job_fields[i]) {
                    int16_t pop_index = Get_Selected_Pop(i);
                    if (pop_index != -1) {
                        Pickup_Colonists(pop_index);
                        *p_clicked_id = 0;
                    }
                }

                if (job_fields[i] == *p_target_id) {
                    scanned_pop = Get_Scanned_Pop(i);
                }
            }
        } else if (field_mode == 1) {
            for (int16_t i = 0; i < 3; ++i) {
                if (job_fields[i] == *p_clicked_id) {
                    COLMOVE::Send_Cluster(colony_handle, i);
                    Show_Colony_Screen();
                    *p_clicked_id = 0;
                }
            }
        }
    }

    void __cdecl Add_Military_Fields() {
        int prev_ret = 0;
        int count = 0;
        int max_val = 0;

        for (int i = 0; i < 2; i++) {
            int current_ret = COLONY::Do_Colony_Info_Military_Stuff_For_Aux(i, prev_ret, count);

            if (current_ret > max_val) {
                max_val = current_ret;
                count++;
            }

            prev_ret = current_ret;
        }
    }

    void __cdecl Set_Colony_Screen_To_New_Colony(int16_t colony_idx) {
        Cleanup_Colony_Queue(colony_handle);

        int16_t planet_idx = MOX::colony[colony_idx].planet;

        MOX::screen_data = MOX::planet[planet_idx].star_index;
        orbit_temp = (int16_t)MOX::planet[planet_idx].orbit;
        temp_colony_idx = colony_idx;

        Reset_Field_Mode();
        fields::Clear_Fields();
        FAKEBUTS::Clear_Fake_Buttons();
        fields::Deactivate_Auto_Function();

        full_draw = 1;
        Set_Last_Colony_Selected(colony_idx, MOX::PLAYER_NUM);
    }

    void __cdecl Clear_Move_Military() {
        move_colonist = -1;
        Reset_Mouse_Picture();
        fields::Enable_Draw_All_Fields();
    }

    void __cdecl Check_Satellite_Fields(int16_t* input_field, int16_t* scan_field) {
        char local_buffer[500];

        scanned_satellite = -1;

        for (int16_t i = 0; i < 10; ++i) {
            if (colony_satellite_fields[i] == *input_field) {
                int16_t building_idx = colony_satellites[i];

                if (!Can_Scrap_Building(MOX::colony[colony_handle], (uint16_t)MOX::stardate)) {
                    char* help_text = ESTRINGS::E_Strings(0x243);
                    GENDRAW::Help(help_text);
                    Restore_Control();
                    Restore_Control();
                } else {
                    int32_t scrap_value = TECHDATA::buildings[building_idx].cost / 2;
                    char* bld_name = Real_Building_Name2(building_idx);
                    char* fmt = ESTRINGS::E_Strings(0x107);

                    snprintf(local_buffer, sizeof(local_buffer), fmt, bld_name, scrap_value);

                    if (GENDRAW::Do_Confirmation_Box(local_buffer) != 0) {
                        Record_Building_Scrap(&MOX::colony[colony_handle], (uint16_t)MOX::stardate);
                        ERICNET::Remove_Building(colony_handle, building_idx);
                        Remove_Satellite_From_Array(building_idx);
                        COLCALC::Colony_Calculation(colony_handle);
                        MOX::player[MOX::PLAYER_NUM].bc += scrap_value;
                        Restore_Control();
                    } else {
                        Restore_Control();
                    }
                }
            } else if (colony_satellite_fields[i] + *input_field == 0) {
                int16_t building_idx = colony_satellites[i];
                COLBLDG::Get_App_Description_String(local_buffer, sizeof(local_buffer), TECHDATA::buildings[building_idx].tech_app);
                char* bld_name = Real_Building_Name2(building_idx);
                TEXTBOX::Text_Box(bld_name, local_buffer);
            }

            if (colony_satellite_fields[i] == *scan_field) {
                scanned_satellite = colony_satellites[i];
            }
        }
    }

    void __cdecl Pickup_Colonists(int16_t colonists) {
        field_mode = 1;
        COLMOVE::Get_Cluster(colony_handle, colonists);
        full_draw = 1;
        Col_Calc_Wrapper(colony_handle);
    }

    int16_t __cdecl Get_Next_Colony(int16_t current_idx, int16_t direction) {
        int8_t target_owner = MOX::colony[current_idx].owner;
        int16_t search_idx = current_idx;

        do {
            if (direction == 0) {
                // Next colony logic: (current + 1) % NUM_COLONIES
                search_idx = (search_idx + 1) % MOX::NUM_COLONIES;
            } else {
                // Previous colony logic
                search_idx--;
                if (search_idx < 0) {
                    // Wrap around to the end of the list
                    // BUG: This should likely be MOX::NUM_COLONIES - 1
                    search_idx = MOX::NUM_COLONIES;
                }
            }

            // Check if colony matches owner and is not an outpost (outpost_flag == 0)
            s_colony* col = &MOX::colony[search_idx];
            if (col->owner == target_owner && col->outpost_flag == 0) {
                return search_idx;
            }

        } while (search_idx != current_idx);

        return current_idx;
    }

    void __cdecl Remove_Satellite_From_Array(int16_t satellite_id) {
        for (int16_t i = 0; i < 10; ++i) {
            if (colony_satellites[i] == satellite_id) {
                for (int16_t shift_idx = i; shift_idx < 9; ++shift_idx) {
                    colony_satellites[shift_idx] = colony_satellites[shift_idx + 1];
                }
                colony_satellites[9] = -1;
                break;
            }
        }

        full_draw = 1;
        Make_Bldg_Array_For_Colony(colony_handle, 0);
    }

    void __cdecl Add_Move_Colonist_Job_Field_For(int16_t index) {
        char* hotkey = ESTRINGS::E_Strings(12);

        // The assembly uses 16-bit math for coordinate and offset calculations.
        // x1: 0x136 (310)
        // y1: (index + 2) * 30
        // x2: 0x1fe (510)
        // y2: index * 30 + 88 (0x58)
        int16_t field_id = fields::Add_Hidden_Field(
            310,
            (index + 2) * 30,
            510,
            index * 30 + 88,
            hotkey,
            0);

        job_fields[index] = field_id;
    }

    int32_t __cdecl Do_Colony_Info_Military_Stuff_For_Aux(int32_t military_index, int32_t prev_ret, int32_t drawn_count) {
        return COLONY::Do_Colony_Info_Military_Stuff_For(military_index, prev_ret, drawn_count, 1);
    }

    void __cdecl Add_Bldg_Fields() {
        // Table of building coordinates (row, col) in a specific processing order (diagonal/diamond scan)
        static const uint8_t building_coords[36][2] = {
            {0, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 2}, {2, 0}, {1, 2}, {2, 1}, {0, 3}, {3, 0}, {2, 2}, {1, 3}, {3, 1}, {0, 4}, {4, 0}, {3, 2}, {2, 3}, {1, 4}, {4, 1}, {0, 5}, {5, 0}, {3, 3}, {2, 4}, {4, 2}, {1, 5}, {5, 1}, {3, 4}, {4, 3}, {2, 5}, {5, 2}, {4, 4}, {3, 5}, {5, 3}, {4, 5}, {5, 4}, {5, 5}};

        for (int i = 0; i < 36; ++i) {
            uint8_t grid_y = building_coords[i][0];
            uint8_t grid_x = building_coords[i][1];
            int index = grid_y * 6 + grid_x;

            if (COLONY::colony_bldgs[grid_y].buildings[grid_x] != 0) {
                int center_x = COLONY::Bldg_Coords_To_Centered_Screen_Coord(grid_y, grid_x, 0);
                int center_y = COLONY::Bldg_Coords_To_Centered_Screen_Coord(grid_y, grid_x, 1);
                char* hotkey_label = ESTRINGS::E_Strings(12);

                COLONY::colony_bldg_fields[index] = fields::Add_Hidden_Field(
                    (int16_t)(center_x - 20),
                    (int16_t)(center_y - 30),
                    (int16_t)(center_x + 20),
                    (int16_t)(center_y + 10),
                    hotkey_label,
                    0);
            } else {
                // -1000 indicates no field/empty slot
                COLONY::colony_bldg_fields[index] = (int16_t)0xfc18;
            }
        }
    }

    void Add_Satellite_Fields() {
        for (int16_t i = 0; i < 10; ++i) {
            int16_t satellite_id = COLONY::colony_satellites[i];

            if (satellite_id == -1) {
                COLONY::colony_satellite_fields[i] = -1000;
            } else {
                s_animation_header* anim = COLONY::Satellite_Anim(satellite_id);
                int16_t width = animate::Get_Width(anim);
                int16_t height = animate::Get_Height(anim);

                // Alternating horizontal offset calculation:
                // side is 1 if even, -1 if odd
                int16_t side = (i % 2 == 0) ? 1 : -1;
                int16_t x_offset = side * i * 50;

                int16_t x1 = x_offset + 295;
                int16_t y1 = 162;
                int16_t x2 = width + x1;
                int16_t y2 = height + y1;

                char* hotkey = ESTRINGS::E_Strings(12);
                int16_t field_id = fields::Add_Hidden_Field(x1, y1, x2, y2, hotkey, 0);

                COLONY::colony_satellite_fields[i] = field_id;
            }
        }
    }

    void __cdecl Add_Stacking_Fields() {
        s_colony* colony = &MOX::colony[colony_handle];

        if (colony->occupation_policy == 0) {
            char* str = ESTRINGS::E_Strings(12);
            colony_fields[8] = fields::Add_Hidden_Field(0, 0, 639, 19, str, 40);
        } else {
            colony_fields[8] = -1000;
        }

        int16_t first_unemployed = COLMOVE::Get_First_Unemployed(colony_handle);

        if (first_unemployed != -1 && valid_colony != 0) {
            char* str = ESTRINGS::E_Strings(12);
            s_animation_header* pic = C_Anims(24);
            char* help = ESTRINGS::E_Strings(12);

            colony_fields[11] = fields::Add_Button_Field(307, 159, help, pic, str, 40);
        } else {
            colony_fields[11] = -1000;
        }
    }

    uint32_t __cdecl Check_System_Display_Fields(int16_t* input_field, int16_t* scan_field) {
        int16_t scanned_star_out;
        int16_t scanned_val_out;
        char description[2000];

        if (COLONY::drawing_display == 0) {
            if (*input_field == COLONY::sys_disp_fields[0]) {
                COLONY::drawing_display = 1;
                COLONY::full_draw = 1;
            } else {
                for (int16_t i = 0; i < 5; ++i) {
                    if (COLONY::sys_disp_planet_fields[i] == *input_field) {
                        if (COLONY::field_mode == 1) {
                            int16_t planet_idx = HAROLD::Planet_Index(COLONY::sys_disp_star_handle, i);
                            COLMOVE::Send_Cluster(MOX::planet[planet_idx].colony_index, -1);
                            COLONY::Restore_Control();
                        } else {
                            int16_t planet_idx = HAROLD::Planet_Index(COLONY::sys_disp_star_handle, i);
                            int16_t col_idx = MOX::planet[planet_idx].colony_index;

                            if (col_idx != -1 &&
                                MOX::colony[col_idx].outpost_flag != 1 &&
                                col_idx != COLONY::colony_handle &&
                                MOX::colony[col_idx].owner == MOX::PLAYER_NUM) {
                                MOX::screen_data = COLONY::sys_disp_star_handle;
                                COLONY::orbit_temp = i;
                                COLONY::temp_colony_idx = col_idx;

                                COLONY::Cleanup_Colony_Queue(COLONY::colony_handle);
                                COLONY::Reset_Field_Mode();
                                fields::Clear_Fields();
                                FAKEBUTS::Clear_Fake_Buttons();
                                fields::Deactivate_Auto_Function();
                                COLONY::full_draw = 1;

                                COLONY::Set_Last_Colony_Selected(col_idx, MOX::PLAYER_NUM);
                                return 0;
                            }
                            return 1;
                        }
                    } else if (COLONY::sys_disp_summ_fields[i] == *input_field) {
                        int16_t planet_idx = HAROLD::Planet_Index(COLONY::sys_disp_star_handle, i);
                        if (MOX::planet[planet_idx].planet_type == PLANET_TYPE_PLANET) {
                            COLONY::planet_display_handle = HAROLD::Planet_Index(COLONY::sys_disp_star_handle, i);
                            COLONY::drawing_display = 2;
                            return 1;
                        }
                    } else if (*scan_field == COLONY::sys_disp_planet_fields[i]) {
                        COLONY::g_scanned_planet = HAROLD::Planet_Index(COLONY::sys_disp_star_handle, i);
                    }
                }
            }
        } else if (COLONY::drawing_display == 1) {
            int16_t scan_res = (int16_t)MAINSCR::Scan_Galaxy_Map_Fields(*scan_field, *input_field, &scanned_star_out, &scanned_val_out);
            COLONY::sys_disp_scanned_star_handle = -1;

            if (*input_field == COLONY::galaxy_map_name_field) {
                COLONY::drawing_display = 0;
                COLONY::full_draw = 1;
                COLONY::g_scanned_planet = -1;
            } else if (scan_res == 4) {
                COLONY::g_scanned_star = scanned_star_out;
            } else if (scan_res == 2) {
                int16_t center_x = (int16_t)((((int)MOX::galaxy_map_grid_y * (int)MOX::max_map_scale) / 10 * (5060 / (int)MOX::galaxy_map_height)) / 10);
                int16_t center_y = (int16_t)((((int)MOX::galaxy_map_grid_x * (int)MOX::max_map_scale) / 10 * (4000 / (int)MOX::galaxy_map_width)) / 10);
                MAINSCR::Center_Map(center_x, center_y);
                COLONY::full_draw = 1;
            } else if (scan_res == 0) {
                if (COLONY::debug_flag != 0) {
                    fonts::PSTRS(0, 0, ESTRINGS::E_Strings(0x138));
                }
                COLONY::drawing_display = 0;
                COLONY::full_draw = 1;
                COLONY::sys_disp_star_handle = scanned_val_out;
                COLONY::g_scanned_planet = -1;
            }
        } else if (COLONY::drawing_display == 2) {
            MAINPUPS::Build_Generic_Planet_Or_Colony_Description_String_Formatted(COLONY::planet_display_handle, description, sizeof(description));
            COLONY::drawing_display = 0;
            COLONY::planet_display_handle = -1;
            GENDRAW::Help(description);
            COLONY::Restore_Control();
        }

        return 1;
    }

    void __cdecl Do_Cheats() {
        char input_buf[20];

        // Copy menu title into buffer
        strlcpy(input_buf, ESTRINGS::E_Strings(12), sizeof(input_buf));

        RUSS::PFMT(0, 0x17, ESTRINGS::E_Strings(ESTR_ELSE__COL_CALC_WRAPPER));
        RUSS::PFMT(0, 0x16, ESTRINGS::E_Strings(ESTR_Q__PRODUCTION__10000));
        RUSS::PFMT(0, 0x15, ESTRINGS::E_Strings(ESTR_P__PRODUCTION__100));
        RUSS::PFMT(0, 0x14, ESTRINGS::E_Strings(ESTR_C__COLONIZE_PLANET_SUMMARY_HANDLE));
        RUSS::PFMT(0, 0x13, ESTRINGS::E_Strings(ESTR_F__ADD_FREIGHTERS_TO_POOL));
        RUSS::PFMT(0, 0x12, ESTRINGS::E_Strings(ESTR_B__ADD_BCS_TO_TREASURY));

        int key;

        key = key::Wait_For_Key();

        input_buf[0] = (char)key;
        input_buf[1] = '\0';

        if (strcasecmp(input_buf, ESTRINGS::E_Strings(ESTR_P)) == 0) {
            uint16_t production_spent;

            memcpy(&production_spent, &MOX::colony[COLONY::colony_handle].production_spent, sizeof(production_spent));
            production_spent += 100;
            memcpy(&MOX::colony[COLONY::colony_handle].production_spent, &production_spent, sizeof(production_spent));

            COLONY::full_draw = 1;
        }

        if (strcasecmp(input_buf, ESTRINGS::E_Strings(ESTR_Q)) == 0) {
            uint16_t production_spent;

            memcpy(&production_spent, &MOX::colony[COLONY::colony_handle].production_spent, sizeof(production_spent));
            production_spent += 10000;
            memcpy(&MOX::colony[COLONY::colony_handle].production_spent, &production_spent, sizeof(production_spent));

            COLONY::full_draw = 1;
            return;
        }

        if (strcasecmp(input_buf, ESTRINGS::E_Strings(ESTR_C1)) == 0) {
            if (COLONY::drawing_display == 2) {
                if (!COLONY::Colonize_Planet(COLONY::planet_display_handle)) {
                    GENDRAW::Help(ESTRINGS::E_Strings(ESTR_PLANET_ALREADY_COLONIZED));
                    COLONY::Restore_Control();
                    return;
                }
            }
        } else if (strcasecmp(input_buf, ESTRINGS::E_Strings(ESTR_F)) == 0) {
            MOX::player[MOX::PLAYER_NUM].n_freighters += 5;
            return;
        } else if (strcasecmp(input_buf, ESTRINGS::E_Strings(ESTR_B)) == 0) {
            MOX::player[MOX::PLAYER_NUM].bc += 100;
            return;
        } else {
            COLONY::Col_Calc_Wrapper(COLONY::colony_handle);
        }
    }

    void __cdecl Check_Bldg_Fields(int16_t* input_field, int16_t* scan_field) {
        scanned_bldg_C = -1;
        scanned_bldg_R = -1;

        int16_t mouse_x = mouse::Pointer_X();
        int16_t mouse_y = mouse::Pointer_Y();

        char buffer[1000];

        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 6; ++col) {
                int16_t field_val = colony_bldg_fields[row * 6 + col];
                int32_t bldg_id = colony_bldgs[row].buildings[col];

                // Case: Potential Demolish
                bool demolish_match = false;
                if (field_val == *input_field) {
                    demolish_match = true;
                } else if (colony_fields[0] == *input_field) {
                    if (XY_In_Bldg_Poly(mouse_x, mouse_y, (int16_t)row, (int16_t)col) && bldg_id != 0) {
                        demolish_match = true;
                    }
                }

                if (demolish_match) {
                    if (bldg_id == BUILDING_ARTEMIS_SYSTEM_NET ||
                        bldg_id == BUILDING_DIMENSIONAL_PORTAL ||
                        bldg_id == BUILDING_STAR_BASE ||
                        bldg_id == BUILDING_STAR_FORTRESS ||
                        bldg_id == BUILDING_BATTLESTATION ||
                        bldg_id == BUILDING_CAPITOL) {
                        return;
                    }

                    s_colony& col_ref = MOX::colony[colony_handle];
                    if (!Can_Scrap_Building(col_ref, (uint16_t)MOX::stardate)) {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x243));
                        Show_Colony_Screen();
                        return;
                    }

                    int32_t refund_val = TECHDATA::buildings[bldg_id].cost / 2;

                    snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x107), CR_Building_Name((int16_t)bldg_id), refund_val);
                    if (GENDRAW::Confirmation_Box(buffer)) {
                        Record_Building_Scrap(&col_ref, (uint16_t)MOX::stardate);
                        Turn_To_Rubble_CR_Bldg((int16_t)bldg_id);
                        ERICNET::Remove_Building(colony_handle, (int16_t)bldg_id);
                        COLCALC::Colony_Calculation(colony_handle);
                        MOX::player[MOX::PLAYER_NUM].bc += refund_val;
                    }

                    Show_Colony_Screen();
                    return;
                }

                // Case: Potential Info Description
                bool info_match = false;
                if (field_val + *input_field == 0) {
                    info_match = true;
                } else if (colony_fields[0] + *input_field == 0) {
                    if (XY_In_Bldg_Poly(mouse_x, mouse_y, (int16_t)row, (int16_t)col) && bldg_id != 0) {
                        info_match = true;
                    }
                }

                if (info_match) {
                    if (bldg_id == BUILDING_ARTEMIS_SYSTEM_NET ||
                        bldg_id == BUILDING_DIMENSIONAL_PORTAL ||
                        bldg_id == BUILDING_STAR_BASE ||
                        bldg_id == BUILDING_STAR_FORTRESS ||
                        bldg_id == BUILDING_BATTLESTATION) {
                        return;
                    }

                    COLBLDG::Get_App_Description_String(buffer, sizeof(buffer), TECHDATA::buildings[bldg_id].tech_app);
                    TEXTBOX::Text_Box(CR_Building_Name((int16_t)bldg_id), buffer);
                    Show_Colony_Screen();
                    return;
                }

                // Case: Potential Selection Scan
                bool scan_match = false;
                if (field_val == *scan_field) {
                    scan_match = true;
                } else if (XY_In_Bldg_Poly(mouse_x, mouse_y, (int16_t)row, (int16_t)col) && bldg_id != 0) {
                    scan_match = true;
                }

                if (scan_match) {
                    scanned_bldg_C = (int16_t)row;
                    scanned_bldg_R = (int16_t)col;
                    return;
                }
            }
        }
    }

    void __cdecl Check_Military_Fields(int16_t* input_field, int16_t* scan_field) {
        COLONY::scanned_military = -1;

        if (COLONY::field_mode == 2 && *input_field != 0) {
            COLONY::Drop_Military();
            *input_field = 0;
        }

        for (int16_t i = 0; i < 2; ++i) {
            // Note: *input_field is re-evaluated in each iteration as per assembly MOV CX, [ESI]
            if (COLONY::military_fields[i] == *input_field) {
                NEWPUP::Troop_Popup();
                COLONY::Show_Colony_Screen();
                *input_field = 0;
            }

            if (COLONY::military_fields[i] == *scan_field) {
                COLONY::scanned_military = i;
            }
        }
    }

    /**
     * @brief Updates the last planet selected in a star system for a specific player based on a colony.
     *
     * @param colony_idx The index of the colony.
     * @param player_idx The index of the player.
     */
    void __cdecl Set_Last_Colony_Selected(int16_t colony_idx, int16_t player_idx) {
        if (MOX::colony[colony_idx].outpost_flag == 0) {
            int16_t planet_idx = MOX::colony[colony_idx].planet;

            // Note: star_index and orbit are extracted from the planet associated with the colony
            int16_t star_idx = MOX::planet[planet_idx].star_index;
            char orbit = MOX::planet[planet_idx].orbit;

            // Update the player's last selected planet in the system's star data
            MOX::star[star_idx].last_planet_selected[player_idx] = orbit;
        }
    }

    bool __cdecl XY_In_Bldg_Poly(int16_t x, int16_t y, int16_t bldg_type, int32_t bldg_id) {
        int16_t x_coords[4];
        int16_t y_coords[4];
        int32_t hit_count = 0;

        COLONY::Get_Bldg_Box(bldg_type, bldg_id, x_coords, y_coords);

        // Store y in a local because the original code spills/overwrites a stack parameter
        int32_t y_val = static_cast<int32_t>(y);

        for (int32_t i = 0; i < 4; ++i) {
            // Calculate indices for the polygon segments (mod 4 wrapping)
            int32_t next = (i + 1) % 4;
            int32_t next_next = (i + 2) % 4;

            // Check if the target point (x, y) lies on the same side of edge (i -> next)
            // as the vertex (next_next)
            if (GEO::Points_On_Same_Side_Of_Line(
                    static_cast<int32_t>(x),
                    y_val,
                    static_cast<int32_t>(x_coords[i]),
                    static_cast<int32_t>(y_coords[i]),
                    static_cast<int32_t>(x_coords[next]),
                    static_cast<int32_t>(y_coords[next]),
                    static_cast<int32_t>(x_coords[next_next]),
                    static_cast<int32_t>(y_coords[next_next])) > 0) {
                hit_count++;
            }
        }

        return hit_count == 4;
    }

    void __cdecl Reset_Step_Squish() {
        step_squish = 0;
    }

    int16_t __cdecl Sum_Colonists(int16_t colony_idx, int16_t job, int16_t state, int16_t race, int16_t moved_flag, int16_t conquered_flag) {
        (void)moved_flag;

        colonists_count = 0;
        s_colony* colony = &MOX::colony[colony_idx];

        if (colony->n_pops != 0) {
            for (int16_t i = 0; i < (int16_t)(uint8_t)colony->n_pops; ++i) {
                int16_t effective_player = Get_Effective_Pop_Player(colony_idx, i);
                uint32_t pop_data = colony->pop[i];
                uint32_t pop_race_val = pop_data & 0xF;

                bool race_match = false;
                if ((int)MOX::player[effective_player].race == (int)race && pop_race_val == (uint32_t)effective_player) {
                    race_match = true;
                } else if (race == -1) {
                    race_match = true;
                } else if (pop_race_val >= 14 && pop_race_val == (uint32_t)race) {
                    race_match = true;
                }

                if (race_match) {
                    int16_t pop_state = Pop_To_Pop_State(colony_idx, i);
                    if (pop_state == state || state == -1) {
                        if ((int)((pop_data & 0x180) >> 7) == (int)job || job == -1) {
                            if ((int)((pop_data & 0x400) >> 10) == (int)conquered_flag || conquered_flag == -1) {
                                // Bit 9: 0x200
                                if (pop_data & 0x200) {
                                    colonists_count++;
                                }
                            }
                        }
                    }
                }
            }
        }

        return (int16_t)colonists_count;
    }

    void __cdecl Set_Colony_Globals(int16_t colony_idx) {
        colony_handle = colony_idx;
        planet_handle = MOX::colony[colony_idx].planet;
        star_handle = MOX::planet[planet_handle].star_index;
        orbit_n = MOX::planet[planet_handle].orbit;
    }

    s_animation_header* __cdecl Load_Govt_Anim(int16_t govt_anim_id) {
        return (s_animation_header*)buffer::Buffer_Reload(COLONY2_LBX, govt_anim_id + 0x13, MOX::global_cache_seg);
    }

    s_animation_header* __cdecl Govt_Anim(int16_t govt_anim_id) {
        return Load_Govt_Anim(govt_anim_id);
    }

    void __cdecl Get_Bldg_Box(int16_t c, int16_t r, int16_t* x_arr, int16_t* y_arr) {
        x_arr[0] = (int16_t)CR_To_XY(c, r, 0);
        x_arr[1] = (int16_t)CR_To_XY(c + 1, r, 0);
        x_arr[2] = (int16_t)CR_To_XY(c + 1, r + 1, 0);
        x_arr[3] = (int16_t)CR_To_XY(c, r + 1, 0);

        y_arr[0] = (int16_t)CR_To_XY(c, r, 1);
        y_arr[1] = (int16_t)CR_To_XY(c + 1, r, 1);
        y_arr[2] = (int16_t)CR_To_XY(c + 1, r + 1, 1);
        y_arr[3] = (int16_t)CR_To_XY(c, r + 1, 1);
    }

    s_animation_header* __cdecl Import_Anims(int16_t type, int16_t is_tens, int8_t frame) {
        if (frame == 0) {
            return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(COLONY2_LBX, type + is_tens * 4 + 8, MOX::global_cache_seg));
        }
        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(COLONY2_LBX, type + is_tens * 4 + 0x23, MOX::global_cache_seg));
    }

    s_animation_header* __cdecl Short_Anims(int16_t type, int16_t is_tens, uint8_t frame) {
        s_animation_header* anim = Import_Anims(type, is_tens, frame);

        bitmap::Create_Blank_Bitmap(30, 30, swap_bitm);
        bitmap::Add_Picture_To_Bitmap(1, 1, anim, swap_bitm);
        bitmap::Outline_Bitmap(swap_bitm, swap_bitm, 0xED, 0);

        return swap_bitm;
    }

    /**
     * @brief Draws a darkened version of a population animation at the specified coordinates.
     * 
     * @param x The screen X-coordinate.
     * @param y The screen Y-coordinate.
     * @param colony_idx The index of the colony.
     * @param pop_idx The index of the population unit.
     */
    void __cdecl Draw_Pop_Dark(int16_t x, int16_t y, int16_t colony_idx, int16_t pop_idx) {
        s_animation_header* anim;

        anim = Colony_Pop_Anim(colony_idx, pop_idx);

        // Prepare a scratch buffer (30x30) with the darkened population frame
        bitmap::Create_Blank_Bitmap(30, 30, swap_bitm);
        bitmap::Add_Picture_To_Bitmap(0, 0, anim, swap_bitm);
        bitmap::Tint_Bitmap(swap_bitm, swap_bitm, 2);

        animate::Draw(x, y, swap_bitm);
    }

    /**
     * @brief Creates a "ghosted" or shadowed mouse bitmap for the farmer icon.
     * This version is used when dragging a farmer in the colony screen.
     */
    void __cdecl Create_Shadow_Farmer(int16_t unused) {
        s_animation_header* anim;
        int32_t race_idx;

        // Create a 30x30 workspace for the mouse cursor
        bitmap::Create_Blank_Bitmap(30, 30, (void*)COLONY::mouse_bitm);

        // Fetch current player's race index
        race_idx = (int32_t)(uint8_t)MOX::player[MOX::PLAYER_NUM].race;

        // Get the first frame of the farmer animation for this race
        anim = COLONY::People_Anim(0, 2, (int16_t)race_idx);

        // Draw the frame, replace color 240 with transparency/background,
        // apply the "glassed" (translucent) effect and outline it in white.
        bitmap::Draw_Picture_To_Bitmap(COLONY::mouse_bitm, anim);
        replace::Replace_Color_All(COLONY::mouse_bitm, 240);
        bitmap::Set_Picture_Glassed(COLONY::mouse_bitm);
        bitmap::Outline_Bitmap(COLONY::mouse_bitm, COLONY::mouse_bitm, 229 /* WHITE_OUTLINE */, 0);
    }

    s_animation_header* __cdecl Shadow_Farmer_Bitm() {
        return COLONY::mouse_bitm;
    }

    void __cdecl Picture_To_Mouse(s_animation_header* anim_header) {
        if (animate::Get_Width(anim_header) > 0x18 || animate::Get_Height(anim_header) > 0x18) {
            if (debug_flag != 0) {
                fonts::PSTRS(0, 1, ESTRINGS::E_Strings(0x122));
            }
            key::Wait_For_Key();
        }

        bitmap::Draw_Picture_To_Bitmap(mouse_bitm, anim_header);
        bitmap::Clear_Mouse_Image(0x13);
        bitmap::Draw_Bitmap_To_Mouse(0, 0, mouse_bitm, 0x13);
        mouse::Set_Mouse_List(&MOX::mouse_list_variable, 1);
    }

    int16_t __cdecl CR_To_XY(int16_t col, int16_t row, int16_t axis) {
        return building_cr_xy[col][row][axis * 2];
    }

    int32_t __cdecl Bldg_Coords_To_Centered_Screen_Coord(int32_t grid_y, int32_t grid_x, int32_t axis) {
        // Calculate index based on 7-column stride and 2 components per cell
        int index = axis + (grid_y * 7 + grid_x) * 2;

        // Calculate the center point between the two boundary coordinates
        int coord_min = COLONY::building_cr[index];
        int coord_max = COLONY::building_cr[index + 16];

        return (coord_min + coord_max) / 2;
    }

    int16_t __cdecl N_Bldgs(int16_t colony_idx) {
        int16_t count = 0;

        for (int16_t i = BUILDING_NO_BUILDING + 1; i < BUILDING_COUNT; ++i) {
            if (MOX::colony[colony_idx].buildings[i] != 0) {
                count++;
            }
        }

        return count;
    }

    char* __cdecl Player_Troop_Name(int16_t player_idx, int16_t troop_type, int16_t race) {
        switch (troop_type) {
            case 0:
                if (MOX::player[player_idx].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    return ESTRINGS::E_Strings(0x1F8);
                }
                return ESTRINGS::E_Strings(0xC3);
            case 1:
                if (MOX::player[player_idx].tech_applications[TECH_APP_POWERED_ARMOR] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    return ESTRINGS::E_Strings(0x14D);
                }
                return ESTRINGS::E_Strings(0x1AC);
            case 2:
                return ESTRINGS::E_Strings(0x174);
            case 3:
                return ESTRINGS::E_Strings(0x1BC);
            default:
                return nullptr;
        }
    }


    void __cdecl Do_Load_Colony_Anims() {
        swap_bitm = (s_animation_header*)allocate::Allocate_Next_Block(MOX::global_screen_seg, 0x3A4);
        mouse_bitm = (s_animation_header*)allocate::Allocate_Next_Block(MOX::global_screen_seg, 0x3A4);
    }

    void __cdecl Load_Colony_Anims() {
        Do_Load_Colony_Anims();
    }

    void __cdecl Set_Colony_Font_To_Reg_Blue(uint8_t font_idx, uint8_t state) {
        fonts::Set_Outline_Color(0x50, 0x50);

        if (state == 0) {
            fonts::Set_Font_Style_Shadow_Down(font_idx, &font_dim_color_array);
        } else if (state == 1) {
            fonts::Set_Font_Style_Shadow_Down(font_idx, &font_color_array);
        } else if (state == 2) {
            fonts::Set_Font_Style_Shadow_Down(font_idx, &font_bright_color_array);
        }

        fonts::Set_Font_Highlight_Colors(&bright_blue_font_color_array);
        fonts::Set_Font_Special_Colors(&red_font_color_array[state]);
    }

    void __cdecl Set_Colony_Font_To_Player(uint8_t font_style, uint8_t player_idx) {
        fonts::Set_Outline_Color(0x50, 0x50);
        uint8_t color = MOX::player[player_idx].color;

        fonts::Set_Font_Style_Shadow_Down(
            font_style,
            &player_font_color_array[color]);
        fonts::Set_Font_Highlight_Colors(&bright_blue_font_color_array);
        fonts::Set_Font_Special_Colors(&red_font_color_array[0]);
    }
} // namespace COLONY