#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

namespace {
    test_stars::Slots<MAX_STARS> stars;

    test_colonies::Slots<MAX_COLONIES> colonies;
    auto& players = game::current_state.players;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        return rows;
    }();
    char event_message[300];
    uint32_t random_result;
    uint32_t last_weights[MAX_PLAYERS];
}

namespace TEST_EVENTS {
    int background_music_calls;
    int animation_reset_calls;
    int release_calls;
    void* released_block;
    MOX2::action_func_t restored_auto_function;
    void* animation_buffer;
    int derelict_calls;
    int wormhole_calls;
    bool wormhole_success;
    bool wormhole_to_antares;
    int population_calls;
    int16_t max_population;

    void Reset_() {
        stars.Reset();
        test_planets::Reset(0);
        colonies.Reset();
        players = ::player_roster::Make_Roster();
        game::current_state = game::s_game_state{};
        memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
        EVENTS::current_event = 0;
        EVENTS::event_message[0] = '\0';
        event_message[0] = '\0';

        for (s_star_data& star : stars) {
            for (int index = 0; index < 5; ++index) {
                star.orbit_planet_ids[index] = game::NO_PLANET;
            }
        }
        for (auto& [id, planet] : game::current_state.planets) {
            planet.colony_id = game::NO_COLONY;
        }

        game::current_state.player_count = 0;
        ship_slots::Resize(game::current_state, 0);
        test_stars::Resize(0);
        MOX::stardate = 35000;
        EVENTS::last_event_year = 35000;
        EVENTS::event_delay_count = 0;
        MOX::auto_turn_exit = 0;
        diplodef::COUNCIL_FLAG = 0;
        EVENTS::force_event_type = 0;
        background_music_calls = animation_reset_calls = release_calls = 0;
        derelict_calls = wormhole_calls = population_calls = 0;
        released_block = animation_buffer = nullptr;
        restored_auto_function = nullptr;
        wormhole_success = true;
        wormhole_to_antares = false;
        max_population = 20;
        MOX::settings = {};
        memset(MOX::food_per_farmer_table, 0, sizeof(MOX::food_per_farmer_table));
        EVENTS::event_roll = {512, 1000};
        EVENTS::donation_bc = {20, 100, 100};
        EVENTS::pirate_raid = {21, 29};
        EVENTS::supernova = {0};
        EVENTS::computer_virus = {50, 50};
        random_result = 1;
        memset(last_weights, 0, sizeof(last_weights));
    }

    void Set_Random_Result_(uint32_t value) {
        random_result = value;
    }

    uint32_t Last_Weight_(int index) {
        return last_weights[index];
    }

    void Set_Event_Message_(const char* message) {
        strlcpy(event_message, message, sizeof(event_message));
    }
}

namespace MOX {
    s_animation_header* screen_seg;
    int8_t game_type;
    s_mouse_list mouse_list_arrow;
    int32_t stardate;
    game::id_player local_player{0};


    uint8_t auto_turn_exit;

    char* mineral_class_string[MINERAL_RESOURCE_COUNT];
    s_settings settings = {};
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT];
}

namespace diplodef {
    int16_t COUNCIL_FLAG;
}

namespace farload {
    void* __cdecl Farload_Data_Static(const char*, int32_t, void* buffer,
                                      int32_t, int32_t, int32_t size) {
        strlcpy((char*)buffer, event_message, (size_t)size);
        return buffer;
    }
}

// Setup_Next_Event shares its message path with the graphical GNN screen.
namespace fields {
    void Deactivate_Help_List() {}
    void Enable_Draw_All_Fields() {}
    void Deactivate_Auto_Function() {}
    void Clear_Fields() {}
    void Set_Input_Delay(int16_t) {}
    void Assign_Auto_Function(MOX2::action_func_t, int16_t) {}
    int16_t Add_Hidden_Field(int16_t, int16_t, int16_t, int16_t, const char*, int8_t) {
        return 0;
    }
    int16_t Get_Input() {
        return 0;
    }
} // namespace fields

namespace graphics {
    void Reset_Window() {}
    void Fill(int16_t, int16_t, int16_t, int16_t, uint8_t) {}
} // namespace graphics

