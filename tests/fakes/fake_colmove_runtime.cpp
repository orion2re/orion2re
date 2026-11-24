#include "pch.h"
#include "../fixtures/colony_roster.h"

namespace {
    test_colonies::Slots<MAX_COLONIES> colonies;
    char help_message[] = "help";
    char planet_name[] = "Planet";
}

namespace TEST_COLMOVE {
    int16_t help_call_count;
    int16_t settle_call_count;

    void Reset_() {
        colonies.Reset();
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        help_call_count = 0;
        settle_call_count = 0;
        COLMOVE::cluster_colony_n = game::NO_COLONY;
        COLMOVE::native_population_transfer = false;
    }
}

namespace MOX {
    s_settings settings;
}

namespace COLONY {
    uint8_t full_draw;

    int16_t __cdecl Pop_To_Pop_State(const s_colony_pop&) {
        return 0;
    }

    int16_t __cdecl Sum_Colonists(
            game::id_colony colony_idx,
            int16_t job,
            int16_t,
            int16_t,
            int16_t,
            int16_t) {
        int16_t count = 0;
        const s_colony& colony = test_colonies::Record(colony_idx.id);

        for (int16_t i = 0; i < colony.n_pops; ++i) {
            const auto pop = colony.pop[i];
            if (POP::Is_Assigned(pop) && (int16_t)POP::Get_Prof(pop) == job) {
                ++count;
            }
        }

        return count;
    }

    s_animation_header* __cdecl C_Anims(int16_t) {
        return nullptr;
    }

    s_animation_header* __cdecl Colony_Pop_Anim(const s_colony&, int16_t) {
        return nullptr;
    }

    char* __cdecl Pop_State_String(int16_t) {
        return help_message;
    }

    char* __cdecl Conquered_String(bool) {
        return help_message;
    }

    char* __cdecl Pop_Race_String(game::id_colony, int16_t) {
        return help_message;
    }

    char* __cdecl Job_String(int16_t) {
        return help_message;
    }

    void __cdecl Reset_Mouse_Picture() {
    }

    void __cdecl Clear_Mouse_Picture() {
    }

    void __cdecl Col_Calc_Wrapper(game::id_colony) {
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
    void __cdecl Colony_Calculation(game::id_colony) {
    }
}

namespace HACCESS {
    char* Get_Planet_Name(const game::s_game_state&, game::id_planet) {
        return planet_name;
    }
}

namespace SETTLER {
    void __cdecl Pop_Tries_To_Settle(
            const game::s_game_state&,
            game::id_colony,
            int16_t,
            game::id_colony,
            s_settler_settle_result* result) {
        memset(result, 0, sizeof(*result));
        result->can_settle = 1;
    }

    void __cdecl Settle_Pop(game::id_colony source_id,
        int16_t pop_idx, game::id_colony destination_id) {
        auto* source_colony = &test_colonies::Record(source_id.id);
        auto* destination_colony = &test_colonies::Record(destination_id.id);
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
