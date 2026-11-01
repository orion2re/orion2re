#pragma once
#include "leader_read.h"
#include <cstdint>

struct s_colors;
struct s_mouse_list;
struct s_animation_header;

namespace EVENTS {
    struct s_event_roll {
        uint32_t all_events;
        uint32_t lucky_player;
    };

    struct s_donation_bc {
        uint8_t turn_divisor;
        uint8_t multiplier;
        uint8_t base;
    };

    struct s_pirate_raid {
        uint8_t random_percent;
        uint8_t fixed_percent;
    };

    struct s_computer_virus {
        uint16_t random_research;
        uint8_t fixed_research;
    };

    struct s_supernova {
        uint16_t minimum_research_per_turn;
    };

    extern uint8_t current_event;
    extern s_animation_header* event_display_seg;
    extern s_event_data event_data[EVENT_COUNT];
    extern MOX2::action_func_t jim_saved_autofunction;
    extern uint32_t last_event_year;
    extern int16_t event_delay_count;
    extern int8_t event_good_array[EVENT_COUNT];
    extern uint8_t force_event_type;
    extern uint8_t all_events_delay;
    extern uint8_t between_events_delay;
    extern uint16_t event_delay[EVENT_RANDOM_COUNT];
    extern s_event_roll event_roll;
    extern s_donation_bc donation_bc;
    extern s_pirate_raid pirate_raid;
    extern s_supernova supernova;
    extern s_computer_virus computer_virus;

    struct s_event_duration {
        uint8_t anomaly_min;
        uint8_t beast_min;
        uint8_t flux_min;
        uint8_t anomaly_chance;
        uint8_t beast_chance;
        uint8_t flux_chance;
    };

    extern s_event_duration duration;

    int16_t __cdecl Get_Antaran_Victim();
    void __cdecl Init_Events();
    int32_t __cdecl Event_Get_Fleet_Strength(int16_t player_idx);

    int16_t __cdecl Event_Check_Comet(int16_t target_id);

    int16_t __cdecl Event_Check_Space_Anomaly(int16_t colony_idx);
    int16_t __cdecl Event_Check_Space_Anomaly_At_Star(int16_t star_idx);

    int16_t __cdecl Event_Check_Population_Boom(int16_t colony_idx);
    int16_t __cdecl Event_Check_Plague(int16_t colony_idx);

    int16_t __cdecl Event_Check_Pirate_Activity(uint8_t player_idx);
    int16_t __cdecl Event_Check_Colony_Researching(int16_t colony_idx);

    int16_t __cdecl Event_Check_Hyperspace_Flux();

    void __cdecl Event_Surrender(int8_t player_idx, int16_t target_id);

    int16_t __cdecl Colony_Has_Event(int16_t colony_idx);

    /** Clears a monster target retained for all players in a hotseat report pass. */
    void __cdecl Clear_Deferred_Monster_Event_Target();

    bool __cdecl Player_Has_Bad_Event(int16_t player_idx);

    void __cdecl Determine_Event();

    void __cdecl Event_Twiddle();

    void __cdecl Update_Lucky_Players_Events();

    int16_t __cdecl Determine_Lucky_Players_Events();

    /** Calculates a Donation event award for the given elapsed game turn. */
    int16_t __cdecl Donation_Amount(int32_t game_turn);

    /** Calculates a Pirate Raid loss from the selected player's treasury. */
    int16_t __cdecl Pirate_Raid_Amount(int32_t bc);

    /** Calculates the total research required to prevent a Supernova. */
    int16_t __cdecl Supernova_Research_Requirement(int16_t star_idx,
        int16_t turn_count);

    int16_t __cdecl Get_Event_Climate_Change_Colony(int16_t player_idx);

    int16_t __cdecl Pick_Random_Player_In_Contact(int16_t player_idx);

    int16_t __cdecl Pick_Random_Ship(int16_t player_idx);

    int16_t __cdecl Get_Event_Industrial_Accident_Colony(int16_t player_idx);

    int16_t __cdecl Get_Event_Mineral_Depletion_Colony(int16_t player_idx);
    int16_t __cdecl Get_Event_Mineral_Deposit_Colony(int16_t player_idx);

