#include "pch.h"
#include "game_events.h"
#include "colony_read.h"

namespace COLSYSDI {
    s_animation_header* __cdecl Planet_Anims(int16_t climate, int16_t planet_size) {
        return (s_animation_header*)buffer::Buffer_Reload(COLONY::COLSYSDI_LBX, climate * PLANET_SIZE_COUNT + planet_size + 11, MOX::global_cache_seg);
    }

    void __cdecl Draw_Col_Sys_Disp(int16_t x, int16_t y) {
        int16_t text_y = y + 7;
        int16_t star_idx = COLONY::sys_disp_star_handle;

        for (int16_t i = 0; i < 5; i++) {
            int16_t planet_idx = MOX::star[star_idx].planet_index[i];

            if (planet_idx != -1) {
                s_planet_data* planet = &MOX::planet[planet_idx];
                uint8_t climate;

                if (planet->colony_index == -1) {
                    climate = planet->climate;
                } else {
                    climate = MOX::colony[planet->colony_index].climate;
                }

                if (planet->planet_type == PLANET_TYPE_PLANET) {
                    s_animation_header* anim = Planet_Anims(climate, planet->size);
                    ERIC::Draw_Centered(x + 15, y + i * 24 + 14, anim);
                } else if (planet->planet_type == PLANET_TYPE_GAS_GIANT) {
                    s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLSYSDI_LBX, 0x3E, MOX::global_cache_seg);
                    ERIC::Draw_Centered(x + 15, y + i * 24 + 14, anim);
                } else if (planet->planet_type == PLANET_TYPE_ASTEROID) {
                    s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLSYSDI_LBX, 0x3F, MOX::global_cache_seg);
                    ERIC::Draw_Centered(x + 15, y + i * 24 + 14, anim);
                }

                COLONY::Set_Colony_Font_To(1);

                char summary_text[500];
                Planet_Summary_String(summary_text, sizeof(summary_text), planet_idx, 1);

                BILL::_Print_Paragraph(x + 28, text_y, 89 - x, summary_text, 0);
                game_events::Observe_Settlement_Text(planet_idx, "colony-system-summary", summary_text, false, planet->planet_type == PLANET_TYPE_PLANET && planet->colony_index >= 0 && colony_read::Strategic_Owner_Disclosed(ESTRINGS::E_Strings(0x29), 1, sizeof(summary_text)));
            }

            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLSYSDI_LBX, 0x41, MOX::global_cache_seg);
            ERIC::Draw_Centered(x + 2, y + i * 24 + 12, anim);

            text_y += 24;
        }
    }

    void __cdecl Draw_Col_Sys_Disp_Last_Scanned(int16_t x, int16_t y) {
        s_colony* colony = &MOX::colony[COLONY::colony_handle];

        if (colony->planet != -1) {
            s_planet_data* planet = &MOX::planet[colony->planet];
            uint8_t climate;

            if (planet->colony_index == -1) {
                climate = planet->climate;
            } else {
                climate = MOX::colony[planet->colony_index].climate;
            }

            int16_t display_row = 0;
            if (planet->orbit >= 0) {
                int16_t star_idx = planet->star_index;
                do {
                    volatile int16_t unused_planet_idx = MOX::star[star_idx].planet_index[display_row];
                    (void)unused_planet_idx;
                    display_row++;
                } while (display_row <= planet->orbit);
            }

            s_animation_header* anim = Planet_Anims(climate, planet->size);
            animate::Get_Height(anim);
            animate::Get_Width(anim);

            int16_t frame = ERIC::Global_Cycler(50, 6);

            s_animation_header* marker_anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLSYSDI_LBX, 0x40, MOX::global_cache_seg);
            animate::Set_Animation_Frame(marker_anim, (int32_t)frame);

            marker_anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::COLSYSDI_LBX, 0x40, MOX::global_cache_seg);
            ERIC::Draw_Centered(x + 2, (int16_t)(y + display_row * 24 - 12), marker_anim);
        }
    }

    void __cdecl Planet_Summary_String(char* summary_text, size_t summary_text_size, int16_t planet_idx, int16_t font_player_idx) {
        char* gravity_str[PLANET_GRAVITY_COUNT];
        char* mineral_str[MINERAL_RESOURCE_COUNT];
        char* size_str[PLANET_SIZE_COUNT];
        char* gas_giant_str;
        char* asteroid_str;
        char pop_str[500];

        if (summary_text == nullptr || summary_text_size == 0) {
            return;
        }

        summary_text[0] = '\0';

        s_planet_data* planet = &MOX::planet[planet_idx];
        int16_t colony_idx = planet->colony_index;

        gravity_str[PLANET_GRAVITY_LOW_G] = ESTRINGS::E_Strings(0x169);
        gravity_str[PLANET_GRAVITY_NORMAL_G] = ESTRINGS::E_Strings(0xC);
        gravity_str[PLANET_GRAVITY_HEAVY_G] = ESTRINGS::E_Strings(0x25B);

        size_str[PLANET_SIZE_TINY] = ESTRINGS::E_Strings(0x1F6);
        size_str[PLANET_SIZE_SMALL] = ESTRINGS::E_Strings(0x25C);
        size_str[PLANET_SIZE_MEDIUM] = ESTRINGS::E_Strings(0x25D);
        size_str[PLANET_SIZE_LARGE] = ESTRINGS::E_Strings(0x25E);
        size_str[PLANET_SIZE_HUGE] = ESTRINGS::E_Strings(0x25F);

        mineral_str[MINERAL_RESOURCE_ULTRA_POOR] = ESTRINGS::E_Strings(0x222);
        mineral_str[MINERAL_RESOURCE_POOR] = ESTRINGS::E_Strings(0x1A7);
        mineral_str[MINERAL_RESOURCE_ABUNDANT] = ESTRINGS::E_Strings(0xC);
        mineral_str[MINERAL_RESOURCE_RICH] = ESTRINGS::E_Strings(0x1C2);
        mineral_str[MINERAL_RESOURCE_ULTRA_RICH] = ESTRINGS::E_Strings(0x223);

        ESTRINGS::E_Strings(0x1B9);
        ESTRINGS::E_Strings(0x14E);
        ESTRINGS::E_Strings(0x21B);
        ESTRINGS::E_Strings(0xC2);
        ESTRINGS::E_Strings(0x106);
        ESTRINGS::E_Strings(0x21F);
        ESTRINGS::E_Strings(0x18F);
        ESTRINGS::E_Strings(0x1F5);
        ESTRINGS::E_Strings(0xB8);
        ESTRINGS::E_Strings(0x1FC);
        ESTRINGS::E_Strings(0x12F);

        gas_giant_str = ESTRINGS::E_Strings(0x132);
        asteroid_str = ESTRINGS::E_Strings(0xBA);

        uint8_t climate;
        if (colony_idx == -1) {
            climate = planet->climate;
        } else {
            climate = MOX::colony[colony_idx].climate;
        }

        if (planet->planet_type != PLANET_TYPE_PLANET) {
            if (planet->planet_type == PLANET_TYPE_GAS_GIANT) {
                snprintf(summary_text, summary_text_size, ESTRINGS::E_Strings(0x46), gas_giant_str);
            }
            if (planet->planet_type == PLANET_TYPE_ASTEROID) {
                snprintf(summary_text, summary_text_size, ESTRINGS::E_Strings(0x46), asteroid_str);
            }
            return;
        }

        if (colony_idx == -1) {
            strlcpy(pop_str, ESTRINGS::E_Strings(0xC), sizeof(pop_str));
        } else {
            if (font_player_idx != -1) {
                COLONY::Set_Colony_Font_To_Player((uint8_t)font_player_idx, MOX::colony[colony_idx].owner);
            }
            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(planet, MOX::colony[colony_idx].owner);
            snprintf(pop_str, sizeof(pop_str), ESTRINGS::E_Strings(0x8), (int)MOX::colony[colony_idx].n_pops, (int)max_pop);
        }

        if (colony_idx == -1) {
            snprintf(summary_text, summary_text_size, ESTRINGS::E_Strings(0x2C), size_str[(int)planet->size], MOX::planet_climate_string[(int)climate], gravity_str[(int)planet->gravity_class], mineral_str[(int)planet->mineral_class], pop_str);
        } else {
            snprintf(summary_text, summary_text_size, ESTRINGS::E_Strings(0x29), MOX::player[(int)MOX::colony[colony_idx].owner].race_name, gravity_str[(int)planet->gravity_class], mineral_str[(int)planet->mineral_class], pop_str);
        }
    }

    void __cdecl Add_Col_Sys_Disp_Fields(int16_t x, int16_t y, int16_t end_x) {
        int16_t planet_idx;
        int16_t height;
        int16_t width;
        int16_t star_handle;
        int16_t i;
        s_animation_header* anim;
        char* help_str;
        int16_t y1;
        int16_t y2;
        int16_t x1;
        int16_t x2;
        int16_t display_idx = 0;

        star_handle = COLONY::sys_disp_star_handle;

        for (i = 0; i < 5; i++) {
            planet_idx = MOX::star[star_handle].planet_index[i];

            if (planet_idx == -1) {
                COLONY::sys_disp_planet_fields[i] = -1000;
                display_idx++;
            } else {
                anim = Planet_Anims(MOX::planet[planet_idx].climate, MOX::planet[planet_idx].size);

                height = animate::Get_Height(anim);
                width = animate::Get_Width(anim);

                height /= 2;
                y2 = y + height + 14 + display_idx * 24;

                width /= 2;
                y1 = y - height + 14 + display_idx * 24;

                help_str = ESTRINGS::E_Strings(12);
                x1 = x - width + 15;
                x2 = x + width + 15;
                COLONY::sys_disp_planet_fields[i] = fields::Add_Hidden_Field(x1, y1, x2, y2, help_str, 0);

                help_str = ESTRINGS::E_Strings(12);
                x1 = x + width + 17;
                x2 = end_x - 2;
                COLONY::sys_disp_summ_fields[i] = fields::Add_Hidden_Field(x1, y1, x2, y2, help_str, 0);

                display_idx++;
            }
        }
    }
} // namespace COLSYSDI
