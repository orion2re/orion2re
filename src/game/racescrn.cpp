#include "pch.h"
#include "relations_read.h"
#include "game_events.h"

namespace RACESCRN {

    static void __cdecl Clear_Mouse_Group_Bitmap(s_racescrn_mouse_group* group) {
        bitmap::Create_Blank_Bitmap(187, 25, group->sprite2);
    }

    s_racescrn_icon_layout mouse_group_data = {0, 0, 186, 24, 25, Clear_Mouse_Group_Bitmap};
    int16_t g_spy_xfer_count;
    s_racescrn_mouse_group* g_agent_group_p;
    s_racescrn_mouse_group* g_mouse_group_p;
    int32_t net_state;
    s_racescrn_race_display* g_display_p;
    uint8_t g_in_who_mode;
    s_animation_header* g_mouse_seg;
    int16_t g_agent_field;
    uint8_t g_in_spy_mode;
    s_animation_header* arrow_seg;
    char trade_treaty_buffer[30];
    char ignoring_str[20];
    s_animation_header* netbox_seg;
    char agent_bonus_str[20];
    char no_contact_str[20];
    char declare_war_buffer[120];
    char spy_bonus_str[20];
    char giving_treaty_buffer[80];
    char research_treaty_buffer[30];
    char relation_text[17][15];
    uint8_t slider_remap_list[26] = {74, 72, 68, 66, 62, 60, 56, 54, 50, 48, 44, 42, 38, 36, 32, 30, 26, 24, 20, 18, 14, 12, 8, 6, 2, 0};
    uint8_t relation_name_table[17] = {9, 21, 33, 45, 57, 69, 81, 93, 107, 119, 131, 143, 155, 167, 179, 191, 255};
    s_racescrn_icon_layout spy_group_data[MAX_PLAYERS - 1] = {
        {121, 98, 307, 122, 25, nullptr},
        {121, 205, 307, 229, 25, nullptr},
        {121, 311, 307, 335, 25, nullptr},
        {121, 417, 307, 441, 25, nullptr},
        {333, 98, 519, 122, 25, nullptr},
        {333, 206, 519, 230, 25, nullptr},
        {333, 311, 519, 335, 25, nullptr},
    };
    int16_t race_bar[(MAX_PLAYERS - 1) * 2] = {105, 49, 105, 155, 105, 262, 105, 370, 528, 49, 528, 156, 528, 261};
    int16_t race_spies[(MAX_PLAYERS - 1) * 2] = {121, 98, 121, 205, 121, 311, 121, 417, 333, 98, 333, 206, 333, 311};
    int16_t race_spy_btns[(MAX_PLAYERS - 1) * 2] = {120, 126, 120, 233, 120, 338, 120, 445, 332, 126, 332, 233, 332, 338};
    int16_t race_slider_x[MAX_PLAYERS - 1] = {96, 96, 96, 96, 520, 520, 520};
    s_racescrn_icon_layout agent_group_data = {332, 392, 613, 415, 24, nullptr};
    int16_t race_picture[(MAX_PLAYERS - 1) * 2] = {21, 49, 21, 156, 21, 262, 21, 369, 544, 50, 544, 157, 544, 262};
    int16_t race_text[(MAX_PLAYERS - 1) * 2] = {125, 50, 125, 157, 125, 262, 125, 366, 333, 50, 333, 157, 333, 262};
    s_colors race_normal_color = {{176, 185, 181, 182, 184, 185, 185, 186}};
    s_colors race_high_color = {{176, 191, 183, 185, 187, 188, 191, 191}};
    s_colors banner_color_high = {{67, 139, 147, 38, 61, 95, 106, 47}};
    s_colors banner_color_low = {{70, 138, 149, 35, 59, 96, 110, 49}};
    s_colors banner_color_edge = {{72, 97, 150, 26, 57, 116, 158, 73}};
    char receiving_treaty_buffer[80];
    char diplomacy_net_wait_buffer[100];
    char diplomacy_net_requesting_buffer[100];

    void __cdecl Calculate_Icon_Group_Data(s_racescrn_mouse_group* group) {
        int16_t width = (group->x2 - group->x1) + 1;
        group->total_height = (group->y2 - group->y1) + 1;

        if (group->count <= 1) {
            group->spacing_x = group->icon_width;
            group->total_width = group->icon_width;
        } else {
            // Calculate spacing based on available width and icon count
            int16_t spacing = (width - group->icon_width) / (group->count - 1);

            // Clamp spacing: cannot exceed icon width and cannot be negative
            if (spacing > group->icon_width) {
                spacing = group->icon_width;
            }

            if (spacing < 0) {
                spacing = 0;
            }

            group->spacing_x = spacing;
            group->total_width = group->icon_width + (group->count - 1) * spacing;

            // Ensure total width does not exceed the defined bounding box width
            if (group->total_width > width) {
                group->total_width = width;
            }
        }
    }

    void __cdecl Setup_Icon_Group_Data(s_racescrn_mouse_group* group, s_racescrn_icon_layout* layout, int16_t count, s_animation_header* sprite1, s_animation_header* sprite2) {
        group->x1 = layout->x1;
        group->y1 = layout->y1;
        group->x2 = layout->x2;
        group->y2 = layout->y2;

        group->total_height = layout->spacing_x;

        group->render_fn = layout->render_fn;

        group->count = count;

        if (sprite1 == nullptr) {
            group->icon_width = 0;
        } else {
            group->icon_width = animate::Get_Width(sprite1);
        }

        group->sprite1 = sprite1;
        group->sprite2 = sprite2;

        Calculate_Icon_Group_Data(group);
    }

