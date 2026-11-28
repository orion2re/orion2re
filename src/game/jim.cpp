#include "pch.h"
#include "string_overrides.h"

namespace JIM {

    int16_t jim_current_stream = -1;
    int16_t jim_old_music_id = -1;
    int16_t jim_fade_music_flag = -1;
    int16_t jim_old_transition_id = -1;
    s_mouse_list logo_mouse_list = {0, 0, 0, 0, 639, 479};
    int16_t darken_palette_value;

    bool __cdecl Check_For_Simtex_Logo() {
        char resolved_path[80];

        return vfs::Resolve_Read_Path_No_Zip("simtex.lbx", resolved_path, sizeof(resolved_path)) != 0;
    }

    void __cdecl Draw_Logos(void) {
        fields::Deactivate_Auto_Function();
        mouse::Set_Mouse_List(&logo_mouse_list, 1);
        darken_palette_value = 0;
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);

        allocate::Push_Block(MOX::global_screen_seg);
        int32_t remaining_space = allocate::Get_Remaining_Block_Space(MOX::global_screen_seg);
        void* ani_buffer = allocate::Allocate_Next_Block(MOX::global_screen_seg, remaining_space - 100);
        file_ani::Set_File_Animation_Buffer(ani_buffer);

        int16_t skip_requested = 0;
        bool user_interrupted = false;

        if (Check_For_Simtex_Logo() == 0) {
            file_ani::Open_File_Animation("logo.LBX", 0);
            video::Set_Page_Off();
            palstore::Darken_Palette(100);
            mouse::Set_Mouse_List(&logo_mouse_list, 1);
            palette::Clear_Palette_Changes(0, 0xFF);
            platform::Toggle_Pages_Stub();

            int16_t total_frames = file_ani::Get_File_Animation_Total_Frames();
            file_ani::Draw_File_Animation();
            video::Copy_Off_To_Back();
            RUSS::Mox_Update();
            platform::Toggle_Pages_Stub();
            darken_palette_value = 0;

            skip_requested = palstore::Slow_Fade_In();

            int16_t end_delay_counter = 15;
            mouse::Set_Mouse_List(&logo_mouse_list, 1);

            while (!user_interrupted) {
                if (skip_requested != 0) break;

                timer::Mark_Time();
                if (key::Keyboard_Status() == 1) {
                    key::Read_Key();
                    skip_requested = 1;
                    user_interrupted = true;
                }

                if (mouse::Mouse_Button() != 0) {
                    skip_requested = 1;
                    user_interrupted = true;
                }

                if (user_interrupted) break;

                video::Set_Page_Off();
                if (file_ani::Get_File_Animation_Frame() == total_frames) {
                    if (--end_delay_counter < 1) {
                        user_interrupted = true;
                    }
                } else {
                    file_ani::Draw_File_Animation();
                }

                platform::Toggle_Pages_Stub();
                RUSS::Mox_Update();
                timer::Release_Time(1);
            }
        } else {
            // Play Simtex cinematic if Simtex logo is present
            SMACK::Play_Cinematic("simtex.lbx", 0, 0);
            sound::Stop_Stream();
            jim_current_stream = -1;
        }

        // Second Logo (Master of Orion / Microprose)
        if (skip_requested == 0) {
            Logo_Fadeout();
            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);

            file_ani::Open_File_Animation("logo.LBX", 1);
            int16_t total_frames = file_ani::Get_File_Animation_Total_Frames();
            file_ani::Draw_File_Animation();
            video::Copy_Off_To_Back();
            RUSS::Mox_Update();

            int16_t end_delay_counter = 5;
            platform::Toggle_Pages_Stub();

            user_interrupted = false;
            ERIC::Fast_Fade_In();
            mouse::Set_Mouse_List(&logo_mouse_list, 1);

