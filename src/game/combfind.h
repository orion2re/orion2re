#pragma once

namespace COMBFIND {
    struct s_system_discovery_bc {
        uint8_t space_debris;
        uint8_t pirate_cache;
    };

    extern uint16_t confirm_battle_message_timeout;
    extern s_system_discovery_bc system_discovery_bc;
    extern uint32_t dragon_bribe_bc;
    extern bool monsters_stay;
    extern uint8_t monsters_stay_chance;

    extern uint8_t messed_up_fleet_palette;
    extern uint8_t in_net_update;
    extern s_combfind_target target;
    extern uint16_t (*player_needs_to_attack)[MAX_STARS + 1];
    extern int16_t* battle_stars;
    extern uint8_t (*star_has_nonplayer)[MAX_STARS + 1];
    extern int16_t n_battle_stars;
    extern int8_t (*attack_confirmed)[MAX_PLAYERS][MAX_PLAYERS];
    extern s_combfind_star_has_player (*star_has_player)[MAX_STARS + 1][MAX_PLAYERS];
    extern uint8_t* current_star_has_nonplayer;
    extern s_combfind_star_has_player* current_star_has_player;
    extern char* research_treaty_msg;
    extern char* s_attacks_you_at_s_msg;
    extern char* outpost_destroyed_msg;
    extern char* combat_is_reported_between_s_and_s_msg;
    extern char* you_have_following_treaties_msg;
    extern char* receiving_tribute_msg;
    extern char* noncombat_ships_destroyed_msg;
    extern char* colony_seized_by_mind_control_msg;
    extern char* really_risk_war_msg;
    extern char* trade_treaty_msg;

    void __cdecl Search_For_Battles();

    void __cdecl Process_Retreating_Ships();

    bool Wandering_Monster_Retires(
        int8_t monster_type, bool amoeba_toxicized_colony = false);

    bool Is_Star_Or_Antares_Location(int16_t location, int16_t star_count);

    int16_t __cdecl Closest_Other_Colony_Star(int16_t player_idx, int16_t star_idx);

    void __cdecl Pick_Star_And_Attacker(int16_t* star_idx, int16_t* attacker_idx);

    void __cdecl Reset_One_Shot_System_Special(s_star_data* star);

    void __cdecl Do_System_Discoveries_At_Star(s_star_data* star);


    void __cdecl Update_Player_Needs_To_Attack(int16_t star_idx, int player_a_idx, int player_b_idx);

    void __cdecl Get_Local_Human_Target(int16_t star_idx, int16_t player_idx, int16_t* target_player_idx_out, int16_t* target_colony_idx_out);

    int8_t __cdecl Human_Confirms_Attack(int player_idx, int target_idx);

    void __cdecl Get_AI_Target(int16_t star_idx, int16_t player_idx, int16_t* target_player, int16_t* target_colony, int8_t* is_sneak_attack);

    void __cdecl Get_Ship_Strength_Info(uint32_t* enemy_strength, uint32_t* player_strength, uint32_t* shield_strength, int16_t star_idx, int16_t player_idx);

    void __cdecl Get_Best_Colony_Target(
        uint32_t* enemy_fleet_space_str_arr,
        uint32_t* our_space_str_arr,
        uint32_t* our_ground_str_arr,
        int16_t star_idx,
        int16_t player_idx,
        uint32_t* best_score,
        int16_t* best_colony_idx,
        int32_t preferred_owner);

    /**
     * Chooses a non-player combat target at a star.
     * @param star_idx Star containing the pending combat.
     * @param attacker_idx Non-player attacker identifier.
     * @param target_player_idx_out Receives the chosen ship-only defender, or -1.
     * @param target_colony_idx_out Receives the chosen colony target, or -1.
     */
    void __cdecl Get_NPC_Target(int16_t star_idx, int16_t attacker_idx, int16_t* target_player_idx_out, int16_t* target_colony_idx_out);

    void __cdecl Do_Dragon_Bribe(int player_idx, int16_t star_idx, int16_t dest_idx);

    void __cdecl Client_Combat_Loop();

