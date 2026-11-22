#pragma once

namespace diplomac {
    constexpr int STARTING_RELATIONS_RACE_COUNT = 14;

    extern uint8_t alternate_diplomacy;


    extern int8_t diplomacy_proposal_tribute_bribe;

    extern int16_t diplomacy_proposal_gold_bribe;

    extern uint8_t diplomacy_proposal_tech_bribe1;

    extern int16_t starting_relations[STARTING_RELATIONS_RACE_COUNT][STARTING_RELATIONS_RACE_COUNT];

    extern int16_t personality_peace_duration[12];

    extern int16_t personality_break_peace_treaty_chance[8];

    extern int16_t personality_sneak_attack_modifiers[8];

    void Init_Diplomatic_Relations();

    void __cdecl Change_Relations(int16_t amount, int16_t p_a, int16_t p_b, int16_t incident, int16_t system, int16_t value);

    void __cdecl Declare_War(int16_t player_idx, int16_t target_idx);

    void __cdecl Declare_Peace(int16_t p1, int16_t p2);

    int __cdecl Fleet_Comparison(int16_t player_idx, int16_t opponent_idx);

    void __cdecl Break_Treaties(int16_t initiator_idx, int16_t target_idx);

    void __cdecl Break_Trade(int16_t p1_idx, int16_t p2_idx);

    void __cdecl Break_Research(int16_t player1_idx, int16_t player2_idx);

    void __cdecl Break_Tribute(int16_t p1_idx, int16_t p2_idx);

    void __cdecl Get_Break_Treaty_Message(int16_t victim_idx, int16_t actor_idx, int16_t* msg_id);

    void __cdecl Get_Break_Trade_Message(int16_t other_player_idx, int16_t this_player_idx, int16_t* out_message_id);

    void __cdecl Get_Break_Research_Message(int16_t other_player_idx, int16_t this_player_idx, int16_t* out_msg_id);

    void __cdecl Get_Break_Tribute_Treaty_Message(int16_t other_player_idx, int16_t acting_player_idx, int16_t* out_msg_id);

    void __cdecl Start_Trade_Treaty(int16_t player_idx, int16_t target_player_idx);

    void __cdecl Start_Research_Treaty(int16_t player_idx, int16_t other_idx);

    void __cdecl Modifier_Diplomacy_Adjustments();

    void NoOp2();

    void Clear_Diplomacy_Messages();

    void __cdecl Diplomacy_Growth();

    void Determine_First_Contacts();

    void Determine_Diplomacy_Messages();

    void __cdecl Limit_Treaty_Modifiers();

    /**
     * Chooses a hostile diplomacy message or escalation for the current incident.
     * @param current_player_idx Player generating the response.
     * @param target_player_idx Other player involved in the incident.
     */
    void __cdecl Determine_Bad_Message(int16_t current_player_idx, int16_t target_player_idx);

    /**
     * Chooses a favorable diplomacy message and any follow-up demand for the current incident.
     * @param player_idx Player generating the response.
     * @param other_player_idx Other player involved in the incident.
     */
    void __cdecl Determine_Good_Message(int16_t player_idx, int16_t other_player_idx);

    void __cdecl Peace_Proposal(int16_t player_idx, int16_t target_idx, int16_t unused = 0);

    /**
     * Tries to queue a diplomacy message asking another player to attack a third party.
     * @param player_idx Requesting player index.
     * @param other_player_idx Player receiving the request.
     */
    void __cdecl Required_Alliances(int16_t player_idx, int16_t other_player_idx);

    /**
     * Runs the NPC-only alliance-pressure path that can immediately redirect war state.
     * @param p1_idx Requesting NPC player index.
     * @param p2_idx Other player involved in the check.
     */
    void __cdecl NPC_Required_Alliances(int16_t p1_idx, int16_t p2_idx);

    void __cdecl Unprovoked_Good_Messages(int16_t player_idx, int16_t target_idx);

    void __cdecl End_Of_Turn_Diplomacy_Adjustments();

    void __cdecl Reward_Check(int16_t amount, int16_t p_a, int16_t p_b, int16_t system, int16_t value);

    void __cdecl NPC_Sneak_Attacks();

    /**
     * Evaluates whether an AI player escalates toward threats, demands, or a sneak attack.
     * @param player_idx Evaluating player index.
     * @param target_idx Candidate target index.
     * @param out_eval_threshold Receives the severity threshold used for the chosen action.
     * @param out_message_id Receives the diplomacy message id for the chosen action.
     * @param out_unused Preserved unused output slot from the original call signature.
     */
    int16_t __cdecl Sneak_Attack_Evaluations(int16_t player_idx, int16_t target_idx, int16_t* out_eval_threshold, int16_t* out_message_id, int16_t* out_unused);

