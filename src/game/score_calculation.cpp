#include "pch.h"

namespace SCORE {
    s_hiscore* hi;

    int16_t __cdecl Get_Elimination_Score(game::id_player player) {
        int16_t score = 0;
        SCORE::hi->num_opponents_defeated = 0;

        if (0 < game::current_state.player_count) {
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                uint16_t defeated = game::current_state.players.at(player.id).player_defeated[i];
                if (defeated != 0) {
                    score += 50;
                    SCORE::hi->num_opponents_defeated++;
                }
            }
        }

        return score;
    }

    int32_t __cdecl Get_Captured_Colonist_Score(game::id_player player) {
        int16_t captured_pop = game::current_state.players.at(player.id).captured_population;

        SCORE::hi->raw_captured_colonies = captured_pop;

        int32_t product = (int32_t)((uint32_t)(uint16_t)captured_pop * 2);

        return product / (int32_t)(Galaxy_Size_For_Classic_Rules_(MOX::settings.galaxy_size) + 1);
    }

    void __cdecl Calculate_Hi_Scores(game::id_player player) {
        char scor_buf[16];

        // This local string copy appears in the original binary but is not explicitly used.
        strlcpy(scor_buf, "scor", sizeof(scor_buf));

        SCORE::hi->player = player;
        SCORE::hi->score_elimination = SCORE::Get_Elimination_Score(player);
        SCORE::hi->score_technology = SCORE::Get_Technology_Score(player);
        SCORE::hi->score_antares = SCORE::Get_Antares_Score(player);
        SCORE::hi->score_orion = SCORE::Get_Orion_Score(player);
        SCORE::hi->score_council = SCORE::Get_Won_Council_Score(player);
        SCORE::hi->score_captured_colonists = SCORE::Get_Captured_Colonist_Score(player);
        SCORE::hi->score_population = SCORE::Get_Population_Score(player);
        SCORE::hi->score_time = SCORE::Get_Time_Score();

        uint16_t raw_subtotal = (uint16_t)SCORE::hi->score_time;
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_antares);
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_council);
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_captured_colonists);
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_orion);
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_population);
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_technology);
        raw_subtotal = (uint16_t)(raw_subtotal + (uint16_t)SCORE::hi->score_elimination);
        SCORE::hi->score_raw_subtotal = (int16_t)raw_subtotal;

        SCORE::hi->score_modifier_pct = raceopt::Player_Score_Modifier(SCORE::hi->player);

        int32_t product = (int32_t)SCORE::hi->score_modifier_pct * (int32_t)SCORE::hi->score_raw_subtotal;
        SCORE::hi->score_final = (int16_t)((product + 50) / 100);
    }

    int16_t __cdecl Get_Technology_Score(game::id_player player) {
        int16_t tech_points;
        int16_t total_techs;

        SCORE::Get_Technology(player, &tech_points, &total_techs);

        return (tech_points * 3) + (total_techs * 5);
    }

    /**
     * @brief Calculates the score bonus for capturing Antares.
     * @param player_idx The index of the player.
     * @return The configured Antares bonus if captured, 0 otherwise.
     */
    int16_t __cdecl Get_Antares_Score(game::id_player player) {
        if (SCORE::Captured_Antares(player)) {
            return (int16_t)victory_bonus.antares;
        }

        return 0;
    }

    int16_t __cdecl Get_Orion_Score(game::id_player player) {
        if (SCORE::Captured_Orion(player)) {
            return (int16_t)victory_bonus.orion;
        }

        return 0;
    }

    /**
     * Calculates the time-based score component.
     * The score depends on the galaxy size, the number of players, and the current stardate.
     * Returns 0 if the player has no population.
     */
    int16_t __cdecl Get_Time_Score() {
        int32_t time_score =
            ((Galaxy_Size_For_Classic_Rules_(MOX::settings.galaxy_size) + 5) *
                game::current_state.player_count * 20) -
            (int16_t)((uint16_t)MOX::stardate + 30536);

        return (SCORE::hi->raw_population != 0) ? (int16_t)time_score : 0;
    }

    /**
     * Calculates and stores the total population score for a given player.
     * Updates the raw_population field in the high score structure.
     */
    int16_t __cdecl Get_Population_Score(game::id_player player) {
        hi->raw_population = SCORE::Get_Total_Population(player);
        return hi->raw_population;
    }

    int8_t __cdecl Captured_Orion(game::id_player player) {
        return game::current_state.players.at(player.id).captured_orion;
    }

    void __cdecl Get_Technology(game::id_player player, int16_t* tech_count, int16_t* hyper_tech_count) {
        const auto& record = game::current_state.players.at(player.id);
        *tech_count = 0;
        *hyper_tech_count = 0;

        // Count completed technology fields (value 3 indicates known/researched)
        for (int16_t i = 0; i < TECH_FIELD_COUNT; ++i) {
            if (record.tech_fields[i] == 3) {
                *tech_count += 1;
            }
        }

        // Count active hyper advanced technologies
        for (int16_t i = 0; i < 8; ++i) {
            if (record.hyper_advanced_tech[i] != 0) {
                *hyper_tech_count += 1;
            }
        }
    }

    /**
     * @brief Checks if the specified player has captured Antares.
     *
     * @param player_idx The index of the player to check.
     * @return int8_t Non-zero if Antares was captured, zero otherwise.
     */
    int8_t __cdecl Captured_Antares(game::id_player player) {
        return game::current_state.players.at(player.id).captured_antares;
    }

    int16_t __cdecl Get_Total_Population(game::id_player player) {
        int16_t total_pop = 0;

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            if (roster_colony.owner == player) {
                // Assembly zero-extends the n_pops byte into CX before adding to AX
                total_pop += (uint8_t)roster_colony.n_pops;
            }
        }

        return total_pop;
    }

    int8_t __cdecl Won_Council_Vote(game::id_player player) {
        return (int8_t)game::current_state.players.at(player.id).won_council_vote;
    }

    int16_t __cdecl Get_Won_Council_Score(game::id_player player) {
        return SCORE::Won_Council_Vote(player) != 0
            ? (int16_t)victory_bonus.council
            : 0;
    }

    int16_t __cdecl Player_Score(game::id_player player) {
        allocate::Push_Block(MOX::screen_seg);
        hi = (s_hiscore*)allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, sizeof(s_hiscore), alignof(s_hiscore));
        memset(hi, 0, sizeof(s_hiscore));
        Calculate_Hi_Scores(player);
        allocate::Pop_Block(MOX::screen_seg);
        return hi->score_final;
    }
} // namespace SCORE
