#include "pch.h"

namespace SCIENCE {
    uint8_t finds_presented_by_scientist;

    const char* spy_anim_fmt = "SR_R%x_SP.LBX";
    const char* trooper_anim_fmt = "SR_R%x_TR.LBX";
    const char* spy_anim_find_pat = "SR_R?_SP.LBX";
    const char* trooper_anim_find_pat = "SR_R?_TR.LBX";

    s_colors science_high_color = {{240, 246, 243, 244, 244, 245, 246, 247}};
    s_colors science_normal_color = {{242, 245, 244, 244, 244, 244, 245, 246}};

    int16_t bm_scale_x[5] = {6, 21, 37, 60, 85};
    int16_t bm_scale_y[5] = {6, 20, 37, 58, 85};
    int16_t bm_scaled_x1[5] = {373, 349, 320, 283, 239};
    int16_t bm_scaled_y1[5] = {203, 179, 149, 113, 69};

    void* science_mouse_bitmap;

    void __cdecl Show_Off_Captured_Tech(int16_t* captured_tech_ids, uint16_t tech_count, int16_t message_arg) {
        char text_buffer[0x100];

        allocate::Push_Block(MOX::screen_seg);

        void* animation_buffer = allocate::Allocate_First_Block(MOX::screen_seg, 0x7C830);
        file_ani::Set_File_Animation_Buffer(animation_buffer);

        JIM::Get_Text_Message("billtext.lbx", 3, text_buffer, sizeof(text_buffer));
        JIM::Decode_Text_System(text_buffer, message_arg, sizeof(text_buffer));

        char* trooper_anim = BILL::Cache_Player_Trooper_Anim();
        SCIENCE::Science_Room(trooper_anim, tech_count, captured_tech_ids, text_buffer);

        file_ani::Reset_File_Animation_Buffer();
        allocate::Pop_Block(MOX::screen_seg);
        COLONY::Reset_Mouse_Picture();
        JIM::Play_Background_Music();
    }

    void __cdecl Show_Off_Artifact_Tech(uint16_t tech_idx, ::game::id_planet planet_idx) {
        int16_t discovered_tech = (int16_t)tech_idx;

        if (finds_presented_by_scientist == 0) {
            Show_Off_Captured_Tech(&discovered_tech, 1, planet_idx);
            return;
        }

        char message_text[0x100];
        char scientist_animation[64];

        allocate::Push_Block(MOX::screen_seg);
        void* animation_block = allocate::Allocate_First_Block(MOX::screen_seg, 0x7C830);
        file_ani::Set_File_Animation_Buffer(animation_block);
        JIM::Get_Text_Message("billtext.lbx", 0, message_text, sizeof(message_text));
        JIM::Decode_Text_Name_And_Tech(message_text, MOX::PLAYER_NUM, tech_idx, sizeof(message_text));
        snprintf(
            scientist_animation,
            sizeof(scientist_animation),
            "SR_R%x_SC.LBX",
            (unsigned int)(uint8_t)MOX::player[MOX::PLAYER_NUM].race);
        Science_Room(scientist_animation, 1, &discovered_tech, message_text);
        file_ani::Reset_File_Animation_Buffer();
        allocate::Pop_Block(MOX::screen_seg);
        COLONY::Reset_Mouse_Picture();
        JIM::Play_Background_Music();
    }

    void __cdecl Update_Science_Anims(int32_t unused_arg, uint8_t draw_multi_anim, uint8_t draw_anim_2, uint8_t draw_anim_1) {
        (void)unused_arg;

        if (MOX::settings.animations_on != 0) {
            file_ani::Draw_Multi_File_Animation_Stencil(1);
        } else {
            file_ani::Set_File_Animation_Frame(18);
        }

        if (draw_anim_2 == 1) {
            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 2, MOX::global_cache_seg);
            animate::Draw(0, 0, anim);
        }

        if (draw_multi_anim != 0) {
            file_ani::Draw_Multi_File_Animation(0);
        }

