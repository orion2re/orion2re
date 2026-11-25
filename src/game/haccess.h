#pragma once
#include "deps.h"
#include <vector>

namespace HACCESS {
    /** Copies the resolved race name into the caller-provided buffer.
     * @param race_id Player or race identifier to resolve.
     * @param race_name_out Destination buffer for the copied name.
     * @param race_name_out_size Size of the destination buffer in bytes.
     */
    void __cdecl Race_Name(game::owner_slot owner, char* race_name_out, uint32_t race_name_out_size);

    void __cdecl Set_Colony_Auto_Select_For_Star(int16_t star_idx, game::id_player player, game::id_planet* out_planet_idx);

    bool __cdecl Star_Has_Relocation(int16_t star_idx, game::id_player player);

    int16_t __cdecl Relocation(int16_t star_idx, game::id_player player);

    game::owner_slot __cdecl Ship_Owner(int16_t ship_idx);
    int16_t __cdecl Ship_Stack_From_Saved_Array();
    int16_t __cdecl Node_Ptr_From_Ship_Stack(int16_t index);

    /** Returns the ship icon index that contains the given ship-node index.
     * @param ship_node_idx Ship-node index to search for.
     */
    int16_t __cdecl Ship_Icon_Id_From_Node_Ptr(int16_t ship_node_idx);

    /** Returns the owning player index for a ship icon.
     * @param icon_idx Ship icon index.
     */
    game::owner_slot __cdecl Ship_Icon_Owner(int16_t icon_idx);

    /** Returns the selected design name in the shared temporary buffer.
     * @param player_idx Owning player index.
     * @param design_idx Ship design index.
     */
    char* __cdecl Design_Name(game::id_player player, int16_t design_idx);

    int16_t __cdecl Design_Weapon(game::id_player player, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Weapon_Count(game::id_player player, int16_t design_idx, int16_t weapon_idx);

    uint16_t __cdecl Design_Weapon_Specials(game::id_player player, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Weapon_Firing_Arc(game::id_player player, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Weapon_Ammo(game::id_player player, int16_t design_idx, int16_t weapon_idx);

    int8_t __cdecl Design_Picture_Num(game::id_player player, int16_t design_idx);

    bool __cdecl Fleet_Movement_Icon_Selected(int16_t icon_idx);


    char* Get_Planet_Name(const game::s_game_state& state, game::id_planet id);

    game::id_colony __cdecl Last_Colony_Selected(int16_t star_idx, game::id_player player);

    void __cdecl Set_All_Star_Relocations(int16_t target_star_idx, game::id_player player);

    void __cdecl Clear_All_Star_Relocations(game::id_player player);

    game::id_ship Fleet_Movement_Ship_Key(int16_t icon_idx);

    int16_t Ship_Stack_From_Ship_Key(game::id_ship ship);
    int16_t __cdecl Ship_Stack_Star_Id(int16_t node_idx);

    int8_t __cdecl Get_Stack_Start_From_Saved_Ship_Array();

    // Map previews use node-array order; deployment uses linked stack order.
    std::vector<game::id_ship> Selected_Fleet_Ship_Keys(int16_t ship_type_filter = 0xFF);
    std::vector<game::id_ship> Selected_Stack_Ship_Keys(int16_t stack);

    void __cdecl Clear_Fleet_Box_Selected_Ship_Status();

    game::owner_slot __cdecl Ship_Stack_Owner(int16_t stack_idx);

    int16_t __cdecl Ship_Stack_From_Ship_Icon(int16_t ship_icon_id);

    int16_t __cdecl Weapons_Index_From_Tech_App(int16_t tech_app_id);

    char* __cdecl Race_Name_String(game::owner_slot owner);

    bool __cdecl Player_Has_Colony_In_System(game::id_player player, int16_t system_idx);

    void __cdecl Set_Relocation(int16_t star_idx, game::id_player player, int16_t target_star_idx);

    int16_t __cdecl N_Colonies_At_Star(int16_t star_index, game::id_player owner_filter);


    char* Get_Planet_Name_No_Prime(const game::s_game_state& state, game::id_planet id);

    int16_t __cdecl N_Star_Relocations(game::id_player player);

    game::id_player __cdecl Orion_Owner();

    int16_t __cdecl Ship_Stack_X(int16_t stack_idx);
    int16_t __cdecl Ship_Stack_Y(int16_t node_idx);

    int16_t __cdecl Ship_Icon_Star_Id(int16_t ship_icon_idx);

    int16_t __cdecl Ship_Icon_Ship_Id(int16_t icon_idx);

    void __cdecl Build_Saved_Ship_Array();

    bool Player_Can_Order_Ship(game::id_player player, const s_ship_data* ship);

    /** Finds the nearest reachable player-owned star to the given coordinates.
     * @param ship_coords Pointer to the source XY coordinate pair.
     */
    game::id_star Nearest_Friendly_Star(game::id_ship ship, game::id_player player);

    void __cdecl Clear_Star_Relocation(int16_t star_idx, game::id_player player);

    int8_t __cdecl Special_Is_Planet_Special(int16_t special_type);

    bool __cdecl Player_Has_Tech(int16_t tech_idx, game::id_player player);


    bool __cdecl Enemy_Ship_Going_To_My_Colony();

    void __cdecl Set_All_Colony_Auto_Select_Flags();

    game::id_planet __cdecl System_Planet_Scanned_To_Planet_Id(int16_t star_idx, int16_t planet_idx);

    int16_t __cdecl System_Ship_Scanned_To_Ship_Id(int16_t ship_node_idx);

    void __cdecl Set_Hotseat_Fleet_Box_To_Last_Stack(game::id_player player);

    void __cdecl Save_Hotseat_Fleet_Box_Ship(game::id_player player);
} // namespace HACCESS
