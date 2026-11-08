#include "pch.h"

#include "ai_legacy.h"

#include "game/savegame.h"
#include "game/config_refresh.h"

namespace FILEDEF {
    const char s_Auto_Save_00556938[12] = "(Auto Save)";

    void __cdecl Load_Game_Settings() {
        RUSS::Assert_Settings();

        FILE* file = fopen_case(GAME_SETTINGS_FILENAME, "rb");
        if (file != nullptr) {
            s_settings loaded_settings;
            const bool loaded = fread(&loaded_settings, sizeof(loaded_settings), 1, file) == 1;
            fclose(file);
            if (loaded) {
                loaded_settings.number_of_players = (int8_t)INITGAME::Normalize_New_Game_Player_Count(
                    loaded_settings.number_of_players, NEW_GAME_DEFAULT_PLAYER_COUNT);
                MOX::settings = loaded_settings;
                RUSS::Assert_Settings();
            }
        }

        HAROLD::Set_Sound_Volume_To_Settings();
    }

    bool Save_Game_Settings(void) {
        RUSS::Assert_Settings();

        FILE* file = fopen_case(GAME_SETTINGS_FILENAME, "wb");
        if (file == nullptr) {
            return false;
        }

        fwrite(&MOX::settings, sizeof(MOX::settings), 1, file);
        fclose(file);

        RUSS::Assert_Settings();

        return true;
    }

    void __cdecl Save_Game(int16_t slot_index) {
        uint32_t random_seed = game_random::Get_Random_Seed();

        HAROLD::Save_Mouse_List();

        char slot_str[20];
        snprintf(slot_str, sizeof(slot_str), "%i", slot_index + 1);

        char filename[260];
        strlcpy(filename, "SAVE", sizeof(filename));
        strlcat(filename, slot_str, sizeof(filename));
        strlcat(filename, ".GAM", sizeof(filename));

        RUSS::Assert_Settings();

        uint8_t* save_buffer = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
        if (save_buffer == nullptr) {
            char error_message[] = "Error saving game.\n";
            GENDRAW::Message_Box(error_message);
            HAROLD::Restore_Mouse_List();
            return;
        }

        int16_t description_slot_index = (slot_index > 8) ? 9 : slot_index;
        SERIAL::Writer_ writer(save_buffer, SAVEGAME::BUFFER_CAPACITY, "Save game buffer overflow");
        std::string save_error;
        if (!SAVEGAME::Write_Game(&writer,
                MOX::save_game_description[description_slot_index].description,
                random_seed,
                &save_error)) {
            GENDRAW::Warning_Box(save_error.c_str());
            free(save_buffer);
            HAROLD::Restore_Mouse_List();
            return;
        }

        FILE* fp = fopen_case(filename, "wb");

        if (fp == nullptr) {
            char error_message[] = "Error saving game.\n";
            GENDRAW::Message_Box(error_message);
            free(save_buffer);
            HAROLD::Restore_Mouse_List();
            return;
        }

        MISC::Increment_Mouse_Clock_Image();
        fwrite(save_buffer, writer.Length(), 1, fp);
        fclose(fp);
        free(save_buffer);

        HAROLD::Restore_Mouse_List();
        RUSS::Assert_Settings();
        FILEDEF::Save_Game_Settings();
    }

    uint32_t __cdecl Load_Game(int16_t slot_index) {
        char filename[256];
        char error_message[128] = {};
        uint32_t random_seed;
        s_settings saved_settings;

        HAROLD::Save_Mouse_List();

        snprintf(filename, sizeof(filename), "SAVE%d.GAM", slot_index + 1);

        RUSS::Mox_Update();
        RUSS::Assert_Settings();

        FILE* file = fopen_case(filename, "rb");
        if (file == nullptr) {
            RUSS::PFMT(0, 0, "File read error");
            key::Wait_For_Key();
            return 0;
        }

        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return 0;
        }
        long file_size_long = ftell(file);
        if (file_size_long <= 0 || file_size_long > static_cast<long>(SAVEGAME::BUFFER_CAPACITY) ||
            fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            return 0;
        }

