#pragma once
#include <cstdint>

struct s_fleet_box;

namespace FLEETPOP {
    void __cdecl Fleet_Movement_Scroll_Callback();

    bool __cdecl Set_Attack_Antares_Button_Flag(void);

    int8_t __cdecl Set_Transport_Troops_Flag(int16_t ship_count);

    int8_t __cdecl Set_Colonize_Button_Flag(int16_t ship_count);

    uint8_t __cdecl Set_Build_Outpost_Button_Flag(int16_t ship_count);

    uint8_t __cdecl Can_Build_Outpost(game::id_planet planet_idx);

    int8_t __cdecl Set_Engage_Enemy_Forces_Flag(int16_t ship_count);

    int8_t __cdecl Planet_Is_Colonizable(game::id_planet planet_idx);

    void __cdecl Reload_Fleet_Box_Data(void);

    bool __cdecl Ship_Should_Be_Auto_Selected(int16_t ship_idx);

    int16_t __cdecl N_Fleet_Icons(int16_t node_idx);

    void __cdecl Initialize_Scroll_Bar(s_scroll_bar* scroll_bar, int16_t total_rows);

    void __cdecl Buffer_Reload_Fleetpop_Frame();

    void __cdecl Draw_Large_Fleet_Box_Frame(
        int16_t x,
        int16_t y,
        int16_t x_offset,
        int16_t y_offset,
        uint32_t unused_fleet_count,
        int16_t* current_y);

    void __cdecl Draw_Small_Fleet_Box_Frame(
        int16_t x,
        int16_t y,
        int16_t x_offset,
        int16_t y_offset,
        int16_t fleet_count,
        int16_t* current_y);

    void __cdecl Draw_Fleet_Pop_Added_Buttons(int16_t x, int16_t y);

    void __cdecl Get_Fleet_Icon_Button_XY(int16_t fleet_icon_index, int16_t* out_x, int16_t* out_y);

    void __cdecl Print_Fleet_Box_Info();

    void __cdecl Get_Absolute_Location_Scan_Info(
        int16_t ship_idx,
        int16_t* value,
        int16_t* star_idx,
        int16_t* info_type);

    /** Formats the currently scanned fleet-ship name into the caller-provided buffer. */
    void __cdecl Get_Fleetpop_Scanned_Ship_Info(char* out_text, uint32_t out_text_size);

    extern uint8_t n_added_buttons;

    bool __cdecl Check_For_Colonization_Button();

    bool __cdecl Check_For_Transport_Troops_Button();

    bool __cdecl Check_For_Build_Outpost_Button();

    bool __cdecl Check_For_Engage_Enemy_Forces_Button();

    bool __cdecl Check_For_Attack_Antares_Button();

    void __cdecl Set_Fleet_Box_Data_Dimensions(void);

    void __cdecl Set_Added_Button_Stuff();

    void __cdecl Fleet_Box_Data(int16_t icon_count);

    int16_t __cdecl Fleet_Buttons_Chosen();
    bool __cdecl At_Least_One_Ship_Of_Type_Selected(int16_t ship_type);

    /** Marks all selectable fleet icons as selected or unselected, with optional auto-select filtering. */
    void __cdecl Set_Fleet_Movement_Icon_Selections(int16_t state, uint8_t auto_select_flag);

    /** Returns true when at least one fleet icon is currently selected. */
    bool __cdecl Some_Icon_Selected();

    /** Restores ship-node selection flags from the saved fleet-icon bitfield. */
    void __cdecl Reload_Ship_Selection_Status();

    int16_t __cdecl First_Unselected_Icon();

    /** Saves ship-node selection flags into the fleet-icon selection bitfield. */
    void __cdecl Save_Ship_Selection_Status();

    int16_t __cdecl Fleet_Box_Grid_Height(int16_t grid_type);
    int16_t __cdecl Fleet_Box_Width(int16_t unused_ship_count);
    int16_t __cdecl Fleet_Box_Height(int16_t fleet_count);

    void __cdecl Set_Fleet_Movement_Box_Dimensions(int16_t count);

    bool __cdecl Can_Select_Fleet_Box_Ship(int16_t icon_index);

    int16_t __cdecl Icon_Selection_Status(int16_t icon_field_id);

    void __cdecl Clear_Ship_Node_Selection_Flags();

    void __cdecl Reload_Fleet_Box_Palette(game::owner_slot owner);

    void __cdecl Draw_Damage_Bars(int16_t x, int16_t y, int16_t width, int16_t y_offset, int16_t x_offset, int16_t ship_idx);

    void __cdecl Add_Added_Buttons(int16_t x_offset, int16_t y_offset);

    /** Adds fleet-box input fields for the current stack when the stack head has not changed. */
    void __cdecl Add_Fleet_Movement_Box_Fields(int16_t node_ptr, int16_t allow_input_fields);

    void __cdecl Draw_Fleet_Movement_Box();
} // namespace FLEETPOP
