#include "pch.h"

#include "legacy_world.h"
#include "string_overrides.h"

#include "lua/lua_game.h"
#include "lua/lua_game_ship_identity.h"

namespace INITGAME {

    /**
     * Initializes the ship data array. 
     * Zeroes out the entire ship array, then sets default status,
     * location, and officer values for each.
     */
    void Init_Ships_Array() {
        orion2re::lua::game::Reset_Ship_Generations();
        memset(MOX::ship, 0, MAX_SHIPS * sizeof(s_ship_data));

        for (MOX::NUM_SHIPS = 0; MOX::NUM_SHIPS < MAX_SHIPS; ++MOX::NUM_SHIPS) {
            s_ship_data& ship = MOX::ship[MOX::NUM_SHIPS];
            ship.status = 5;
            ship.location = -1;
            ship.officer_index = -1;
        }

        // Reset ship count to 0 and perform slot allocation logic
        MOX::NUM_SHIPS = 0;
        SHIPMOVE::Allocate_New_Ship_Slots();
    }

    /**
     * @brief Initializes the global colonies array.
     * Clears the entire array and sets initial sentinel values for owner, planet, and officer.
     */
    void Init_Colonies_Array() {
        memset(MOX::colony, 0, MAX_COLONIES * sizeof(s_colony));

        for (MOX::NUM_COLONIES = 0; MOX::NUM_COLONIES < MAX_COLONIES; ++MOX::NUM_COLONIES) {
            s_colony& colony = MOX::colony[MOX::NUM_COLONIES];

            colony.planet = -1;
            colony.owner = -1;
            colony.officer = -1;
        }

        MOX::NUM_COLONIES = 0;
    }

    /**
     * Sets all game settings to their default values, clears specific memory ranges,
     * generates a default network game name using the current clock, and persists the settings.
     */
    void Set_Default_Game_Settings() {
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        // Initialize general game settings
        MOX::settings.end_of_turn_summary = 1;
        MOX::settings.end_of_turn_wait = 1;
        MOX::settings.random_events = 1;
        MOX::settings.enemy_moves = 0;
        MOX::settings.expanding_help = 0;
        MOX::settings.auto_select_ships = 1;
        MOX::settings.animations_on = 1;
        MOX::settings.auto_select_colony = 0;
        MOX::settings.show_relocation_lines = 1;
        MOX::settings.show_gnn_report = 1;
        MOX::settings.auto_delete_tg_housing = 0;
        MOX::settings.auto_saves = 1;
        MOX::settings.only_show_serious_turn_summary_reports = 0;
        MOX::settings.ship_initiative = 1;
        MOX::settings.tactical_combat_speed = 0;
        MOX::settings.message_delay = 0;

        // Sound and Music
        MOX::settings.sound_fx_on = 1;
        MOX::settings.sound_fx_level = 50;
        MOX::settings.music_on = 1;
        MOX::settings.music_level = 50;

        // Galaxy and Difficulty
        MOX::settings.xenons_exist = 1;
        MOX::settings.game_difficulty = GAME_DIFFICULTY_TUTOR;
        MOX::settings.galaxy_size = GALAXY_SIZE_MEDIUM;
        MOX::settings.galaxy_age = GALAXY_AGE_AVERAGE;
        MOX::settings.strategic_combat_flag = 0;
        MOX::settings.starting_civilization_level = STARTING_CIVILIZATION_LEVEL_AVERAGE;
        MOX::settings.number_of_players = NEW_GAME_DEFAULT_PLAYER_COUNT;

        // Combat Flags
        MOX::settings.combat_speedx2_flag = 0;
        MOX::settings.combat_legal_moves_flag = 1;
        MOX::settings.combat_missile_impact_flag = 1;
        MOX::settings.combat_shields_arc_flag = 0;
        MOX::settings.combat_grid_flag = 0;

        // Save Slot and Versioning
        MOX::settings.active_save_slot = -1; // 0xFF
        MOX::settings.version = 0x82;

        MOX::settings.sound_driver_id = 2;
        MOX::settings.music_driver_id = 1;

        // Clear the first byte of each save-game description record.
        for (auto& save_game_description : MOX::save_game_description) {
            save_game_description.description[0] = '\0';
        }

        // Generate default network name: MOX-<clock % 10>
        const int32_t clock_value = (int32_t)clock();
        snprintf(MOX::settings.net_game_name, sizeof(MOX::settings.net_game_name), "MOX-%d", clock_value % 10);

        // Finalize default settings
        MULTPLAY::Set_Default_Online_Settings();
        INITGAME::Write_Game_Settings();
    }

