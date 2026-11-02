#pragma once
#include <cstdint>

struct s_ship_data;
struct s_ship_design;

namespace INITSHIP {
    struct s_lander_ship_uses_stealth {
        uint8_t colony_ship;
        uint8_t outpost_ship;
        uint8_t transport_ship;
    };

    extern uint8_t satellite_uses_torpedoes;
    extern uint8_t td_ftl_bonus;
    extern uint8_t transport_ship_extended_range;
    extern uint8_t landers_size_class;
    extern s_lander_ship_uses_stealth lander_ship_uses_stealth;

    void __cdecl Repair_Ships_At_Colonies();

    /** Fully repairs a ship and restores its weapon and special-device state.
     * @param ship_idx Index of the ship to repair.
     */
    void __cdecl Repair_Ship_Full(int16_t ship_idx);

    void __cdecl Init_Ship_Designs(int16_t player_idx);

    /** Calculates combat attack and defense bonuses for a ship.
     * @param ship_idx Index of the ship to inspect.
     * @param ship_defense Receives the defense rating.
     * @param ship_missile_defense Receives the missile defense rating.
     * @param ship_attack Receives the attack bonus.
     */
    void __cdecl Get_Ship_Combat_Bonuses(int16_t ship_idx, int16_t* ship_defense, int16_t* ship_missile_defense, int16_t* ship_attack);

    int16_t __cdecl Best_Ship_Fuel(int16_t player_idx);

    void __cdecl Update_Player_Ship_Designs(int16_t player_idx);

    void __cdecl Update_Player_Strategic_Ship_Designs(int16_t player_idx);

    void __cdecl Update_Automatic_Design_Equipment(int16_t player_idx);

    void __cdecl Update_Equipment_On_Ships_Being_Built(int16_t player_idx);

    int16_t __cdecl Best_Warp_Drive(int16_t player_idx);

    int16_t __cdecl Calc_Player_FTL_Speed(int16_t player_idx);

    int16_t __cdecl Best_Armor(int16_t player_idx);

    int16_t __cdecl Best_Computer(int16_t player_idx);

    int16_t __cdecl Best_Ship_Shield(int16_t player_idx);

    int16_t __cdecl Best_Shield_Special(int16_t player_idx);

    int16_t __cdecl Best_Beam_Weapon(
        int16_t player_idx, uint16_t required_mods = 0);

    int16_t __cdecl Best_Point_Defense_Weapon_By_Size(int16_t player_idx);

    int16_t __cdecl Point_Defense_Damage_Per_Space(int16_t weapon_idx, int16_t player_idx);

    int16_t __cdecl Best_Missile_Weapon(int16_t player_idx);

    int16_t __cdecl Best_Missile_Weapon_NT(int16_t player_idx);

    int16_t __cdecl Best_Strat_Special_Weapon(int16_t player_idx, int16_t size);

    int16_t __cdecl Get_FTL_Speed(int16_t player_idx, int16_t ftl_type);

    int16_t __cdecl Get_Design_Structure(s_ship_design design);

    /** Returns the total structure for a hull after armor and fortification modifiers.
     * @param hull_index Hull data index to inspect.
     * @param is_fort Non-zero to apply the fortification multiplier.
     * @param player_idx Player index used to resolve the best available armor.
     */
    int16_t __cdecl Get_Structure(int16_t hull_index, bool is_fort, int16_t player_idx);

    int16_t __cdecl Get_Ship_Structure(int16_t ship_idx);

    int16_t __cdecl Get_Ship_Armor_Hits(int16_t ship_idx);

    void __cdecl Auto_Strategic_Design_Ship(int16_t player_idx, int16_t design_idx, s_ship_design* design);

    void __cdecl Load_Small_Antaran_Design(s_ship_design* design);

    void __cdecl Load_Medium_Antaran_Design(s_ship_design* design);

    void __cdecl Load_Large_Antaran_Design(s_ship_design* design);

    void __cdecl Load_Huge_Antaran_Design(s_ship_design* design);

    void __cdecl Load_Titan_Antaran_Design(s_ship_design* design);