    void __cdecl Draw_Race_Text(s_racescrn_race_display* display) {
        char treaty_text_buffer[200];
        char treaty_segment_buffer[64];
        int16_t spy_bonus;
        int16_t agent_bonus;

        for (int i = 0; i < (MAX_PLAYERS - 1); ++i) {
            int16_t race_text_x = race_text[i * 2];
            int16_t race_text_y = race_text[i * 2 + 1];

            if (display->is_empty_slot != 0 ||
                (HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM) && MOX::player[MOX::PLAYER_NUM].contact[display->player_idx] == 0)) {
                fonts::Set_Font_Style(4, &race_normal_color);
                fonts::Print_Centered(race_text_x + 0x5B, race_text_y + 10, no_contact_str);

                if (i < MOX::active_count &&
                    HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM) &&
                    MOX::player[MOX::PLAYER_NUM].contact[display->player_idx] == 0) {
                    fonts::Set_Font_Style_Outline(2, &race_high_color);
                    BILL::Squeeze_Centered_Print(
                        race_picture[i * 2],
                        race_picture[i * 2 + 1] + 0x4C,
                        display->player_name,
                        0x4C);
                }
            } else {
                fonts::Set_Font_Style_Outline(2, &race_high_color);
                BILL::Squeeze_Centered_Print(
                    race_picture[i * 2],
                    race_picture[i * 2 + 1] + 0x4C,
                    display->player_name,
                    0x4C);

                if (display->is_eliminated == 0) {
                    relations_read::Copy_Treaty_Summary(MOX::PLAYER_NUM, display->player_idx, treaty_text_buffer, sizeof(treaty_text_buffer));

                    fonts::Set_Font_Style(2, &race_normal_color);
                    BILL::Squeeze_Paragraph_Centered(race_text_x, race_text_y, 0xB6, 0x2B, treaty_text_buffer, 2);
                }
            }
            display++;
        }

        fonts::Set_Font_Style(3, &race_normal_color);
        relations_read::s_organization organization;
        relations_read::Read_Organization(MOX::PLAYER_NUM, &organization);
        spy_bonus = organization.spy_bonus;
        agent_bonus = organization.agent_bonus;

        snprintf(treaty_segment_buffer, sizeof(treaty_segment_buffer), "%s%hi%%", spy_bonus_str, spy_bonus);
        BILL::Squeeze_Centered_Print(0x1A1, 0x174, treaty_segment_buffer, 0x62);

        snprintf(treaty_segment_buffer, sizeof(treaty_segment_buffer), "%s%hi%%", agent_bonus_str, agent_bonus);
        BILL::Squeeze_Centered_Print(0x207, 0x174, treaty_segment_buffer, 0x61);
    }

    void __cdecl Draw_Relations_Sliders(s_racescrn_race_display* displays) {
        allocate::Push_Block(MOX::screen_seg);
        s_animation_header* bar_anim = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 3, MOX::screen_seg);

        for (int16_t i = 0; i < (MAX_PLAYERS - 1); ++i) {
            int16_t x;
            int16_t y;
            s_animation_header* slider_img;

            if (i < MOX::active_count) {
                animate::Draw(race_bar[i * 2], race_bar[i * 2 + 1], bar_anim);
                y = displays[i].relationship_slider_y;
                x = race_slider_x[i];
                slider_img = displays[i].relationship_slider_img;
            } else {
                x = race_slider_x[i];
                slider_img = displays[i].relationship_slider_img;
                y = race_bar[i * 2 + 1] + 0x4B;
            }

            animate::Draw(x, y, slider_img);
        }

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Draw_Race_Photos(s_racescrn_race_display* race_displays) {
        for (int i = 0; i < (int)MOX::active_count; ++i) {
            s_racescrn_race_display* display = &race_displays[i];
            int16_t x = race_picture[i * 2];
            int16_t y = race_picture[i * 2 + 1];

            animate::Draw(x, y, display->race_portrait_img);

            if (display->is_eliminated != 0) {
                animate::Draw(x, y, display->eliminated_overlay_img);
            }

            uint8_t color_idx = MOX::player[display->player_idx].color;

            graphics::Box(x, y, 76, 88, 1, banner_color_high.colors[color_idx]);
            graphics::Box(x + 1, y + 1, 74, 86, 1, banner_color_low.colors[color_idx]);

            uint8_t edge_color = banner_color_edge.colors[color_idx];
            graphics::Dot(x, y, edge_color);
            graphics::Dot(x + 75, y, edge_color);
            graphics::Dot(x, y + 87, edge_color);
            graphics::Dot(x + 75, y + 87, edge_color);
        }
    }

    void __cdecl Init_Race_Display_Data(s_racescrn_race_display* race_displays, s_racescrn_mouse_group* agent_group) {
        memset(race_displays, 0, sizeof(*race_displays) * (MAX_PLAYERS - 1));

        s_animation_header* spy_agent_icon_sprite = (s_animation_header*)farload::Far_Reload_Next("races.lbx", MOX::player[MOX::PLAYER_NUM].race + 0x2E, MOX::screen_seg);
        s_animation_header* slider_sprite = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 2, MOX::screen_seg);
        s_animation_header* eliminated_overlay_sprite = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 0x1F, MOX::screen_seg);

        for (int16_t i = 0; i < (MAX_PLAYERS - 1); ++i) {
            race_displays[i].relationship_slider_img = slider_sprite;

            if (i < MOX::active_count) {
                int16_t active_player_idx = (uint8_t)MOX::active_player[i];
                s_player* player = &MOX::player[active_player_idx];
                int16_t relation_idx = relations_read::Relationship_Bucket(MOX::PLAYER_NUM, active_player_idx);

                race_displays[i].relationship_text = RACESCRN::relation_text[relation_idx];

                race_displays[i].list_idx = i;
                race_displays[i].player_idx = active_player_idx;
                race_displays[i].player = player;
                race_displays[i].player_name = player->name;
                race_displays[i].race = (int16_t)(uint8_t)player->race;
                race_displays[i].relationship_slider_y = RACESCRN::race_bar[i * 2 + 1] + relations_read::Relationship_Indicator_Offset(MOX::PLAYER_NUM, active_player_idx);
                race_displays[i].eliminated_overlay_img = eliminated_overlay_sprite;

                if (!player->eliminated) {
                    relations_read::s_assignment assignment;
                    relations_read::Read_Assignment(MOX::PLAYER_NUM, active_player_idx, &assignment);
                    int16_t spy_count = assignment.count;
                    race_displays[i].agent_mission_idx = assignment.mission + 1;

                    RACESCRN::Setup_Icon_Group_Data(&race_displays[i].spy_icons_group, &RACESCRN::spy_group_data[i], spy_count, spy_agent_icon_sprite, nullptr);

                    race_displays[i].spy_icon_img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", i + 10, MOX::screen_seg);
                    race_displays[i].sabotage_icon_img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", i + 17, MOX::screen_seg);
                    race_displays[i].assassination_icon_img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", i + 24, MOX::screen_seg);
                } else {
                    race_displays[i].is_eliminated = 1;
                }

                race_displays[i].race_portrait_img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", race_displays[i].race + 32, MOX::screen_seg);
            } else {
                race_displays[i].is_empty_slot = 1;
            }
        }

        relations_read::s_organization organization;
        relations_read::Read_Organization(MOX::PLAYER_NUM, &organization);
        int16_t agent_count = organization.agents;
        RACESCRN::Setup_Icon_Group_Data(agent_group, &agent_group_data, agent_count, spy_agent_icon_sprite, nullptr);
    }

    int16_t __cdecl Setup_Main_Fields(int16_t* ok_btn_id, int16_t* agriculture_btn_id, int16_t* agriculture_field_id, int16_t* research_btn_id, int16_t* research_field_id, int16_t* industry_btn_id, int16_t* industry_field_id, int16_t* diplomacy_btn_id, int16_t* diplomacy_field_id) {
        fields::Clear_Fields();

        s_animation_header* img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 9, MOX::screen_seg);
        /* fmtpara 0x1B is a color-attribute control byte, not a printable space. */
        *ok_btn_id = fields::Add_Button_Field(535, 433, "", img, "\x1b", 40 /* '(' */);

        *agriculture_field_id = 0;
        *research_field_id = 0;
        *industry_field_id = 0;
        *diplomacy_field_id = 0;

        if (MOX::active_count < 1) {
            *industry_btn_id = 0;
            *research_btn_id = 0;
            *agriculture_btn_id = 0;
            *diplomacy_btn_id = 0;
        } else {
            img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 6, MOX::screen_seg);
            *industry_btn_id = fields::Add_Radio_Button_Field(429, 444, "", img, industry_field_id, "I", 40 /* '(' */);

            img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 7, MOX::screen_seg);
            *research_btn_id = 0;
            if (DIP_SCRN::noreport == 0) {
                *research_btn_id = fields::Add_Radio_Button_Field(429, 423, "", img, research_field_id, "R", 40 /* '(' */);
            }

            img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 8, MOX::screen_seg);
            *agriculture_btn_id = fields::Add_Radio_Button_Field(334, 423, "", img, agriculture_field_id, "A", 40 /* '(' */);

            img = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 60, MOX::screen_seg);
            *diplomacy_btn_id = fields::Add_Radio_Button_Field(334, 444, "", img, diplomacy_field_id, "D", 40 /* '(' */);
        }

        return (int16_t)(fields::Get_Field_Counter() - 1);
    }

    void __cdecl Move_Icon_Group_Icons(s_racescrn_mouse_group* src, s_racescrn_mouse_group* dst, uint16_t count) {
        if (src->count < count) {
            count = src->count;
        }

        if (count != 0) {
            dst->count += count;
            src->count -= count;
            dst->icon_width = src->icon_width;
            dst->sprite1 = src->sprite1;

            Calculate_Icon_Group_Data(dst);
            Calculate_Icon_Group_Data(src);
        }
    }

    void __cdecl Move_Spy_Group_From_Mouse(s_racescrn_mouse_group* target_group) {
        int16_t num_to_move = g_mouse_group_p->count;

        if (num_to_move != 0) {
            if (target_group->count + num_to_move > 63) {
                int16_t available_space = 63 - target_group->count;
                g_spy_xfer_count = num_to_move - available_space;
                num_to_move = available_space;
            }

            Move_Icon_Group_Icons(g_mouse_group_p, target_group, num_to_move);

            if (g_mouse_group_p->count == 0) {
                JIM::Bill_Picture_To_Mouse(g_mouse_seg);
            } else {
                Redraw_Spy_Mouse(arrow_seg);
            }
        }
    }

    void __cdecl Move_Spies_Group(s_racescrn_mouse_group* mouse_group, s_animation_header* mouse_sprite) {
        Move_Icon_Group_Icons(mouse_group, RACESCRN::g_mouse_group_p, RACESCRN::g_spy_xfer_count);
        Redraw_Spy_Mouse(mouse_sprite);
    }

    void __cdecl Adjust_Spy_Mission_Data() {
        int16_t active_count = MOX::active_count;

        for (int i = 0; i < (int)active_count; ++i) {
            if (g_display_p[i].agent_mission_idx == 0) {
                g_display_p[i].agent_mission_idx = 3;
            }
        }
    }

    int8_t __cdecl Setup_Race_Display_Fields(s_racescrn_race_display* race_displays, int16_t base_field_id) {
        if (MOX::active_count <= 0) {
            return 0;
        }

        fields::Clear_Fields_Above(base_field_id);

        RACESCRN::g_agent_field = fields::Add_Hidden_Field(328, 386, 613, 415, "", 0);

        for (int i = 0; i < (int)MOX::active_count; ++i) {
            s_racescrn_race_display* display = &race_displays[i];
            int16_t btn_x = RACESCRN::race_spy_btns[i * 2];
            int16_t btn_y = RACESCRN::race_spy_btns[i * 2 + 1];

            // Add mission toggle buttons (Spy, Sabotage, Assassination)
            fields::Add_Multi_Button_Field(btn_x, btn_y, "", display->spy_icon_img, &display->agent_mission_idx, 1, "", 41);
            fields::Add_Multi_Button_Field(btn_x + 76, btn_y, "", display->sabotage_icon_img, &display->agent_mission_idx, 2, "", 41);
            fields::Add_Multi_Button_Field(btn_x + 149, btn_y, "", display->assassination_icon_img, &display->agent_mission_idx, 3, "", 41);

            // Add interaction field for spy/agent area
            int16_t spy_x = RACESCRN::race_spies[i * 2];
            int16_t spy_y = RACESCRN::race_spies[i * 2 + 1];
            display->main_interaction_field_id = fields::Add_Hidden_Field(spy_x, spy_y - 6, spy_x + 187, spy_y + 25, "", 0);

            // Add interaction field for relationship bar area
            int16_t bar_x = RACESCRN::race_bar[i * 2];
            int16_t bar_y = RACESCRN::race_bar[i * 2 + 1];
            display->relationshiop_field_id = fields::Add_Hidden_Field(bar_x - 6, bar_y - 6, bar_x + 8, bar_y + 88, "", 0);
        }

        return 1;
    }

    int8_t __cdecl Setup_Race_Who_Fields(s_racescrn_race_display* race_displays, int16_t field_id_base) {
        if (MOX::active_count <= 0) {
            return 0;
        }

        fields::Clear_Fields_Above(field_id_base);

        int16_t x_offset = 0;
        int16_t* race_picture = RACESCRN::race_picture;

        for (int16_t i = 0; i < (int16_t)MOX::active_count; ++i) {
            if (i == 4) {
                x_offset = 320;
            }

            int16_t y_base = race_picture[i * 2 + 1];

            race_displays[i].main_interaction_field_id = fields::Add_Hidden_Field(
                x_offset,
                y_base,
                x_offset + 319,
                y_base + 88,
                "",
                0);
        }

        g_agent_field = 9999;
        return 1;
    }

    /**
     * @brief Finds the spy group corresponding to a specific UI field ID.
     * 
     * @param field_id The UI field ID to look up.
     * @param out_index Pointer to store the index of the race display entry found.
     * @return Pointer to the s_racescrn_mouse_group if found, otherwise nullptr.
     */
    s_racescrn_mouse_group* __cdecl Get_Spy_Group_For_Field(int16_t field_id, int16_t* out_index) {
        *out_index = 0;

        if (g_agent_field == field_id) {
            return g_agent_group_p;
        }

        if (MOX::active_count > 0) {
            for (int16_t i = 0; i < MOX::active_count;) {
                if (g_display_p[i].main_interaction_field_id == field_id) {
                    return &g_display_p[i].spy_icons_group;
                }

                i++;
                *out_index = i;
            }
        }

        return nullptr;
    }

    void __cdecl Update_Spy_Stuff(s_racescrn_race_display* race_displays) {
        int16_t active_count = MOX::active_count;
        s_racescrn_race_display* end = &race_displays[active_count];

        for (s_racescrn_race_display* display = race_displays; display != end; ++display) {
            if (display->agent_mission_idx > 0) {
                BILL::Set_My_Spy_Mission(display->player_idx, display->agent_mission_idx - 1);
            }
            BILL::Set_My_Spy_Number(display->player_idx, (uint8_t)display->spy_icons_group.count);
        }

        BILL::Set_My_Agent_Number((uint8_t)RACESCRN::g_agent_group_p->count);
    }

    void __cdecl Draw_Race_Screen() {
        s_racescrn_race_display* display;
        int16_t input_field;
        int16_t mouse_x;
        int16_t mouse_y;
        char text_buffer[100];
        int32_t race_idx;
        relations_read::s_disclosure disclosures[MAX_PLAYERS];
        int16_t disclosure_count = 0;

        s_racescrn_race_display* end = &RACESCRN::g_display_p[MOX::active_count];
        input_field = fields::Scan_Input();
        mouse_x = mouse::Pointer_X();
        mouse_y = mouse::Pointer_Y();

        fields::Enable_Draw_All_Fields();
        fields::Draw_Visible_Fields();
        fields::Disable_Draw_All_Fields();

        for (display = RACESCRN::g_display_p; display != end; ++display) {
            int16_t list_idx = display->list_idx;
            RACESCRN::Draw_Icon_Group(&display->spy_icons_group);

            if (display->is_eliminated == 0) {
                int16_t spy_count = BILL::Get_Their_Spy_Number(display->player_idx, MOX::PLAYER_NUM);
                if (spy_count > 0) {
                    fonts::Set_Font_Style_Outline(2, &RACESCRN::race_high_color);
                    const STRBUILD::s_named_argument argument = STRBUILD::Named_Argument("SPIES", spy_count);
                    TEXT_FORMAT::Print(
                        TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES],
                        &argument,
                        1,
                        RACESCRN::race_picture[list_idx * 2 + 1],
                        RACESCRN::race_picture[list_idx * 2]);
                    if (relations_read::Read_Overlay(MOX::PLAYER_NUM, display->player_idx, &disclosures[disclosure_count])) ++disclosure_count;
                }
            }

            if (RACESCRN::g_in_who_mode != 0) {
                if (display->main_interaction_field_id == input_field) {
                    graphics::Box(RACESCRN::race_picture[list_idx * 2], RACESCRN::race_picture[list_idx * 2 + 1], 0x4C, 0x58, 1, 0xFF);
                }
            } else {
                if (display->main_interaction_field_id == input_field) {
                    if (RACESCRN::g_in_spy_mode != 0 || display->spy_icons_group.count > 0) {
                        fonts::Set_Font_Style_Outline(2, &RACESCRN::race_high_color);
                        RACESCRN::g_spy_xfer_count = RACESCRN::Draw_Icon_Group_Cursor(&display->spy_icons_group, mouse_x, 0xFF, 1);
                    }
                } else if (display->relationshiop_field_id == input_field) {
                    fonts::Set_Font_Style_Outline(2, &RACESCRN::race_high_color);
                    fonts::Print_Centered(RACESCRN::race_bar[list_idx * 2] + 4, display->relationship_slider_y + 1, display->relationship_text);
                }
            }

            relations_read::s_relation relation;
            relations_read::Read_Relation(MOX::PLAYER_NUM, display->player_idx, &relation);
            if (relation.ignoring) {
                fonts::Set_Font_Style_Outline(1, &RACESCRN::race_high_color);
                fonts::Print_Centered(RACESCRN::race_picture[list_idx * 2] + 0x25, RACESCRN::race_picture[list_idx * 2 + 1] + 0x42, RACESCRN::ignoring_str);
            }
        }

        RACESCRN::Draw_Icon_Group(RACESCRN::g_agent_group_p);

        if (input_field == RACESCRN::g_agent_field) {
            if (RACESCRN::g_agent_group_p->count > 0 || RACESCRN::g_in_spy_mode != 0) {
                fonts::Set_Font_Style_Outline(2, &RACESCRN::race_high_color);
                RACESCRN::g_spy_xfer_count = RACESCRN::Draw_Icon_Group_Cursor(RACESCRN::g_agent_group_p, mouse_x, 0xFF, 1);
            }
        }

        if (RACESCRN::g_mouse_group_p->count != 0) {
            video::Clip_On();
            animate::Draw(mouse_x + 3, mouse_y + 2, RACESCRN::g_mouse_group_p->sprite2);
            video::Clip_Off();
        }

        palette::Cycle_Palette_Color(0xFF, 10, 20, 5, 40, 60, 10, 7);
        fonts::Set_Font_Style_Outline(4, &RACESCRN::race_high_color);

        if (RACESCRN::net_state == 1) {
            animate::Draw(0x85, 0xBF, (s_animation_header*)RACESCRN::netbox_seg);
            strlcpy(text_buffer, RACESCRN::diplomacy_net_requesting_buffer, sizeof(text_buffer));
            race_idx = NETMOX::Net_Diplomacy_With();
            JIM::Decode_Text_Race(text_buffer, (int16_t)race_idx, sizeof(text_buffer));
            FMTPARA::Print_Formatted_Paragraph(0x8F, 0xDA, 0x15E, text_buffer, 2);
        } else if (RACESCRN::net_state == 2) {
            animate::Draw(0x85, 0xBF, (s_animation_header*)RACESCRN::netbox_seg);
            strlcpy(text_buffer, RACESCRN::diplomacy_net_wait_buffer, sizeof(text_buffer));
            race_idx = NETMOX::Net_Diplomacy_With();
            JIM::Decode_Text_Race2(text_buffer, (int16_t)race_idx, sizeof(text_buffer));
            FMTPARA::Print_Formatted_Paragraph(0x8F, 0xD1, 0x15E, text_buffer, 2);
        }
        game_events::Publish_Relations_View(false, disclosures, disclosure_count);
    }

    void __cdecl Redraw_Spy_Mouse(s_animation_header* spy_icon) {
        char num_str[16];

        Draw_Icon_Group(g_mouse_group_p);
        bitmap::Add_Picture_To_Bitmap(1, 1, spy_icon, g_mouse_group_p->sprite2);

        if (g_spy_xfer_count > 1) {
            fonts::Set_Font_Style_Outline(2, &race_high_color);
            snprintf(num_str, sizeof(num_str), "%i", (int32_t)g_spy_xfer_count);
            fonts::Print_To_Bitmap(8, 4, num_str, g_mouse_group_p->sprite2);
        }

        COLONY::Clear_Mouse_Picture();
    }

    int16_t __cdecl Draw_Icon_Group_Cursor(s_racescrn_mouse_group* group, int16_t x_pos, uint8_t color, bool show_count) {
        if (group->count == 0) {
            graphics::Box(group->x1, group->y1, group->icon_width, group->total_height, 1, color);
            return 0;
        }

        int16_t max_x = group->total_width + group->x1 - 1;
        if (x_pos > max_x) {
            x_pos = max_x;
        }
        if (x_pos < group->x1) {
            x_pos = group->x1;
        }

        int16_t index = (x_pos - group->x1) / group->spacing_x;
        if (index >= group->count) {
            index = group->count - 1;
        }

        int16_t remaining_count = group->count - index;
        if (remaining_count > 0) {
            int16_t draw_x = group->spacing_x * index + group->x1;
            int16_t width = (max_x - draw_x) + 1;

            graphics::Box(draw_x, group->y1, width, group->total_height, 1, color);

            if (show_count && remaining_count > 1) {
                fonts::Print_Integer(draw_x + 3, group->y1 + 3, remaining_count);
            }

            return remaining_count;
        }

        return 0;
    }

    void __cdecl Draw_Icon_Group(s_racescrn_mouse_group* group) {
        if (group->render_fn != nullptr) {
            group->render_fn(group);
        }

        if (group->count > 0) {
            int16_t current_x = group->x1;
            for (int16_t i = 0; i < group->count; ++i) {
                if (group->sprite2 == nullptr) {
                    animate::Draw(current_x, group->y1, group->sprite1);
                } else {
                    bitmap::Add_Picture_To_Bitmap(current_x, group->y1, group->sprite1, group->sprite2);
                }
                current_x += group->spacing_x;
            }
        }
    }

    void Load_Relations_Strings() {
        for (int16_t i = 0; i < 17; ++i) JIM::Get_Text_Message("billtext.lbx", 0x21 + i, relation_text[i], sizeof(relation_text[i]));
        JIM::Get_Text_Message("billtext.lbx", 0x39, research_treaty_buffer, sizeof(research_treaty_buffer));
        JIM::Get_Text_Message("billtext.lbx", 0x3A, trade_treaty_buffer, sizeof(trade_treaty_buffer));
        JIM::Get_Text_Message("billtext.lbx", 0x3B, giving_treaty_buffer, sizeof(giving_treaty_buffer));
        JIM::Get_Text_Message("billtext.lbx", 0x3C, receiving_treaty_buffer, sizeof(receiving_treaty_buffer));
    }

    void __cdecl Race_Screen() {
        int16_t selected_race_idx = -1;
        int16_t old_mission_idx = -1;
        MOX2::action_func_t auto_function = nullptr;

        MOX2::Get_Auto_Function(&auto_function);
        palette::Save_Palette();
        fields::Disable_Draw_All_Fields();
        RUSS::Mox_Update();

        JIM::Get_Text_Message("billtext.lbx", 0x32, ignoring_str, sizeof(ignoring_str));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x33, no_contact_str, sizeof(no_contact_str));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x34, spy_bonus_str, sizeof(spy_bonus_str));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x35, agent_bonus_str, sizeof(agent_bonus_str));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x4C, diplomacy_net_requesting_buffer, sizeof(diplomacy_net_requesting_buffer));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x4D, diplomacy_net_wait_buffer, sizeof(diplomacy_net_wait_buffer));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtex2.lbx", 0x1A, declare_war_buffer, sizeof(declare_war_buffer));
        RUSS::Mox_Update();

        ERIC::Fast_Fade_Out();

        while (true) {
            game_events::s_relations_view_scope relations_owner(false);
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            RUSS::Mox_Update();

            s_racescrn_race_display race_display_data[MAX_PLAYERS - 1];
            s_racescrn_mouse_group agent_group;
            char message_buffer[120];
            s_racescrn_mouse_group mouse_group;

            MOX::cur_plyr_ptr = MOX::player + MOX::PLAYER_NUM;
            g_display_p = race_display_data;
            g_mouse_group_p = &mouse_group;
            g_agent_group_p = &agent_group;

            RUSS::Mox_Update();
            BILLHELP::Races_Help();
            fields::Clear_Fields();
            video::Set_Page_Off();
            video::Set_Refresh_Stencil();
            RUSS::Mox_Update();

            uint32_t bmp_size = bitmap::Get_Bitmap_Size(187, 25);
            s_animation_header* sprite = (s_animation_header*)allocate::Allocate_First_Block(MOX::screen_seg, bmp_size);
            bitmap::Create_Blank_Bitmap(187, 25, sprite);
            RUSS::Mox_Update();

            s_animation_header* anim = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 0, MOX::screen_seg);
            RUSS::Mox_Update();

            arrow_seg = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 1, MOX::screen_seg);
            RUSS::Mox_Update();

            g_mouse_seg = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 4, MOX::screen_seg);
            RUSS::Mox_Update();

            int16_t sprite_idx = 5;
            switch (MOX::settings.language) {
                case 0: sprite_idx = 5; break;
                case 1: sprite_idx = 61; break;
                case 2: sprite_idx = 62; break;
                case 3: sprite_idx = 5; break;
                case 4: sprite_idx = 63; break;
            }
            s_animation_header* mouse_pic = (s_animation_header*)farload::Far_Reload_Next("races.lbx", sprite_idx, MOX::screen_seg);
            RUSS::Mox_Update();

            netbox_seg = (s_animation_header*)farload::Far_Reload_Next("races.lbx", 59, MOX::screen_seg);
            RUSS::Mox_Update();

            JIM::Bill_Picture_To_Mouse(g_mouse_seg);
            BILL::Get_Players_Dead_Or_Alive_Or_Omniscient(0, 0);
            RUSS::Mox_Update();

            Init_Race_Display_Data(race_display_data, &agent_group);
            RUSS::Mox_Update();

            Setup_Icon_Group_Data(&mouse_group, &mouse_group_data, 0, nullptr, sprite);
            RUSS::Mox_Update();

            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, anim);
            RUSS::Mox_Update();

            if (ERIC::Get_Fade_Percent() > 0) {
                BILL::Turn_Off_Palettes(0, 255);
            }
            palette::Reset_Cycle_Palette_Color();
            RUSS::Mox_Update();

            fonts::Set_Outline_Color(177, 177);
            Draw_Race_Photos(race_display_data);
            RUSS::Mox_Update();

            Draw_Race_Text(race_display_data);
            RUSS::Mox_Update();

            BILL::Get_Active_Players(0, 0);
            Draw_Relations_Sliders(race_display_data);
            RUSS::Mox_Update();

            video::Copy_Off_To_Back();

            int16_t exit_field;
            int16_t btn_diplomacy;
            int16_t btn_report;
            int16_t btn_ignoring;
            int16_t btn_war;
            int16_t who_cancel_field = 0;

            int16_t state_diplomacy = 0;
            int16_t state_report = 0;
            int16_t state_ignoring = 0;
            int16_t state_war = 0;

            int16_t display_base = Setup_Main_Fields(&exit_field, &btn_diplomacy, &state_diplomacy, &btn_report, &state_report, &btn_ignoring, &state_ignoring, &btn_war, &state_war);
            RUSS::Mox_Update();

            Setup_Race_Display_Fields(race_display_data, display_base);

            who_cancel_field = fields::Add_Hidden_Field(0, 0, 639, 479, strings::s__00551100, 0);

            fields::Assign_Auto_Function(Draw_Race_Screen, 2);
            RUSS::Mox_Update();
            Draw_Race_Screen();
            RUSS::Mox_Update();

            s_racescrn_mouse_group* active_spy_group = nullptr;
            palette::Clear_Palette_Changes(0, 255);
            RUSS::Mox_Sync_Update();
            ERIC::Fast_Fade_In();

            g_in_spy_mode = 0;
            g_in_who_mode = 0;

            int16_t active_action_field = 0;
            uint8_t net_cancel_requested = 0;
            uint8_t human_rejected = 0;

            while (true) {
                game_events::Ready_Relations_View(false);
                int16_t input = fields::Get_Input();

                if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                    if (g_in_spy_mode) {
                        Move_Spy_Group_From_Mouse(active_spy_group);
                    }
                    Update_Spy_Stuff(race_display_data);
                    break;
                }

                if (input < 0) {
                    input = -input;
                }

                if (input == 0) {
                    timer::Mark_Time();
                    Draw_Race_Screen();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(2);
                    continue;
                }

                if (input == exit_field) {
                    if (g_in_spy_mode) {
                        Move_Spy_Group_From_Mouse(active_spy_group);
                        if (active_spy_group != &agent_group) {
                            race_display_data[selected_race_idx].agent_mission_idx = old_mission_idx;
                        }
                    }
                    Update_Spy_Stuff(race_display_data);
                    relations_owner.Close();
                    ERIC::Fast_Fade_Out();
                    MOX2::Put_Auto_Function(&auto_function);
                    fields::Deactivate_Help_List();
                    COLONY::Reset_Mouse_Picture();
                    palette::Set_Palette_Changes(0, 255);
                    palette::Restore_Palette();
                    BILL::Turn_Off_Palettes(0, 255);
                    fields::Enable_Draw_All_Fields();
                    MOX::current_screen = SCREEN_MAIN;
                    return;
                }

                if (input > display_base) {
                    if (g_in_who_mode) {
                        if (input == who_cancel_field) {
                            break;
                        }

                        int16_t race_display_idx = input - display_base - 1;
                        selected_race_idx = race_display_idx;
                        int16_t target_player_idx = race_display_data[race_display_idx].player_idx;

                        if (active_action_field == btn_ignoring) {
                            MOX::cur_plyr_ptr->ignoring ^= (uint8_t)(1 << (target_player_idx & 0x1F));
                            break;
                        } else if (active_action_field == btn_report) {
                            JIM::Bill_Picture_To_Mouse(g_mouse_seg);
                            ERIC::Fast_Fade_Out();
                            Update_Spy_Stuff(race_display_data);
                            RACERPRT::Race_Report_Screen(target_player_idx);
                            RUSS::Mox_Update();
                            break;
                        } else if (active_action_field == btn_war) {
                            Update_Spy_Stuff(race_display_data);
                            if (MOX::cur_plyr_ptr->treaty[target_player_idx] >= 4) {
                                break;
                            }
                            strlcpy(message_buffer, declare_war_buffer, sizeof(message_buffer));
                            JIM::Decode_Text_Race(message_buffer, target_player_idx, sizeof(message_buffer));
                            if (GENDRAW::Do_Confirmation_Box(message_buffer) == 1) {
                                MOX::player[MOX::PLAYER_NUM].delayed_diplomacy_orders[target_player_idx] = 0;
                            }
                            break;
                        } else if (active_action_field == btn_diplomacy) {
                            Update_Spy_Stuff(race_display_data);
                            MOX::cur_plyr_ptr->ignoring &= (uint8_t) ~(1 << (target_player_idx & 0x1F));

                            if (MOX::game_type != 2 && MOX::game_type != 3) {
                                DIP_SCRN::Diplomacy_Screen(target_player_idx);
                                break;
                            }

                            if (MOX::player[target_player_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                                net_state = 1;
                                net_cancel_requested = 0;
                                NETMOX::Net_Request_AI_Diplomacy((uint8_t)target_player_idx);
                                if (NETMOX::Net_May_Begin_AI_Diplomacy() == 0) {
                                    if (net_cancel_requested == 0) {
                                        if (NETMOX::No_AI_Diplomacy_Allowed() == 0) {
                                            while (true) {
                                                timer::Mark_Time();
                                                fields::Clear_Fields();
                                                fields::Add_Hidden_Field(0, 0, 639, 479, strings::s__00551100, 0);
                                                if (fields::Get_Input() != 0) {
                                                    net_cancel_requested = 1;
                                                }
                                                Draw_Race_Screen();
                                                RUSS::Mox_Sync_Update();
                                                RUSS::Mox_Update();
                                                timer::Release_Time(2);
                                                if (NETMOX::Net_May_Begin_AI_Diplomacy() != 0) break;
                                                if (net_cancel_requested != 0) break;
                                                if (NETMOX::No_AI_Diplomacy_Allowed() != 0) break;
                                            }
                                        }
                                    }
                                }
                                net_state = 0;
                                if (net_cancel_requested == 0) {
                                    if (NETMOX::No_AI_Diplomacy_Allowed() != 0) {
                                        JIM::Get_Text_Message("billtex2.lbx", 27, message_buffer, 80);
                                        GENDRAW::Message_Box(message_buffer);
                                    } else {
                                        DIP_SCRN::Diplomacy_Screen(target_player_idx);
                                        NETMOX::Net_End_AI_Diplomacy();
                                    }
                                } else {
                                    NETMOX::Net_Cancel_AI_Diplomacy_Request();
                                    while (NETMOX::Net_Cancel_AI_Diplomacy_Confirmed() == 0) {
                                        RUSS::Mox_Update();
                                    }
                                }
                                break;
                            } else {
                                human_rejected = 0;
                                net_cancel_requested = 0;
                                net_state = 1;
                                NETMOX::Net_Request_Human_Diplomacy((uint8_t)target_player_idx);
                                if (NETMOX::Net_May_Begin_Human_Diplomacy() == 0) {
                                    if (net_cancel_requested == 0) {
                                        if (NETMOX::Human_Hit_Next_Turn(target_player_idx) == 0) {
                                            while (true) {
                                                timer::Mark_Time();
                                                fields::Clear_Fields();
                                                fields::Add_Hidden_Field(0, 0, 639, 479, strings::s__00551100, 0);
                                                if (fields::Get_Input() != 0) {
                                                    if (NETMOX::Allowed_To_Cancel_Human_Diplomacy() != 0) {
                                                        net_cancel_requested = 1;
                                                    }
                                                }
                                                if (NETMOX::Human_Diplomat_Says_Wait() != 0) {
                                                    net_state = 2;
                                                }
                                                if (NETMOX::Net_Human_Rejected_Diplomacy() != 0) {
                                                    human_rejected = 1;
                                                }
                                                Draw_Race_Screen();
                                                RUSS::Mox_Sync_Update();
                                                RUSS::Mox_Update();
                                                timer::Release_Time(2);
                                                if (human_rejected != 0) break;
                                                if (NETMOX::Net_May_Begin_Human_Diplomacy() != 0) break;
                                                if (net_cancel_requested != 0) break;
                                                if (NETMOX::Human_Hit_Next_Turn(target_player_idx) != 0) break;
                                            }
                                        }
                                    }
                                }
                                net_state = 0;
                                if (NETMOX::Human_Hit_Next_Turn(target_player_idx) != 0) {
                                    JIM::Get_Text_Message("billtext.lbx", 55, message_buffer, 80);
                                    GENDRAW::Message_Box(message_buffer);
                                    NETMOX::Net_End_Human_Diplomacy();
                                } else {
                                    if (human_rejected != 0) {
                                        JIM::Get_Text_Message("billtext.lbx", 56, message_buffer, 80);
                                        GENDRAW::Message_Box(message_buffer);
                                        NETMOX::Net_End_Human_Diplomacy();
                                    } else {
                                        if (NETMOX::Net_May_Begin_Human_Diplomacy() != 0) {
                                            DIP_SCRN::Diplomacy_Screen(target_player_idx);
                                            NETMOX::Net_End_Human_Diplomacy();
                                        } else {
                                            NETMOX::Net_Cancel_Human_Diplomacy_Request();
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    } else {
                        state_diplomacy = 0;
                        state_report = 0;
                        state_ignoring = 0;
                        state_war = 0;

                        int16_t hover_race_idx = 0;
                        s_racescrn_mouse_group* spy_group = Get_Spy_Group_For_Field(input, &hover_race_idx);

                        if (g_in_spy_mode) {
                            if (spy_group == nullptr) {
                                spy_group = active_spy_group;
                                if (active_spy_group != &agent_group) {
                                    race_display_data[selected_race_idx].agent_mission_idx = old_mission_idx;
                                }
                            }
                            Move_Spy_Group_From_Mouse(spy_group);
                            if (g_mouse_group_p->count == 0) {
                                g_in_spy_mode = 0;
                                active_spy_group = nullptr;
                            }
                        } else {
                            if (spy_group != nullptr && spy_group->count > 0) {
                                selected_race_idx = hover_race_idx;
                                Move_Spies_Group(spy_group, arrow_seg);
                                g_in_spy_mode = 1;
                                old_mission_idx = race_display_data[selected_race_idx].agent_mission_idx;
                                active_spy_group = spy_group;
                            }
                        }
                        Adjust_Spy_Mission_Data();
                    }
                } else {
                    if (g_in_spy_mode) {
                        Move_Spy_Group_From_Mouse(active_spy_group);
                        if (active_spy_group != &agent_group) {
                            race_display_data[selected_race_idx].agent_mission_idx = old_mission_idx;
                        }
                        if (g_mouse_group_p->count == 0) {
                            g_in_spy_mode = 0;
                        }
                    }

                    if (g_in_who_mode) {
                        JIM::Bill_Picture_To_Mouse(g_mouse_seg);
                        state_diplomacy = 0;
                        state_report = 0;
                        state_ignoring = 0;
                        state_war = 0;
                        Setup_Race_Display_Fields(race_display_data, display_base);
                        who_cancel_field = fields::Add_Hidden_Field(0, 0, 639, 479, strings::s__00551100, 0);
                        g_in_who_mode = 0;
                    } else {
                        if (input == btn_ignoring) {
                            state_diplomacy = 0;
                            state_report = 0;
                            state_war = 0;
                        } else if (input == btn_report) {
                            state_diplomacy = 0;
                            state_ignoring = 0;
                            state_war = 0;
                        } else if (input == btn_diplomacy) {
                            state_report = 0;
                            state_ignoring = 0;
                            state_war = 0;
                        } else if (input == btn_war) {
                            state_report = 0;
                            state_ignoring = 0;
                            state_diplomacy = 0;
                        }

                        active_action_field = input;
                        fields::Enable_Draw_All_Fields();
                        fields::Draw_Visible_Fields();
                        fields::Disable_Draw_All_Fields();
                        fields::Draw_All_Fields();
                        video::Copy_Off_To_Back();

                        Setup_Race_Who_Fields(race_display_data, display_base);
                        who_cancel_field = fields::Add_Hidden_Field(0, 0, 639, 479, strings::s__00551100, 0);
                        COLONY::Picture_To_Mouse(mouse_pic);
                        g_in_who_mode = 1;
                    }
                }
            }
        }
    }
} // namespace RACESCRN