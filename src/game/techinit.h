#pragma once
#include <cstdint>
#include <string>

struct s_tech_app_data;
struct s_tech_field_data;

namespace TECHINIT {
    inline constexpr int16_t POSTWARP_TECH_COUNT = 29;
    inline constexpr int16_t POSTWARP_TACTICAL_ONLY_TECH_COUNT = 7;

    extern int16_t postwarp_tech[POSTWARP_TECH_COUNT];
    extern uint8_t lithovore_allows_biomorphic_fungi;

    /**
     * Grants configured Post-warp technologies to every player.
     */
    void __cdecl Grant_Postwarp_Technologies();

    /**
     * Initializes global technology metadata and populates per-field application lists.
     */
    void __cdecl Init_Tech();

    /**
     * Rebuilds derived technology application types and field membership.
     */
    void __cdecl Rebuild_Derived_Tech_Data();

    bool Validate_And_Rebuild_Technology(s_tech_app_data* applications,
        s_tech_field_data* fields,
        const int16_t* starting_fields,
        std::string* diagnostic);

    bool Finalize_Configured_New_Game_Technology(
        std::string* diagnostic);

    /**
     * Loads localized technology, component, and hull names from the techname LBX resource.
     */
    void __cdecl Load_Tech_Names();

    /**
     * Returns whether a technology application is available to the specified player.
     * @param player_idx Index of the player being checked.
     * @param tech_id Technology application ID to validate.
     */
    int16_t __cdecl Tech_Is_Legal_For_Player(int16_t player_idx, int16_t tech_id);

    /**
     * Seeds a player's starting technology fields and applications.
     * @param player_idx Index of the player to initialize.
     */
    void __cdecl Init_Player_Tech(int16_t player_idx);
} // namespace TECHINIT