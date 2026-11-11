#include "pch.h"

#include "newgame_options.h"

namespace NEWGAME {
    int16_t opp_button_variable;
    int16_t diff_button_variable;
    int16_t xenon_attacks_button_variable;
    int16_t civ_button_variable;
    uint16_t combat_button_variable;
    int16_t gal_size_button_variable;
    int16_t random_events_button_variable;
    int16_t gal_age_button_variable;

    namespace {
        bool random_player_count;

        int16_t Galaxy_Size_Button_From_Setting(uint8_t galaxy_size) {
            switch (galaxy_size) {
                case GALAXY_SIZE_CLUSTER:
                    return NEW_GAME_GALAXY_SIZE_HUGE;
                case GALAXY_SIZE_HUGE:
                    return NEW_GAME_GALAXY_SIZE_HUGE;
                case GALAXY_SIZE_MAXIMUM:
                    return NEW_GAME_GALAXY_SIZE_MAXIMUM;
                default:
                    return galaxy_size;
            }
        }

        uint8_t Galaxy_Size_Setting_From_Button(int16_t button) {
            switch (button) {
                case NEW_GAME_GALAXY_SIZE_HUGE:
                    return GALAXY_SIZE_HUGE;
                case NEW_GAME_GALAXY_SIZE_MAXIMUM:
                    return GALAXY_SIZE_MAXIMUM;
                default:
                    return (uint8_t)button;
            }
        }

        int16_t Civilization_Button_From_Setting(uint8_t civilization_level) {
            if (civilization_level == STARTING_CIVILIZATION_LEVEL_POST_WARP) {
                return NEW_GAME_CIVILIZATION_POST_WARP;
            }
            if (civilization_level == STARTING_CIVILIZATION_LEVEL_ADVANCED) {
                return NEW_GAME_CIVILIZATION_ADVANCED;
            }
            return civilization_level;
        }

        uint8_t Civilization_Setting_From_Button(int16_t button) {
            if (button == NEW_GAME_CIVILIZATION_POST_WARP) {
                return STARTING_CIVILIZATION_LEVEL_POST_WARP;
            }
            if (button == NEW_GAME_CIVILIZATION_ADVANCED) {
                return STARTING_CIVILIZATION_LEVEL_ADVANCED;
            }
            return (uint8_t)button;
        }
    } // namespace

    bool Random_Player_Count_Selected() {
        return random_player_count;
    }

    int16_t Player_Count_For_Initialization() {
        if (random_player_count ||
            MOX::settings.number_of_players == NEW_GAME_RANDOM_PLAYER_COUNT) {
            return NEW_GAME_RANDOM_PLAYER_COUNT;
        }
        return MOX::settings.number_of_players;
    }

    void __cdecl Globals_To_Temps() {
        diff_button_variable = (int16_t)(int8_t)MOX::settings.game_difficulty;
        gal_size_button_variable = Galaxy_Size_Button_From_Setting(MOX::settings.galaxy_size);
        gal_age_button_variable = (int16_t)(int8_t)MOX::settings.galaxy_age;
        const int16_t player_count = INITGAME::Normalize_New_Game_Player_Count(
            MOX::settings.number_of_players, NEW_GAME_DEFAULT_PLAYER_COUNT);
        random_player_count =
            player_count == NEW_GAME_RANDOM_PLAYER_COUNT;
        opp_button_variable = random_player_count
            ? NEW_GAME_PLAYER_COUNT_RANDOM
            : (int16_t)(player_count - 2);
        combat_button_variable = (uint16_t)(MOX::settings.strategic_combat_flag == 0);
        civ_button_variable = Civilization_Button_From_Setting(
            MOX::settings.starting_civilization_level);
        random_events_button_variable = (uint8_t)MOX::settings.random_events;
        xenon_attacks_button_variable = MOX::settings.xenons_exist;
    }

    void Temps_To_Globals() {
        MOX::settings.game_difficulty = (uint8_t)diff_button_variable;
        MOX::settings.galaxy_size = Galaxy_Size_Setting_From_Button(gal_size_button_variable);
        MOX::settings.galaxy_age = (uint8_t)gal_age_button_variable;
        random_player_count = opp_button_variable == NEW_GAME_PLAYER_COUNT_RANDOM;
        MOX::settings.number_of_players = random_player_count
            ? NEW_GAME_RANDOM_PLAYER_COUNT
            : (uint8_t)opp_button_variable + 2;
        MOX::settings.starting_civilization_level =
            Civilization_Setting_From_Button(civ_button_variable);
        MOX::settings.strategic_combat_flag = (combat_button_variable == 0);
        MOX::settings.random_events = (int8_t)random_events_button_variable;
        MOX::settings.xenons_exist = (uint8_t)xenon_attacks_button_variable;
    }
} // namespace NEWGAME
