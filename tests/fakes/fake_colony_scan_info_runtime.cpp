#include "pch.h"

#include <cstdarg>

namespace {
    s_colony colonies[MAX_COLONIES];
    s_planet_data planets[MAX_PLANETS];
    s_player players[MAX_PLAYERS];
    char output[1000];
    int32_t product_cost;
    int32_t breakthrough_chance;
    bool capture_empire_info;

    char size_medium[] = "Medium";
    char climate_terran[] = "Terran";
    char gravity_normal[] = "Normal G";
    char mineral_rich[] = "Rich";
    char classic_format[] =
        "%s%s %s \n%sravity\nMineral %s\nPopulation (%d/%d)\n%s%+dk %s";
    char bc_format[] = "%sBC: %s%d";
    char income_format[] = "%sIncome: %s%s%d";
    char population_format[] = "%sPopulation: %s%d";
    char freighters_format[] = "%sFreighters: %s%d";
    char food_format[] = "%sFood: %s%d";
    char research_format[] = "%d";
    char research_percentage_format[] = "%d%%";
    char empire_research_format[] = "%sResearch: %s%d";
    char empire_research_percentage_format[] = "%sResearch: %s%d%%";
    char unpatched_population_format[] = "l:\\temp\\russcolony.h";
}

namespace TEST_COLONY_SCAN_INFO {
    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(players, 0, sizeof(players));
        memset(output, 0, sizeof(output));
        product_cost = 0;
        breakthrough_chance = 0;
        capture_empire_info = false;

        COLONY::g_colony_n = 0;
        COLSUM::print_research_value = COLSUM::RESEARCH_VALUE_CLASSIC;
        MOX::PLAYER_NUM = 0;
        MOX::settings.language = 0;
    }

    const char* Output_() {
        return output;
    }

    void Set_Product_Cost_(int32_t value) {
        product_cost = value;
    }

    void Set_Breakthrough_Chance_(int32_t value) {
        breakthrough_chance = value;
    }

    void Capture_Empire_Info_() {
        capture_empire_info = true;
    }
}

namespace MOX {
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
    s_player* player = players;
    s_ship_data* ship;
    int16_t PLAYER_NUM;
    s_settings settings;

    char* planet_size_string[PLANET_SIZE_COUNT] = {
        nullptr, nullptr, size_medium, nullptr, nullptr,
    };
    char* planet_climate_string[PLANET_CLIMATE_COUNT] = {
        nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, climate_terran, nullptr,
    };
    char* planet_gravity_string[PLANET_GRAVITY_COUNT] = {
        nullptr, gravity_normal, nullptr,
    };
    char* mineral_class_string[MINERAL_RESOURCE_COUNT] = {
        nullptr, nullptr, nullptr, mineral_rich, nullptr,
    };
}

namespace COLONY {
    int16_t g_colony_n;

    int16_t __cdecl Calculate_Current_Production_Turn_Count(int16_t) {
        return 0;
    }

    void __cdecl Set_Colony_Font_To(int32_t) {
    }

    void __cdecl Set_Colony_Font() {
    }

    int16_t __cdecl Sum_Colonists(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {
        return 0;
    }
}

namespace COLXPORT {
    int16_t* g_colony_list_ptr;
}

namespace HACCESS {
    char* __cdecl Get_Planet_Name_No_Prime(int16_t) {
        return size_medium;
    }
}

namespace COLBLDG {
    char* __cdecl Selection_Name(int16_t) {
        return size_medium;
    }
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT];
}

namespace COLDRAW {
    void __cdecl Draw_Colony_Wee_Prod(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {
    }

    void __cdecl Draw_Info_Wee_Morale(int16_t, int16_t, int16_t, int32_t) {
    }
}

namespace COLCALC {
    int __cdecl Player_Research_Cost(s_player*, int16_t) {
        return product_cost;
    }

    int32_t __cdecl Chance_For_Research_Breakthrough(s_player*) {
        return breakthrough_chance;
    }

    int16_t __cdecl Planet_Max_Population_For_Player(s_planet_data*, int16_t) {
        return 20;
    }

    int32_t __cdecl Colony_Producing_Product_Cost(s_colony*) {
        return product_cost;
    }
}

namespace ESTRINGS {
    const char s_0_0055110c[4] = "";
    const char s_1_00551110[4] = "";

    char* __cdecl E_Strings(int16_t string_id) {
        switch (string_id) {
            case 0x66: return food_format;
            case 0x67: return freighters_format;
            case 0x6A: return income_format;
            case 0x72: return population_format;
            case 0x75: return capture_empire_info ? empire_research_format : research_format;
            case 0x76: return bc_format;
            case 0x7E: return capture_empire_info ? empire_research_percentage_format : research_percentage_format;
            case 358:
            case 359:
                return unpatched_population_format;
            default:
                return classic_format;
        }
    }
}

namespace ERIC {
    const char* __cdecl Red_If_Negative_Fmt_String(int16_t) {
        return "";
    }

    void __cdecl String_Builder2(char* dest, int32_t dest_size,
                                  int16_t build_mode, const char* format, ...) {
        if (!capture_empire_info && build_mode != 5) {
            return;
        }

        va_list args;
        va_start(args, format);
        if (capture_empire_info) {
            char part[500];
            vsnprintf(part, sizeof(part), format, args);
            if (build_mode == 0) {
                snprintf(dest, dest_size, "%s", part);
            } else {
                int32_t used = (int32_t)strlen(dest);
                if (used < dest_size) {
                    snprintf(dest + used, dest_size - used, "\n%s", part);
                }
            }
        } else {
            va_arg(args, const char*);
            va_arg(args, const char*);
            int32_t value = va_arg(args, int32_t);
            snprintf(dest, dest_size, format, value);
        }
        va_end(args);
    }
}

namespace FMTPARA {
    const char* __cdecl Print_Formatted_Paragraph(int16_t, int16_t, int16_t,
                                                   const char* text, int16_t) {
        snprintf(output, sizeof(output), "%s", text);
        return text;
    }
}

namespace BILL {
    void __cdecl Squeeze_Print_Formatted_Paragraph(int16_t, int16_t, int16_t, int16_t, char* text, int16_t) {
        snprintf(output, sizeof(output), "%s", text);
    }
}