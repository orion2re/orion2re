#pragma once
#include "deps.h"

namespace HACCESS {
    /** Copies the resolved race name into the caller-provided buffer.
     * @param race_id Player or race identifier to resolve.
     * @param race_name_out Destination buffer for the copied name.
     * @param race_name_out_size Size of the destination buffer in bytes.
     */
    void __cdecl Race_Name(int16_t race_id, char* race_name_out, uint32_t race_name_out_size);

    void __cdecl Set_Colony_Auto_Select_For_Star(int16_t star_idx, int16_t player_idx, int16_t* out_planet_idx);

    bool __cdecl Star_Has_Relocation(int16_t star_idx, int16_t player_idx);

    int16_t __cdecl Relocation(int16_t star_idx, int16_t player_idx);

    int16_t __cdecl Ship_Owner(int16_t ship_idx);
    int16_t __cdecl Ship_Stack_From_Saved_Array();
    int16_t __cdecl Node_Ptr_From_Ship_Stack(int16_t index);

    /** Returns the ship icon index that contains the given ship-node index.
     * @param ship_node_idx Ship-node index to search for.
     */
    int16_t __cdecl Ship_Icon_Id_From_Node_Ptr(int16_t ship_node_idx);

    /** Returns the owning player index for a ship icon.
     * @param icon_idx Ship icon index.
     */
    int16_t __cdecl Ship_Icon_Owner(int16_t icon_idx);

    /** Returns the selected design name in the shared temporary buffer.
     * @param player_idx Owning player index.
     * @param design_idx Ship design index.
     */
    char* __cdecl Design_Name(int16_t player_idx, int16_t design_idx);

    int16_t __cdecl Design_Weapon(int16_t player_idx, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Weapon_Count(int16_t player_idx, int16_t design_idx, int16_t weapon_idx);

    uint16_t __cdecl Design_Weapon_Specials(int16_t player_idx, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Weapon_Firing_Arc(int16_t player_idx, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Weapon_Ammo(int16_t player_idx, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Picture_Num(int16_t player_idx, int16_t design_idx);

    bool __cdecl Fleet_Movement_Icon_Selected(int16_t icon_idx);

    void __cdecl Do_Get_Planet_Name(int16_t planet_idx, char* buffer, uint8_t localized_flag);

    char* __cdecl Get_Planet_Name(int16_t planet_idx);
    char* Get_Planet_Name(const game::s_game_state& state, game::id_planet id);

    int16_t __cdecl Last_Colony_Selected(int16_t star_idx, int16_t player_idx);

    void __cdecl Set_All_Star_Relocations(int16_t target_star_idx, int16_t player_idx);

    void __cdecl Clear_All_Star_Relocations(int16_t player_idx);

    int16_t __cdecl Fleet_Movement_Ship_Id(int16_t icon_idx);

    int16_t __cdecl Ship_Stack_From_Ship_Id(int16_t ship_id);
    int16_t __cdecl Ship_Stack_Star_Id(int16_t node_idx);

    int8_t __cdecl Get_Stack_Start_From_Saved_Ship_Array();

    /** Collects selected ship IDs into the caller-provided buffer.
     * @param selected_ids_out Output buffer for ship IDs.
     * @param selected_ids_capacity Number of int16_t entries available in the output buffer.
     * @param ship_type_filter Ship type to keep, or 0x00FF for no type filter.
     */
    int16_t __cdecl Get_Fleet_Box_Selected_Ship_Ids(
        int16_t* selected_ids_out,
        int16_t selected_ids_capacity,
        int16_t ship_type_filter);

    void __cdecl Clear_Fleet_Box_Selected_Ship_Status();

    int16_t __cdecl Ship_Stack_Owner(int16_t stack_idx);

    int16_t __cdecl Ship_Stack_From_Ship_Icon(int16_t ship_icon_id);

    int16_t __cdecl Weapons_Index_From_Tech_App(int16_t tech_app_id);

    char* __cdecl Race_Name_String(int16_t race_index);

    bool __cdecl Player_Has_Colony_In_System(int16_t player_idx, int16_t system_idx);

    void __cdecl Set_Relocation(int16_t star_idx, int16_t player_idx, int16_t target_star_idx);

    int16_t __cdecl N_Colonies_At_Star(int16_t star_index, int16_t player_index);

    bool __cdecl Is_Combat_Ship(int16_t ship_index);

    char* __cdecl Get_Planet_Name_No_Prime(int16_t planet_idx);
    char* Get_Planet_Name_No_Prime(const game::s_game_state& state, game::id_planet id);

    int16_t __cdecl N_Star_Relocations(int16_t player_idx);

    int8_t __cdecl Orion_Owner();

    int16_t __cdecl Ship_Stack_X(int16_t stack_idx);
    int16_t __cdecl Ship_Stack_Y(int16_t node_idx);

    int16_t __cdecl Ship_Icon_Star_Id(int16_t ship_icon_idx);

    int16_t __cdecl Ship_Icon_Ship_Id(int16_t icon_idx);

    void __cdecl Build_Saved_Ship_Array();

    bool __cdecl Player_Can_Order_Ship(int16_t player_idx, s_ship_data* ship = nullptr);

    /** Finds the nearest reachable player-owned star to the given coordinates.
     * @param ship_coords Pointer to the source XY coordinate pair.
     */
    int16_t __cdecl Nearest_Star_To_XY(int16_t* ship_coords);

    void __cdecl Clear_Star_Relocation(int16_t star_idx, int16_t player_idx);

    int8_t __cdecl Special_Is_Planet_Special(int16_t special_type);

    bool __cdecl Player_Has_Tech(int16_t tech_idx, int16_t player_idx);

    int16_t __cdecl Planet_Orbit(int16_t planet_idx);

    bool __cdecl Enemy_Ship_Going_To_My_Colony();

    void __cdecl Set_All_Colony_Auto_Select_Flags();

    int16_t __cdecl System_Planet_Scanned_To_Planet_Id(int16_t star_idx, int16_t planet_idx);

    int16_t __cdecl System_Ship_Scanned_To_Ship_Id(int16_t ship_node_idx);

    void __cdecl Set_Hotseat_Fleet_Box_To_Last_Stack(int16_t player_idx);

    void __cdecl Save_Hotseat_Fleet_Box_Ship(int16_t fleet_box_idx);
} // namespace HACCESS