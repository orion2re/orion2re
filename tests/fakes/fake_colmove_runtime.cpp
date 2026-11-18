#include "pch.h"

namespace {
    s_colony colonies[MAX_COLONIES];
    char help_message[] = "help";
    char planet_name[] = "Planet";
}

namespace TEST_COLMOVE {
    int16_t help_call_count;
    int16_t settle_call_count;

    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        help_call_count = 0;
        settle_call_count = 0;
        COLMOVE::cluster_colony_n = -1;
        COLMOVE::native_population_transfer = false;
    }
}

namespace MOX {
    s_colony* colony = colonies;
    s_settings settings;
}

namespace COLONY {
    uint8_t full_draw;

    int16_t __cdecl Pop_To_Pop_State(int16_t, int16_t) {
        return 0;
    }

    int16_t __cdecl Sum_Colonists(
            int16_t colony_idx,
            int16_t job,
            int16_t,
            int16_t,
            int16_t,
            int16_t) {
        int16_t count = 0;
        const s_colony& colony = MOX::colony[colony_idx];

        for (int16_t i = 0; i < colony.n_pops; ++i) {
            const uint32_t pop = colony.pop[i];
            if (POP::Is_Assigned(pop) && (int16_t)POP::Get_Prof(pop) == job) {
                ++count;
            }
        }

        return count;
    }

    s_animation_header* __cdecl C_Anims(::game::s_world_state* world_state, int16_t) {
        return nullptr;
    }

    s_animation_header* __cdecl Colony_Pop_Anim(int16_t, int16_t) {
        return nullptr;
    }

    char* __cdecl Pop_State_String(int16_t) {
        return help_message;
    }

    char* __cdecl Conquered_String(bool) {
        return help_message;
    }

    char* __cdecl Pop_Race_String(int16_t, int16_t) {
        return help_message;
    }

    char* __cdecl Job_String(int16_t) {
        return help_message;
    }

    void __cdecl Reset_Mouse_Picture() {
    }

    void __cdecl Clear_Mouse_Picture() {
    }

    void __cdecl Col_Calc_Wrapper(int16_t) {
    }

    void __cdecl Reset_Field_Mode() {
    }
}

namespace ESTRINGS {
    char* __cdecl E_Strings(int16_t) {
        return help_message;
    }
}

namespace GENDRAW {
    void __cdecl Help(const char*) {
        ++TEST_COLMOVE::help_call_count;
    }

    int16_t __cdecl Do_Confirmation_Box(const char*) {
        return 1;
    }
}

namespace ERIC {
    void __cdecl String_Builder2(char*, int32_t, int16_t, const char*, ...) {
    }
}

namespace HAROLD {
    int8_t __cdecl Was_Mouse_List_Saved() {
        return 0;
    }
}

namespace COLCALC {
    void __cdecl Colony_Calculation(int16_t) {
    }
}

namespace HACCESS {
    char* __cdecl Get_Planet_Name(game::id_planet) {
        return planet_name;
    }
}

namespace SETTLER {
    void __cdecl Pop_Tries_To_Settle(::game::s_world_state* world_state, 
            s_colony*,
            int16_t,
            s_colony*,
            s_settler_settle_result* result) {
        memset(result, 0, sizeof(*result));
        result->can_settle = 1;
    }

    void __cdecl Settle_Pop(::game::s_world_state* world_state, s_colony* source_colony, int16_t pop_idx, s_colony* destination_colony) {
        destination_colony->pop[destination_colony->n_pops] = source_colony->pop[pop_idx];
        ++destination_colony->n_pops;

        for (int16_t i = pop_idx; i + 1 < source_colony->n_pops; ++i) {
            source_colony->pop[i] = source_colony->pop[i + 1];
        }

        --source_colony->n_pops;
        ++TEST_COLMOVE::settle_call_count;
    }
}

namespace TEXTBOX {
    void __cdecl Text_Box(const char*, const char*) {
    }
}