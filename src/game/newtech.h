#pragma once
#include <cstdint>
#include "object_ids.h"

struct s_player;

namespace newtech {
    extern int16_t best_category[12];

    extern int32_t tech_research_level_values[30];

    struct s_ai_tech_priority {
        int16_t double_field_1;
        int16_t double_field_2;
        uint16_t double_application;
        uint16_t quintuple_application;
    };

    extern s_ai_tech_priority tech_priority;

    /**
     * Caches the best competitor field levels used by the tech chooser for the specified player.
     * @param player_id Player whose competition baseline is being prepared.
     */
    void __cdecl Set_Competition_Tech_Values(game::id_player player_id);

    /**
     * Calculates the AI desirability score for researching a specific technology application.
     * @param player_id Player whose research priorities are being evaluated.
     * @param tech_app_idx Technology application index to score.
     */
    int32_t __cdecl Calc_Tech_Value(game::id_player player_id, int16_t tech_app_idx);

    /**
     * Checks whether the player's required gray-box tech fields are all fully researched.
     * @param player Player state to inspect.
     */
    int8_t __cdecl All_Gray_Boxes_Researched(s_player* player);

    /** Randomly selects a fallback hyper-advanced technology application. */
    int32_t __cdecl Choose_Hyper_Advanced_Tech();

    /**
     * Chooses the next technology application for the player to research.
     * @param player_id Player whose available research choices are being evaluated.
     */
    int16_t __cdecl Choose_Tech_Application(game::id_player player_id);
} // namespace newtech