    int16_t __cdecl Pick_Random_Colony_No_Capitol(int16_t player_idx);

    int16_t __cdecl Pick_Random_Colony_No_Outpost(int8_t player_idx);

    int16_t __cdecl Pick_Random_Star(int16_t player_idx);

    int16_t __cdecl Star_Has_Event(int16_t star_idx);

    int16_t __cdecl Event_Pirate_Activity_Strength(int16_t star_idx);

    int16_t __cdecl Pick_Completely_Random_Star();

    void __cdecl Event_Check_Planet_Warning();

    void __cdecl Event_Check_General_Status();

    extern uint8_t event_status_flag;

    extern uint8_t* event_mouse_bitmap;

    extern char event_message[300];

    int16_t __cdecl Ship_Size_Points_At_Star(int16_t star_idx);

    void __cdecl Allocate_Comet_Hit(int16_t colony_idx);

    void __cdecl Allocate_Earthquake_Hit(int16_t colony_idx, int16_t* pop_killed, int16_t* buildings_destroyed_count);

    int16_t __cdecl Colony_Research_Points(int16_t colony_idx);

    void __cdecl Allocate_Industrial_Accident_Hit(int16_t colony_idx);

    int16_t __cdecl Get_Event_Victim(int8_t type);

    void __cdecl Event_Orion_Discovered(int8_t player_idx);
    void __cdecl Event_Race_Eliminated(int8_t player_idx);
    int16_t __cdecl Event_Check_Warp_Beast();

    void __cdecl Kill_All_Events_At_Colony(int16_t colony_idx);

    bool __cdecl Check_For_Event_Message();

    /** Advances event processing and queues any reportable events for display. */
    void __cdecl Check_For_Event();

    /** Initializes the event-report screen state, music, and drawing hooks. */
    void __cdecl Start_Main_Event();

    extern s_colors event_normal_colors;

    extern s_mouse_list event_blank_mouse;

    /** Fades in the event screen and primes its animation and mouse buffers. */
    void __cdecl Event_Fade_In();

    /** Draws the current event illustration and formatted report text. */
    void __cdecl Draw_Event_Screen();

    /**
     * Builds the localized message text for a specific event report variant.
     * @param colony_idx Colony index used by planet-name substitution tokens.
     * @param planet_idx Explicit planet index, or -1 to resolve through colony_idx.
     * @param val1 First numeric or lookup substitution value.
     * @param val2 Second numeric substitution value.
     * @param out_str Destination buffer that receives the formatted message.
     * @param out_str_size Size of the destination buffer in bytes.
     */
    void __cdecl Get_Event_Message(
        int16_t event_type,
        int16_t event_subtype,
        int16_t colony_idx,
        int16_t planet_idx,
        int16_t val1,
        int16_t val2,
        int16_t player_idx1,
        int16_t tech_field_idx1,
        int16_t player_idx2,
        int16_t leader_idx,
        int16_t player_idx3,
        int16_t star_idx,
        int16_t tech_app_idx1,
        int16_t tech_app_idx2,
        char* out_str,
        uint32_t out_str_size,
        leader_read::s_leader* disclosed_leader = nullptr);

    /** Runs the event-report input loop until the player dismisses the screen. */
    void __cdecl Drive_Event_Screen();

    /** Loads the event illustration for the specified event. */
    void __cdecl Load_Event_Picture(int16_t event_id);

    /** Prepares and presents the next pending event report. */
    void __cdecl Setup_Next_Event(int16_t event_id);

    /** Reinitializes the event table after a full cleanup checkpoint. */
    void __cdecl Reset_Events();

    /**
     * Updates monster-event state after a monster move or resolution step.
     * @param monster_type Nonplayer monster ship type used to select the event slot.
     * @param target_id Target index for a location update, or -1 for a payload-only update.
     * @param player_idx_or_amount Player index for target updates, or amount for payload updates.
     */
    void __cdecl Event_Monster_Update(int16_t monster_type, int16_t target_id, int16_t player_idx_or_amount);
} // namespace EVENTS