        uint32_t file_size = static_cast<uint32_t>(file_size_long);
        uint8_t* save_buffer = static_cast<uint8_t*>(malloc(file_size));
        if (save_buffer == nullptr || fread(save_buffer, file_size, 1, file) != 1) {
            free(save_buffer);
            fclose(file);
            return 0;
        }
        fclose(file);

        SERIAL::Reader_ reader(save_buffer, file_size, "Invalid or truncated save game");
        SAVEGAME::s_metadata metadata = {};
        std::string load_error;
        if (!SAVEGAME::Read_Game(
                &reader, &metadata, &saved_settings, &random_seed, &load_error)) {
            if (!load_error.empty()) {
                GENDRAW::Warning_Box(load_error.c_str());
            } else {
                const char* fmt = nullptr;
                switch (MOX::settings.language) {
                    case 0: fmt = "SAVE%d.GAM is invalid! Try another save slot or start a NEW game"; break;
                    case 1: fmt = "SAVE%d.GAM ist ung\x9Fltig! W\x8Ahlen Sie ein anderes Spiel, oder starten Sie ein NEUES Spiel."; break;
                    case 2: fmt = "SAUVEGARDER%d.GAM n'est pas valide! Essayez un autre emplacement de sauvegarde ou commencez une NOUVELLE partie"; break;
                    case 3: fmt = "#SAVE%d.GAM no es v<lido! Prueba otro hueco para guardar or comienza una partida NUEVA"; break;
                    case 4: fmt = "Il file SAVE%d.GAM non > valido! Provate a caricarne un altro o iniziate una nuova partita"; break;
                }
                if (fmt) {
                    snprintf(error_message, sizeof(error_message), fmt, slot_index + 1);
                }
                GENDRAW::Warning_Box(error_message);
            }
            video::Set_Page_Off();
            graphics::Fill(0, 0, 639, 479, 0);
            free(save_buffer);
            return 0;
        }
        free(save_buffer);

        AIDATA::Reset(&AIDATA::state);
        game_world::current = AI_LEGACY::World();
        MISC::Increment_Mouse_Clock_Image();

        int16_t description_slot_index = (slot_index >= 9) ? 9 : slot_index;
        memcpy(MOX::save_game_description[description_slot_index].description, metadata.description, sizeof(metadata.description));
        MOX::stardate = metadata.stardate;
        MOX::game_type = metadata.game_type;

        if (MOX::nebula_screen_seg == 0) {
            MOX::NUM_NEBULAS = 0;
        }

        MOX2::Init_Map_Defaults();
        game_random::Set_Random_Seed(random_seed);
        RUSS::Mox_Update();
        BILL::Cache_Player_Scientist_Anim();
        RUSS::Mox_Update();

        if (MOX::NUM_NEBULAS != 0) {
            MAPGEN::Load_Nebula_Pictures();
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
        if (MOVEBOX::Moveable_Box_Selected(2)) {
            FLEETPOP::Reload_Fleet_Box_Data();
        }

        COLCALC::No_Op1();
        MOX::Apply_Save_Settings(&saved_settings);
        HAROLD::Confirm_Planet_And_Star_Index_Link("loading a game");
        RUSS::Assert_Settings();
        MAINSCR::Update_Star_Owners();
        RUSS::Assert_Settings();
        HAROLD::Load_Leader_Titles();

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (MOX::player[i].dead_field != 0) {
                MOX::player[i].research_accumulated = (uint16_t)MOX::player[i].dead_field;
                MOX::player[i].dead_field = 0;
            }
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            for (int j = 0; j < MOX::NUM_PLAYERS; ++j) {
                MOX::player[i].delayed_diplomacy_orders[j] = -1;
            }
        }

