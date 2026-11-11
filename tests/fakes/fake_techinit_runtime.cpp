#include "pch.h"

#include <string>

#include "game/arena.h"
#include "lua/lua_game.h"

namespace {
    s_player players[MAX_PLAYERS];
    std::string tech_name_block;
    arena::s_arena* string_arena;
    int reload_count;
    int resource_request_count;
    uint32_t random_count;
    int16_t granted_apps[TECHINIT::POSTWARP_TECH_COUNT];
    int grant_count;
}

namespace TEST_TECHINIT {
    void Set_Tech_Name_Block_(const std::string& value) {
        tech_name_block = value;
    }

    char* Copy_Config_String_(const char* value) {
        if (string_arena == nullptr) {
            string_arena = arena::Create();
        }
        return arena::Copy_String(string_arena, value);
    }

    int Reload_Count_() {
        return reload_count;
    }

    int Resource_Request_Count_() {
        return resource_request_count;
    }

    int Grant_Count_() {
        return grant_count;
    }

    int16_t Granted_App_(int index) {
        return granted_apps[index];
    }

    void Reset_() {
        if (string_arena != nullptr) {
            arena::Release(string_arena);
            string_arena = nullptr;
        }
        tech_name_block.clear();
        reload_count = 0;
        resource_request_count = 0;
        random_count = 0;
        grant_count = 0;
        memset(granted_apps, 0, sizeof(granted_apps));
        memset(players, 0, sizeof(players));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        MOX::NUM_PLAYERS = 1;
    }
}

namespace CONFIG_REFRESH {
    void Request_Resource_Reload() {
        ++resource_request_count;
    }
}

namespace orion2re::lua::game {
    bool Technology_Setup(const s_technology_setup_event*,
        std::string*) {
        return true;
    }
} // namespace orion2re::lua::game

namespace config {
    bool Is_Active_String(const char* value) {
        return arena::Contains(string_arena, value);
    }
}

namespace farload {
    void* __cdecl Far_Reload(const char*, int32_t, void*) {
        ++reload_count;
        return tech_name_block.data();
    }
}

namespace LOADER {
    void __cdecl Load_Player_Special_Names() {
    }
}

namespace RUSS {
    void __cdecl Mox_Update() {
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t max_val) {
        if (max_val == 0) {
            return 1;
        }
        return random_count++ % max_val + 1;
    }
}

namespace newtech {
    int16_t __cdecl Choose_Tech_Application(int16_t) {
        return 1;
    }
}

namespace INITSHIP {
    int16_t __cdecl Best_Warp_Drive(int16_t) {
        return 0;
    }

    int16_t __cdecl Calc_Player_FTL_Speed(int16_t) {
        return 0;
    }
}


namespace MOX {
    int16_t first_field_in_group[10] = {
        0, 18, 55, 57, 29, 7, 22, 28, 10, 74};
    s_player* player = players;
    s_settings settings = {};
    int16_t NUM_PLAYERS = 1;
    void* techname_seg;
}

namespace COLCALC {
    int16_t __cdecl Colony_Empire_Base_Food2_Produced(
        const s_colony*, int16_t, int16_t, s_colony_job_production*) {
        return 0;
    }

    int16_t __cdecl Colony_Empire_Base_Industry_Produced(
        const s_colony*, int16_t, int16_t, s_colony_job_production*) {
        return 0;
    }

    int16_t __cdecl Colony_Empire_Base_Research_Produced(
        const s_colony*, int16_t, int16_t, s_colony_job_production*) {
        return 0;
    }

    void __cdecl Player_Gets_Tech_App(s_player* player, int16_t tech_app_id) {
        granted_apps[grant_count++] = tech_app_id;
        player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
        player->got_new_app = 1;
    }
} // namespace COLCALC