namespace video {
    void Clip_Off() {}
    void Set_Refresh_Stencil() {}
    void Set_Page_Off() {}
    void Copy_Off_To_Back() {}
} // namespace video

namespace MOX2 {
    void Get_Auto_Function(action_func_t*) {}
    void Put_Auto_Function(action_func_t* slot) {
        TEST_EVENTS::restored_auto_function = *slot;
    }
} // namespace MOX2

namespace mouse {
    void Set_Mouse_List(s_mouse_list*, int16_t) {}
} // namespace mouse

namespace JIM {
    void Play_Streaming_Music(int16_t, int16_t, int16_t) {}
    void Play_Background_Music() { ++TEST_EVENTS::background_music_calls; }
} // namespace JIM

namespace ERIC {
    void Set_Fade_Percent(int16_t) {}
    void Fast_Fade_Out() {}
} // namespace ERIC

namespace fonts {
    void Set_Outline_Color(uint16_t, uint16_t) {}
    void Set_Font_Style_Outline(int16_t, s_colors*) {}
} // namespace fonts

namespace timer {
    void Mark_Time() {}
    void Release_Time(int32_t) {}
} // namespace timer

namespace RUSS {
    void Mox_Sync_Update() {}
    void Release_All_Blocks(void* block) {
        ++TEST_EVENTS::release_calls;
        TEST_EVENTS::released_block = block;
    }
} // namespace RUSS

namespace FMTPARA {
    int16_t Get_Formatted_Paragraph_Max_Height(int16_t, const char*) {
        return 0;
    }
    const char* Print_Formatted_Paragraph(int16_t, int16_t, int16_t, const char* text, int16_t) {
        return text;
    }
} // namespace FMTPARA

namespace file_ani {
    void Draw_File_Animation_Stencil() {}
    int16_t Get_File_Animation_Frame() {
        return 0;
    }
    void Set_File_Animation_Frame(int16_t) {}
    void Set_File_Animation_Buffer(void* buffer) { TEST_EVENTS::animation_buffer = buffer; }
    void Reset_File_Animation_Buffer() {
        ++TEST_EVENTS::animation_reset_calls;
        TEST_EVENTS::animation_buffer = nullptr;
    }
    void Open_File_Animation(const char*, int16_t) {}
    void Draw_File_Animation() {}
} // namespace file_ani

namespace animate {
    void Draw(int16_t, int16_t, s_animation_header*) {}
    void Draw_Palette(s_animation_header*) {}
} // namespace animate

namespace bitmap {
    void Clear_Mouse_Image(int16_t) {}
    void Create_Blank_Bitmap(int16_t, int16_t, void*) {}
    void Draw_Picture_To_Bitmap(s_animation_header*, s_animation_header*) {}
    void Draw_Bitmap_To_Mouse(int16_t, int16_t, s_animation_header*, int16_t) {}
} // namespace bitmap

namespace palette {
    void Set_Palette_Changes(int16_t, int16_t) {}
} // namespace palette

namespace allocate {
    void* Allocate_First_Block(void*, uint32_t) {
        return nullptr;
    }
    void* Allocate_Next_Block(void*, uint32_t) {
        return nullptr;
    }
    void Mark_Block(void*) {}
    void Release_Block(void*) {}
} // namespace allocate

namespace farload {
    void* Far_Reload_Next(const char*, int32_t, void*) {
        return nullptr;
    }
} // namespace farload

namespace MAINSCR {
    void Mini_Main_Screen_Text_Box(char*, char*) {}
} // namespace MAINSCR

namespace HACCESS {
    char* Get_Planet_Name(const game::s_game_state&, game::id_planet planet) {
        static char name[16];
        snprintf(name, sizeof(name), "Planet %u", planet.id);
        return name;
    }
}

namespace TECHDATA {
    s_tech_app_data technology_applications[TECH_APP_COUNT];
    s_tech_field_data technology_fields[TECH_FIELD_COUNT];
}

namespace JIM {
    void __cdecl Get_Text_Message(const char*, int16_t, char* buffer,
                                  int16_t buffer_size) {
        if (buffer_size > 0) buffer[0] = '\0';
    }
}

namespace string {
    const char s_Newline[] = "\n";
}

namespace COLCALC {
    uint8_t keep_research_on_breakthrough;