    void __cdecl Bomb_Results_Popups(s_battle_results* res);

    void __cdecl Strategic_Results_Popup(s_battle_results* battle_results);

    void __cdecl Get_Combat_Ships(int16_t* ships, int16_t star_idx, int16_t plr1_idx, int16_t* plr1_offset, int16_t* plr1_count, int16_t plr2_idx, int16_t* plr2_offset, int16_t* plr2_count);

    int16_t __cdecl Russ_Combat(int16_t attacker_idx, int16_t* attacker_ships, int16_t attacker_count, int16_t defender_idx, int16_t* defender_ships, int16_t defender_count, int16_t star_idx, int16_t colony_idx, int8_t is_sneak_attack, s_battle_results* results);

    void __cdecl Kill_Noncombat_Ships(int16_t* ship_indices, int16_t count);

    /**
     * Marks a defeated fleet to retreat or be destroyed when no colony can shelter it.
     * @param star_idx Star where combat ended.
     * @param player_idx Owner of the retreating fleet.
     * @param colony_idx Colony that can still anchor the fleet, or -1.
     */
    void __cdecl Determine_Retreat_Ships(int16_t star_idx, int16_t player_idx, int16_t colony_idx);

    void __cdecl Reset_Combat_Ships_At_Current_Star_Has_Player(int16_t player_idx);

    void __cdecl Do_Attacker_Beat_Colony_Stuff(int16_t* ships, int n_ships, int16_t colony_idx, s_battle_results* results);

    char* __cdecl Load_Combfind_Strings(char* strings_block);

    s_colony* __cdecl Free_Colony_Slot();

    bool __cdecl Planet_Has_Splinter_Colony(s_planet_data* planet);

    int16_t __cdecl Target_Planet_To_Colony(int16_t planet_idx);

    int16_t __cdecl Target_Colony_To_Planet(int16_t colony_idx);

    /**
     * Resolves one combat at a star and optionally fills a battle-results record.
     * @param star_idx Star where combat occurs.
     * @param attacker_idx Attacking player or monster.
     * @param defender_idx Defending player or monster, or -1 to derive from the colony.
     * @param planet_idx Defending planet, or -1 for ship-only combat.
     * @param result Receives battle results when non-null.
     * @param is_sneak_attack Nonzero when sneak-attack handling applies.
     */
    int16_t __cdecl Do_1_Combat(int16_t star_idx, int16_t attacker_idx, int16_t defender_idx, int16_t planet_idx, s_battle_results* result, int8_t is_sneak_attack);

    int8_t __cdecl AI_Can_Retreat(int8_t player_idx, int16_t excluded_colony_idx);

    int8_t __cdecl Some_Star_Player_Is_Busy(int16_t star_idx);

    int16_t __cdecl Pick_Attacker_At_Star(int16_t star_idx);

    void __cdecl Collect_Targets_For_Combat(int16_t star_idx, int16_t player_idx);

    int8_t __cdecl Player_May_Attack_Player(int attacker_idx, int target_idx);

    void __cdecl Copy_Target_Info(s_combfind_target* src);

    void __cdecl Decode_Star_Player_Presence(NETPROTO::Packet_Reader_* reader, int16_t star_idx);

    int32_t __cdecl Do_Bombardment(int8_t attacker_id, int16_t colony_idx);

    uint8_t __cdecl Has_Stellar_Converter(int16_t* ship_list, int16_t ship_count);

    int32_t __cdecl N_Enemy_Stars_In_Range(int16_t player_idx, uint8_t enemy_player_idx);

    int32_t __cdecl N_Enemy_Stars(int8_t player_idx);

    int8_t __cdecl Player_Owns_Transports(int player_idx);

    void __cdecl GNN_Reports_Combat(int player1_idx, int player2_idx);

    void __cdecl Do_Nonhuman_Attack_Against_Human(int player_idx);

    /** Advances multiplayer combat synchronization state. */
    void __cdecl Do_Net_Combat_Update(bool unused);

    void __cdecl Send_Star_Player_Presence(NETPROTO::Packet_Writer_* writer, int16_t star_idx);
} // namespace COMBFIND