            while (!user_interrupted) {
                timer::Mark_Time();
                if (key::Keyboard_Status() == 1) {
                    key::Read_Key();
                    skip_requested = 1;
                    user_interrupted = true;
                }

                if (mouse::Mouse_Button() != 0) {
                    skip_requested = 1;
                    user_interrupted = true;
                }

                if (user_interrupted) break;

                video::Set_Page_Off();
                if (file_ani::Get_File_Animation_Frame() == total_frames) {
                    if (--end_delay_counter < 1) {
                        user_interrupted = true;
                    }
                } else {
                    file_ani::Draw_File_Animation_Stencil();
                }

                platform::Toggle_Pages_Stub();
                RUSS::Mox_Update();
                timer::Release_Time(1);
            }
        }

        allocate::Pop_Block(MOX::global_screen_seg);
        file_ani::Reset_File_Animation_Buffer();
        Logo_Fadeout();
        RUSS::Mox_Update();

        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
        video::Copy_Off_To_Back();

        if (skip_requested == 0) {
            SMACK::Play_Cinematic("intro.lbx", 0, 1);
        }
    }

    void __cdecl Logo_Fadeout() {
        video::Set_Refresh_Full();

        for (int16_t current_fade = darken_palette_value; current_fade <= 100; current_fade += 10) {
            palstore::Darken_Palette((int32_t)current_fade);
            video::Submit_Palette();
            RUSS::Mox_Update();
        }

        video::Set_Page_Back();
        graphics::Fill(0, 0, 639, 479, 0);
        video::Copy_Back_To_Off();
        palette::Clear_Palette_Changes(0, 255);
        RUSS::Mox_Sync_Update();
        video::Set_Refresh_Stencil();
    }

    void Play_Background_Music() {
        if (MOX::settings.music_on != 1) {
            return;
        }

        jim_old_music_id = -1;

        if (jim_current_stream == 1 || jim_current_stream == 2 || jim_current_stream == 3) {
            return;
        }

        sound::Fade_Stream_Down(25);

        if (sound::Get_Stream_Volume() > 0) {
            int16_t current_volume;
            do {
                current_volume = (int16_t)sound::Get_Stream_Volume();
                RUSS::Mox_Update();
            } while (current_volume > 0);
        }

        sound::Stop_Stream();

        jim_current_stream = (int16_t)((clock() % 3) + 1);

        sound::Set_Stream_Filename("STREAM.LBX");
        sound::Play_Stream(jim_current_stream);
        sound::Loop_Stream(-1);

        jim_old_music_id = -1;
        sound::Set_Stream_Volume(0);
        sound::Fade_Stream_Up(30);
    }

    void __cdecl Decode_Text_Stat(char* buffer, int16_t numeric_value, int16_t max_len) {
        Decode_Text_Message(buffer, game::NO_PLAYER, -1, -1, numeric_value, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Year(char* buffer, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, game::NO_PLAYER, -1, -1, -1, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Race(char* buffer, game::id_player player, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, player, -1, -1, -1, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Name_And_Stat(char* buffer, game::id_player player, int16_t numeric_value, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, player, -1, -1, numeric_value, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Name_Tech_And_Other_Tech(char* buffer, game::id_player first, game::id_player second, int16_t first_tech_idx, int16_t second_tech_idx, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, first, -1, first_tech_idx, -1, second, second_tech_idx, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Name_And_System(char* buffer, game::id_player player, int16_t system_idx, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, player, system_idx, -1, -1, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_System(char* buffer, int16_t system_idx, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, game::NO_PLAYER, system_idx, -1, -1, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Name_And_Other_Tech(char* buffer, game::id_player player, game::id_player other, int16_t tech_idx, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, player, -1, -1, -1, other, tech_idx, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Name_And_Tech(char* buffer, game::id_player player, int16_t tech_idx, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, player, -1, tech_idx, -1, game::NO_PLAYER, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Name_And_Name(char* buffer, game::id_player first, game::id_player second, int16_t max_len) {
        Decode_Text_Message(buffer, first, -1, -1, -1, second, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Tech_And_Other_Name(char* buffer, game::id_player player, int16_t tech_id, game::id_player other, int16_t max_len) {
        JIM::Decode_Text_Message(buffer, player, -1, tech_id, -1, other, -1, TECH_FIELD_INVALID, max_len);
    }

    void __cdecl Decode_Text_Field(char* buffer, int16_t field_id, int16_t max_len) {
        Decode_Text_Message(buffer, game::NO_PLAYER, -1, -1, -1, game::NO_PLAYER, -1, field_id, max_len);
    }

    void __cdecl Decode_Text_Message(char* message_buffer, game::id_player first, int16_t star_id, int16_t tech_app1_id, int16_t number, game::id_player second, int16_t tech_app2_id, int16_t tech_field_id, int16_t max_len, game::id_planet planet) {
        int16_t dest_len = 0;
        char temp_buf[40];
        char dest_buf[200];

        for (int32_t i = 0; i < (int32_t)sizeof(dest_buf); ++i) {
            char current_char = message_buffer[i];
            if (current_char == '\0') {
                break;
            }

            const char* src_ptr = "";
            if ((uint8_t)current_char >= 0x80 && (uint8_t)current_char <= 0x8A) {
                dest_buf[dest_len] = '\0';

                switch ((uint8_t)current_char) {
                    case 0x80:
                        if (first.id < MAX_PLAYERS) src_ptr = game::current_state.players[first.id].name;
                        break;
                    case 0x81:
                        src_ptr = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_id)).name;
                        break;
                    case 0x82:
                        snprintf(temp_buf, sizeof(temp_buf), "%i", number);
                        src_ptr = temp_buf;
                        break;
                    case 0x83:
                        strlcpy(temp_buf, TECH::Technology_Applications_Name(tech_app1_id, first), sizeof(temp_buf));
                        src_ptr = temp_buf;
                        break;
                    case 0x84:
                        if (second.id < MAX_PLAYERS) src_ptr = game::current_state.players[second.id].name;
                        break;
                    case 0x85:
                        strlcpy(temp_buf, TECH::Technology_Applications_Name(tech_app2_id, second), sizeof(temp_buf));
                        src_ptr = temp_buf;
                        break;
                    case 0x86:
                        if (first.id < MAX_PLAYERS) src_ptr = game::current_state.players[first.id].race_name;
                        break;
                    case 0x87:
                        if (second.id < MAX_PLAYERS) src_ptr = game::current_state.players[second.id].race_name;
                        break;
                    case 0x88:
                        if (MOX::settings.language == 0) {
                            snprintf(temp_buf, sizeof(temp_buf), "%d.%d", MOX::stardate / 10, MOX::stardate % 10);
                        } else {
                            snprintf(temp_buf, sizeof(temp_buf), "%d,%d", MOX::stardate / 10, MOX::stardate % 10);
                        }
                        src_ptr = temp_buf;
                        break;
                    case 0x89:
                        strlcpy(temp_buf, TECH::Technology_Fields_Name(tech_field_id), sizeof(temp_buf));
                        src_ptr = temp_buf;
                        break;
                    case 0x8A:
                        strlcpy(temp_buf, HACCESS::Get_Planet_Name(game::current_state, planet), sizeof(temp_buf));
                        src_ptr = temp_buf;
                        break;
                }

                strlcpy(&dest_buf[dest_len], src_ptr, sizeof(dest_buf) - (size_t)dest_len);
                dest_len = (int16_t)strlen(dest_buf);
            } else {
                dest_buf[dest_len++] = current_char;
            }
        }

        dest_buf[dest_len] = '\0';

        if (strlen(dest_buf) >= (uint32_t)(max_len - 1)) {
            snprintf(dest_buf, sizeof(dest_buf), "Decode message too long (%d).\n%s", max_len, message_buffer);
            exitm::Exit_With_Message(dest_buf);
        } else {
            strlcpy(message_buffer, dest_buf, max_len);
        }
    }

    void __cdecl Get_Text_Message(const char* filename, int16_t message_index, char* dest, int16_t max_len) {
        char buffer[100];

        uint16_t lang = (uint8_t)MOX::settings.language;
        if (lang > 5) {
            lang = 0;
        }

        RUSS::Mox_Update();

        string_overrides::Load_Message(filename, (int16_t)(message_index * 6 + lang), 0,
            sizeof(buffer), size_t(message_index) + 1, buffer, sizeof(buffer));

        RUSS::Mox_Update();

        int16_t message_len = (int16_t)strlen(buffer);

        if (max_len - 1 <= message_len || message_len >= (int16_t)(sizeof(buffer) - 1)) {
            snprintf(buffer, sizeof(buffer), "Message #%d in %s too long (%d).", message_index, filename, message_len);
            exitm::Exit_With_Message(buffer);
            return;
        }

        strlcpy(dest, buffer, max_len);
    }

    void __cdecl Bill_Picture_To_Mouse(s_animation_header* anim) {
        if (animate::Get_Width(anim) <= 24) {
            animate::Get_Height(anim);
        }

        bitmap::Draw_Picture_To_Bitmap(COLONY::mouse_bitm, anim);
        bitmap::Clear_Mouse_Image(0);
        bitmap::Draw_Bitmap_To_Mouse(0, 0, COLONY::mouse_bitm, 0);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }

    void __cdecl Play_Combat_Music() {
        if (MOX::settings.music_on == 1) {
            jim_old_music_id = -1;
            sound::Fade_Stream_Down(35);

            if (sound::Get_Stream_Volume() > 0) {
                int16_t current_volume;
                do {
                    current_volume = (int16_t)sound::Get_Stream_Volume();
                    RUSS::Mox_Update();
                } while (current_volume > 0);
            }

            sound::Stop_Stream();

            int32_t ticks = clock();
            jim_current_stream = (int16_t)((ticks % 3) + 4);

            sound::Set_Stream_Filename("STREAM.LBX");
            sound::Play_Stream(jim_current_stream);
            sound::Loop_Stream(-1);
            sound::Set_Stream_Volume(0);
            sound::Fade_Stream_Up(30);
        }
    }

    void __cdecl Stop_Background_Music() {
        jim_old_music_id = -1;
        jim_current_stream = -1;
        sound::Set_Stream_Volume(0);
        sound::Stop_Stream();
    }

    void __cdecl Play_Streaming_Music(int16_t music_id, int16_t transition_id, int16_t loop_flag) {
        if (MOX::settings.music_on != 1) {
            return;
        }

        if (music_id != jim_old_music_id) {
            jim_old_music_id = music_id;
            jim_old_transition_id = transition_id;
            jim_fade_music_flag = 1;
        }

        if (jim_fade_music_flag == 1) {
            sound::Fade_Stream_Down(30);
            jim_fade_music_flag = 2;
            return;
        }

        if (jim_fade_music_flag == 2) {
            if (sound::Get_Stream_Volume() != 0) {
                return;
            }

            sound::Stop_Stream();

            if (jim_old_music_id <= 100) {
                sound::Set_Stream_Filename("STREAM.LBX");
                sound::Play_Stream(jim_old_music_id);
            } else {
                sound::Set_Stream_Filename("STREAMHD.LBX");
                sound::Play_Stream(jim_old_music_id - 100);
            }

            jim_current_stream = jim_old_music_id;

            if (jim_old_transition_id == -1 && loop_flag == 1) {
                sound::Loop_Stream(-1);
            }

            sound::Set_Stream_Volume(0);
            sound::Fade_Stream_Up(30);

            if (jim_old_transition_id == -1) {
                jim_fade_music_flag = 4;
            } else {
                jim_fade_music_flag = 3;
            }
        } else if (jim_fade_music_flag == 3) {
            if (sound::Is_Stream_Playing() != 0) {
                return;
            }

            sound::Stop_Stream();

            if (jim_old_transition_id == -2) {
                int32_t ticks = (int32_t)clock();
                jim_current_stream = (int16_t)((ticks % 3) + 1);
                sound::Set_Stream_Filename("STREAM.LBX");
                sound::Play_Stream(jim_current_stream);
            } else {
                if (jim_old_transition_id <= 100) {
                    sound::Set_Stream_Filename("STREAM.LBX");
                    sound::Play_Stream(jim_old_transition_id);
                } else {
                    sound::Set_Stream_Filename("STREAMHD.LBX");
                    sound::Play_Stream(jim_old_transition_id - 100);
                }
                jim_current_stream = jim_old_transition_id;
            }

            if (loop_flag == 1) {
                sound::Loop_Stream(-1);
            }

            sound::Set_Stream_Volume(0);
            sound::Fade_Stream_Up(30);
            jim_fade_music_flag = 4;
        }
    }
} // namespace JIM
