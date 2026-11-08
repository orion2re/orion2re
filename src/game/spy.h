#pragma once

struct s_leader_data;

namespace spy {
    enum e_steal_mode : uint8_t {
        STEAL_MODE_EXCEPT_CURRENT_FIELD,
        STEAL_MODE_EXCEPT_CURRENT_TECH,
        STEAL_MODE_ANYTHING,
    };

    struct s_spy_tech_bonus {
        uint8_t neural_scanner;
        uint8_t xeno_psychology;
        uint8_t telepathic_training;
        uint8_t cyber_security_link;
        uint8_t stealth_suit;
        uint8_t psionics;
    };

    struct s_spy_success {
        uint8_t steal;
        uint8_t steal_and_frame;
        uint8_t sabotage;
        uint8_t sabotage_and_frame;
    };

    struct s_spy_count_bonus {
        uint8_t plus_two;
        uint8_t plus_one;
    };

    struct s_spy_random_bonus {
        uint32_t attacker;
        uint32_t defender;
    };

    struct s_spy_guaranteed_roll {
        uint8_t steal;
        uint8_t sabotage;
    };

    extern s_spy_tech_bonus spy_tech_bonus;
    extern s_spy_success success;
    extern s_spy_count_bonus count_bonus;
    extern s_spy_random_bonus random_bonus;
    extern s_spy_guaranteed_roll guaranteed_roll;
    extern uint8_t telepathic_bonus;
    extern e_steal_mode steal_mode;
    extern bool frame_anyone;

    inline bool Current_Research_Allows_Theft(
        int16_t candidate_field, int16_t candidate_application, int8_t current_field, int8_t current_application) {
        if (steal_mode == STEAL_MODE_EXCEPT_CURRENT_FIELD) {
            return current_field != candidate_field;
        }
        if (steal_mode == STEAL_MODE_EXCEPT_CURRENT_TECH) {
            return current_application !=
                (int8_t)candidate_application;
        }
        return true;
    }

    extern int8_t lookup_table[GOVERNMENT_COUNT];

    /* Returns the spy bonus granted by the player's completed spy-related technologies.
     * @param player Player whose technology bonuses are evaluated.
     */
    int16_t __cdecl Tech_Spy_Bonus(s_player* player);

    /* Computes the player's defensive and offensive spy bonuses.
     * @param player Player whose spy modifiers are evaluated.
     * @param defense_bonus_out Receives the computed defensive bonus.
     * @param attack_bonus_out Receives the computed offensive bonus.
     */
    void __cdecl Compute_Spy_Bonuses(s_player* player, int16_t* defense_bonus_out, int16_t* attack_bonus_out);

    /* Resolves the turn's global spy phase for all active players. */
    void __cdecl Resolve_Spies(const AIDATA::s_player_assessment* players);

    extern int16_t spy_bonuses[16];

    /* Resolves assassin attempts made by eligible leaders this turn. */
    void __cdecl Resolve_Assassins();

    /* Resolves a completed assassination attempt against the given leader.
     * @param leader Leader targeted by the assassination event.
     */
    void __cdecl Leader_Assassinates(s_leader_data* leader);

    /* Allocates an AI player's available spies across home and foreign targets.
     * @param player AI player whose spies are being reassigned.
     */
    void __cdecl Allocate_AI_Spies(s_player* player, int16_t contact_count);

    /* Resolves all spy actions currently targeting the given player.
     * @param player Player whose incoming spy actions are processed.
     */
    void __cdecl Resolve_Player_Spies(s_player* player);

    /* Returns the player's spies home from the specified enemy.
     * @param player_idx Player whose spies are being recalled.
     * @param enemy_idx Enemy player the spies are returning from.
     */
    void __cdecl Bring_Home_Spies_Vs(int16_t player_idx, int16_t enemy_idx);

    /* Returns the roll bonus contributed by the given number of spies.
     * @param spy_count Number of spies contributing to the bonus.
     */
    int32_t __cdecl N_Spies_Bonus(int32_t spy_count);

    /* Attempts to steal a technology application from the victim.
     * @param attacker Player performing the theft.
     * @param victim Player being targeted.
     * @param frame_flag Nonzero to attempt framing another player for the theft.
     */
    void __cdecl Steal_App(s_player* attacker, s_player* victim, int8_t frame_flag);

    /* Selects another player to frame for a spy action when possible.
     * @param player_idx_a Acting player performing the spy action.
     * @param player_idx_b Victim player being targeted.
     * @param framed_player_idx Receives the selected framed player index.
     * @param tech_app_id Optional stolen tech id used to filter candidates, or -1.
     */
    bool __cdecl Frame_Another(int16_t player_idx_a, int16_t player_idx_b, int16_t* framed_player_idx, int32_t tech_app_id);

    /* Destroys a random victim building and reports the sabotage outcome.
     * @param framed_player_idx Player blamed for the sabotage.
     * @param victim_player_idx Player whose building is destroyed.
     * @param acting_player_idx Player who actually performed the sabotage.
     */
    void __cdecl Destroy_Random_Building(int16_t framed_player_idx, int16_t victim_player_idx, int32_t acting_player_idx);
} // namespace spy