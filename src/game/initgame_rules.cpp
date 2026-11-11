#include "pch.h"

#include "string_overrides.h"
#include "lua/lua_game.h"

namespace INITGAME {

    static uint32_t Starting_Money() {
        const int civilization_level =
            MOX::settings.starting_civilization_level;
        if (civilization_level >= STARTING_CIVILIZATION_LEVEL_PRE_WARP &&
            civilization_level < STARTING_CIVILIZATION_LEVEL_COUNT) {
            const int32_t configured_money =
                tech_start[civilization_level].money;
            if (configured_money >= 0) {
                return (uint32_t)configured_money;
            }
        }
        return start.money;
    }

    void __cdecl Init_Players(void) {
        std::string technology_diagnostic;
        if (!TECHINIT::Finalize_Configured_New_Game_Technology(
                &technology_diagnostic)) {
            const std::string message =
                "Lua new_game.technology_setup failed: " +
                technology_diagnostic;
            exitm::Exit_With_Message(message.c_str());
        }
        for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            s_player& player = MOX::player[player_idx];

            if (player.objectives == PLAYER_OBJECTIVE_HUMAN) {
                struct_::Copy_Structure(
                    player.evolutionary_upgrade_traits,
                    player.traits,
                    sizeof(player.traits));
                continue;
            }

            int16_t race_idx = 0;
            while (true) {
                bool unique_race = true;
                race_idx = (int16_t)(game_random::Random(STOCK_RACE_COUNT) - 1);

                for (int16_t prev_player_idx = 0; prev_player_idx < player_idx; ++prev_player_idx) {
                    if (!unique_race) {
                        break;
                    }

                    if ((uint8_t)MOX::player[prev_player_idx].race ==
                        (uint8_t)race_idx) {
                        unique_race = false;
                    }
                }

                if (unique_race) {
                    break;
                }
            }

            player.race = (uint8_t)race_idx;
            strlcpy(player.race_name, MOX::race_names[race_idx], sizeof(player.race_name));

            struct_::Copy_Structure(player.traits, RACESEL::race_specials[race_idx], sizeof(player.traits));
            struct_::Copy_Structure(player.evolutionary_upgrade_traits, RACESEL::race_specials[race_idx], sizeof(player.evolutionary_upgrade_traits));
            RUSS::Mox_Update();

            RACECUST::Race_Customization(player_idx);
            RUSS::Mox_Update();

            for (int16_t trait_idx = 1; trait_idx < 10; ++trait_idx) {
                uint8_t trait_value = (uint8_t)player.traits[trait_idx];
                player.traits[trait_idx] =
                    trait_value ? raceopt::pick_rules.primary[trait_idx - 1][trait_value - 1].value : 0;

                trait_value = (uint8_t)player.evolutionary_upgrade_traits[trait_idx];
                player.evolutionary_upgrade_traits[trait_idx] =
                    trait_value ? raceopt::pick_rules.primary[trait_idx - 1][trait_value - 1].value : 0;
            }

            RUSS::Mox_Update();

            int16_t color_idx = (int16_t)(game_random::Random(8) - 1);
            while (true) {
                bool duplicate_color = false;

                for (int16_t prev_player_idx = 0; prev_player_idx < player_idx; ++prev_player_idx) {
                    if (MOX::player[prev_player_idx].color == (uint8_t)color_idx) {
                        duplicate_color = true;
                    }
                }

                if (!duplicate_color) {
                    break;
                }

                int32_t next_color_idx = (int16_t)color_idx + 1;
                int32_t sign_bits = next_color_idx >> 31;
                color_idx = (int16_t)((((next_color_idx ^ sign_bits) - sign_bits) & 7 ^ sign_bits) - sign_bits);
            }

            player.color = (uint8_t)color_idx;
            RUSS::Mox_Update();

            allocate::Push_Block(MOX::screen_seg);

            const uint32_t name_index = race_idx * 8 + game_random::Random(8) - 1;
            char* player_name = (char*)farload::Far_Reload_Next_Data(
                "RACENAME.LBX",
                0,
                MOX::screen_seg,
                name_index,
                1,
                20);
            string_overrides::Apply_String("RACENAME.LBX", name_index + 1, player_name, 20);
            RUSS::Mox_Update();

            allocate::Pop_Block(MOX::screen_seg);

            strlcpy(player.name, player_name, sizeof(player.name));
        }

        RUSS::Mox_Update();
        KEN::Init_NPC_Personalities_Objectives_Themes();
        RUSS::Mox_Update();

        const int32_t starting_money = (int32_t)Starting_Money();
        for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            s_player& player = MOX::player[player_idx];

            player.officer_for_hire = -1;
            player.offensive_stagepoint = -1;
            player.contact[player_idx] = 1;
            player.player_done_flags = 0;
            player.bc = starting_money;
            RUSS::Mox_Update();

            player.spies[player_idx] = 0;
            player.history_btns = 'O';
            player.ignoring = 0;
            player.n_freighters = 0;

            for (int16_t other_player_idx = 0; other_player_idx < MOX::NUM_PLAYERS; ++other_player_idx) {
                player.last_tech_report_date[other_player_idx] = MOX::stardate;
            }

            RUSS::Mox_Update();
            TECHINIT::Init_Player_Tech(player_idx);
            RUSS::Mox_Update();
            const orion2re::lua::game::s_player_setup_event setup_event = {
                player_idx,
            };
            orion2re::lua::game::Player_Setup(&setup_event);
            INITSHIP::Init_Ship_Designs(player_idx);

            const int16_t best_fuel_idx = INITSHIP::Best_Ship_Fuel(player_idx);
            player.ship_range = TECHDATA::fuel[best_fuel_idx].ship_range;
        }

        MOX::player[MOX::PLAYER_NUM].contact[MOX::PLAYER_NUM] = 1;
        RUSS::Mox_Update();
        HOMEGEN::Assign_Home_Star_Names();
        RUSS::Mox_Update();
        diplomac::Init_Diplomatic_Relations();
        RUSS::Mox_Update();
    }

    int16_t Normalize_New_Game_Player_Count(
        int16_t requested_player_count,
        int16_t fallback_player_count) {
        if ((requested_player_count >= 2 && requested_player_count <= MAX_PLAYERS) ||
            requested_player_count == NEW_GAME_RANDOM_PLAYER_COUNT) {
            return requested_player_count;
        }
        if (fallback_player_count >= 2 && fallback_player_count <= MAX_PLAYERS) {
            return fallback_player_count;
        }
        return NEW_GAME_DEFAULT_PLAYER_COUNT;
    }

    int16_t Resolve_New_Game_Player_Count(
        int16_t requested_player_count,
        const s_player* players) {
        if (requested_player_count != NEW_GAME_RANDOM_PLAYER_COUNT) {
            return requested_player_count;
        }

        int16_t minimum_player_count = MAX_PLAYERS;
        for (int16_t player_idx = 1; player_idx < MAX_PLAYERS - 1; ++player_idx) {
            if (players[player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                minimum_player_count = player_idx == 1 ? 2 : player_idx;
                break;
            }
        }

        return minimum_player_count +
            (int16_t)game_random::Random(MAX_PLAYERS - minimum_player_count + 1) - 1;
    }

} // namespace INITGAME