        if (draw_anim_1 == 1) {
            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 1, MOX::global_cache_seg);
            animate::Draw(0, 0, anim);
        }
    }

    void __cdecl Show_Off_Stolen_Tech(uint16_t tech_idx, int16_t other_player_idx) {
        char message_text[0x100];
        int16_t discovered_tech = (int16_t)tech_idx;
        void* animation_block;
        char* spy_anim;

        allocate::Push_Block(MOX::screen_seg);
        animation_block = allocate::Allocate_First_Block(MOX::screen_seg, 0x7C830);
        file_ani::Set_File_Animation_Buffer(animation_block);
        JIM::Get_Text_Message("billtext.lbx", 2, message_text, sizeof(message_text));
        JIM::Decode_Text_Tech_And_Other_Name(message_text, MOX::PLAYER_NUM, tech_idx, other_player_idx, sizeof(message_text));
        spy_anim = BILL::Cache_Player_Spy_Anim();
        SCIENCE::Science_Room(spy_anim, 1, &discovered_tech, message_text);
        file_ani::Reset_File_Animation_Buffer();
        allocate::Pop_Block(MOX::screen_seg);
        COLONY::Reset_Mouse_Picture();
        JIM::Play_Background_Music();
    }

    void __cdecl Science_Room(char* intro_animation_name, uint16_t discovery_count, int16_t* discovery_data, char* footer_text) {
        MOX2::action_func_t saved_auto_function = nullptr;
        int16_t science_hot_key;
        uint16_t remaining_discoveries;
        uint16_t current_entry_index;
        bool first_entry;
        bool room_animation_ready;
        int16_t jumble_amount;
        int16_t jumble_steps;
        s_animation_header* science_room_picture;
        void* text_data_buffer;
        void* current_text_bitmap;
        void* previous_text_bitmap;
        void* unused_strip_bitmap;
        char help_lbx_name[20];
        char bill_text[80];
        char animation_lbx_names[8][64];
        char weapon_mod_text[256];

        MOX2::Get_Auto_Function(&saved_auto_function);
        fields::Deactivate_Help_List();
        video::Clip_Off();
        allocate::Push_Block(MOX::screen_seg);
        fields::Clear_Fields();

        remaining_discoveries = discovery_count;
        if (discovery_count != 0) {
            for (uint16_t i = 0; i < discovery_count; ++i) {
                snprintf(animation_lbx_names[i], sizeof(animation_lbx_names[i]), "TANM_%03hi.LBX", discovery_data[i] & 0x01FF);
            }
        }

        RUSS::Mox_Update();

        science_room_picture = (s_animation_header*)farload::Far_Reload_Next("SCIENCE.LBX", 0, MOX::screen_seg);
        SCIENCE::science_mouse_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0x1E, 0x1E));
        text_data_buffer = allocate::Allocate_Next_Block(MOX::screen_seg, 0x57C);
        current_text_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0xAF, 0xC3));
        previous_text_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0xAF, 0xC3));
        unused_strip_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0x258, 0x3C));

        buffer::Flush_Buffer_Space(MOX::global_cache_seg);

        if (MOX::settings.animations_on == 0) {
            animate::Set_Animation_Frame((s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 1, MOX::global_cache_seg), 5);
        }

        bitmap::Create_Blank_Bitmap(0xAF, 0xC3, current_text_bitmap);
        bitmap::Create_Blank_Bitmap(0xAF, 0xC3, previous_text_bitmap);
        bitmap::Create_Blank_Bitmap(0x258, 0x3C, unused_strip_bitmap);

        fonts::Set_Outline_Color(0, 0xF0);
        fonts::Set_Font_Style_Outline(4, &SCIENCE::science_normal_color);
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        ERIC::Fast_Fade_Out();

        file_ani::Open_Multi_File_Animation(intro_animation_name, 0, 1);
        fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "", 0);
        /* fmtpara \x1B color-attribute control byte reused as a non-printing hotkey label. */
        science_hot_key = fields::Add_Hot_Key("\x1B");

        video::Set_Page_Off();
        fields::Assign_Auto_Function(&SCIENCE::Draw_Science_Room, 2);

        COLONY::Clear_Mouse_Picture();
        bitmap::Clear_Mouse_Image(0);
        bitmap::Create_Blank_Bitmap(0x1E, 0x1E, (s_animation_header*)SCIENCE::science_mouse_bitmap);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)SCIENCE::science_mouse_bitmap, science_room_picture);
        bitmap::Draw_Bitmap_To_Mouse(0, 0, (s_animation_header*)SCIENCE::science_mouse_bitmap, 0);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        JIM::Get_Text_Message("billtext.lbx", 4, bill_text, sizeof(bill_text));

        buffer::Buffer_Reload("SCIENCE.LBX", 1, MOX::global_cache_seg);
        palette::Set_Palette_Changes(0, 0xFF);
        animate::Draw_Palette(science_room_picture);
        BILL::Turn_Off_Palettes(0, 0xFF);
        video::Set_Refresh_Stencil();
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
        file_ani::Draw_Multi_File_Animation(1);
        video::Copy_Off_To_Back();

        {
            int16_t fade_amount = 100;

            BILL::Turn_Off_Palettes(0, 0xFF);
            fields::Disable_Mouse_Wait();
            fields::Clear_Mouse_Buffer();

            do {
                JIM::Play_Streaming_Music(0x75, -2, 1);
                timer::Mark_Time();

                fade_amount -= 0x14;
                if (fade_amount < 0) {
                    fade_amount = 0;
                }

                palette::Set_Palette_Changes(0, 0xFF);
                palstore::Darken_Palette(fade_amount);
                palette::Clear_Palette_Changes(0, 0xFF);

                animate::Draw(0, 0, (s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 2, MOX::global_cache_seg));
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            } while (fade_amount > 0);
        }

        ERIC::Set_Fade_Percent(0);

        if (discovery_count != 0) {
            first_entry = true;
            room_animation_ready = false;
            current_entry_index = 0;
            memcpy(&jumble_amount, animation_lbx_names[0], sizeof(jumble_amount));
            memcpy(&jumble_steps, animation_lbx_names[0], sizeof(jumble_steps));

            bool should_close = false;

            for (;;) {
                JIM::Play_Streaming_Music(0x75, -2, 1);
                timer::Mark_Time();

                const int16_t input_key = fields::Get_Input();
                if (input_key == 0 && !first_entry) {
                    SCIENCE::Update_Science_Anims(true, true, true, true);
                } else {
                    if (remaining_discoveries == 0 || science_hot_key == input_key) {
                        should_close = true;
                        break;
                    }

                    SCIENCE::Update_Science_Anims(true, false, true, true);
                    RUSS::Mox_Sync_Update();

                    file_ani::Open_Multi_File_Animation(animation_lbx_names[current_entry_index], 0, 0);
                    video::Set_Page_Off();
                    file_ani::Draw_Multi_File_Animation_Palette(0);
                    palette::Refresh_Palette();

                    bitmap::Copy_Bitmap_To_Bitmap((s_animation_header*)previous_text_bitmap, (s_animation_header*)current_text_bitmap);
                    TEXTBOX::Get_Help_Lbx_Name(help_lbx_name, sizeof(help_lbx_name));

                    char* formatted_text = (char*)farload::Far_Reload_Data(
                        help_lbx_name,
                        0,
                        text_data_buffer,
                        discovery_data[current_entry_index] & 0x01FF,
                        1,
                        0x57B);

                    bitmap::Create_Blank_Bitmap(0xAF, 0xC3, current_text_bitmap);

                    fonts::Set_Font_Style(3, &SCIENCE::science_high_color);
                    FMTPARA::Print_Formatted_Paragraph_To_Bitmap(2, 1, 0xAB, formatted_text, 2, (s_animation_header*)current_text_bitmap);

                    const int16_t paragraph_height = FMTPARA::Get_Formatted_Paragraph_Max_Height(0xAB, formatted_text);
                    int16_t text_y = paragraph_height + 1;

                    fonts::Set_Font_Style(2, &SCIENCE::science_high_color);
                    fonts::Set_Font_LF(1);

                    if ((discovery_data[current_entry_index] & 0x4000) != 0) {
                        fonts::Print_To_Bitmap(2, text_y, bill_text, current_text_bitmap);

                        ++current_entry_index;

                        int32_t weapon_mask = discovery_data[current_entry_index];
                        uint16_t weapon_mod_index = WEAPON_MOD_HEAVY_MOUNT;
                        weapon_mod_text[0] = '\0';

                        while (weapon_mask != 0) {
                            if ((weapon_mask & 1) != 0) {
                                strlcat(weapon_mod_text, TECHDATA::weapon_modifications[weapon_mod_index].name, sizeof(weapon_mod_text));
                                /* fmtpara \r starts a new paragraph between modification names. */
                                strlcat(weapon_mod_text, "\r", sizeof(weapon_mod_text));
                            }

                            weapon_mask >>= 1;
                            ++weapon_mod_index;
                        }

                        formatted_text = weapon_mod_text;
                        text_y = paragraph_height + 0x10;
                    } else {
                        formatted_text += 0x67;
                    }

                    FMTPARA::Print_Formatted_Paragraph_To_Bitmap(2, text_y, 0xAB, formatted_text, 2, (s_animation_header*)current_text_bitmap);

                    ++current_entry_index;
                    --remaining_discoveries;

                    first_entry = false;
                    jumble_amount = (MOX::settings.animations_on != 0) ? 100 : 0;
                    jumble_steps = 6;

                    do {
                        SCIENCE::Update_Science_Anims(true, false, true, true);
                    } while (mouse::Mouse_Button() != 0);

                    fields::Clear_Mouse_Buffer();
                }

                if (room_animation_ready) {
                    if (jumble_amount <= 0) {
                        animate::Draw(0xDD, 0x2C, (s_animation_header*)current_text_bitmap);
                    } else {
                        bitmap::Jumble_Bitmap((s_animation_header*)previous_text_bitmap, (s_animation_header*)current_text_bitmap, jumble_amount, jumble_steps);
                        jumble_amount -= 0x14;

                        if (jumble_steps > 1) {
                            --jumble_steps;
                        }
                        animate::Draw(0xDD, 0x2C, (s_animation_header*)previous_text_bitmap);
                    }
                } else {
                    room_animation_ready = animate::Get_Current_Frame(
                                               (s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 1, MOX::global_cache_seg)) >= 5;
                }

                fonts::Set_Outline_Color(0, 0xF2);
                fonts::Set_Font_Style_Outline(4, &SCIENCE::science_normal_color);
                FMTPARA::Print_Formatted_Paragraph(0x91, 0x1A4, 0x17C, footer_text, 2);

                timer::Release_Time(1);
                RUSS::Mox_Sync_Update();
            }

            if (should_close) {
                int16_t frame = animate::Get_Current_Frame(
                    (s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 1, MOX::global_cache_seg));
                --frame;

                if (frame >= 0 && MOX::settings.animations_on != 0) {
                    do {
                        const int32_t frame_index = frame;

                        timer::Mark_Time();

                        animate::Set_Animation_Frame(
                            (s_animation_header*)buffer::Buffer_Reload("SCIENCE.LBX", 1, MOX::global_cache_seg),
                            frame_index);

                        SCIENCE::Update_Science_Anims(false, false, true, true);

                        if (frame_index < 5) {
                            bitmap::Scale_Bitmap(
                                (s_animation_header*)previous_text_bitmap,
                                (s_animation_header*)current_text_bitmap,
                                SCIENCE::bm_scale_x[frame_index],
                                SCIENCE::bm_scale_y[frame_index]);

                            animate::Draw(
                                SCIENCE::bm_scaled_x1[frame_index],
                                SCIENCE::bm_scaled_y1[frame_index],
                                (s_animation_header*)previous_text_bitmap);
                        } else {
                            animate::Draw(0xDD, 0x2C, (s_animation_header*)current_text_bitmap);
                        }

                        --frame;

                        timer::Release_Time(1);
                        RUSS::Mox_Sync_Update();
                        JIM::Play_Streaming_Music(0x75, -2, 1);
                    } while (frame >= 0);
                }

                SCIENCE::Update_Science_Anims(false, false, false, false);
                RUSS::Mox_Sync_Update();
            }
        }

        allocate::Pop_Block(MOX::screen_seg);
        fields::Clear_Fields();
        fields::Clear_Mouse_Buffer();
        fields::Enable_Mouse_Wait();
        MOX2::Put_Auto_Function(&saved_auto_function);
        ERIC::Set_Fade_Percent(0);
    }

    void __cdecl Draw_Science_Room() {
    }

    void __cdecl Show_Off_Researched_Tech(int16_t player_idx) {
        int16_t researched_tech_ids[4];
        char science_room_lbx[64];
        char decoded_text[256];
        uint16_t researched_tech_count = 0;

        MOX::cur_plyr_ptr = &MOX::player[player_idx];

        if (MOX::cur_plyr_ptr->current_research_field != 0) {
            const uint8_t research_field = (uint8_t)MOX::cur_plyr_ptr->current_research_field;

            TECH::Technology_Fields_Name(research_field);

            const int16_t* field_tech_ids = TECHDATA::technology_fields[research_field].tech;
            for (uint16_t tech_slot = 0; tech_slot < 4; ++tech_slot) {
                const int16_t tech_id = field_tech_ids[tech_slot];
                if (tech_id > TECH_APP_NO_TECH && MOX::cur_plyr_ptr->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    researched_tech_ids[researched_tech_count++] = tech_id;
                }
            }

            if (researched_tech_count != 0) {
                JIM::Get_Text_Message("billtext.lbx", 1, decoded_text, 0xFA);
                JIM::Decode_Text_Field(decoded_text, research_field, 0xFA);

                snprintf(
                    science_room_lbx,
                    sizeof(science_room_lbx),
                    "SR_R%x_SC.LBX",
                    (unsigned int)(uint8_t)MOX::player[player_idx].race);

                SCIENCE::Science_Room(science_room_lbx, researched_tech_count, researched_tech_ids, decoded_text);
                return;
            }
        }

        decoded_text[0] = '\0';
        fields::Clear_Fields();
        ERIC::Fast_Fade_Out();
    }
} // namespace SCIENCE