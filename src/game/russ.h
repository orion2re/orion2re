#pragma once

#include <optional>
#include <cstdint>
#include <vector>

namespace RUSS {
    /** Legacy variadic formatting entry point.
     * @param unused1 Legacy control value.
     * @param unused2 Legacy control value.
     * @param unused_str Format string pointer.
     */
    void __cdecl PFMT(int32_t unused1, int32_t unused2, const char* unused_str, ...);
    extern int8_t all_ai_game;

    /** Returns a weighted random index from an 8-bit weight table, or -1 when all weights are zero.
     * @param weights Pointer to count weight values.
     * @param count Number of entries in weights.
     */
    int16_t __cdecl Get_Weighted_Choice_Char(uint8_t* weights, int16_t count);
    std::optional<uint32_t> Choose_Weighted_Index_U8(const uint8_t* weights, uint32_t count);

    /** Returns a weighted random index from a 16-bit weight table, or -1 when all weights are zero.
     * @param weights Pointer to count weight values.
     * @param count Number of entries in weights.
     */
    int16_t __cdecl Get_Weighted_Choice_Short(const uint16_t* weights, int16_t count);
    std::optional<uint32_t> Choose_Weighted_Index_U16(const uint16_t* weights, uint32_t count);

    /** Returns a weighted random index from a 32-bit weight table, or -1 when all weights are zero.
     * @param weights Pointer to count weight values.
     * @param count Number of entries in weights.
     */
    int16_t __cdecl Get_Weighted_Choice_Int(uint32_t* weights, int16_t count);
    std::optional<uint32_t> Choose_Weighted_Index_U32(const uint32_t* weights, uint32_t count);

    /** Returns the display name for a star index, or a fallback name when out of range.
     * @param star_index Star table index to resolve.
     */
    char* __cdecl Star_Name(int16_t star_index);

    /** Maps a star index to its monster group identifier.
     * @param star_index Star table index to classify.
     */
    int16_t __cdecl Star_To_Monster(int16_t star_index);

    /** Runs the regular MOX update and then flips the display pages. */
    void __cdecl Mox_Sync_Update();

    /** Runs per-frame sound and networking updates. */
    void __cdecl Mox_Update();

    /** Placeholder settings assertion hook; the current implementation is a no-op. */
    void __cdecl Assert_Settings();

    /** Randomly shuffles an array of bytes in place.
     * @param buffer Buffer to shuffle.
     * @param length Number of elements in the buffer.
     */
    void __cdecl Shuffle_Char(int8_t* buffer, int16_t length);

    /** Randomly shuffles an array of 16-bit integers in place.
     * @param data Buffer to shuffle.
     * @param count Number of elements in the buffer.
     */
    void __cdecl Shuffle_Sint(int16_t* data, int16_t count);

    /** Randomly shuffles an array of 32-bit integers in place.
     * @param array Buffer to shuffle.
     * @param count Number of elements in the buffer.
     */
    void __cdecl Shuffle_Int(int32_t* array, int16_t count);

    /** Randomly shuffles fixed-size elements in place.
     * @param buffer Buffer containing the elements to shuffle.
     * @param element_size Size of each element in bytes.
     * @param num_elements Number of elements in the buffer.
     */
    void __cdecl Shuffle(void* buffer, int16_t element_size, int16_t num_elements);

    /** Resets an allocation segment back to its first block.
     * @param segment Segment handle to reset.
     */
    void __cdecl Release_All_Blocks(void* segment);

    /** Converts a single-player game into an all-AI game when allowed. */
    void __cdecl Make_All_AI_Game();

    /** Replaces a player slot with AI settings and recalculates its colonies.
     * @param player_idx Player index to convert.
     */
    void __cdecl Make_Player_Into_AI(int32_t player_idx);

    /** Compares two ratios without converting them to floating point.
     * @param first_numerator Numerator of the first ratio.
     * @param first_denominator Denominator of the first ratio.
     * @param second_numerator Numerator of the second ratio.
     * @param second_denominator Denominator of the second ratio.
     */
    int32_t __cdecl Compare_Ratios(uint32_t first_numerator, uint32_t first_denominator, uint32_t second_numerator, uint32_t second_denominator);

    /** Loads the shared RUSS string table and hands slices to subsystem loaders. */
    void __cdecl Load_Russ_Strings();

    extern std::vector<char> russ_strings;

    extern const char s_Seed_ld[];
    extern const char s_Map_d[];
} // namespace RUSS