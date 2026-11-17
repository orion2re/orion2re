#include "pch.h"

#include "lua/lua_game.h"

namespace {
    s_player players[1];
    uint8_t race_specials[31];
    char race_name[] = "Test";
    char player_name[] = "Player";
    int technology_setup_order;
    int player_technology_order;
    int player_setup_order;
    int ship_design_order;
    int next_order;
}

namespace TEST_NEWGAME {

    void Reset_Player_Setup_Order_() {
        technology_setup_order = 0;
        player_technology_order = 0;
        player_setup_order = 0;
        ship_design_order = 0;
        next_order = 1;
    }

    int Technology_Setup_Order_() {
        return technology_setup_order;
    }

    int Player_Technology_Order_() {
        return player_technology_order;
    }

    int Player_Setup_Order_() {
        return player_setup_order;
    }

    int Ship_Design_Order_() {
        return ship_design_order;
    }

} // namespace TEST_NEWGAME

namespace MOX {
    int16_t NUM_PLAYERS;
    int16_t PLAYER_NUM;
    int32_t stardate;
    s_settings settings = {};
    s_player* player = players;
    char* race_names[20] = {race_name};
    s_animation_header* screen_seg;
}

namespace struct_ {
    void __cdecl Copy_Structure(void* dest, void* src, uint32_t size) {
        memcpy(dest, src, size);
    }
}

namespace RUSS {
    void __cdecl Mox_Update() {}
}

namespace RACESEL {
    uint8_t* race_specials[14] = {::race_specials};
}

namespace RACECUST {
    void __cdecl Race_Customization(int16_t) {}
}

namespace allocate {
    void __cdecl Push_Block(void*) {}

    void __cdecl Pop_Block(void*) {}
}

namespace farload {
    void* __cdecl Far_Reload_Next_Data(const char*, int32_t, void*, uint32_t,
                                        uint16_t, uint16_t) {
        return player_name;
    }
}

namespace KEN {
    void __cdecl Init_NPC_Personalities_Objectives_Themes() {}
}

namespace TECHINIT {
    bool Finalize_Configured_New_Game_Technology(std::string*) {
        technology_setup_order = next_order++;
        return true;
    }

    void __cdecl Init_Player_Tech(int16_t) {
        player_technology_order = next_order++;
    }
}

namespace INITSHIP {
    void __cdecl Init_Ship_Designs(int16_t) {
        ship_design_order = next_order++;
    }

    int16_t __cdecl Best_Ship_Fuel(int16_t) {
        return 0;
    }
}

namespace TECHDATA {
    s_tech_fuel_data fuel[FUEL_COUNT] = {};
}

namespace raceopt {
    s_pick_rules pick_rules = {};
}

namespace HOMEGEN {
    void __cdecl Assign_Home_Star_Names(::game::s_world_state* world_state) {}
}

namespace diplomac {
    void Init_Diplomatic_Relations() {}
}

namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char*) {
        std::abort();
    }
} // namespace exitm

namespace orion2re::lua::game {

    void Reset_Semantic_State() {}

    void Player_Setup(const s_player_setup_event*) {
        player_setup_order = next_order++;
    }

} // namespace orion2re::lua::game