    /** Loads one of the predefined Antaran ship designs for a hull size.
     * @param design Design record to populate.
     * @param hull_size Hull size selector from small through titan.
     */
    void __cdecl Load_Antaran_Ship_Design(s_ship_design* design, int16_t hull_size);

    void __cdecl Load_Small_Antaran_Combat_Ship(int16_t ship_idx);

    void __cdecl Load_Medium_Antaran_Combat_Ship(int16_t ship_idx);

    void __cdecl Load_Large_Antaran_Combat_Ship(int16_t ship_idx);

    void __cdecl Load_Huge_Antaran_Combat_Ship(int16_t ship_idx);

    void __cdecl Load_Titan_Antaran_Combat_Ship(int16_t ship_idx);

    void __cdecl Load_Guardian_Ship_Design(s_ship_design* design);

    void __cdecl Load_Amoeba_Ship_Design(s_ship_design* design, uint8_t is_large);

    void __cdecl Load_Crystal_Ship_Design(s_ship_design* design, uint8_t variant);

    void __cdecl Load_Dragon_Ship_Design(s_ship_design* design, uint8_t is_monster);

    void __cdecl Load_Eel_Ship_Design(s_ship_design* design, uint8_t is_greater_eel);

    void __cdecl Load_Hydra_Ship_Design(s_ship_design* design, uint8_t is_monster);

    void __cdecl Load_Loknar_Ship_Design(s_ship_design* design);

    void __cdecl Load_Colony_Ship_Design(int16_t player_idx, s_ship_design* design);

    void __cdecl Load_Outpost_Ship_Design(int16_t player_idx, s_ship_design* design);

    void __cdecl Load_Doom_Star_Ship_Design(
        int16_t owner_idx, s_ship_design* design, bool strategic_combat);

    void __cdecl Update_Strategic_Design_Weapons_Specials(s_ship_design* design, int16_t player_idx, int16_t design_idx);

    void __cdecl Update_Strategic_Ship(int16_t ship_idx);
    void Copy_Current_Ship_Configuration(int16_t owner, const s_ship_design* stored, s_ship_design* out);
    void Read_Ship_Combat_Bonuses(const s_ship_data* ship, const s_ship_design* design, int16_t* ship_defense, int16_t* ship_missile_defense, int16_t* ship_attack);

    int16_t __cdecl Strat_Missile_Weapon_Count(int16_t hull_idx);

    int16_t __cdecl Strat_Special_Weapon_Count(int16_t hull_idx);

    int16_t __cdecl Strat_Beam_Weapon_Count(int16_t player_idx, int16_t hull_idx);

    int16_t __cdecl Strat_Bomb_Weapon_Count(int16_t hull_idx);

    int16_t __cdecl Best_Beam_To_Hit_Special(int16_t player_idx);

    int16_t __cdecl Best_Beam_Damage_Special(int16_t player_idx);

    int16_t __cdecl Best_Beam_Defense_Special(int16_t player_idx);

    int16_t __cdecl Best_Missile_Defense_Special(int16_t player_idx);

    int16_t __cdecl Best_Point_Defense_Weapon(int16_t player_idx);

    int16_t __cdecl Best_Bomb_Weapon(int16_t player_idx);

    int16_t __cdecl Cost_Of_Strategic_Design(int16_t govt_type, struct s_ship_design* design);

    void __cdecl Get_Design_Combat_Bonuses(s_ship_design design, int16_t* attack_bonus_out, int16_t* defense_rating_out, int16_t* missile_defense_out);

    /** Loads the standard transport ship design for a player.
     * @param player_idx Player index whose current tech determines the loadout.
     * @param design Design record to populate.
     */
    void __cdecl Load_Transport_Ship_Design(int16_t player_idx, s_ship_design* design);

    /** Updates localized non-combat ship names after a language change. */
    void __cdecl Update_Ship_Names_To_New_Language();

    int16_t __cdecl Best_Missile_Damage_Special(int16_t player_idx);

    /** Checks whether a ship carries a weapon type in any slot.
     * @param ship_index Index of the ship to inspect.
     * @param weapon_type Weapon type id to search for.
     */
    int16_t __cdecl Ship_Has_Weapon(int16_t ship_index, int16_t weapon_type);
} // namespace INITSHIP