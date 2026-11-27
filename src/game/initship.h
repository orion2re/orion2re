#pragma once
#include <cstdint>

struct s_ship_data;
struct s_ship_design;
namespace game { struct s_game_state; struct owner_slot; }

namespace INITSHIP {
    struct s_lander_ship_uses_stealth {
        uint8_t colony_ship;
        uint8_t outpost_ship;
        uint8_t transport_ship;
    };

    enum e_lander_tech : uint8_t {
        LANDER_TECH_END = 0,
        LANDER_TECH_NONE = 254,
        LANDER_TECH_LEGACY = 255,
    };

    using t_lander_tech_list = uint8_t[5];

    struct s_lander_tech {
        t_lander_tech_list colony_ship = {LANDER_TECH_LEGACY};
        t_lander_tech_list outpost_ship = {LANDER_TECH_LEGACY};
        t_lander_tech_list transport_ship = {LANDER_TECH_LEGACY};
    };

    extern uint8_t satellite_uses_torpedoes;
    extern uint8_t td_ftl_bonus;
    extern uint8_t transport_ship_extended_range;
    extern uint8_t landers_size_class;
    extern s_lander_ship_uses_stealth lander_ship_uses_stealth;
    extern s_lander_tech lander_tech;

    void Repair_Ships_At_Colonies(game::s_game_state& state);

    /** Fully repairs a ship and restores its weapon and special-device state. */
    void Repair_Ship_Full(s_ship_data& ship);

    void __cdecl Init_Ship_Designs(game::id_player player);

    /** Calculates combat attack and defense bonuses for a ship.
     * @param world Supplies the owner, officer, and combat mode.
     * @param ship Ship record to inspect.
     * @param ship_defense Receives the defense rating.
     * @param ship_missile_defense Receives the missile defense rating.
     * @param ship_attack Receives the attack bonus.
     */
    void Get_Ship_Combat_Bonuses(game::s_game_state& world, int16_t strategic_combat, const s_ship_data& ship,
        int16_t* ship_defense, int16_t* ship_missile_defense, int16_t* ship_attack);

    int16_t __cdecl Best_Ship_Fuel(game::owner_slot owner);
    int16_t Best_Ship_Fuel(const game::s_game_state& state, game::owner_slot owner);

    void __cdecl Update_Player_Ship_Designs(game::id_player player);
    void Update_Player_Ship_Designs(game::s_game_state& state, game::id_player player);

    void __cdecl Update_Player_Strategic_Ship_Designs(game::id_player player);

    void __cdecl Update_Automatic_Design_Equipment(game::id_player player);

    void __cdecl Update_Equipment_On_Ships_Being_Built(game::id_player player);

    int16_t Best_Warp_Drive(game::owner_slot faction);
    int16_t Best_Warp_Drive(const game::s_game_state& state, game::owner_slot faction);

    int16_t __cdecl Calc_Player_FTL_Speed(game::id_player player);
    int16_t Calc_Player_FTL_Speed(const game::s_game_state& state, game::id_player player);

    int16_t Best_Armor(game::owner_slot faction);
    int16_t Best_Armor(const game::s_game_state& state, game::owner_slot faction);

    int16_t Best_Computer(game::owner_slot faction);
    int16_t Best_Computer(const game::s_game_state& state, game::owner_slot faction);

    int16_t Best_Ship_Shield(game::owner_slot faction);
    int16_t Best_Ship_Shield(const game::s_game_state& state, game::owner_slot faction);

    int16_t __cdecl Best_Shield_Special(game::owner_slot owner);

    int16_t __cdecl Best_Beam_Weapon(
        game::owner_slot owner, uint16_t required_mods = 0);

    int16_t __cdecl Best_Point_Defense_Weapon_By_Size(game::owner_slot owner);
    int16_t Best_Point_Defense_Weapon_By_Size(const game::s_game_state& state, game::owner_slot owner);

    int16_t __cdecl Point_Defense_Damage_Per_Space(int16_t weapon_idx, game::id_player player);

    int16_t __cdecl Best_Missile_Weapon(game::owner_slot owner);

