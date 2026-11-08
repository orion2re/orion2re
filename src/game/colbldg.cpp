#include "pch.h"

#include "build_queue.h"
#include "config/config_loader.h"
#include "lua/lua_game_ship_identity.h"
#include "strbuild.h"

namespace COLBLDG {

    bool warn_blockers = true;

    void Commit_Autobuild_State() {
        if (BUILD_QUEUE::autobuild_settings.enabled == 0) {
            MOX::colony[COLONY::colony_handle].auto_building =
                static_cast<uint8_t>(colony_auto_building);
        }
    }

    s_mouse_list mouse_list_to[4] = {
        {18, 0, 199, 324, 465, 472},
        {16, 0, 476, 10, 632, 306},
        {16, 0, 10, 10, 188, 471},
        {1, 0, 0, 0, 639, 479},
    };
    s_mouse_list mouse_list_what = {16, 0, 0, 0, 639, 479};
    s_mouse_list mouse_list_which_ship = {17, 0, 0, 0, 639, 479};
    s_colors l_font_color_array = {{184, 187, 186, 0, 0, 0, 0, 0}};
    s_colors l_font_dim_color_array = {{183, 203, 202, 0, 0, 0, 0, 0}};
    uint8_t return_to_build_popup_from_custom_design;
    uint8_t hacky_speed_refitted_ship_flag;
    uint8_t field_mode;
    int16_t queue_fields[7] = {-1, -1, -1, -1, -1, 0, 0};
    int16_t building_indexes[54] = {-1};
    int16_t n_building_indexes;
    int16_t military_indexes[54] = {-1};
    int16_t n_military_indexes;
    int16_t active_prod[4] = {-1, 0, 0, 0};
    int16_t current_item[7] = {
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
        0,
        0,
    };
    int16_t last_app = -1;
    int16_t last_setting = -1;
    int16_t scanned_field = -1000;
    int16_t military_fields[54];
    int16_t colony_bldg_saved_screen;
    s_animation_header* anims[7];
    int16_t scanned_prod;
    int16_t refitting_ship_n;
    int16_t building_fields[54];
    uint8_t* desc_record_ptr;
    int16_t colony_auto_building;
    s_ship_design pending_refit_design_;
    bool pending_refit_design_valid_;
    int16_t fields[11];
    int16_t build_queue_hotkeys[11];
    int16_t shortcut_previous_colony;
    int16_t shortcut_next_colony;
    int16_t shortcut_accept;
    int16_t automatic_design_command;
    int16_t reload_config_command;
    int16_t reload_interface_command;
    int16_t crunch_command;
    char g_msg[800];
    s_animation_header* colsum_pal_anim;
    s_animation_header* bldg_bitm;
    uint8_t* buffer;

    int16_t __cdecl Calc_Font_Off() {
        if (COLONY::step_squish == 0) {
            return 0;
        }

        // Returns 3 if step_squish >= 5, otherwise 2.
        return (COLONY::step_squish >= 5 ? 1 : 0) + 2;
    }

    void __cdecl Get_Font_Off_For_Military(int16_t* font_offset) {
        if (n_military_indexes <= 0) {
            return;
        }

        for (int16_t i = 0; i < n_military_indexes; ++i) {
            int16_t text_width;
            do {
                fonts::Set_Font_Style_Shadow_Down(4 - *font_offset, &l_font_color_array);

                char* selection_name = Selection_Name(military_indexes[i]);

                text_width = BILL::_Get_Paragraph_Max_Width(0x280, selection_name, 0);

                if (text_width > 138) {
                    *font_offset += 1;
                }

                if (*font_offset > 2) {
                    text_width = 0;
                }
            } while (text_width > 138);
        }
    }

    void __cdecl Get_Font_Off_For_Buildings(int16_t* font_offset) {
        for (int16_t i = 0; i < n_building_indexes; ++i) {
            int16_t width;
            do {
                int16_t building_idx = building_indexes[i];
                s_colors* colors;

                if (Has_Prod_In_Queue(building_idx)) {
                    colors = &l_font_color_array;
                } else {
                    colors = &l_font_dim_color_array;
                }

                fonts::Set_Font_Style_Shadow_Down(4 - *font_offset, colors);

                char* name = Selection_Name(building_idx);
                width = BILL::_Get_Paragraph_Max_Width(0x280, name, 0);

                if (width > 171) {
                    *font_offset += 1;
                }

                if (*font_offset > 2) {
                    width = 0;
                }
            } while (width > 171);
        }
    }

    bool __cdecl Has_Prod_In_Queue(int16_t prod_id) {
        for (int32_t i = 0; i < 7; ++i) {
            if (current_item[i] == prod_id) {
                return true;
            }
        }
        return false;
    }

    int16_t __cdecl N_Prods_In_Queue(int16_t product_id) {
        int16_t match_count = 0;
        for (int32_t i = 0; i < 7; ++i) {
            if (current_item[i] == product_id) {
                match_count++;
            }
        }
        return match_count;
    }

    int16_t __cdecl Get_Prod_Queue_Len() {
        for (int16_t i = 0; i < 7; ++i) {
            if (current_item[i] == COLONY_PRODUCTION_NONE) {
                return i;
            }
        }
        return -1;
    }

    static int16_t Normalize_Repeat_Pair_Position(int16_t* slot_idx) {
        if (*slot_idx > 0 &&
            current_item[*slot_idx] == COLONY_PRODUCTION_REPEAT) {
            --*slot_idx;
            return 2;
        }
        if (*slot_idx < 6 &&
            current_item[*slot_idx + 1] == COLONY_PRODUCTION_REPEAT) {
            return 2;
        }
        return 1;
    }

    static int32_t Build_List_Product_Index(int16_t product_id) {
        if (product_id == COLONY_PRODUCTION_TRADE_GOODS) {
            return BUILD_LIST_PRODUCT_TRADE_GOODS;
        }
        if (product_id == COLONY_PRODUCTION_HOUSING) {
            return BUILD_LIST_PRODUCT_HOUSING;
        }
        return product_id + BUILD_LIST_PRODUCT_BUILDING_BASE - 1;
    }

    static const char* Build_List_Product_Name(int16_t product_id) {
        if (Colony_Production_Is_Building(product_id)) {
            return COLONY::Real_Building_Name(product_id);
        }
        if (product_id == COLONY_PRODUCTION_HOUSING) {
            return ESTRINGS::E_Strings(ESTR_HOUSING);
        }
        return ESTRINGS::E_Strings(ESTR_TRADE_GOODS);
    }

    static s_colors Build_List_Product_Colors(int16_t product_id) {
        const s_build_list_format& format =
            build_list_format[Build_List_Product_Index(product_id)];
        const e_build_list_highlight highlight = Has_Prod_In_Queue(product_id)
            ? BUILD_LIST_HIGHLIGHT_QUEUED
            : BUILD_LIST_HIGHLIGHT_NORMAL;
        const s_build_list_color& configured =
            build_list_colors[format.color][highlight];

        s_colors colors = {};
        colors.colors[0] = configured.dark;
        colors.colors[1] = configured.base;
        colors.colors[2] = configured.dim;
        return colors;
    }

    int __cdecl qsort_building_by_name(const void* a, const void* b) {
        const int16_t* bld1 = (const int16_t*)a;
        const int16_t* bld2 = (const int16_t*)b;
        const int32_t order1 = build_list_format[Build_List_Product_Index(*bld1)].order;
        const int32_t order2 = build_list_format[Build_List_Product_Index(*bld2)].order;

        if (order1 != order2) {
            return order1 - order2;
        }
        return strcmp(Build_List_Product_Name(*bld1), Build_List_Product_Name(*bld2));
    }

    void __cdecl Calculate_Building_Array() {
        building_indexes[0] = COLONY_PRODUCTION_TRADE_GOODS;
        n_building_indexes = 1;
        if (COLCALC::disable_housing == 0) {
            building_indexes[n_building_indexes++] = COLONY_PRODUCTION_HOUSING;
        }

        for (int16_t building_id = BUILDING_NO_BUILDING + 1; building_id < BUILDING_COUNT; ++building_id) {
            uint8_t can_build = COLCALC::Colony_Can_Build_Product(&MOX::colony[COLONY::colony_handle], building_id);

            if ((((n_building_indexes <= 54) && can_build == 1) || COLONY::cheats) && building_id != BUILDING_COLONY_BASE) {
                int16_t current_index = n_building_indexes;
                n_building_indexes++;
                building_indexes[current_index] = building_id;
            }
        }

        qsort(
            building_indexes,
            n_building_indexes,
            sizeof(int16_t),
            qsort_building_by_name);
    }

