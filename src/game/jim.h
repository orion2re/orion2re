#pragma once

struct s_animation_header;
struct s_mouse_list;

namespace JIM {
    extern int16_t jim_current_stream;

    extern int16_t jim_old_music_id;

    extern int16_t jim_fade_music_flag;
    extern int16_t jim_old_transition_id;

    extern s_mouse_list logo_mouse_list;
    extern int16_t darken_palette_value;

    // Returns true when the Simtex logo asset exists on either configured data path.
    bool __cdecl Check_For_Simtex_Logo();

    // Darkens the current palette to black and resets the logo display pages.
    void __cdecl Logo_Fadeout();

    // Plays the logo sequence and optionally the intro cinematic.
    void __cdecl Draw_Logos(void);

    // Starts a random looping background stream if one is not already active.
    void Play_Background_Music();

    // Stops the active background stream and clears JIM music state.
    void __cdecl Stop_Background_Music();

    // Starts a random looping combat stream when music is enabled.
    void __cdecl Play_Combat_Music();

    /* Starts a streamed music track and optional transition track.
     * @param music_id Primary stream identifier.
     * @param transition_id Follow-up stream identifier, or a sentinel for background fallback.
     * @param loop_flag Nonzero to loop the resulting stream.
     */
    void __cdecl Play_Streaming_Music(int16_t music_id, int16_t transition_id, int16_t loop_flag);

    // Expands name placeholders in buffer in place.
    void __cdecl Decode_Text_Name_And_Name(char* buffer, game::id_player first, game::id_player second, int16_t max_len);

    /* Loads a localized text entry into dest.
     * @param filename LBX filename containing the message table.
     * @param message_index Message-table entry index.
     * @param dest Destination buffer that receives the loaded message.
     * @param max_len Capacity of dest, including the terminator.
     */
    void __cdecl Get_Text_Message(const char* filename, int16_t message_index, char* dest, int16_t max_len);

    void __cdecl Decode_Text_Name_And_Tech(char* buffer, game::id_player player, int16_t tech_idx, int16_t max_len);

    void __cdecl Decode_Text_Name_And_Other_Tech(char* buffer, game::id_player player, game::id_player other, int16_t tech_idx, int16_t max_len);

    void __cdecl Decode_Text_System(char* buffer, int16_t system_idx, int16_t max_len);

    void __cdecl Decode_Text_Name_And_System(char* buffer, game::id_player player, int16_t system_idx, int16_t max_len);

    void __cdecl Decode_Text_Name_Tech_And_Other_Tech(char* buffer, game::id_player first, game::id_player second, int16_t first_tech_idx, int16_t second_tech_idx, int16_t max_len);

    void __cdecl Bill_Picture_To_Mouse(s_animation_header* anim);

    void __cdecl Decode_Text_Name_And_Stat(char* buffer, game::id_player player, int16_t numeric_value, int16_t max_len);

    void __cdecl Decode_Text_Tech_And_Other_Name(char* buffer, game::id_player player, int16_t tech_id, game::id_player other, int16_t max_len);

    void __cdecl Decode_Text_Race(char* buffer, game::id_player player, int16_t max_len);

    void __cdecl Decode_Text_Stat(char* buffer, int16_t numeric_value, int16_t max_len);

    /* Expands placeholder tokens in message_buffer in place.
     * @param message_buffer Source and destination tokenized message buffer.
     * @param player1_id Index used for first-player name or race substitutions.
     * @param star_id Index used for star or planet-name substitutions.
     * @param tech_app1_id Index used for the first technology-name substitution.
     * @param number Numeric value used for number substitution.
     * @param player2_id Index used for second-player name or race substitutions.
     * @param tech_app2_id Index used for the second technology-name substitution.
     * @param tech_field_id Index used for technology-field substitutions.
     * @param max_len Capacity of message_buffer, including the terminator.
     */
    void __cdecl Decode_Text_Message(char* message_buffer, game::id_player first, int16_t star_id, int16_t tech_app1_id, int16_t number, game::id_player second, int16_t tech_app2_id, int16_t tech_field_id, int16_t max_len, game::id_planet planet = game::NO_PLANET);

    void __cdecl Decode_Text_Year(char* buffer, int16_t max_len);

    void __cdecl Decode_Text_Field(char* buffer, int16_t field_id, int16_t max_len);
} // namespace JIM