    int16_t __cdecl Best_Missile_Weapon_NT(game::owner_slot owner);

    int16_t __cdecl Best_Strat_Special_Weapon(game::owner_slot owner, int16_t size);

    int16_t __cdecl Get_FTL_Speed(game::owner_slot owner, int16_t ftl_type);
    int16_t Get_FTL_Speed(const game::s_game_state& state, game::owner_slot owner, int16_t ftl_type);

    int16_t Get_Design_Structure(const s_ship_design& design, bool strategic_combat);

    /** Returns the total structure for a hull after armor and fortification modifiers.
     * @param hull_index Hull data index to inspect.
     * @param is_fort Non-zero to apply the fortification multiplier.
     * @param player_idx Player index used to resolve the best available armor.
     */
    int16_t __cdecl Get_Structure(int16_t hull_index, bool is_fort, game::owner_slot owner);

    int16_t Get_Ship_Structure(const s_ship_data& ship, bool strategic_combat);

    int16_t Get_Ship_Armor_Hits(const s_ship_data& ship);

    void __cdecl Auto_Strategic_Design_Ship(game::id_player player, int16_t design_idx, s_ship_design* design);

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

    void __cdecl Load_Colony_Ship_Design(game::id_player player, s_ship_design* design);
    void Load_Colony_Ship_Design(const game::s_game_state& world, game::id_player player, s_ship_design* design);

    void __cdecl Load_Outpost_Ship_Design(game::id_player player, s_ship_design* design);

    void __cdecl Load_Doom_Star_Ship_Design(
        game::id_player player, s_ship_design* design, bool strategic_combat);
    void Load_Doom_Star_Ship_Design(game::s_game_state& world, game::id_player player, s_ship_design* design, bool strategic_combat, int16_t difficulty);


    void Update_Strategic_Ship(game::s_game_state& world, int16_t strategic_combat, s_ship_data& ship);

    int16_t __cdecl Strat_Missile_Weapon_Count(int16_t hull_idx);

    int16_t __cdecl Strat_Special_Weapon_Count(int16_t hull_idx);

    int16_t __cdecl Strat_Beam_Weapon_Count(game::owner_slot owner, int16_t hull_idx);

    int16_t __cdecl Strat_Bomb_Weapon_Count(int16_t hull_idx);

    int16_t __cdecl Best_Beam_To_Hit_Special(game::owner_slot owner);

    int16_t __cdecl Best_Beam_Damage_Special(game::owner_slot owner);

    int16_t __cdecl Best_Beam_Defense_Special(game::owner_slot owner);

    int16_t __cdecl Best_Missile_Defense_Special(game::owner_slot owner);

    int16_t Best_Point_Defense_Weapon(game::owner_slot faction);
    int16_t Best_Point_Defense_Weapon(const game::s_game_state& state, game::owner_slot faction);

    int16_t __cdecl Best_Bomb_Weapon(game::owner_slot owner);
    int16_t Best_Bomb_Weapon(game::s_game_state& world, int16_t strategic_combat, game::owner_slot owner);

    int16_t __cdecl Cost_Of_Strategic_Design(game::id_player player, struct s_ship_design* design);

    void __cdecl Get_Design_Combat_Bonuses(s_ship_design design, int16_t* attack_bonus_out, int16_t* defense_rating_out, int16_t* missile_defense_out);

    /** Loads the standard transport ship design for a player.
     * @param player_idx Player index whose current tech determines the loadout.
     * @param design Design record to populate.
     */
    void __cdecl Load_Transport_Ship_Design(game::id_player player, s_ship_design* design);
    void Load_Transport_Ship_Design(const game::s_game_state& world, game::id_player player, s_ship_design* design);

    /** Updates localized non-combat ship names after a language change. */
    void __cdecl Update_Ship_Names_To_New_Language();

    int16_t __cdecl Best_Missile_Damage_Special(game::owner_slot owner);

    /** Checks whether a ship carries a weapon type in any slot.
     * @param ship Ship record to inspect.
     * @param weapon_type Weapon type id to search for.
     */
    bool Ship_Has_Weapon(const s_ship_data& ship, int16_t weapon_type);
} // namespace INITSHIP