    void __cdecl Calculate_Military_Array() {
        n_military_indexes = 0;

        s_colony& colony = MOX::colony[COLONY::colony_handle];
        s_planet_data& planet = MOX::planet[colony.planet];

        int16_t uncolonized_worlds = Star_N_Uncolonized_Worlds(planet.star_index);
        if (N_Prods_In_Queue(BUILDING_COLONY_BASE) < uncolonized_worlds) {
            if (COLCALC::Colony_Can_Build_Product(&colony, BUILDING_COLONY_BASE)) {
                military_indexes[n_military_indexes++] = BUILDING_COLONY_BASE;
            }
        }

        s_player& current_player = MOX::player[MOX::PLAYER_NUM];

        if (current_player.tech_applications[TECH_APP_FREIGHTERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            if (COLCALC::Colony_Can_Build_Product(&colony, COLONY_PRODUCTION_FREIGHTERS)) {
                military_indexes[n_military_indexes++] = COLONY_PRODUCTION_FREIGHTERS;
            }
        }

        s_player& owner_player = MOX::player[colony.owner];

        if (owner_player.ship_range > 0 && owner_player.ship_speed != 0) {
            if (owner_player.tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) {
                military_indexes[n_military_indexes++] = COLONY_PRODUCTION_COLONY_SHIP;
            }

            if (owner_player.tech_applications[TECH_APP_OUTPOST_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) {
                military_indexes[n_military_indexes++] = COLONY_PRODUCTION_OUTPOST_SHIP;
            }

            if (owner_player.tech_applications[TECH_APP_TRANSPORT] == TECH_RESEARCH_STATUS_RESEARCHED &&
                colony.buildings[BUILDING_MARINE_BARRACKS] != 0) {
                military_indexes[n_military_indexes++] = COLONY_PRODUCTION_TRANSPORT_SHIP;
            }

            if (n_military_indexes != 0) {
                military_indexes[n_military_indexes++] = COLONY_PRODUCTION_SEPARATOR;
            }

            for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
                const char* default_design_text = ESTRINGS::E_Strings(12 /* HESTR_00C_YOUR_CURRENT_DESIGN_IS */);
                const char* design_name = Selection_Name(COLBLDG::Colony_Production_From_Ship_Design_Index(i));

                if (strcmp(default_design_text, design_name) != 0) {
                    bool can_build = false;

                    if (MOX::settings.strategic_combat_flag == 0) {
                        can_build = true;
                    } else if (current_player.ship_designs[i].size == SHIP_SIZE_TITAN) {
                        can_build = owner_player.tech_applications[TECH_APP_TITAN_CONSTRUCTION] ==
                            TECH_RESEARCH_STATUS_RESEARCHED;
                    } else if (current_player.ship_designs[i].size == SHIP_SIZE_DOOM_STAR) {
                        can_build = owner_player.tech_applications[TECH_APP_DOOM_STAR_CONSTRUCTION] ==
                            TECH_RESEARCH_STATUS_RESEARCHED;
                    } else {
                        can_build = true;
                    }

                    if (can_build) {
                        military_indexes[n_military_indexes++] = COLBLDG::Colony_Production_From_Ship_Design_Index(i);
                    }
                }
            }
        }

        if (n_military_indexes != 0) {
            military_indexes[n_military_indexes++] = COLONY_PRODUCTION_SEPARATOR;
        }

        if (COLCALC::Colony_Can_Build_Product(&colony, COLONY_PRODUCTION_FARMER)) {
            military_indexes[n_military_indexes++] = COLONY_PRODUCTION_FARMER;
        }
        if (COLCALC::Colony_Can_Build_Product(&colony, COLONY_PRODUCTION_WORKER)) {
            military_indexes[n_military_indexes++] = COLONY_PRODUCTION_WORKER;
        }
        if (COLCALC::Colony_Can_Build_Product(&colony, COLONY_PRODUCTION_SCIENTIST)) {
            military_indexes[n_military_indexes++] = COLONY_PRODUCTION_SCIENTIST;
        }
        if (COLCALC::Colony_Can_Build_Product(&colony, COLONY_PRODUCTION_SPY)) {
            military_indexes[n_military_indexes++] = COLONY_PRODUCTION_SPY;
        }
    }

    void __cdecl Add_Militarys_Fields() {
        COLDRAW::Calculate_Squish_Step(13, 308, n_military_indexes, 19);

        for (int16_t i = 0; i < n_military_indexes; ++i) {
            int16_t step = 19 - COLONY::step_squish;
            char* hotkey = ESTRINGS::E_Strings(12);

            int16_t field_id = ::fields::Add_Hidden_Field(
                485,
                i * step + 20,
                623,
                (i + 1) * step + 19,
                hotkey,
                41);

            military_fields[i] = field_id;
        }
    }

    void __cdecl Load_Build_Anims() {
        for (int16_t i = 0; i < 7; ++i) {
            if (i == 0) {
                anims[i] = (s_animation_header*)farload::Far_Reload(COLONY::COLBLDG_LBX, 0, MOX::screen_seg);
            } else {
                anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLBLDG_LBX, i, MOX::screen_seg);
            }
        }

        bldg_bitm = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x1FA4);
        colsum_pal_anim = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLSUM_LBX, 0x15, MOX::screen_seg);
    }

    void __cdecl Add_Buildings_Fields() {
        COLDRAW::Calculate_Squish_Step(13 /* 0x0D */, 469 /* 0x1D5 */, n_building_indexes, 19 /* 0x13 */);

        if (n_building_indexes > 0) {
            for (int16_t i = 0; i < n_building_indexes; ++i) {
                int16_t step_size = 19 /* 0x13 */ - COLONY::step_squish;
                char* hotkey = ESTRINGS::E_Strings(12 /* 0x0C */);

                // Coordinates calculation
                int16_t y1 = i * step_size + 20 /* 0x14 */;
                int16_t y2 = (i + 1) * step_size + 19 /* 0x13 */;

                building_fields[i] = ::fields::Add_Hidden_Field(
                    13 /* 0x0D */,
                    y1,
                    184 /* 0xB8 */,
                    y2,
                    hotkey,
                    41 /* 0x29 */
                );
            }
        }
    }

    void __cdecl Add_Build_Queue_Fields() {
        int16_t y_end = 350 /* 0x15e */;

        for (int i = 0; i < 7; ++i) {
            char* hotkey = ESTRINGS::E_Strings(12 /* 0xc */);

            // Adds a hidden field for each item in the build queue
            queue_fields[i] = ::fields::Add_Hidden_Field(
                207 /* 0xcf */,
                y_end - 21 /* 0x15 */,
                458 /* 0x1ca */,
                y_end,
                hotkey,
                41 /* 0x29 */
            );

            y_end += 20 /* 0x14 */;
        }
    }

    void __cdecl Update_Fields() {
        ::fields::Clear_Fields();

        fields[0] = ::fields::Add_Button_Field(493, 447, ESTRINGS::E_Strings(12), anims[1], ESTRINGS::E_Strings(12), 40);
        fields[3] = ::fields::Add_Button_Field(560, 447, ESTRINGS::E_Strings(12), anims[3], ESTRINGS::E_Strings(12), 40);

        fields[1] = ::fields::Add_Hot_Key(ESTRINGS::E_Strings(135));
        fields[8] = ::fields::Add_Hot_Key(ESTRINGS::E_Strings(139));
        fields[9] = ::fields::Add_Hot_Key(ESTRINGS::E_Strings(140));
        fields[10] = ::fields::Add_Hot_Key(ESTRINGS::E_Strings(193));

        fields[2] = ::fields::Add_Radio_Button_Field(490, 342, ESTRINGS::E_Strings(12), anims[2], (int16_t*)&colony_auto_building, ESTRINGS::E_Strings(12), 41);

        if (MOX::settings.strategic_combat_flag != 0) {
            fields[4] = -1000;
            fields[5] = -1000;
        } else {
            fields[4] = ::fields::Add_Button_Field(492, 379, ESTRINGS::E_Strings(12), anims[4], ESTRINGS::E_Strings(436 /* ESTR_R */), 40);
            fields[5] = ::fields::Add_Button_Field(561, 379, ESTRINGS::E_Strings(12), anims[5], ESTRINGS::E_Strings(251 /* ESTR_D3 */), 40);
        }

        fields[6] = ::fields::Add_Button_Field(503, 411, ESTRINGS::E_Strings(12), anims[6], (char*)"e", 40);
        fields[7] = -1000;

        build_queue_hotkeys[0] = ::fields::Add_Hot_Key("q");
        for (int preset = 1; preset <= 9; ++preset) {
            const char hotkey[2] = {static_cast<char>('0' + preset), '\0'};
            build_queue_hotkeys[preset] = ::fields::Add_Hot_Key(hotkey);
        }
        build_queue_hotkeys[10] = ::fields::Add_Hot_Key("0");
        shortcut_previous_colony = ::fields::Add_Hot_Key(",");
        shortcut_next_colony = ::fields::Add_Hot_Key(".");
        shortcut_accept = ::fields::Add_Hot_Key(" ");
        reload_config_command = ::fields::Add_Multi_Hot_Key_Field("RE");
        reload_interface_command = ::fields::Add_Multi_Hot_Key_Field("RI");
        automatic_design_command = ::fields::Add_Multi_Hot_Key_Field("AD");
        crunch_command = ::fields::Add_Multi_Hot_Key_Field("CRUNCH");

        Add_Build_Queue_Fields();
        Add_Buildings_Fields();
        Add_Militarys_Fields();
    }

    bool __cdecl Apply_Build_Queue_Hotkey(int16_t field_id) {
        for (int preset = 0; preset < BUILD_QUEUE::PRESET_COUNT; ++preset) {
            if (field_id != build_queue_hotkeys[preset]) {
                continue;
            }
            Do_Exit_Screen_Cleanup();
            if (BUILD_QUEUE::Apply_Preset_To_Colony(COLONY::colony_handle, preset)) {
                memcpy(current_item, MOX::colony[COLONY::colony_handle].producing, sizeof(current_item));
                active_prod[0] = -1;
                field_mode = 0;
                Build_Arrays();
                Do_Draw_Build_Queue();
            }
            return true;
        }
        return false;
    }

    void __cdecl Build_Arrays() {
        Calculate_Building_Array();
        Calculate_Military_Array();
    }

    int16_t __cdecl Create_Ship_From_Design(int16_t player_idx, const s_ship_design* design, int16_t colony_idx) {
        int16_t ship_idx = 0;
        int16_t planet_idx = MOX::colony[colony_idx].planet;
        int16_t star_idx = MOX::planet[planet_idx].star_index;

        while (ship_idx < MAX_SHIPS) {
            auto& ship = MOX::ship[ship_idx];
            if (ship_idx >= MOX::NUM_SHIPS ||
                (ship.owner == (int8_t)player_idx && ship.status == 5)) {
                break;
            }
            ship_idx++;
        }

        if (ship_idx >= MAX_SHIPS) {
            return -1;
        }
        if (!orion2re::lua::game::Advance_Ship_Generation(ship_idx)) {
            return -1;
        }

        if (ship_idx >= MOX::NUM_SHIPS) {
            MOX::NUM_SHIPS = ship_idx + 1;
        }

        auto& new_ship = MOX::ship[ship_idx];
        memset(&new_ship, 0, sizeof(s_ship_data));

        memcpy(&new_ship.d, design, sizeof(new_ship.d));
        new_ship.owner = (int8_t)player_idx;
        new_ship.d.previous_owner = (uint8_t)player_idx;
        new_ship.status = 0;
        new_ship.location = star_idx;
        new_ship.x = MOX::star[star_idx].x;
        new_ship.y = MOX::star[star_idx].y;
        new_ship.officer_index = -1;

        if (MOX::settings.strategic_combat_flag != 0) {
            char name_buffer[500];
            int32_t ships_built_count = MOX::player[player_idx].ships_built[(int32_t)new_ship.d.size];

            snprintf(name_buffer, sizeof(name_buffer), ESTRINGS::E_Strings(0x2a), new_ship.d.name, ships_built_count);

            if (strlen(name_buffer) >= 15) {
                snprintf(name_buffer, sizeof(name_buffer), ESTRINGS::E_Strings(0x48), new_ship.d.name, ships_built_count);
                if (strlen(name_buffer) >= 15) {
                    strlcpy(name_buffer, new_ship.d.name, sizeof(name_buffer));
                }
            }

            strlcpy(new_ship.d.name, name_buffer, sizeof(new_ship.d.name));
            MOX::player[player_idx].ships_built[(int32_t)new_ship.d.size]++;
        }

        return ship_idx;
    }

    int16_t __cdecl Create_Ship(int16_t player_idx, int16_t design_idx, int16_t colony_idx) {
        return Create_Ship_From_Design(
            player_idx, &MOX::player[player_idx].ship_designs[design_idx], colony_idx);
    }

    void Set_Pending_Refit_Design(int16_t ship_idx, const s_ship_design* design) {
        refitting_ship_n = ship_idx;
        pending_refit_design_ = *design;
        pending_refit_design_valid_ = true;
    }

    s_ship_design* Pending_Refit_Design() {
        return pending_refit_design_valid_ ? &pending_refit_design_ : nullptr;
    }

    void Clear_Pending_Refit_Design() {
        pending_refit_design_valid_ = false;
    }

    void __cdecl Build_Queue_Popup() {
        uint32_t original_seed = game_random::Get_Random_Seed();
        game_random::Set_Random_Seed(COLONY::colony_handle);

        COLONY::star_handle = MOX::screen_data;
        COLONY::orbit_n = COLONY::orbit_temp;

        int16_t planet_idx = HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
        COLONY::planet_handle = planet_idx;

        int16_t colony_idx = MOX::planet[planet_idx].colony_index;
        COLONY::colony_handle = colony_idx;
        COLBLDG::last_app = -1;
        COLONY::using_colony_screen_palette = 1;

        if (MOX::return_screen != MOX::current_screen && COLBLDG::return_to_build_popup_from_custom_design == 0) {
            COLBLDG::colony_bldg_saved_screen = MOX::return_screen;
        }

        COLONY::sys_disp_star_handle = COLONY::star_handle;
        COLBLDG::Clear_Last_App_Desc();
        fonts::Set_Font_Style_Shadow_Down(4, &COLBLDG::l_font_color_array);
        fonts::Set_Font_Spacing(1);
        COLONY::Load_Colony_Screen_Seg();
        COLONY::Load_Planet_Screen();
        ERICHELP::Set_Building_Queue_Screen_Help_List();

        if (COLBLDG::return_to_build_popup_from_custom_design == 0) {
            for (int i = 0; i < 7; ++i) {
                COLBLDG::current_item[i] = MOX::colony[colony_idx].producing[i];
            }
            COLBLDG::active_prod[0] = -1;
        }

        COLBLDG::Build_Arrays();

        if (COLBLDG::return_to_build_popup_from_custom_design == 0) {
            COLBLDG::colony_auto_building =
                MOX::colony[colony_idx].auto_building != 0;
        }

        COLONY::Make_Bldg_Array_For_Colony(COLONY::colony_handle, 0);

        if (ERIC::Fast_Fade_Out_Needed_For_Colony_Palette()) {
            ERIC::Fast_Fade_Out();
            COLONY::Update_Colony_Palette();
        }

        COLBLDG::Update_Fields();
        COLONY::full_draw = 1;
        ::fields::Assign_Auto_Function(COLBLDG::Draw_Build_Queue_Popup, 1);
        ERICHELP::Set_Building_Queue_Screen_Help_List();
        palette::Clear_Palette_Changes(0, 0xFF);
        COLBLDG::Draw_Build_Queue_Popup();
        ERIC::Fast_Fade_In();
        video::Set_Refresh_Stencil();

        int16_t field_id;
        int16_t scan_code;
        do {
            COLBLDG::Draw_Build_Queue_Popup();
            RUSS::Mox_Sync_Update();
            field_id = ::fields::Get_Input();
            scan_code = ::fields::Scan_Input();
            COLBLDG::Evaluate_Input(&field_id, &scan_code);

            if (field_id == COLBLDG::fields[3] ||
                field_id == COLBLDG::fields[0] ||
                field_id == COLBLDG::fields[1]) {
                break;
            }
            if (COLBLDG::return_to_build_popup_from_custom_design != 0) {
                break;
            }
            if (field_id == COLBLDG::fields[9] ||
                field_id == COLBLDG::fields[8]) {
                break;
            }
        } while (true);

        ::fields::Clear_Fields();
        if (COLBLDG::active_prod[0] != -1) {
            COLONY::Reset_Mouse_Picture();
        }

        fonts::Set_Font_Spacing(3);
        ::fields::Deactivate_Help_List();
        ::fields::Deactivate_Auto_Function();
        video::Copy_Back_To_Off();

        if (field_id != COLBLDG::fields[9] && field_id != COLBLDG::fields[8]) {
            if (COLBLDG::return_to_build_popup_from_custom_design != 0) {
                MOX::first_design_screen_call = 1;
                MOX::return_screen = MOX::current_screen;
                MOX::current_screen = SCREEN_DESIGN;
            } else {
                if (MOX::return_screen != MOX::current_screen) {
                    MOX::current_screen = MOX::return_screen;
                } else {
                    MOX::current_screen = COLBLDG::colony_bldg_saved_screen;
                    MOX::return_screen = COLBLDG::colony_bldg_saved_screen;
                }
            }
        }

        game_random::Set_Random_Seed(original_seed);
    }

    int16_t __cdecl Star_N_Uncolonized_Worlds(int16_t star_index) {
        int16_t uncolonized_count = 0;
        const int8_t current_owner = MOX::colony[COLONY::colony_handle].owner;
        const s_star_data& star = MOX::star[star_index];

        for (int16_t i = 0; i < 5; ++i) {
            int16_t planet_idx = star.planet_index[i];
            if (planet_idx != -1) {
                const s_planet_data& planet = MOX::planet[planet_idx];

                bool is_available = false;
                if (planet.colony_index == -1) {
                    is_available = true;
                } else {
                    const s_colony& colony = MOX::colony[planet.colony_index];
                    if (colony.outpost_flag != 0 && colony.owner == current_owner) {
                        is_available = true;
                    }
                }

                if (is_available && planet.planet_type == PLANET_TYPE_PLANET) {
                    uncolonized_count++;
                }
            }
        }

        return uncolonized_count;
    }

    void __cdecl Clear_Last_App_Description_String() {
        last_app = -1;
    }

    void __cdecl Clear_Last_App_Desc() {
        Clear_Last_App_Description_String();
        buffer = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, 0xAF6);
    }

    void __cdecl Draw_Militarys_List() {
        int16_t font_offset;

        COLDRAW::Calculate_Squish_Step(13, 308, n_military_indexes, 19);
        font_offset = Calc_Font_Off();
        Get_Font_Off_For_Military(&font_offset);

        int16_t style_y_offset = 4 - font_offset;
        fonts::Set_Font_Style_Shadow_Down(style_y_offset, &l_font_color_array);

        for (int16_t i = 0; i < n_military_indexes; ++i) {
            int16_t mil_idx = military_indexes[i];
            s_colors* current_palette = &l_font_color_array;

            if (COLBLDG::Colony_Production_Is_Ship_Design(mil_idx)) {
                s_colony* colony = &MOX::colony[COLONY::colony_handle];

                if (colony->buildings[BUILDING_STAR_BASE] == 0 &&
                    colony->buildings[BUILDING_STAR_FORTRESS] == 0 &&
                    colony->buildings[BUILDING_BATTLESTATION] == 0 &&
                    MOX::player[MOX::PLAYER_NUM].ship_designs[COLBLDG::Colony_Production_Ship_Design_Index(mil_idx)].size >= SHIP_SIZE_LARGE &&
                    field_mode != 1) {
                    current_palette = &l_font_dim_color_array;
                } else if (field_mode == 1 && mil_idx == scanned_prod) {
                    if (ERIC::Global_Cycler(30, 2) != 0) {
                        current_palette = &l_font_dim_color_array;
                    }
                } else if (!COLCALC::Colony_Can_Build_Product(&MOX::colony[COLONY::colony_handle], mil_idx)) {
                    if (field_mode != 1) {
                        current_palette = &l_font_dim_color_array;
                    }
                }
            } else if (mil_idx == COLONY_PRODUCTION_TRANSPORT_SHIP ||
                mil_idx == COLONY_PRODUCTION_COLONY_SHIP ||
                mil_idx == COLONY_PRODUCTION_CUSTOM_DESIGN ||
                mil_idx == COLONY_PRODUCTION_OUTPOST_SHIP ||
                mil_idx == COLONY_PRODUCTION_DOOM_STAR) {
                if (Auto_Design_Type(mil_idx) == 0) {
                    if (mil_idx != COLONY_PRODUCTION_DOOM_STAR ||
                        !COLCALC::Colony_Can_Build_Product(&MOX::colony[COLONY::colony_handle], COLBLDG::Colony_Production_From_Ship_Design_Index(SHIP_SIZE_DOOM_STAR))) {
                        current_palette = &l_font_dim_color_array;
                    }
                }
            }

            fonts::Set_Font_Style_Shadow_Down(style_y_offset, current_palette);

            char* selection_name = Selection_Name(mil_idx);
            int16_t draw_y = (19 - COLONY::step_squish) * i + 20;

            BILL::Squeeze_Print(485, draw_y, selection_name, 138);
        }
    }

    void __cdecl Draw_Buildings_List() {
        COLDRAW::Calculate_Squish_Step(13, 469, n_building_indexes, 19);

        int16_t font_offset = Calc_Font_Off();
        Get_Font_Off_For_Buildings(&font_offset);

        fonts::Set_Font_Style_Shadow_Down(4 - font_offset, &l_font_color_array);

        if (n_building_indexes > 0) {
            for (int16_t i = 0; i < n_building_indexes; ++i) {
                int16_t building_id = building_indexes[i];
                s_colors colors = Build_List_Product_Colors(building_id);
                fonts::Set_Font_Style_Shadow_Down(4 - font_offset, &colors);

                char* building_name = Selection_Name(building_id);
                int16_t y_pos = (19 - COLONY::step_squish) * i + 20;

                BILL::Squeeze_Print(13, y_pos, building_name, 171);
            }
        }
    }

    int16_t __cdecl Auto_Design_Type(int16_t design_code) {
        if (design_code == COLONY_PRODUCTION_COLONY_SHIP) {
            return SHIP_TYPE_COLONY;
        }
        if (design_code == COLONY_PRODUCTION_OUTPOST_SHIP) {
            return SHIP_TYPE_OUTPOST;
        }
        if (design_code == COLONY_PRODUCTION_TRANSPORT_SHIP) {
            return SHIP_TYPE_TRANSPORT;
        }

        return SHIP_TYPE_COMBAT;
    }

    void __cdecl Draw_Build_Queue() {
        char buffer[500];
        int16_t y_pos = 335;
        int16_t scanned_id = scanned_prod;

        for (int16_t i = 0; i < 7; ++i) {
            int16_t active_idx = (active_prod[0] < BUILDING_COUNT) ? -1 : (active_prod[0] - BUILDING_COUNT);

            bool show_delete = false;

            if (i == active_idx) {
                if (current_item[i] == scanned_id) {
                    show_delete = true;
                } else if (i < 6 && current_item[i + 1] == scanned_id && current_item[i + 1] == COLONY_PRODUCTION_REPEAT) {
                    show_delete = true;
                } else if (i != 0 && current_item[i - 1] == scanned_id && current_item[i] == COLONY_PRODUCTION_REPEAT) {
                    show_delete = true;
                }
            }

            if (!show_delete) {
                if (i != 0 && current_item[i] == COLONY_PRODUCTION_REPEAT && (current_item[i] == scanned_id || current_item[i - 1] == scanned_id)) {
                    if (i == active_idx || i == active_idx + 1) {
                        show_delete = true;
                    }
                }
                if (!show_delete && i < 6 && current_item[i + 1] == COLONY_PRODUCTION_REPEAT && (current_item[i] == scanned_id || current_item[i + 1] == scanned_id)) {
                    if (i == active_idx || i == active_idx - 1) {
                        show_delete = true;
                    }
                }
            }

            const char* text_to_print;
            if (show_delete && scanned_field >= queue_fields[0] && scanned_field <= queue_fields[6] && current_item[i] != COLONY_PRODUCTION_NONE) {
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                char* item_name = Selection_Name(current_item[i]);
                snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(ESTR_DELETE_S), item_name);
                text_to_print = buffer;
            } else {
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                text_to_print = Selection_Name(current_item[i]);
            }

            BILL::Squeeze_Print_Centered(332, y_pos, text_to_print, 251);
            y_pos += 20;
        }
    }

    void __cdecl Get_App_Description_String(char* dest_str, uint32_t dest_str_size, int16_t app_id) {
        char lbx_name[20];
        s_help_record* record_ptr;

        TEXTBOX::Get_Help_Lbx_Name(lbx_name, sizeof(lbx_name));

        if (last_app != app_id) {
            last_app = app_id;
            record_ptr = static_cast<s_help_record*>(farload::Far_Reload_Data(lbx_name, 0, buffer, (int)app_id, 1, sizeof(s_help_record)));
        } else {
            record_ptr = reinterpret_cast<s_help_record*>(desc_record_ptr);
        }

        desc_record_ptr = (uint8_t*)record_ptr;

        strlcpy(dest_str, record_ptr->body, dest_str_size);
    }

    void __cdecl Get_Setting_Description_String(char* buffer, int16_t setting_id) {
        if (setting_id != last_setting) {
            last_setting = setting_id;
            last_app = -1;

            char* file_path = ESTRINGS::E_Strings(HESTR_170_MAX_POPULATION);
            char* loaded_data = (char*)farload::Far_Reload_Data(
                file_path,
                (int32_t)setting_id,
                COLBLDG::buffer,
                0,
                1,
                0x258 /* 600 */
            );

            if (loaded_data != nullptr) {
                strlcpy(g_msg, loaded_data, sizeof(g_msg));
            }
        }
    }

    char* __cdecl Selection_Name(int16_t id) {
        if (COLBLDG::Colony_Production_Is_Building(id)) {
            return COLONY::Real_Building_Name(id);
        }

        if (!COLBLDG::Colony_Production_Is_Queued_Ship(id)) {
            return Option_String(id);
        }

        int16_t index = COLBLDG::Colony_Production_Ship_Index(id);
        s_ship_data* ship = &MOX::ship[index];

        if (ship->status != 4) {
            return ship->d.name;
        }

        last_setting = -1;
        last_app = -1;

        char* opt_str = Option_String(COLONY_PRODUCTION_REFIT);
        char* format = ESTRINGS::E_Strings(45);
        snprintf(g_msg, sizeof(g_msg), format, opt_str, ship->d.name);

        return g_msg;
    }

    void __cdecl Draw_Current_Selection() {
        int16_t y_line_count = 0;
        char buffer[500];
        int16_t item_id = (scanned_prod != -1) ? scanned_prod : current_item[0];

        switch (item_id) {
            case COLONY_PRODUCTION_OUTPOST_SHIP: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Print_App_Description(209, 119, 248, 109 /* 0x6D */);
                COLONY::Remap_Draw_Ship_Seg_N_Centered(244, 56, 46 /* 0x2E */);
                break;
            }
            case COLONY_PRODUCTION_DOOM_STAR: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Ship_Design_Info(COLBLDG::Colony_Production_From_Ship_Design_Index(SHIP_SIZE_DOOM_STAR));
                break;
            }
            case COLONY_PRODUCTION_FREIGHTERS: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                fonts::Print(306, (y_line_count * 13 + 21), Selection_Name(item_id));
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Print_App_Description(209, 119, 248, 69 /* 0x45 */);
                COLONY::Remap_Draw_Ship_Seg_N_Centered(244, 56, 48 /* 0x30 */);
                break;
            }
            case COLONY_PRODUCTION_CUSTOM_DESIGN: {
                y_line_count = 1;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Ship_Design_Info(-54 /* 0x36 */);
                break;
            }
            case COLONY_PRODUCTION_COLONY_SHIP: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Print_App_Description(209, 119, 248, 41 /* 0x29 */);
                COLONY::Remap_Draw_Ship_Seg_N_Centered(244, 56, 45 /* 0x2D */);
                break;
            }
            case COLONY_PRODUCTION_TRANSPORT_SHIP: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                fonts::Print(306, (y_line_count * 13 + 21), Selection_Name(item_id));
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                COLONY::Remap_Draw_Ship_Seg_N_Centered(244, 56, 47 /* 0x2F */);
                Print_App_Description(209, 119, 248, 189 /* 0xBD */);
                break;
            }
            case COLONY_PRODUCTION_REPEAT: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Print_Selection_Centered(Selection_Name(item_id));
                break;
            }
            case COLONY_PRODUCTION_REFIT: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                fonts::Print(306, (y_line_count * 13 + 21), ESTRINGS::E_Strings(0x1BD));
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                fonts::Print(306, (y_line_count * 13 + 21), Selection_Name(item_id));
                snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x54), ESTRINGS::E_Strings(0xF3), ESTRINGS::E_Strings(0x1EA));
                y_line_count++;
                fonts::Print(306, (y_line_count * 13 + 21), buffer);
                y_line_count++;
                fonts::Set_Font_Style_Shadow_Down(5, &l_font_color_array);
                Print_Selection_Centered(ESTRINGS::E_Strings(0x7D));
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                break;
            }
            case COLONY_PRODUCTION_SPY: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                fonts::Print(306, (y_line_count * 13 + 21), Selection_Name(item_id));
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Print_App_Description(209, 119, 248, 166 /* 0xA6 */);
                Draw_Icon(COLONY::Spy_Anim(MOX::player[MOX::PLAYER_NUM].race));
                break;
            }
            case COLONY_PRODUCTION_SCIENTIST: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Draw_Icon(COLONY::People_Anim(0, 4, MOX::player[MOX::PLAYER_NUM].race));
                Print_App_Description(209, 119, 248, 7);
                break;
            }
            case COLONY_PRODUCTION_WORKER: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Draw_Icon(COLONY::People_Anim(0, 4, MOX::player[MOX::PLAYER_NUM].race));
                Print_App_Description(209, 119, 248, 8);
                break;
            }
            case COLONY_PRODUCTION_FARMER: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                Draw_Cost_And_Time_Info(COLONY::colony_handle, item_id, &y_line_count);
                Draw_Icon(COLONY::People_Anim(0, 4, MOX::player[MOX::PLAYER_NUM].race));
                Print_App_Description(209, 119, 248, 6);
                break;
            }
            case COLONY_PRODUCTION_HOUSING: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                Draw_Building_Centered_In_Window(14 /* 0xE */);
                Print_Setting_Description(209, 119, 248, 13 /* 0xD */);
                break;
            }
            case COLONY_PRODUCTION_TRADE_GOODS: {
                fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);
                BILL::Squeeze_Print(306, (y_line_count * 13 + 21), Selection_Name(item_id), 156);
                y_line_count += 2;
                animate::Draw(235 /* 0xEB */, 54 /* 0x36 */, COLONY::Prod_Anims(3, 1, 0));
                Print_Setting_Description(209, 119, 248, 12 /* 0xC */);
                break;
            }
            case COLONY_PRODUCTION_NONE:
            case 0:
                return;
            default: {
                if (COLBLDG::Colony_Production_Is_Building(item_id)) {
                    Draw_Building_Info(item_id);
                    return;
                }
                if (COLBLDG::Colony_Production_Is_Queued_Ship(item_id)) {
                    Draw_Ship_Info(item_id);
                    return;
                }
                if (COLBLDG::Colony_Production_Is_Ship_Design(item_id)) {
                    Draw_Ship_Design_Info(item_id);
                    return;
                }
                RUSS::PFMT(0, 0, ESTRINGS::E_Strings(248 /* 0xF8 */), item_id);
                key::Wait_For_Key();
                break;
            }
        }
    }

    void __cdecl Print_Selection_Centered(const char* text) {
        fonts::Print_Centered(244, 56, text);
    }

    void __cdecl Draw_Build_Queue_Popup() {
        char buffer[200];

        if (COLONY::full_draw != 0) {
            graphics::Fill(0, 0, 640, 479, 0);

            s_animation_header* anim = COLONY::C_Anims(0);
            animate::Draw(0, 0, anim);

            COLDRAW::Draw_Road_List();
            COLONY::Draw_Colony_Bldgs();

            replace::Interlaced_Gray_Scale_Off_Page(246, 9, 244, 10, 0);

            animate::Draw_Palette(COLBLDG::anims[0]);
            palette::Set_Palette_Changes(0, 255);
            remap::Update_Glass_Remap_Colors();

            graphics::Darken_Fill(202, 304, 463, 470);
            animate::Draw(0, 0, COLBLDG::anims[0]);

            char* planet_name = HACCESS::Get_Planet_Name(MOX::colony[COLONY::colony_handle].planet);
            char* format = ESTRINGS::E_Strings(ESTR_BUILD_LIST_FOR_S);
            snprintf(buffer, sizeof(buffer), format, planet_name);

            fonts::Set_Font_Style_Shadow_Down(3, &COLBLDG::l_font_color_array);
            BILL::Squeeze_Paragraph_Centered(235, 309, 194, 17, buffer, 2);

            if (MOX::previous_screen == SCREEN_COLONY_SUMMARY ||
                COLBLDG::colony_bldg_saved_screen == SCREEN_COLONY_SUMMARY) {
                animate::Draw_Palette(COLBLDG::anims[0]);
                remap::Update_Glass_Remap_Colors();
                animate::Draw_Palette(COLBLDG::colsum_pal_anim);
            } else {
                remap::Update_Glass_Remap_Colors();
            }

            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        fonts::Set_Font_Style_Shadow_Down(4, &COLBLDG::l_font_color_array);
        fonts::Set_Font_Spacing(1);

        COLBLDG::Draw_Buildings_List();
        COLBLDG::Draw_Militarys_List();
        COLBLDG::Draw_Build_Queue();
        COLBLDG::Draw_Current_Selection();
        COLBLDG::Draw_Active_Prod();
    }

    void __cdecl Print_Setting_Description(int16_t x, int16_t y, int16_t width, int16_t setting_id) {
        fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
        fonts::Set_Font_Spacing(1);
        fonts::Set_Font_Spacing_Width(2);

        Get_Setting_Description_String(g_msg, setting_id);

        BILL::_Print_Paragraph(x, y, width, g_msg, 3);

        fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
    }

    void __cdecl Print_App_Description(uint16_t x, int16_t y, uint16_t width, int16_t app_id) {
        char help_lbx_name[20];
        char description_buffer[1332];

        TEXTBOX::Get_Help_Lbx_Name(help_lbx_name, sizeof(help_lbx_name));
        fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
        fonts::Set_Font_Spacing(1);
        fonts::Set_Font_Spacing_Width(2);

        if (app_id != last_app) {
            last_app = app_id;
            desc_record_ptr = (uint8_t*)farload::Far_Reload_Data(help_lbx_name, 0, buffer, app_id, 1, sizeof(s_help_record));
        }

        const char* record_text = reinterpret_cast<const s_help_record*>(desc_record_ptr)->body;
        strlcpy(description_buffer, record_text, sizeof(description_buffer));

        BILL::_Print_Paragraph(x, y, width, description_buffer, 3);

        fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
    }

    void __cdecl Draw_Ship_Design_Info(int16_t id) {
        int16_t index = COLBLDG::Colony_Production_Ship_Design_Index(id);

        if (COLBLDG::Colony_Production_Is_Ship_Design(id)) {
            int16_t line_count = 0;
            fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);

            if (id != 4) {
                s_player* player = &MOX::player[MOX::PLAYER_NUM];
                s_ship_design* design = &player->ship_designs[index];

                int16_t y_pos = line_count * 13 + 21;
                line_count++;

                fonts::Print(306, y_pos, design->name);

                line_count++;

                fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
                fonts::Set_Font_Spacing(1);
                fonts::Set_Font_Spacing_Width(2);

                Draw_Cost_And_Time_Info(COLONY::colony_handle, id, &line_count);
                Draw_Ship_Design_Info2(design);
            }
        }

        COLONY::Remap_Draw_Ship_D_Centered(244, 56, index);
    }

    void __cdecl Draw_Building_Centered_In_Window(int16_t building_id) {
        graphics::Set_Window(203, 9, 285, 103);
        video::Clip_On();
        COLONY::Draw_Building_With_Bottom_Centered(244, 93, building_id, 5, 5);
        video::Clip_Off();
    }

    void __cdecl Draw_Cost_And_Time_Info(int16_t colony_idx, int16_t product_idx, int16_t* line_count) {
        char buffer[500];
        int32_t y_pos;
        char* format_str;

        // 1. Draw Production Cost
        int16_t cost = COLCALC::Colony_Product_Cost(&MOX::colony[colony_idx], product_idx);
        format_str = ESTRINGS::E_Strings(ESTR_COST_HERE_D);
        snprintf(buffer, sizeof(buffer), format_str, (int32_t)cost);

        y_pos = (*line_count) * 13 + 21;
        fonts::Print(306, (int16_t)y_pos, buffer);
        (*line_count)++;

        // 2. Draw Maintenance if applicable
        if (COLBLDG::Colony_Production_Is_Building(product_idx)) {
            int32_t maint = (int32_t)TECHDATA::buildings[product_idx].maintenance;
            format_str = ESTRINGS::E_Strings(HESTR_16F_PENALTY);
            snprintf(buffer, sizeof(buffer), format_str, maint);

            y_pos = (*line_count) * 13 + 21;
            fonts::Print(306, (int16_t)y_pos, buffer);
            (*line_count)++;
        }

        // 3. Draw Build Time
        int16_t build_time = COLONY::Calculate_Colony_Turn_Count_From_Scrap_For_Prod(colony_idx, product_idx);
        format_str = ESTRINGS::E_Strings(ESTR_BUILD_TIME_D);
        snprintf(buffer, sizeof(buffer), format_str, (int32_t)build_time);

        y_pos = (*line_count) * 13 + 21;
        BILL::Squeeze_Print(306, (int16_t)y_pos, buffer, 156);
        (*line_count)++;

        // 4. Draw Turns Left if currently producing
        if (MOX::colony[colony_idx].producing[0] == product_idx) {
            int16_t turns_left = COLONY::Calculate_Current_Production_Turn_Count(colony_idx);
            format_str = ESTRINGS::E_Strings(ESTR_TURNS_LEFT_D);
            snprintf(buffer, sizeof(buffer), format_str, (int32_t)turns_left);

            y_pos = (*line_count) * 13 + 21;
            BILL::Squeeze_Print(306, (int16_t)y_pos, buffer, 156);
            (*line_count)++;
            return;
        }

        // 5. Special case for Gravity Generator (ID 25)
        if (COLBLDG::Colony_Production_Is_Building(product_idx) && product_idx == BUILDING_GRAVITY_GENERATOR) {
            int16_t planet_idx = MOX::colony[colony_idx].planet;
            int8_t gravity_class = MOX::planet[planet_idx].gravity_class;
            char* gravity_str = MOX::planet_gravity_string[gravity_class];

            format_str = ESTRINGS::E_Strings(ESTR_GRAVITY_HERE_S);
            snprintf(buffer, sizeof(buffer), format_str, gravity_str);

            y_pos = (*line_count) * 13 + 21;
            BILL::Squeeze_Print(306, (int16_t)y_pos, buffer, 156);
            (*line_count)++;
        }
    }

    void __cdecl Draw_Building_Info(int16_t building_idx) {
        // BUG: building_idx might be -1 when accessing buildings
        if (TECHDATA::buildings[building_idx].group == 7) {
            s_animation_header* anim = COLONY::Satellite_Anim(building_idx);
            Draw_Icon(anim);
        } else {
            Draw_Building_Centered_In_Window(building_idx);
        }

        if (building_idx != -1) {
            int16_t y_offset = 0;
            fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);

            int y_pos = (y_offset * 13) + 21;
            y_offset++;

            char* building_name = COLONY::Real_Building_Name(building_idx);
            BILL::Squeeze_Print(0x132, y_pos, building_name, 0x9c);

            y_offset++;
            fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);

            Draw_Cost_And_Time_Info(COLONY::colony_handle, building_idx, &y_offset);

            y_offset = 0;
            Print_App_Description(0xd1, 0x77, 0xf8, *(int16_t*)&TECHDATA::buildings[building_idx].tech_app);
        }
    }

    void __cdecl Draw_Ship_Design_Info2(s_ship_design* design) {
        char buffer[20000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        fonts::Set_Font_Style_Shadow_Down(1, &l_font_color_array);
        fonts::Set_Font_Spacing(1);
        fonts::Set_Font_Spacing_Width(2);

        Build_Ship_Design_Info_Formatted_Paragraph(design, &builder);
        BILL::Squeeze_Print_Formatted_Paragraph(212, 114, 248, 186, buffer, 0);
    }

    static void Append_Ship_Design_Info_Formatted_Paragraph_Details(s_ship_design* design, STRBUILD::s_builder* builder) {
        char prefix_str[16];
        char wide_item_strings[8][200];

        strlcpy(prefix_str, strings::s_t_T0_5_124__0055ac38, sizeof(prefix_str));
        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(70), prefix_str);

        for (int i = 0; i < 8; i++) {
            wide_item_strings[i][0] = '\0';
        }

        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(99), strings::s_0_0055110c, strings::s__0055ac34, design->cost);
        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(119), strings::s_0_0055110c, strings::s__0055ac34, TECHDATA::shields[design->shield_type].name);
        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(98), strings::s_0_0055110c, strings::s__0055ac34, TECHDATA::computers[design->computer_type].name);
        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(91), strings::s_0_0055110c, strings::s__0055ac34, TECHDATA::armor[design->armor_type].name);
        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(104), strings::s_0_0055110c, strings::s__0055ac34, TECHDATA::drives[design->ftl_type].name);
        STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(130), strings::s_0_0055110c);

        int weapons_added = 0;
        int num_wide_weapons = 0;
        int wide_weapon_counts[8];

        for (int i = 0; i < 8; i++) {
            if (design->ship_weapon[i].type != 0) {
                char temp_weapon_str[500];
                Build_Weapon_String(&design->ship_weapon[i], temp_weapon_str, sizeof(temp_weapon_str), '\0', design->ship_weapon[i].count);

                if (fonts::Get_String_Width(temp_weapon_str) <= 110) {
                    int str_id = ((weapons_added % 2) == 0) ? 87 : 88;
                    STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(str_id), strings::s_0_0055110c, (int)design->ship_weapon[i].count, temp_weapon_str);
                    weapons_added++;
                } else {
                    strlcpy(wide_item_strings[num_wide_weapons], temp_weapon_str, sizeof(wide_item_strings[num_wide_weapons]));
                    wide_weapon_counts[num_wide_weapons] = design->ship_weapon[i].count;
                    num_wide_weapons++;
                }
            }
        }

        if (num_wide_weapons > 0) {
            if ((weapons_added % 2) != 0) {
                STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(127));
            }
            for (int i = 0; i < num_wide_weapons; i++) {
                STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(88), strings::s_0_0055110c, wide_weapon_counts[i], wide_item_strings[i]);
            }
        }

        if ((weapons_added % 2) == 1) {
            STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(127));
        }

        for (int i = 0; i < 8; i++) {
            wide_item_strings[i][0] = '\0';
        }

        int specials_added = 0;
        int num_wide_specials = 0;
        bool has_specials_prefix = false;

        for (int i = 0; i < 39; i++) {
            if (struct_::Test_Bit_Field(design->special_device_flags, i)) {
                if (!has_specials_prefix) {
                    has_specials_prefix = true;
                    STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(129), strings::s_0_0055110c);
                }

                if (fonts::Get_String_Width(TECHDATA::specials[i].name) <= 115) {
                    int str_id = ((specials_added % 2) == 0) ? 89 : 90;
                    STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(str_id), strings::s_0_0055110c, TECHDATA::specials[i].name);
                    specials_added++;
                } else {
                    strlcpy(wide_item_strings[num_wide_specials], TECHDATA::specials[i].name, sizeof(wide_item_strings[num_wide_specials]));
                    num_wide_specials++;
                }
            }
        }

        if (num_wide_specials > 0) {
            if ((specials_added % 2) != 0) {
                STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(127));
            }
            for (int i = 0; i < num_wide_specials; i++) {
                STRBUILD::Sprintf(builder, ESTRINGS::E_Strings(90), strings::s_0_0055110c, wide_item_strings[i]);
            }
        }
    }

    void __cdecl Build_Ship_Design_Info_Formatted_Paragraph(s_ship_design* design, STRBUILD::s_builder* builder) {
        Append_Ship_Design_Info_Formatted_Paragraph_Details(design, builder);
    }

    void __cdecl Build_Ship_Info_Formatted_Paragraph(s_ship_data* ship, STRBUILD::s_builder* builder) {
        if (ship->officer_index < 0) {
            STRBUILD::Sprintf(
                builder,
                ESTRINGS::E_Strings(79),
                strings::s_0_0055110c,
                ship->d.name,
                FLT2::Crew_Description_String((int16_t)(ship - MOX::ship)),
                strings::s_1_00551110);
        } else {
            char leader_name[500];
            char crew_desc[500];
            strlcpy(leader_name, OFFICER::Leader_Name(ship->officer_index, '\0'), sizeof(leader_name));
            strlcpy(crew_desc, FLT2::Crew_Description_String((int16_t)(ship - MOX::ship)), sizeof(crew_desc));

            STRBUILD::Sprintf(
                builder,
                ESTRINGS::E_Strings(78),
                strings::s_0_0055110c,
                ship->d.name,
                leader_name,
                crew_desc);
        }

        Append_Ship_Design_Info_Formatted_Paragraph_Details(&ship->d, builder);
    }

    void __cdecl Build_Ship_Info_Formatted_Paragraph_Binary(void* ship_or_design, STRBUILD::s_builder* builder, char include_crew_details) {
        // The original binary routes both plain design pointers and ship-backed design subobjects through 0050A5C0.
        // Split the source API into design-only and ship-only entry points, and keep this wrapper only for binary call compatibility.
        if (include_crew_details != '\0') {
            Build_Ship_Info_Formatted_Paragraph((s_ship_data*)ship_or_design, builder);
        } else {
            Build_Ship_Design_Info_Formatted_Paragraph((s_ship_design*)ship_or_design, builder);
        }
    }

    int16_t __cdecl Colbldg_Create_Ship(int16_t design_slot) {
        int16_t ship_idx = Create_Ship(
            MOX::colony[COLONY::colony_handle].owner,
            design_slot,
            COLONY::colony_handle);

        if (ship_idx != -1) {
            MOX::ship[ship_idx].status = 6;
        }

        return ship_idx;
    }

    int16_t __cdecl First_Prod_Queue_Slot(int16_t prod_idx) {
        for (int16_t i = 0; i < 7; ++i) {
            if (COLBLDG::Colony_Production_Is_Building(prod_idx)) {
                if (current_item[i] == prod_idx) {
                    return i;
                }
            } else {
                if (COLBLDG::Colony_Production_Is_Ship_Design(prod_idx)) {
                    return -1;
                }

                int16_t curr = current_item[i];
                if (COLBLDG::Colony_Production_Is_Queued_Ship(curr)) {
                    if (prod_idx == curr) {
                        return i;
                    }
                } else if (prod_idx >= BUILDING_COUNT) {
                    if (prod_idx - i == BUILDING_COUNT) {
                        return i;
                    }
                } else if (prod_idx == COLONY_PRODUCTION_SCIENTIST ||
                    prod_idx == COLONY_PRODUCTION_FARMER ||
                    prod_idx == COLONY_PRODUCTION_WORKER ||
                    prod_idx == COLONY_PRODUCTION_SPY ||
                    prod_idx == COLONY_PRODUCTION_HOUSING ||
                    prod_idx == COLONY_PRODUCTION_TRADE_GOODS ||
                    prod_idx == COLONY_PRODUCTION_REPEAT ||
                    prod_idx == COLONY_PRODUCTION_FREIGHTERS) {
                    if (prod_idx == curr) {
                        return i;
                    }
                }
            }
        }
        return -1;
    }

    int16_t __cdecl First_Prod_Colony_Queue_Slot(int16_t colony_idx, int16_t item_id) {
        for (int16_t i = 0; i < 7; ++i) {
            if (COLBLDG::Colony_Production_Is_Building(item_id)) {
                if (MOX::colony[colony_idx].producing[i] == item_id) {
                    return i;
                }
            } else {
                if (COLBLDG::Colony_Production_Is_Ship_Design(item_id)) {
                    return -1;
                }

                int16_t produced_item = MOX::colony[colony_idx].producing[i];

                if (COLBLDG::Colony_Production_Is_Queued_Ship(produced_item)) {
                    if (item_id == produced_item) {
                        return i;
                    }
                } else if (item_id >= BUILDING_COUNT) {
                    if (item_id - i == BUILDING_COUNT) {
                        return i;
                    }
                } else {
                    if (item_id == COLONY_PRODUCTION_SCIENTIST ||
                        item_id == COLONY_PRODUCTION_FARMER ||
                        item_id == COLONY_PRODUCTION_WORKER ||
                        item_id == COLONY_PRODUCTION_SPY ||
                        item_id == COLONY_PRODUCTION_HOUSING ||
                        item_id == COLONY_PRODUCTION_TRADE_GOODS ||
                        item_id == COLONY_PRODUCTION_REPEAT ||
                        item_id == COLONY_PRODUCTION_FREIGHTERS) {
                        if (item_id == produced_item) {
                            return i;
                        }
                    }
                }
            }
        }

        return -1;
    }

    static bool Is_Queue_Blocker(int16_t item) {
        return item == COLONY_PRODUCTION_HOUSING ||
            item == COLONY_PRODUCTION_TRADE_GOODS ||
            item == COLONY_PRODUCTION_REPEAT;
    }

    static void Delete_Queue_Blockers() {
        for (int i = 0; i < 7; ++i) {
            if (Is_Queue_Blocker(current_item[i]) &&
                Something_Behind_Settting() != 0) {
                Squish_Out_Queue_Slot(i);
                --i;
            }
        }
    }

    static void Handle_Queue_Blockers() {
        if (Something_Behind_Settting() == 0) {
            return;
        }

        if (MOX::settings.auto_delete_tg_housing != 0) {
            Delete_Queue_Blockers();
            return;
        }

        if (!warn_blockers) {
            return;
        }

        int16_t blocker = COLONY_PRODUCTION_NONE;
        for (int i = 0; i < 7; ++i) {
            if (Is_Queue_Blocker(current_item[i])) {
                blocker = current_item[i];
                break;
            }
        }

        char buffer[500];
        char* blocker_name = Selection_Name(blocker);
        snprintf(
            buffer,
            sizeof(buffer),
            ESTRINGS::E_Strings(0x3b),
            blocker_name,
            blocker_name);

        if (GENDRAW::Do_Confirmation_Box(buffer) != 0) {
            Delete_Queue_Blockers();
        }
    }

    void __cdecl Evaluate_Input(int16_t* selected_field_id, int16_t* scanned_field_id) {
        const int16_t previous_military_count = n_military_indexes;
        scanned_prod = -1;
        scanned_field = *scanned_field_id;

        if (return_to_build_popup_from_custom_design != 0 || hacky_speed_refitted_ship_flag != 0) {
            return_to_build_popup_from_custom_design = 0;
            hacky_speed_refitted_ship_flag = 0;
            field_mode = 0;
            COLONY::Reset_Mouse_Picture();

            if (MOX::temp_star_handle == 0) {
                MOX::refit_ship = 0;
                Clear_Pending_Refit_Design();
            } else if (MOX::refit_ship != 0) {
                Add_Refited_Ship_To_Queue();
                MOX::refit_ship = 0;
            }
        }

        int16_t input_field = *selected_field_id;

        if (input_field == shortcut_previous_colony) {
            input_field = fields[8];
            *selected_field_id = input_field;
        } else if (input_field == shortcut_next_colony) {
            input_field = fields[9];
            *selected_field_id = input_field;
        } else if (input_field == shortcut_accept) {
            input_field = fields[3];
            *selected_field_id = input_field;
        }

        if (input_field == automatic_design_command) {
            const int16_t player_idx = MOX::PLAYER_NUM;
            const uint8_t saved_civilization_level =
                MOX::settings.starting_civilization_level;
            if (MOX::player[player_idx].tech_applications[TECH_APP_DOOM_STAR_CONSTRUCTION] ==
                TECH_RESEARCH_STATUS_RESEARCHED) {
                MOX::settings.starting_civilization_level = 2;
            }
            INITSHIP::Init_Ship_Designs(player_idx);
            MOX::settings.starting_civilization_level = saved_civilization_level;
            active_prod[0] = -1;
            field_mode = 0;
            Build_Arrays();
            Do_Draw_Build_Queue();
            *selected_field_id = 0;
            return;
        }

        if (input_field == reload_config_command ||
            input_field == reload_interface_command) {
            config::s_cfg_diagnostic diagnostic;
            const bool network_game = MOX::game_type == 2 || MOX::game_type == 3;
            const bool reload_all = input_field == reload_config_command;
            if (reload_all && network_game) {
                GENDRAW::Warning_Box(
                    "You may not reload config in a multiplayer game.");
            } else if (!(reload_all
                               ? config::Reload_All_Active_Config_Groups(
                                     &diagnostic)
                               : config::Reload_Active_Config_Groups(
                                     network_game, &diagnostic))) {
                GENDRAW::Warning_Box(diagnostic.message.c_str());
            } else if (reload_all) {
                GENDRAW::Message_Box("config reloaded");
            }
            Build_Arrays();
            Do_Draw_Build_Queue();
            *selected_field_id = 0;
            return;
        }

        if (input_field == crunch_command) {
            MOX::colony[COLONY::colony_handle].production_spent = 32000;
            MAINSCR::Set_Player_Cheated_Flag(MOX::PLAYER_NUM, 7);
            Do_Draw_Build_Queue();
            *selected_field_id = 0;
            return;
        }

        if (Apply_Build_Queue_Hotkey(input_field)) {
            *selected_field_id = 0;
            return;
        }

        if (fields[2] == input_field && BUILD_QUEUE::autobuild_settings.enabled != 0) {
            if (MOX::colony[COLONY::colony_handle].auto_building !=
                BUILD_QUEUE::AUTOBUILD_DISABLED) {
                MOX::colony[COLONY::colony_handle].auto_building =
                    BUILD_QUEUE::AUTOBUILD_DISABLED;
            } else {
                if (COLONY::Prompt_For_Autobuild_Queue(COLONY::colony_handle)) {
                    memcpy(current_item,
                        MOX::colony[COLONY::colony_handle].producing,
                        sizeof(current_item));
                    active_prod[0] = -1;
                    field_mode = 0;
                    Build_Arrays();
                }
            }
            colony_auto_building =
                MOX::colony[COLONY::colony_handle].auto_building != 0;
            Do_Draw_Build_Queue();
            *selected_field_id = 0;
            return;
        }

        if (fields[3] == input_field) {
            field_mode = 0;
            COLONY::Reset_Mouse_Picture();
            Commit_Autobuild_State();
            Handle_Queue_Blockers();
            Do_Exit_Screen_Cleanup();
        } else if (fields[0] == input_field || fields[1] == input_field) {
            if (field_mode == 0 && active_prod[0] == -1) {
                bool flag = false;
                for (int i = 0; i < 7; ++i) {
                    if (flag) {
                        break;
                    }
                    int16_t item = current_item[i];
                    if (COLBLDG::Colony_Production_Is_Queued_Ship(item)) {
                        if (ERIC::Sint_Is_In_List(item, MOX::colony[COLONY::colony_handle].producing, 14) == 0) {
                            if (MOX::ship[COLBLDG::Colony_Production_Ship_Index(item)].status == 4) {
                                if (GENDRAW::Do_Confirmation_Box(ESTRINGS::E_Strings(0x1dc)) == 0) {
                                    *selected_field_id = 0;
                                    return;
                                }
                                flag = true;
                                Do_Draw_Build_Queue();
                            }
                        }
                    }
                }

                for (int i = 0; i < 7; ++i) {
                    int16_t item = current_item[i];
                    if (COLBLDG::Colony_Production_Is_Queued_Ship(item)) {
                        ERIC::Sint_Is_In_List(item, MOX::colony[COLONY::colony_handle].producing, 14);
                    }
                }

                Remove_Ships(0);
            } else {
                field_mode = 0;
                COLONY::Reset_Mouse_Picture();
                active_prod[0] = -1;
                *selected_field_id = 0;
                COLONY::Reset_Mouse_Picture();
            }
        } else if (fields[5] == input_field) {
            mouse::Set_Mouse_List(&mouse_list_which_ship, 1);
            field_mode = 1;
        } else if (fields[6] == input_field) {
            field_mode = 2;
            mouse::Set_Mouse_List(&mouse_list_what, 1);
        } else if (fields[4] == input_field) {
            int owner = MOX::colony[COLONY::colony_handle].owner;
            if (MOX::player[owner].tech_fields[TECH_FIELD_CHEMISTRY] == TECH_RESEARCH_STATUS_RESEARCHED &&
                MOX::player[owner].tech_fields[TECH_FIELD_NUCLEAR_FISSION] == TECH_RESEARCH_STATUS_RESEARCHED) {
                field_mode = 0;
                COLONY::Reset_Mouse_Picture();

                int16_t refit_ship = COLREFIT::Colony_Refit_Popup();
                if (refit_ship != -1) {
                    Do_Draw_Build_Queue();
                    int16_t popup2 = COLREFIT::Colony_Refit2_Popup(refit_ship);
                    if (popup2 == -3) {
                        MOX::temp_star_handle = 1;
                        MOX::refit_ship = 1;
                        hacky_speed_refitted_ship_flag = 1;
                    } else if (popup2 == -2) {
                        Set_Pending_Refit_Design(refit_ship, &MOX::ship[refit_ship].d);
                        MOX::refit_ship = 1;
                        return_to_build_popup_from_custom_design = 1;
                        MOX::temp_star_handle = SHIP_SIZE_DOOM_STAR;
                    }
                    Do_Draw_Build_Queue();
                } else {
                    Do_Draw_Build_Queue();
                }
            } else {
                GENDRAW::Help(ESTRINGS::E_Strings(0x244));
                Do_Draw_Build_Queue();
            }
        } else if (fields[2] != input_field) {
            if (fields[10] == input_field) {
                Do_Exit_Screen_Cleanup();
                COLONY::Tested_Colony_Buys_Outright(COLONY::colony_handle);
                active_prod[0] = -1;
                Do_Draw_Build_Queue();
                ::fields::Clear_Mouse_Buffer();
                ::fields::Set_Input_Delay(3);
            } else if (fields[8] == input_field) {
                Handle_Queue_Blockers();
                int16_t next_colony = COLONY::Get_Next_Colony(COLONY::colony_handle, 0);
                field_mode = 0;
                COLONY::Reset_Mouse_Picture();
                Commit_Autobuild_State();
                Do_Exit_Screen_Cleanup();
                if (next_colony != COLONY::colony_handle) {
                    COLONY::Set_Colony_Screen_To_New_Colony(next_colony);
                }
                COLONY::full_draw = 1;
            } else if (fields[9] == input_field) {
                Handle_Queue_Blockers();
                int16_t next_colony = COLONY::Get_Next_Colony(COLONY::colony_handle, 1);
                if (next_colony != COLONY::colony_handle) {
                    COLONY::Set_Colony_Screen_To_New_Colony(next_colony);
                }
                field_mode = 0;
                COLONY::Reset_Mouse_Picture();
                Commit_Autobuild_State();
                Do_Exit_Screen_Cleanup();
                COLONY::full_draw = 1;
            } else if (fields[7] == input_field) {
                COLONY::cheats = (COLONY::cheats == 0) ? 1 : 0;
                GENDRAW::Help(ESTRINGS::E_Strings(0xda));
                Do_Draw_Build_Queue();
            }
        }

        if (field_mode != 1) {
            for (int i = 0; i < n_building_indexes; ++i) {
                if (*scanned_field_id == building_fields[i]) {
                    scanned_prod = building_indexes[i];
                }
                if (*selected_field_id == building_fields[i]) {
                    int16_t bidx = building_indexes[i];
                    if (Has_Prod_In_Queue(bidx)) {
                        int16_t first_slot = First_Prod_Queue_Slot(bidx);
                        if (MOX::colony[COLONY::colony_handle].bought_outright != 0 && first_slot == 0) {
                            GENDRAW::Help(ESTRINGS::E_Strings(0x23a));
                            Do_Draw_Build_Queue();
                            active_prod[0] = -1;
                            COLONY::Reset_Mouse_Picture();
                            return;
                        }
                        Delete_From_Queue(first_slot);
                        if (active_prod[0] >= BUILDING_COUNT) {
                            if ((active_prod[0] - first_slot) == BUILDING_COUNT) {
                                active_prod[0] = -1;
                            }
                        }
                    } else {
                        if (Add_To_Queue(bidx) == 0) {
                            GENDRAW::Help(ESTRINGS::E_Strings(0x188));
                            Do_Draw_Build_Queue();
                        }
                    }
                }
            }
        }

        for (int i = 0; i < n_military_indexes; ++i) {
            if (*scanned_field_id == military_fields[i] && military_indexes[i] != COLONY_PRODUCTION_SEPARATOR) {
                scanned_prod = military_indexes[i];
            }
            if (*selected_field_id == military_fields[i] && military_indexes[i] != COLONY_PRODUCTION_SEPARATOR) {
                if (military_indexes[i] == BUILDING_COLONY_BASE) {
                    COLONY::full_draw = 1;
                }

                if (military_indexes[i] == BUILDING_COLONY_BASE) {
                    int16_t star_idx = MOX::planet[MOX::colony[COLONY::colony_handle].planet].star_index;
                    int16_t uncolonized = Star_N_Uncolonized_Worlds(star_idx);
                    int16_t in_queue = N_Prods_In_Queue(BUILDING_COLONY_BASE);

                    if (uncolonized <= in_queue) {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x232));
                        Do_Draw_Build_Queue();
                        continue;
                    }
                }

                int16_t midx = military_indexes[i];
                if (field_mode == 1) {
                    if (COLBLDG::Colony_Production_Is_Ship_Design(midx)) {
                        return_to_build_popup_from_custom_design = 1;
                        MOX::temp_star_handle = COLBLDG::Colony_Production_Ship_Design_Index(midx);
                        COLONY::Reset_Mouse_Picture();
                        return;
                    }
                } else {
                    if (Add_To_Queue(midx) == 0) {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x188));
                        Do_Draw_Build_Queue();
                    }
                }
            }
        }

        for (int i = 0; i < 7; ++i) {
            if (*scanned_field_id == queue_fields[i]) {
                scanned_prod = current_item[i];
            }
            if (*selected_field_id == queue_fields[i]) {
                if (active_prod[0] == -1) {
                    int16_t selected_idx = static_cast<int16_t>(i);
                    if (selected_idx > 0 &&
                        current_item[selected_idx] == COLONY_PRODUCTION_REPEAT) {
                        selected_idx--;
                    }
                    if (MOX::colony[COLONY::colony_handle].bought_outright != 0 &&
                        selected_idx == 0) {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x23a));
                        Do_Draw_Build_Queue();
                        return;
                    }
                    active_prod[0] = selected_idx + BUILDING_COUNT;
                } else {
                    int16_t active_idx = active_prod[0] - BUILDING_COUNT;
                    bool do_delete = false;

                    if (MOX::colony[COLONY::colony_handle].bought_outright != 0) {
                        if (i == 0 || active_idx == 0 ||
                            (i == 1 && current_item[i] == COLONY_PRODUCTION_REPEAT)) {
                            GENDRAW::Help(ESTRINGS::E_Strings(0x23a));
                            Do_Draw_Build_Queue();
                            active_prod[0] = -1;
                            COLONY::Reset_Mouse_Picture();
                            return;
                        }
                    }

                    if (i == active_idx) {
                        do_delete = true;
                    } else if (current_item[active_idx] == COLONY_PRODUCTION_REPEAT && active_idx != 0 && (active_idx - i) == 1) {
                        do_delete = true;
                    } else if ((active_idx + 1) < 7 && current_item[active_idx + 1] == COLONY_PRODUCTION_REPEAT && (active_idx - i) == -1) {
                        do_delete = true;
                    }

                    if (do_delete) {
                        Delete_From_Queue(i);
                        active_prod[0] = -1;
                        COLONY::Reset_Mouse_Picture();
                    } else {
                        Insert_Before_Queue_Slot(active_idx, i);
                        Defragment_Queue();
                        COLONY::Reset_Mouse_Picture();
                    }
                }

                if (field_mode != 0) {
                    field_mode = 0;
                    COLONY::Reset_Mouse_Picture();
                    active_prod[0] = -1;
                    *selected_field_id = 0;
                    COLONY::Reset_Mouse_Picture();
                }
            }
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            Do_Draw_Build_Queue();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        if (selected_field_id != nullptr) {
            COLCALC::Colony_Calculation(COLONY::colony_handle);
        }

        Calculate_Military_Array();
        if (n_military_indexes != previous_military_count) {
            Update_Fields();
        }
    }

    void __cdecl Draw_Active_Prod() {
        int16_t active_idx = active_prod[0];
        if (active_idx == -1) {
            return;
        }

        if (active_idx >= BUILDING_COUNT) {
            int16_t rel_idx = active_idx - BUILDING_COUNT;
            int16_t current = current_item[rel_idx];
            int16_t scanned_prod = COLBLDG::scanned_prod;
            int16_t scanned_field = COLBLDG::scanned_field;

            bool is_match = false;
            if (current == scanned_prod) {
                is_match = true;
            } else if (rel_idx != 0 && current == COLONY_PRODUCTION_REPEAT && current_item[rel_idx - 1] == scanned_prod) {
                is_match = true;
            } else if (rel_idx < 6 && current_item[rel_idx + 1] == COLONY_PRODUCTION_REPEAT && scanned_prod == COLONY_PRODUCTION_REPEAT) {
                is_match = true;
            }

            if (is_match && scanned_field >= queue_fields[0] && scanned_field <= queue_fields[6] && current != -1) {
                int16_t base_y = rel_idx * 20;
                graphics::Box(207, base_y + 332, 252, 16, 1, 239);

                if (rel_idx != 0 && current == COLONY_PRODUCTION_REPEAT) {
                    graphics::Box(207, base_y + 312, 252, 16, 1, 239);
                }

                if (rel_idx < 6 && current_item[rel_idx + 1] == COLONY_PRODUCTION_REPEAT) {
                    graphics::Box(207, base_y + 352, 252, 16, 1, 239);
                }

                COLONY::Reset_Mouse_Picture();
            } else {
                int16_t base_y = rel_idx * 20;
                graphics::Box(207, base_y + 332, 252, 16, 1, 100);

                if (rel_idx != 0 && current == COLONY_PRODUCTION_REPEAT) {
                    graphics::Box(207, base_y + 312, 252, 16, 1, 100);
                }

                if (rel_idx < 6 && current_item[rel_idx + 1] == COLONY_PRODUCTION_REPEAT) {
                    graphics::Box(207, base_y + 352, 252, 16, 1, 100);
                }

                mouse::Set_Mouse_List(mouse_list_to, 4);
            }
        } else {
            for (int16_t i = 0; i < n_building_indexes; ++i) {
                if (building_indexes[i] == active_idx) {
                    int16_t step = COLONY::step_squish;
                    int16_t height = 18 - step;
                    int16_t row_spacing = 19 - step;
                    int16_t y = row_spacing * i + 18;
                    graphics::Box(8, y, 171, height, 1, 100);
                }
            }
        }
    }

    void __cdecl Draw_Icon(s_animation_header* anim) {
        ERIC::Draw_Centered(244, 56, anim);
    }

    void __cdecl Clear_Colony_Queue(int16_t colony_idx) {
        int16_t count = 7;
        do {
            Delete_From_Colony_Queue(colony_idx, 0);
            count--;
        } while (count != 0);
    }

    void __cdecl Delete_Prod_From_Colony_Queue(int16_t colony_idx, int16_t product_id) {
        int16_t slot_idx;

        slot_idx = COLBLDG::First_Prod_Colony_Queue_Slot(colony_idx, product_id);
        while (slot_idx != -1) {
            COLBLDG::Delete_From_Colony_Queue(colony_idx, slot_idx);
            slot_idx = COLBLDG::First_Prod_Colony_Queue_Slot(colony_idx, product_id);
        }
    }

    void __cdecl Delete_Ship(int16_t ship_index) {
        MOX::ship[ship_index].status = 5;
        MOX::ship[ship_index].location = -1;
    }

    void __cdecl Draw_Ship_Info(int16_t item) {
        int16_t index = COLBLDG::Colony_Production_Ship_Index(item);

        if (COLBLDG::Colony_Production_Is_Queued_Ship(item)) {
            index = COLBLDG::Colony_Production_Ship_Index(item);
            int16_t y_offset_counter = 0;
            fonts::Set_Font_Style_Shadow_Down(4, &l_font_color_array);

            int16_t current_y = y_offset_counter * 13 + 21;
            y_offset_counter++;

            s_ship_data* ship = &MOX::ship[index];
            fonts::Print(306, current_y, ship->d.name);

            y_offset_counter++;
            fonts::Set_Font_Style_Shadow_Down(3, &l_font_color_array);
            fonts::Set_Font_Spacing(1);
            fonts::Set_Font_Spacing_Width(2);

            COLBLDG::Draw_Cost_And_Time_Info(COLONY::colony_handle, item, &y_offset_counter);
            COLBLDG::Draw_Ship_Design_Info2(&ship->d);
        }

        COLONY::Remap_Draw_Ship_Centered(244, 56, index);
    }

    void __cdecl Squish_Out_Colony_Queue_Slot(int16_t colony_idx, int16_t queue_idx) {
        s_colony* col = &MOX::colony[colony_idx];

        do {
            for (int16_t i = queue_idx; i < 6; i++) {
                col->producing[i] = col->producing[i + 1];
            }
            col->producing[6] = COLONY_PRODUCTION_NONE;
        } while (col->producing[queue_idx] == COLONY_PRODUCTION_REPEAT);
    }

    /**
     * @brief Removes an item from a colony's production queue and cleans up associated ship data if necessary.
     *
     * @param colony_idx Index of the colony in the global colony array.
     * @param queue_idx Index of the production queue slot to delete.
     */
    void __cdecl Delete_From_Colony_Queue(int16_t colony_idx, int16_t queue_idx) {
        if (queue_idx != -1) {
            if (queue_idx == 0 &&
                !BUILD_QUEUE::Unbuy_Product(&MOX::colony[colony_idx])) {
                return;
            }

            int16_t producing_id = MOX::colony[colony_idx].producing[queue_idx];

            // Queued ship production items use the queued-ship encoding helper/base.
            if (COLBLDG::Colony_Production_Is_Queued_Ship(producing_id)) {
                // Calculate original ship design ID or instance ID from the queue value
                COLBLDG::Delete_Ship(COLBLDG::Colony_Production_Ship_Index(producing_id));
            }

            COLBLDG::Squish_Out_Colony_Queue_Slot(colony_idx, queue_idx);
        }
    }

    void __cdecl Add_Refited_Ship_To_Queue() {
        MOX::refit_ship = 0;
        int16_t ship_n = refitting_ship_n;
        s_ship_design* pending_design = Pending_Refit_Design();
        if (pending_design == nullptr) {
            return;
        }

        if (!Add_To_Queue(COLBLDG::Colony_Production_From_Ship_Index(ship_n))) {
            GENDRAW::Help(ESTRINGS::E_Strings(ESTR_NO_ROOM_IN_QUEUE_SHIP_NOT_REFITTED));
            Clear_Pending_Refit_Design();
            Do_Draw_Build_Queue();
            MAINSCR::Update_Ship_Icons_In_Between_Turns();
            return;
        }

        int8_t owner = MOX::colony[COLONY::colony_handle].owner;
        s_ship_design old_design = MOX::ship[ship_n].d;
        s_ship_design new_design = *pending_design;
        new_design.cost = DESIGN::Cost_Of_Design(owner, &new_design);
        const AIDATA::s_refit_rule_inputs refit_rules{
            TECHDATA::hull_data, COLCALC::government_bonuses, &DESIGN::alternative_refit};
        int16_t refit_cost = AIBUILD::Refit_Cost(&refit_rules, &MOX::player[owner], &old_design, &new_design);

        struct_::Copy_Structure(
            &MOX::ship[ship_n].d, &new_design, sizeof(MOX::ship[ship_n].d));
        MOX::ship[ship_n].status = 4;
        MOX::ship[ship_n].d.cost = refit_cost;
        Clear_Pending_Refit_Design();
        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    void __cdecl Do_Draw_Build_Queue() {
        ::fields::Deactivate_Auto_Function();
        ::fields::Assign_Auto_Function(Draw_Build_Queue_Popup, 1);
        Update_Fields();
        COLONY::full_draw = 1;
    }

    void __cdecl Squish_Out_Queue_Slot(int16_t slot_idx) {
        do {
            for (int16_t i = slot_idx; i < 6; ++i) {
                current_item[i] = current_item[i + 1];
            }
            current_item[6] = COLONY_PRODUCTION_NONE;
        } while (current_item[slot_idx] == COLONY_PRODUCTION_REPEAT);
    }

    int8_t __cdecl Something_Behind_Settting() {
        int16_t found_idx = -1;
        int8_t has_other = 0;

        for (int16_t i = 0; i < 7; ++i) {
            if (found_idx == -1) {
                int16_t item = current_item[i];
                if (item != COLONY_PRODUCTION_NONE &&
                    (item == COLONY_PRODUCTION_HOUSING ||
                        item == COLONY_PRODUCTION_TRADE_GOODS ||
                        item == COLONY_PRODUCTION_REPEAT)) {
                    found_idx = i;
                }
            } else if (current_item[i] != COLONY_PRODUCTION_NONE) {
                has_other = 1;
            }
        }

        return has_other;
    }

    void __cdecl Do_Exit_Screen_Cleanup() {
        s_colony* colony = &MOX::colony[COLONY::colony_handle];
        int16_t original_front_item = colony->producing[0];

        Remove_Ships(1);

        for (int16_t i = 0; i < 7; ++i) {
            colony->producing[i] = current_item[i];
        }

        if (original_front_item != colony->producing[0]) {
            colony->bought_outright = 0;
        }
    }

    int8_t __cdecl Add_To_Queue(int16_t product_id) {
        int16_t q_len = COLBLDG::Get_Prod_Queue_Len();
        if (q_len == -1) {
            if (COLBLDG::field_mode != 0) {
                COLBLDG::field_mode = 0;
                COLONY::Reset_Mouse_Picture();
            }
            return 0;
        }

        if (product_id == COLONY_PRODUCTION_HOUSING &&
            !COLCALC::Colony_Can_Build_Product(
                &MOX::colony[COLONY::colony_handle], product_id)) {
            return 0;
        }

        bool add_to_queue = false;

        if (COLBLDG::Colony_Production_Is_Building(product_id) ||
            product_id == COLONY_PRODUCTION_HOUSING ||
            product_id == COLONY_PRODUCTION_TRADE_GOODS ||
            product_id == COLONY_PRODUCTION_REPEAT ||
            product_id == COLONY_PRODUCTION_FREIGHTERS ||
            product_id == COLONY_PRODUCTION_SPY ||
            COLBLDG::Colony_Production_Is_Queued_Ship(product_id)) {
            add_to_queue = true;
        } else if (product_id == COLONY_PRODUCTION_SCIENTIST ||
            product_id == COLONY_PRODUCTION_FARMER ||
            product_id == COLONY_PRODUCTION_WORKER) {
            int16_t planet_idx = MOX::colony[COLONY::colony_handle].planet;
            int16_t n_terra = COLBLDG::N_Prods_In_Queue(COLONY_PRODUCTION_SCIENTIST);
            int16_t n_rad = COLBLDG::N_Prods_In_Queue(COLONY_PRODUCTION_WORKER);
            int16_t n_grav = COLBLDG::N_Prods_In_Queue(COLONY_PRODUCTION_FARMER);
            uint8_t pops = MOX::colony[COLONY::colony_handle].n_pops;
            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(&MOX::planet[planet_idx], MOX::PLAYER_NUM);

            if ((n_terra + n_rad + n_grav + pops + 1) >= max_pop) {
                if (GENDRAW::Do_Confirmation_Box(ESTRINGS::E_Strings(0x208))) {
                    add_to_queue = true;
                } else {
                    field_mode = 0;
                    COLONY::Reset_Mouse_Picture();
                }
            } else {
                add_to_queue = true;
            }
        } else if (product_id == COLONY_PRODUCTION_TRANSPORT_SHIP ||
            product_id == COLONY_PRODUCTION_COLONY_SHIP ||
            product_id == COLONY_PRODUCTION_CUSTOM_DESIGN ||
            product_id == COLONY_PRODUCTION_DOOM_STAR ||
            product_id == COLONY_PRODUCTION_OUTPOST_SHIP) {
            int16_t design_slot = 4;
            s_ship_design generated_design = {};
            s_ship_design* design = nullptr;
            int16_t auto_type = COLBLDG::Auto_Design_Type(product_id);
            int16_t owner = MOX::colony[COLONY::colony_handle].owner;

            if (auto_type == SHIP_TYPE_COLONY) {
                INITSHIP::Load_Colony_Ship_Design(owner, &generated_design);
                design = &generated_design;
            } else if (auto_type == SHIP_TYPE_OUTPOST) {
                INITSHIP::Load_Outpost_Ship_Design(owner, &generated_design);
                design = &generated_design;
            } else if (auto_type == SHIP_TYPE_TRANSPORT) {
                INITSHIP::Load_Transport_Ship_Design(owner, &generated_design);
                design = &generated_design;
            } else if (auto_type == SHIP_TYPE_COMBAT) {
                if (product_id == COLONY_PRODUCTION_DOOM_STAR) {
                    design_slot = SHIP_SIZE_DOOM_STAR;
                    if (!COLCALC::Colony_Can_Build_Product(&MOX::colony[COLONY::colony_handle], COLBLDG::Colony_Production_From_Ship_Design_Index(SHIP_SIZE_DOOM_STAR))) {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x245));
                        COLBLDG::Do_Draw_Build_Queue();
                        COLBLDG::field_mode = 0;
                        COLONY::Reset_Mouse_Picture();
                        return 1;
                    }
                } else {
                    MOX::temp_star_handle = 4;
                    COLBLDG::return_to_build_popup_from_custom_design = 1;
                    COLBLDG::Commit_Autobuild_State();
                    return 1;
                }
            }

            int16_t created_ship;
            if (design != nullptr) {
                created_ship = Create_Ship_From_Design(owner, design, COLONY::colony_handle);
                if (created_ship != -1) {
                    MOX::ship[created_ship].status = 6;
                }
            } else {
                created_ship = COLBLDG::Colbldg_Create_Ship(design_slot);
            }
            if (created_ship == -1) {
                GENDRAW::Help(ESTRINGS::E_Strings(0x21a));
                COLBLDG::Do_Draw_Build_Queue();
                COLBLDG::field_mode = 0;
            } else {
                COLBLDG::current_item[q_len] = COLBLDG::Colony_Production_From_Ship_Index(created_ship);
            }
        } else if (!COLBLDG::Colony_Production_Is_Ship_Design(product_id)) {
            GENDRAW::Help(ESTRINGS::E_Strings(0x14c));
            COLBLDG::Do_Draw_Build_Queue();
            return 0;
        } else {
            if (COLCALC::Colony_Can_Build_Product(&MOX::colony[COLONY::colony_handle], product_id)) {
                int16_t created_ship = COLBLDG::Colbldg_Create_Ship(COLBLDG::Colony_Production_Ship_Design_Index(product_id));
                if (created_ship == -1) {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x21a));
                    COLBLDG::Do_Draw_Build_Queue();
                    COLBLDG::field_mode = 0;
                } else {
                    COLBLDG::current_item[q_len] = COLBLDG::Colony_Production_From_Ship_Index(created_ship);
                }
            } else {
                int8_t size = MOX::player[MOX::PLAYER_NUM].ship_designs[COLBLDG::Colony_Production_Ship_Design_Index(product_id)].size;
                int16_t owner = MOX::colony[COLONY::colony_handle].owner;
                if (size == SHIP_SIZE_TITAN && MOX::player[owner].tech_applications[TECH_APP_TITAN_CONSTRUCTION] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x246));
                    COLBLDG::Do_Draw_Build_Queue();
                } else if (size == SHIP_SIZE_DOOM_STAR && MOX::player[owner].tech_applications[TECH_APP_DOOM_STAR_CONSTRUCTION] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x245));
                    COLBLDG::Do_Draw_Build_Queue();
                } else {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x237));
                    COLBLDG::Do_Draw_Build_Queue();
                }
                COLBLDG::field_mode = 0;
                COLONY::Reset_Mouse_Picture();
            }
        }

        if (add_to_queue) {
            COLBLDG::current_item[q_len] = product_id;
        }

        if (COLBLDG::active_prod[0] != -1 && COLBLDG::field_mode != 2) {
            COLBLDG::Insert_Before_Queue_Slot(q_len, COLBLDG::active_prod[0] - BUILDING_COUNT);
            COLONY::Reset_Mouse_Picture();
        }

        if (COLBLDG::field_mode == 2) {
            COLBLDG::field_mode = 0;
            COLONY::Reset_Mouse_Picture();
            if (product_id != COLONY_PRODUCTION_HOUSING &&
                product_id != COLONY_PRODUCTION_TRADE_GOODS &&
                product_id != COLONY_PRODUCTION_REPEAT) {
                if (COLBLDG::Add_To_Queue(COLONY_PRODUCTION_REPEAT) == 0) {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x188));
                    COLBLDG::Do_Draw_Build_Queue();
                    if (COLBLDG::field_mode != 0) {
                        COLBLDG::field_mode = 0;
                        COLONY::Reset_Mouse_Picture();
                    }
                }
            }
        }

        COLBLDG::Defragment_Queue();
        return 1;
    }

    void __cdecl Delete_From_Queue(int16_t slot_idx) {
        if (slot_idx == -1) {
            return;
        }

        int16_t item = current_item[slot_idx];

        if (COLBLDG::Colony_Production_Is_Queued_Ship(item)) {
            int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(item);
            if (MOX::ship[ship_idx].status == 4) {
                if (GENDRAW::Do_Confirmation_Box(ESTRINGS::E_Strings(0x213 /* Cancel ship building? */)) == 0) {
                    Do_Draw_Build_Queue();
                    return;
                }
            }

            if (!ERIC::Sint_Is_In_List(item, (int16_t*)MOX::colony[COLONY::colony_handle].producing, 14)) {
                Delete_Ship(ship_idx);
            }
            Do_Draw_Build_Queue();
        }

        if (slot_idx != 0 && item == COLONY_PRODUCTION_REPEAT) {
            int16_t prev_item = current_item[slot_idx - 1];
            if (COLBLDG::Colony_Production_Is_Queued_Ship(prev_item)) {
                int16_t prev_ship_idx = COLBLDG::Colony_Production_Ship_Index(prev_item);
                if (MOX::ship[prev_ship_idx].status == 4) {
                    if (GENDRAW::Do_Confirmation_Box(ESTRINGS::E_Strings(0x213 /* Cancel ship building? */)) == 0) {
                        Do_Draw_Build_Queue();
                        return;
                    }
                }

                if (!ERIC::Sint_Is_In_List(prev_item, (int16_t*)MOX::colony[COLONY::colony_handle].producing, 14)) {
                    Delete_Ship(prev_ship_idx);
                }
                Do_Draw_Build_Queue();
            }

            Squish_Out_Queue_Slot(slot_idx);
            Squish_Out_Queue_Slot(slot_idx - 1);
            return;
        }

        if (slot_idx < 6 && current_item[slot_idx + 1] == COLONY_PRODUCTION_REPEAT) {
            Squish_Out_Queue_Slot(slot_idx + 1);
            Squish_Out_Queue_Slot(slot_idx);
            return;
        }

        Squish_Out_Queue_Slot(slot_idx);
    }

    void __cdecl Defragment_Queue() {
        for (int16_t pass = 2; pass != 0; --pass) {
            for (int16_t slot = 0; slot < 7; ++slot) {
                if (current_item[slot] == COLONY_PRODUCTION_NONE) {
                    Squish_Out_Queue_Slot(slot);
                }
            }
        }
    }

    void __cdecl Insert_Before_Queue_Slot(int16_t source_slot_idx, int16_t target_slot_idx) {
        int16_t source_count = Normalize_Repeat_Pair_Position(&source_slot_idx);
        int16_t product = current_item[source_slot_idx];
        int16_t target_count = Normalize_Repeat_Pair_Position(&target_slot_idx);
        int16_t restore_slot;

        if (source_slot_idx < target_slot_idx) {
            int16_t move_count =
                target_slot_idx + target_count - source_slot_idx - source_count;
            memmove(
                &current_item[source_slot_idx],
                &current_item[source_slot_idx + source_count],
                (size_t)move_count * sizeof(current_item[0]));
            restore_slot = target_slot_idx + target_count - source_count;
        } else {
            int16_t move_count = source_slot_idx - target_slot_idx;
            memmove(
                &current_item[target_slot_idx + source_count],
                &current_item[target_slot_idx],
                (size_t)move_count * sizeof(current_item[0]));
            restore_slot = target_slot_idx;
        }

        current_item[restore_slot] = product;
        if (source_count == 2) {
            current_item[restore_slot + 1] = COLONY_PRODUCTION_REPEAT;
        }

        if (field_mode != 2) {
            active_prod[0] = -1;
        }
    }

    void __cdecl Remove_Ships(int8_t flag) {
        for (int16_t i = 0; i < 7; ++i) {
            int16_t prod;
            if (flag == 0) {
                prod = current_item[i];
                if (COLBLDG::Colony_Production_Is_Queued_Ship(prod)) {
                    if (First_Prod_Colony_Queue_Slot(COLONY::colony_handle, prod) == -1) {
                        int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(prod);
                        int16_t officer_idx = MOX::ship[ship_idx].officer_index;
                        if (officer_idx != -1) {
                            OFFICER::Move_Officer_To_Pool(officer_idx);
                            MOX::ship[ship_idx].officer_index = -1;
                        }
                        Delete_Ship(ship_idx);
                    }
                }
            } else {
                prod = MOX::colony[COLONY::colony_handle].producing[i];
                if (COLBLDG::Colony_Production_Is_Queued_Ship(prod)) {
                    if (First_Prod_Queue_Slot(prod) == -1) {
                        int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(prod);
                        int16_t officer_idx = MOX::ship[ship_idx].officer_index;
                        if (officer_idx != -1) {
                            OFFICER::Move_Officer_To_Pool(officer_idx);
                            MOX::ship[ship_idx].officer_index = -1;
                        }
                        Delete_Ship(ship_idx);
                    }
                }
            }
        }
    }

    void __cdecl Build_Weapon_String(s_ship_weapons* weapon, char* dest, uint32_t dest_size, uint8_t include_postfix_modifiers, uint8_t weapon_count) {
        char buf1[500];
        char buf2[500];
        char buf3[500];

        int16_t weapon_id = weapon->type;

        char* name;
        if (weapon_count <= 1) {
            name = TECHDATA::weapons[weapon_id].name;
        } else {
            name = TECHDATA::weapons[weapon_id].name_plural;
        }

        strlcpy(buf2, name, sizeof(buf2));
        strlcpy(buf1, ESTRINGS::E_Strings(0xC), sizeof(buf1));
        strlcpy(buf3, ESTRINGS::E_Strings(0xC), sizeof(buf3));

        uint8_t* mods_ptr = (uint8_t*)&weapon->specials;
        for (int16_t bit = 0; bit < WEAPON_MOD_COUNT; bit++) {
            if (struct_::Test_Bit_Field(mods_ptr, bit)) {
                if (bit == 1 || bit == 2) {
                    strlcat(buf1, TECHDATA::weapon_modifications[bit].name_plural, sizeof(buf1));
                    strlcat(buf1, " ", sizeof(buf1));
                } else {
                    strlcat(buf3, TECHDATA::weapon_modifications[bit].name_plural, sizeof(buf3));
                }
            }
        }

        if (include_postfix_modifiers != 0) {
            snprintf(dest, dest_size, ESTRINGS::E_Strings(0x51), buf1, buf2, buf3);
        } else {
            snprintf(dest, dest_size, ESTRINGS::E_Strings(0x51), buf1, buf2, ESTRINGS::E_Strings(0xC));
        }
    }

    char* __cdecl Option_String(int16_t option_id) {
        switch (option_id) {
            case COLONY_PRODUCTION_OUTPOST_SHIP:
                return ESTRINGS::E_Strings(0x195);
            case COLONY_PRODUCTION_DOOM_STAR:
                return ESTRINGS::E_Strings(0x10F);
            case COLONY_PRODUCTION_FREIGHTERS:
                return ESTRINGS::E_Strings(0x12D);
            case COLONY_PRODUCTION_CUSTOM_DESIGN:
                return ESTRINGS::E_Strings(0x0FA);
            case COLONY_PRODUCTION_COLONY_SHIP:
                return ESTRINGS::E_Strings(0x0E6);
            case COLONY_PRODUCTION_TRANSPORT_SHIP:
                return ESTRINGS::E_Strings(0x21E);
            case COLONY_PRODUCTION_REPEAT:
                return ESTRINGS::E_Strings(0x088);
            case COLONY_PRODUCTION_SEPARATOR:
            case COLONY_PRODUCTION_NONE:
            case 0:
                return ESTRINGS::E_Strings(0x00C);
            case COLONY_PRODUCTION_REFIT:
                return ESTRINGS::E_Strings(0x1BD);
            case COLONY_PRODUCTION_SPY:
                return ESTRINGS::E_Strings(0x1EC);
            case COLONY_PRODUCTION_SCIENTIST:
                return ESTRINGS::E_Strings(0x0B1);
            case COLONY_PRODUCTION_WORKER:
                return ESTRINGS::E_Strings(0x0B2);
            case COLONY_PRODUCTION_FARMER:
                return ESTRINGS::E_Strings(0x0B0);
            case COLONY_PRODUCTION_HOUSING:
                return ESTRINGS::E_Strings(0x142);
            case COLONY_PRODUCTION_TRADE_GOODS:
                return ESTRINGS::E_Strings(0x21D);
            default:
                break;
        }

        if (COLBLDG::Colony_Production_Is_Ship_Design(option_id)) {
            const int32_t colony_owner = (int32_t)MOX::colony[COLONY::colony_handle].owner;
            const int32_t ship_design_index = COLBLDG::Colony_Production_Ship_Design_Index(option_id);
            return MOX::player[colony_owner].ship_designs[ship_design_index].name;
        }

        RUSS::PFMT(0, 0, ESTRINGS::E_Strings(248 /* 0xF8 */), option_id);
        key::Wait_For_Key();
        return ESTRINGS::E_Strings(0x00C);
    }

} // namespace COLBLDG