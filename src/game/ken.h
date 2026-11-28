#pragma once
#include <cstdint>

struct s_animation_header;

namespace KEN {
    /* Initializes KEN subsystem state. */
    void __cdecl Ken_Init();

    /* Loads the ship picture segment for a player-owned ship type.
     * player_idx: Player owner index used to select the palette.
     * ship_type: Ship picture index within SHIPS.LBX.
     */
    /* Returns the ship picture segment for a player-owned ship type.
     * player_idx: Player owner index used to select the palette.
     * ship_type: Ship picture index within SHIPS.LBX.
     */
    s_animation_header* __cdecl Get_Ship_Picture_Seg(game::owner_slot owner, int16_t ship_type);

    /* Retrieves a KEN text message into the caller-provided buffer.
     * msg_index: Message index across the KENTEXT message tables.
     * buffer: Output buffer that receives the decoded text.
     * message_arg: Extra argument forwarded to JIM::Get_Text_Message.
     */
    void __cdecl Ken_Get_Text_Message(int32_t msg_index, char* buffer, int16_t message_arg);

    extern uint8_t personalities[14][10];

    /* Assigns NPC personalities, objectives, and ship themes. */
    void __cdecl Init_NPC_Personalities_Objectives_Themes();

    /* Performs secondary KEN initialization. */
    void __cdecl Ken_Init2();

    /* Loads and draws the ship palette for the specified owner index.
     * player_idx: Player or special-owner index.
     */
    void __cdecl Load_Player_Ship_Palette(game::owner_slot owner);

    /* Selects an index from a weight table, normalizing the table in place when needed.
     * weights: Input/output weight array that may be scaled down.
     * count: Number of valid entries in weights.
     */
    uint16_t __cdecl Get_Weighted_Choice(uint16_t* weights, int16_t count);

    /* Collects derelict reward applications for the specified player.
     * player_idx: Player receiving the derelict reward.
     * app_array: Output array of application IDs.
     * app_count: Output count written into app_array.
     */
    void __cdecl Get_Derelict_Applications(game::id_player player, uint16_t* app_array, int16_t* app_count);

    /* Returns the picture segment for a ship by ship-table index.
     * ship_idx: Index into the global ship table.
     */
    s_animation_header* Get_Ship_Picture_Seg(const s_ship_data& ship);
} // namespace KEN