    int16_t __cdecl Planet_Max_Population_For_Player(s_planet_data*, game::id_player) {
        ++TEST_EVENTS::population_calls;
        return TEST_EVENTS::max_population;
    }

    void __cdecl Colony_Calculation(game::id_colony) {
    }

    void __cdecl Player_Gets_Tech_App(s_player*, int16_t) {
    }

    void __cdecl Give_Player_Field(s_player*, int, char) {
    }

    void __cdecl Resolve_Research_Completion(s_player*, bool, int64_t) {
    }

    void __cdecl Compute_Total_Building_Costs(int32_t* player_costs) {
        memset(player_costs, 0, MAX_PLAYERS * sizeof(*player_costs));
    }
}

namespace BOMB {
    void __cdecl Apply_Bomb_Hits(s_combat_bomb_result*) {
    }

    void __cdecl Destroy_Colony(game::s_game_state& state, game::id_colony colony_id, int32_t) {
        auto& colony = test_colonies::Record(colony_id.id);
        const auto id = colony.planet_id;
        if (id != game::NO_PLANET) {
            state.planets.at(id).colony_id = game::NO_COLONY;
        }
        colony.owner = legacy_world::Decode_Player_Slot(-1);
        colony.planet_id = game::NO_PLANET;
    }
}

namespace COLBLDG {
    void __cdecl Delete_Prod_From_Colony_Queue(game::s_game_state&, game::id_colony, game::colony_product) {
    }
}

namespace diplomac {
    void __cdecl Change_Relations(int16_t, game::id_player, game::id_player, int16_t, int16_t, int16_t) {
    }
}

namespace OFFICER {
    void __cdecl Kill_Officer(int16_t) {
    }

    void __cdecl Move_Officer_To_Pool(int16_t index) {
        auto& leader = game::current_state.leaders.at(index);
        leader.Clear_Location();
        leader.status = 0;
        leader.eta = 0;
    }
}

namespace HACCESS {
    int16_t __cdecl Ship_Owner(int16_t row) {
        return factions::Legacy_Code(test_ships::Rows(game::current_state)[row].owner);
    }
}

namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char* message) {
        throw std::runtime_error(message);
    }
}

namespace SHIPS {
    game::id_ship Build_Nonplayer_Ship(int16_t, int16_t, uint8_t) {
        return game::NO_SHIP;
    }
}

namespace SHIPMOVE {
    void __cdecl Do_Wormhole_Event(game::id_player, s_wormhole_data* result) {
        ++TEST_EVENTS::wormhole_calls;
        *result = {};
        result->success = TEST_EVENTS::wormhole_success;
        result->ships_moved = 1;
        result->destination = game::Destination_At_Row(game::current_state,
            TEST_EVENTS::wormhole_to_antares ? game::ANTARES_POSITION : 3);
    }
    void __cdecl Do_Hyperspace_Beast(game::id_player) {
    }
}

namespace KEN {
    void __cdecl Get_Derelict_Applications(int16_t, uint16_t* apps, int16_t* count) {
        ++TEST_EVENTS::derelict_calls;
        apps[0] = TECH_APP_NUCLEAR_DRIVE;
        apps[1] = TECH_APP_NUCLEAR_BOMB;
        *count = 2;
    }
    uint16_t __cdecl Get_Weighted_Choice(uint16_t*, int16_t) {
        return 0;
    }
}

namespace strings {
    void __cdecl Swap(int32_t* lhs, int32_t* rhs) {
        const int32_t value = *lhs;
        *lhs = *rhs;
        *rhs = value;
    }

    void __cdecl Swap_Short(int16_t* lhs, int16_t* rhs) {
        const int16_t value = *lhs;
        *lhs = *rhs;
        *rhs = value;
    }
}

namespace antarans {
    s_antaran_fleet_config fleet_config = {
        .attack = {
            .roll_range = 200,
            .minimum_offensive_force_shift = 2,
            .impossible_human_weight_multiplier = 3,
            .lucky_victim_weight_divisor = 3,
        },
    };
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return random_result;
    }
}

namespace RUSS {
    int16_t __cdecl Get_Weighted_Choice_Int(uint32_t* weights, int16_t count) {
        memcpy(last_weights, weights, (size_t)count * sizeof(weights[0]));
        return 0;
    }
}