    int16_t __cdecl Get_Player_Diplomacy_Personality(int16_t player_idx, int16_t target_idx);

    int16_t __cdecl Get_War_Count(int16_t player_idx, int16_t exclude_idx);

    void __cdecl Sneak_Attack_Or_Declare_War(int16_t attacker_idx, int16_t target_idx, int8_t message_idx);

    int16_t __cdecl Total_War_Loss_Comparison(int16_t attacker_idx, int16_t target_idx);

    /**
     * Scores a diplomacy proposal and returns the AI response band.
     * @param evaluator_idx Player evaluating the proposal.
     * @param proposer_idx Player making the proposal.
     * @param difficulty_mod Baseline difficulty adjustment applied before treaty modifiers.
     * @param treaty_type Treaty modifier bucket to include in the score.
     */
    int16_t __cdecl Diplomacy_Test(int16_t evaluator_idx, int16_t proposer_idx, int16_t difficulty_mod, int16_t treaty_type);

    /**
     * Chooses a tribute, tech, gold, or system demand and stores it in the proposal globals.
     * @param player_idx Demanding player index.
     * @param target_idx Player being evaluated as the demand target.
     * @param severity Demand strength from 1 to 10.
     * @param demands_tribute Nonzero to allow tribute demands.
     * @param demands_tech Nonzero to allow technology demands.
     * @param demands_gold Nonzero to allow gold demands.
     * @param demands_system Nonzero to allow system demands.
     */
    int16_t __cdecl Get_Demands(int16_t player_idx, int16_t target_idx, int16_t severity, int16_t demands_tribute, int16_t demands_tech, int16_t demands_gold, int16_t demands_system);

    void __cdecl Set_Demands(int16_t current_player, int16_t target_player);

    void __cdecl Start_Tribute_Treaty(int16_t player_idx, int16_t target_idx, int16_t amount);

    int __cdecl Fleet_Strength_Comparison(int16_t player_idx_1, int16_t player_idx_2);

    int __cdecl Tech_Strength_Comparison(int16_t player_idx1, int16_t player_idx2);

    int __cdecl Maintenance_Strength_Comparison(int16_t player_idx_1, int16_t player_idx_2);

    void __cdecl Start_Treaty(int16_t p1, int16_t p2, int16_t treaty_type);

    void __cdecl Adjust_Diplomat_Modifiers(int16_t player_idx, int16_t target_idx);

    void __cdecl Get_Declare_War_Message(int16_t player_a_idx, int16_t player_b_idx, int16_t* msg_id);

    void __cdecl Diplomacy_Broken_Contacts();

    void __cdecl Clear_Last_Threat_Turn(int16_t player_idx_1, int16_t player_idx_2);

    int16_t __cdecl Find_Worst_Modifier(int16_t player_idx, int16_t target_idx);

    int16_t Valid_Treaty_Proposal(int16_t player_idx, int16_t target_idx, int16_t treaty_type);

    /**
     * Evaluates a treaty proposal and fills any resulting counter-demand details.
     * @param player_idx Player evaluating the proposal.
     * @param target_idx Proposing player.
     * @param proposal_type Treaty or proposal type code.
     * @param response Receives the response message code.
     * @param demand_type Receives the counter-demand category when one is generated.
     * @param demand_tech Receives the demanded tech id when applicable.
     * @param demand_gold Receives the demanded gold amount when applicable.
     * @param demand_system Receives the demanded system id when applicable.
     * @param demand_tribute Receives the demanded tribute level when applicable.
     */
    int16_t __cdecl Check_Treaty_Proposal(int16_t player_idx, int16_t target_idx, int16_t proposal_type, int16_t* response, int16_t* demand_type, int16_t* demand_tech, int16_t* demand_gold, int16_t* demand_system, int16_t* demand_tribute);

    void __cdecl Get_Gift_Response(int16_t plr_a, int16_t plr_b, int16_t gift_type, int16_t gift_data, int16_t* response_code);

    /**
     * Evaluates whether a player accepts a diplomacy demand and selects the reply message.
     * @param player_a Demanding player index.
     * @param player_b Responding player index.
     * @param demand_type Demand category code.
     * @param demand_value Demand payload such as a target player, tech id, or system id.
     * @param accepted Receives 1 when the demand is accepted, otherwise 0.
     * @param response_code Receives the response message code.
     */
    void __cdecl Get_Demand_Response(int16_t player_a, int16_t player_b, int16_t demand_type, int16_t demand_value, int16_t* accepted, int16_t* response_code);

    int32_t __cdecl Tech_Comparison(int16_t player_1, int16_t player_2);

    int32_t __cdecl Population_Comparison(int16_t player_idx1, int16_t player_idx2);
} // namespace diplomac