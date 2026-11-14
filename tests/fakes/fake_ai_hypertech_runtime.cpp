#include "pch.h"

namespace {
    s_player players[MAX_PLAYERS];
    int16_t selected_tech_application;
    int32_t hypertech_choices[MAX_PLAYERS];
    int32_t hypertech_choice_count;
    int32_t hypertech_choice_index;
}

namespace TEST_AI_HYPERTECH {
    void Reset_() {
        memset(players, 0, sizeof(players));
        memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
        memset(aitech::rules.hypertech_threshold, 0, sizeof(aitech::rules.hypertech_threshold));
        selected_tech_application = TECH_APP_NO_TECH;
        hypertech_choice_count = 0;
        hypertech_choice_index = 0;
    }

    s_player* Player_() {
        return &players[0];
    }

    void Set_Selected_Tech_Application_(int16_t tech_application) {
        selected_tech_application = tech_application;
    }

    void Set_Hypertech_Choices_(const int32_t* choices, int32_t count) {
        memcpy(hypertech_choices, choices, (size_t)count * sizeof(hypertech_choices[0]));
        hypertech_choice_count = count;
        hypertech_choice_index = 0;
    }

    int32_t Hypertech_Choice_Calls_() {
        return hypertech_choice_index;
    }
}

namespace MOX {
    s_player* player = players;
}

namespace aitech {
    s_rules rules;
}

namespace TECHDATA {
    s_tech_app_data technology_applications[TECH_APP_COUNT] = {};
}

namespace newtech {
    int16_t __cdecl Choose_Tech_Application(int16_t) {
        return selected_tech_application;
    }

    int32_t __cdecl Choose_Hyper_Advanced_Tech() {
        if (hypertech_choice_index >= hypertech_choice_count) {
            return TECH_APP_NO_TECH;
        }

        return hypertech_choices[hypertech_choice_index++];
    }
}