        HAROLD::Restore_Mouse_List();
        RUSS::Release_All_Blocks(MOX::screen_seg);
        CONFIG_REFRESH::Request_Resource_Reload();

        return 1;
    }

    void __cdecl Save_Moo_Custom_Player_Data(s_player* player) {
    }

    bool __cdecl Read_Moo_Custom_Player_Data(s_player* player) {
        return false;
    }

    void __cdecl Get_Saved_Game_Descriptions(int32_t* save_times) {
        for (int16_t i = 0; i < 10; ++i) {
            char num_str[8];
            char filename[16];

            snprintf(num_str, sizeof(num_str), "%i", i + 1);

            strlcpy(filename, "SAVE", sizeof(filename));
            strlcat(filename, num_str, sizeof(filename));
            strlcat(filename, ".GAM", sizeof(filename));

            if (File_Exists(filename)) {
                FILE* fp = fopen_case(filename, "rb");
                if (fp != nullptr) {
                    int32_t type = -1;
                    char description[sizeof(MOX::save_game_description[i].description)] = {};
                    int32_t save_time = -1;
                    const bool loaded =
                        fread(&type, sizeof(type), 1, fp) == 1 &&
                        fread(description, sizeof(description), 1, fp) == 1 &&
                        fread(&save_time, sizeof(save_time), 1, fp) == 1;
                    fclose(fp);

                    if (loaded && LOADSAVE::Is_Valid_Game_Type(type)) {
                        description[sizeof(description) - 1] = '\0';
                        strlcpy(MOX::save_game_description[i].description, description, sizeof(MOX::save_game_description[i].description));
                        save_times[i] = save_time;
                        if (i == 9) {
                            strlcpy(MOX::save_game_description[i].description, "(Auto Save)", sizeof(MOX::save_game_description[i].description));
                        } else if (MOX::save_game_description[i].description[0] == '\0') {
                            strlcpy(MOX::save_game_description[i].description, "<< no description >>", sizeof(MOX::save_game_description[i].description));
                        }
                    } else {
                        save_times[i] = -1;
                        strlcpy(MOX::save_game_description[i].description, HAROLD::H_Message(0x184), sizeof(MOX::save_game_description[i].description));
                    }
                } else {
                    strlcpy(MOX::save_game_description[i].description, HAROLD::H_Message(0x184), sizeof(MOX::save_game_description[i].description));
                    save_times[i] = -1;
                }
            } else {
                strlcpy(MOX::save_game_description[i].description, HAROLD::H_Message(0x184), sizeof(MOX::save_game_description[i].description));
                save_times[i] = -1;
            }
        }
    }

    int8_t __cdecl Get_Game_Type_From_Saved_Game(int16_t save_slot) {
        int8_t game_type = -1;
        char filename[16];

        snprintf(filename, sizeof(filename), "SAVE%d.GAM", save_slot + 1);
        FILE* file = fopen_case(filename, "rb");
        if (file != nullptr) {
            if (fseek(file, 0x2D, SEEK_SET) == 0) {
                if (fread(&game_type, sizeof(game_type), 1, file) != 1) {
                    game_type = -1;
                }
            }
            fclose(file);
        }
        return game_type;
    }

    void Update_Game_Settings_Version_Number() {
        bool needs_reset = false;
        FILE* file = fopen_case(GAME_SETTINGS_FILENAME, "rb");

        if (file != nullptr) {
            fclose(file);

            Load_Game_Settings();

            // Set version to 0x82 (130)
            MOX::settings.version = 0x82;

            // If saving fails, we reset settings
            if (!Save_Game_Settings()) {
                needs_reset = true;
            }
        } else {
            // If settings file doesn't exist, we must reset
            needs_reset = true;
        }

        if (needs_reset) {
            INITGAME::Set_Default_Game_Settings();
        }
    }
} // namespace FILEDEF