    void Write_Game_Settings() {
        FILE* settings_file = fopen_case(GAME_SETTINGS_FILENAME, "wb");

        if (settings_file == nullptr) {
            exitm::Exit_With_Message("Unable to open ORION2RE.SET");
        }

        fwrite(&MOX::settings, sizeof(MOX::settings), 1, settings_file);
        fflush(settings_file);
        fclose(settings_file);
    }

    bool __cdecl Init_New_Game(int16_t game_difficulty, int16_t galaxy_size, int16_t galaxy_age, int16_t num_players, int16_t strategic_combat_flag, int16_t starting_civilization_level) {
        if (num_players != NEW_GAME_RANDOM_PLAYER_COUNT &&
            (num_players < 2 || num_players > MAX_PLAYERS)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "initgame: invalid player count %d (expected 2..%d or Random)",
                (int)num_players,
                (int)MAX_PLAYERS);
            return false;
        }
        AIDATA::Reset(&AIDATA::state);
        game_world::current = legacy_world::World();
        orion2re::lua::game::Reset_Semantic_State();
        const bool random_player_count =
            num_players == NEW_GAME_RANDOM_PLAYER_COUNT;
        num_players = Resolve_New_Game_Player_Count(num_players, MOX::player);
        if (random_player_count) {
            MOX::settings.number_of_players = NEW_GAME_RANDOM_PLAYER_COUNT;
        }

        bool extra_maps_generated = false;
        int16_t map_index = 0;
        int16_t generation_success;

        MOX::settings.start_of_game_seed = game_random::Get_Random_Seed();
        MOX2::Set_Game_Seed();
        MOX::stardate = 35000;
        MOX::NUM_PLAYERS = num_players;

        Init_Ships_Array();
        RUSS::Mox_Update();
        Init_Colonies_Array();
        RUSS::Mox_Update();

        // Clear antaran invasion data (66 bytes)
        memset(&MOX::antarans, 0, sizeof(MOX::antarans));
        antarans::Reset_Fleet_Runtime();
        RUSS::Mox_Update();

        if (MOX::game_type != 2) {
            MOX::PLAYER_NUM = 0;
        }

        Init_Leaders();
        RUSS::Mox_Update();
        Init_Players();
        RUSS::Mox_Update();
        BILL::Cache_Player_Scientist_Anim();
        RUSS::Mox_Update();
        MSG_::Allocate_Msg_Slots();

        MOX2::Set_Game_Seed();
        MOX::settings.start_of_game_seed = game_random::Get_Random_Seed();

        do {
            MAPGEN::Universe_Generation((uint8_t)galaxy_size, (uint8_t)galaxy_age, (int8_t)game_difficulty);
            RUSS::Mox_Update();

            generation_success = HOMEGEN::Generate_Home_Worlds();
            RUSS::Mox_Update();

            if (generation_success != 0) {
                video::Set_Page_Off();
                graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
                platform::Toggle_Pages_Stub();
                MAPGEN::Enforce_Configured_Planet_Max();
                MAPGEN::Set_Black_Hole_Visited_Flags();
                HAROLD::Confirm_Planet_And_Star_Index_Link(HAROLD::s_after_home_world_gen);

                if (MOX::settings.starting_civilization_level ==
                    STARTING_CIVILIZATION_LEVEL_ADVANCED) {
                    ADVCIV::Advanced_Civilization_Colonies();
                    ADVCIV::Assign_Advanced_Civilization_Starting_Ships();
                }
            }

            if (map_index < MOX::maps_to_generate) {
                extra_maps_generated = true;
                MOX::star_fields_dim[0] = 33;
                MOX::star_fields_dim[1] = 29;
                MOX::star_fields_dim[2] = 25;
                MOX::star_fields_dim[3] = 23;
                MOX::star_fields_dim[4] = 21;
                MOX::star_fields_dim[5] = 17;

                HAROLD::Set_All_Visited_Flags();
                MAINSCR::Update_Star_Owners();
                video::Set_Page_Off();
                MAINSCR::Draw_Mini_Main_Screen();
                RUSS::Mox_Sync_Update();

                const int16_t map_no = map_index;
                map_index++;
                RUSS::PFMT(0, 0x17, RUSS::s_Map_d, map_no);

                RUSS::PFMT(0, 0x18, RUSS::s_Seed_ld, MOX::settings.start_of_game_seed);
                capture::Screen_Flic_Capture();
            }
        } while (generation_success == 0);

