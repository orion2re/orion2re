#pragma once

#include "game/netadapter.h"

namespace netcode {
    using recv_callback_fn = void(__cdecl*)(int16_t net_id, uint16_t packet_id, uint32_t len, uint8_t* packet);

    // Unwinds multiplayer screens that can be blocked waiting for a peer.
    struct session_lost {};

    extern uint32_t thisHost;

    extern uint32_t netInitialized;

    extern int8_t tpSending;

    extern uint8_t inComms;

    extern recv_callback_fn recv_fn;

    extern int32_t connection_type;

    extern int32_t in_send_message;

    /** Returns a specific diagnostic for the last failed initialization, or an empty string. */
    const char* __cdecl Net_Initialization_Error();
    /** Returns a specific diagnostic for the last failed session join, or an empty string. */
    const char* __cdecl Net_Join_Error();

    /** Initializes networking while pumping MOX before and after backend setup.
     * @param mode Connection backend selector.
     * @param recv_fn_ptr Callback invoked for received packets.
     * @param network Network configuration for router-backed modes.
     */
    int32_t __cdecl Do_Net_Init(uint32_t mode, recv_callback_fn recv_fn_ptr, s_network* network);

    /** Initializes the requested networking backend.
     * @param mode Connection backend selector.
     * @param recv_fn_ptr Callback invoked for received packets.
     * @param network Network configuration for router-backed modes.
     */
    int32_t __cdecl Net_Init(uint32_t mode, recv_callback_fn recv_fn_ptr, s_network* network);

    /** Returns the number of discovered game sessions. */
    int32_t __cdecl Net_Get_Game_Count();

    /** Requests a fresh session list from the active router and returns the cached count after pumping for a response. */
    int32_t __cdecl Net_Refresh_Game_List();

    /** Returns the number of known players in the active session list. */
    int32_t __cdecl Get_Player_Count();

    /** Returns the display name for a discovered session.
     * @param session_idx Zero-based session index.
     */
    char* __cdecl Net_Get_Game_Name(uint32_t session_idx);

    /** Returns the router session ID for a discovered session.
     * @param session_idx Zero-based session index.
     */
    uint32_t __cdecl Net_Get_Game_Session_Id(uint32_t session_idx);

    /** Joins a discovered game session by ID and creates the local player.
     * @param session_id Router session ID to join.
     * @param password Optional session password.
     * @param player_name Local player display name.
     */
    int32_t __cdecl Net_Join_Game(uint32_t session_id, const char* password, const char* player_name);

    /** Shuts down the active networking backend and releases session state. */
    void __cdecl Net_Uninit();

    /** Returns whether a transport send is currently in progress. */
    int32_t __cdecl Net_Is_Sending();

    /** Writes connected user IDs into the caller buffer and returns the count.
     * @param users Output buffer for 16-bit user IDs, or nullptr to query the count only.
     */
    int32_t __cdecl Net_Get_Users(int16_t* users);
    void __cdecl Net_Accept_Member(int32_t network_id);

    /** Prevents the active router session from accepting new players. */
    void __cdecl Net_Disable_New_Players();

    /** Returns the current host network ID for the active backend. */
    uint32_t __cdecl Net_Host_ID();
    uint32_t __cdecl Net_Session_Owner_ID();

    /** Sends a packet through the active networking backend.
     * @param target_id Recipient network ID.
     * @param msg_type Application-defined message type.
     * @param data Packet payload buffer.
     * @param size Packet payload size in bytes.
     */
    uint32_t __cdecl Net_Send_Message(int32_t target_id, int16_t msg_type, void* data, uint32_t size);

    /** Pumps backend state and pending communication work. */
    void __cdecl Net_Update();

    /** Returns transport status flags for the active backend. */
    uint32_t __cdecl Net_Status();

    /** Copies logical game traffic and router transport statistics without pumping networking. */
    int32_t __cdecl Net_Get_Traffic_Stats(netadapter::s_traffic_stats* stats);

    /** Creates a new game session and local player.
     * @param name Session display name.
     * @param max_players Maximum player count.
     * @param password Optional session password.
     * @param player_name Local player display name.
     */
    int32_t __cdecl Net_Create_Game(const char* name, int32_t max_players, const char* password, const char* player_name);
} // namespace netcode