        if (extra_maps_generated) {
            exitm::Exit_With_Message("done");
        }

        RUSS::Mox_Update();
        HOMEGEN::Make_System_Monsters();
        RUSS::Mox_Update();
        Make_System_Monsters_Into_Ships();
        RUSS::Mox_Update();
        MAPGEN::Generate_Wormhole_Links();
        RUSS::Mox_Update();

        if (MOX::settings.starting_civilization_level ==
            STARTING_CIVILIZATION_LEVEL_ADVANCED) {
            OFFICER::Allocate_Adv_Civ_Game_Officers();
            RUSS::Mox_Update();
        }

        MAPGEN::Assign_Marooned_Heroes();
        RUSS::Mox_Update();
        RUSS::Mox_Update();

        std::string galaxy_diagnostic;
        if (!MAPGEN::Apply_Lua_Galaxy_Generator(&galaxy_diagnostic)) {
            const std::string message =
                "Lua galaxy_generator failed: " + galaxy_diagnostic;
            exitm::Exit_With_Message(message.c_str());
        }

        GEO::Initialize_Black_Hole_Blocks(45);
        RUSS::Mox_Update();
        GEO::Initialize_Star_In_Nebula_Info();
        RUSS::Mox_Update();

        MOX2::Init_Map_Defaults();
        KEN::Ken_Init();
        HAROLD::Harold_Init();
        ERIC::Eric_Init();
        BILL::Bill_Init();
        MAINSCR::Update_Star_Owners();
        RUSS::Mox_Update();

        COLONIZE::Allocate_New_Colony_Slots();
        RUSS::Mox_Update();
        COLCALC::Twiddle_Initial_Homeworlds();
        RUSS::Mox_Update();
        SHIPMOVE::Allocate_New_Ship_Slots();
        RUSS::Mox_Update();
        COLCALC::Compute_All_Star_Colony_Stuff();
        RUSS::Mox_Update();
        TRADE::Process_Trade_And_Research_Agreements();
        RUSS::Mox_Update();

        if (MOX::settings.auto_select_colony != 0) {
            Set_Initial_Colony_Auto_Selects();
        }
        RUSS::Mox_Update();
        REPORT::Initialize_Reports();
        RUSS::Mox_Update();
        COLCALC::Init_Surrenders();
        RUSS::Mox_Update();
        RUSS::Mox_Update();

        FILEDEF::Save_Game_Settings();
        RUSS::Mox_Update();
        MOX2::Init_Map_Pictures_Default_();

        if (MOX::game_type == 0) {
            FILEDEF::Save_Moo_Custom_Player_Data(MOX::player);
        }

        if (MOX::settings.starting_civilization_level ==
            STARTING_CIVILIZATION_LEVEL_POST_WARP) {
            TECHINIT::Grant_Postwarp_Technologies();
        }

        bool finalization_succeeded = false;
        if (MAPGEN::Finalize_Configured_New_Game(&finalization_succeeded)) {
            COLCALC::Compute_All_Star_Colony_Stuff();
            COLCALC::All_Colony_Calculations();
        }
        if (!finalization_succeeded) return false;
        FILEDEF::Save_Game(9);
        RUSS::Mox_Update();

        game_random::Set_Random_Seed(MOX::settings.start_of_game_seed);
        return true;
    }

    void __cdecl Init_Colony(int16_t colony_idx, int16_t planet_idx, uint8_t player_num) {
        s_colony* colony = &MOX::colony[colony_idx];

        // Zero-initialize the entire colony structure
        memset(colony, 0, sizeof(s_colony));

        s_planet_data* planet = &MOX::planet[planet_idx];
        planet->colony_index = colony_idx;

        colony->planet = planet_idx;
        colony->owner = player_num;
        colony->officer = -1;

        int16_t star_idx = planet->star_index;
        // Mark the star system as visited/seen by the player
        uint8_t visited_player_mask = (uint8_t)(1u << (player_num & 0x1f));
        MOX::star[star_idx].visited |= visited_player_mask;

        for (int32_t producing_idx = 0; producing_idx < (int32_t)(sizeof(colony->producing) / sizeof(colony->producing[0])); ++producing_idx) {
            colony->producing[producing_idx] = COLONY_PRODUCTION_NONE;
        }

        colony->occupation_policy = 4;
        colony->just_produced = COLONY_PRODUCTION_NONE;
    }

    void __cdecl Init_Leaders() {
        char lbx_filename[20];
        HAROLD::Get_Officer_Data_LBX_Name(lbx_filename);

        void* leader_records = farload::Far_Reload_Data(lbx_filename, 0, MOX::screen_seg, 0, MAX_LEADERS, sizeof(s_leader_data));

        memcpy(MOX::leaders, leader_records, MAX_LEADERS * sizeof(s_leader_data));

        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            // Skill masks are misaligned in the packed leader record, so
            // generate them in locals and store them afterward.
            uint32_t special_skills;
            uint32_t general_skills;
            if (i == OFFICER::progression.special_leaders.random_skills_colony_leader_id) {
                // Special handling for leader at index 41
                OFFICER::Get_Random_Skills(&special_skills, &general_skills, 2, 5, 1);
                MOX::leaders[i].special_skills = special_skills;
                MOX::leaders[i].general_skills = general_skills;
            } else if (i == OFFICER::progression.special_leaders.random_skills_ship_officer_id) {
                // Special handling for leader at index 52
                OFFICER::Get_Random_Skills(&special_skills, &general_skills, 1, 5, 1);
                MOX::leaders[i].special_skills = special_skills;
                MOX::leaders[i].general_skills = general_skills;
            } else if (i == 65) {
                // Special randomized tech application for leader at index 65
                MOX::leaders[i].tech_application[0] = (uint8_t)game_random::Random(101);
                MOX::leaders[i].tech_application[1] = (uint8_t)game_random::Random(102) + 101;
            }

            // Calculate and set the skill value for this leader.
            MOX::leaders[i].skill_value = OFFICER::Officer_Skill_Value(i);

            // Refresh systems after each leader update.
            RUSS::Mox_Update();
        }
    }

    void Set_Initial_Colony_Auto_Selects() {
        if (MOX::NUM_STARS <= 0) {
            return;
        }

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            // Initialize selection history to none (-1) for all planets in this system
            for (int planet_idx = 0; planet_idx < 8; ++planet_idx) {
                MOX::star[star_idx].last_planet_selected[planet_idx] = -1;
            }

            // Determine and set the default auto-selection logic for this star
            int16_t selected_planet_idx;
            HACCESS::Set_Colony_Auto_Select_For_Star(star_idx, MOX::PLAYER_NUM, &selected_planet_idx);
        }
    }

    void __cdecl Make_System_Monsters_Into_Ships() {
        int16_t total_stars = MOX::NUM_STARS;
        s_star_data* star_ptr = MOX::star;
        s_star_data* end_ptr = &MOX::star[total_stars];

        // Process standard Space Monsters
        while (star_ptr < end_ptr) {
            if (star_ptr->system_special == SYSTEM_SPECIAL_SPACE_MONSTER) {
                int32_t ship_type = game_random::Random(5) + 9;
                int16_t star_idx = (int16_t)(star_ptr - MOX::star);
                int16_t planet_idx = HOMEGEN::Random_Planet(star_idx);

                // Initialize monster ship owner to Neutral/Orion
                MOX::ship[MOX::NUM_SHIPS].owner = 8;
                SHIPS::Build_Nonplayer_Ship(ship_type, star_idx, 0);

                // Restore previous special status from backup field and clear backup
                star_ptr->system_special = star_ptr->twinkling1;
                star_ptr->twinkling1 = 0;

                HOMEGEN::Upgrade_Planet_Guarded_By_Monster(&MOX::planet[planet_idx], (int16_t)ship_type);
            }
            star_ptr++;
        }

        // Process Orion Star Guardian (Limit to one)
        bool orion_spawned = false;
        for (int16_t i = 0; i < total_stars; ++i) {
            if (orion_spawned) break;

            if (MOX::star[i].system_special == SYSTEM_SPECIAL_ORION_STAR) {
                orion_spawned = true;
                MOX::ship[MOX::NUM_SHIPS].owner = 8;
                SHIPS::Build_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_GUARDIAN, i, 0);
            }
        }
    }

} // namespace INITGAME