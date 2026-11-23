#ifndef ORION2RE_LUA_GAME_INTERNAL_H
#define ORION2RE_LUA_GAME_INTERNAL_H

#include <cstdint>

#include "lua/lua_game.h"
#include "lua/lua_storage.h"

struct lua_State;
struct s_planet_data;
namespace game { struct s_game_state; }
namespace orion2re {
    using game::id_star;
    using game::s_game_state;
    using game::id_ship;
    using game::NO_SHIP;
}

namespace config {
    struct s_generated_enum;
}

namespace orion2re::lua::game::internal {

    inline s_game_state* galaxy_planet_candidate = nullptr;

    enum class e_player_builtin_trait_result {
        unknown,
        valid,
        invalid,
    };

    struct s_destroyed_ship_record {
        id_ship ship_id = NO_SHIP;
        std::int16_t owner_id = -1;
        std::int16_t cost = 0;
        std::int16_t cause = 0;
        char name[30] = {};
    };

    struct s_ship_destroyed_record {
        s_destroyed_ship_record destroyed_ship;
        std::int16_t killer_combat_ship = -1;
        id_ship killer_ship_id = NO_SHIP;
        std::uint64_t sequence = 0;
    };

    void Create_Player_Metatables(lua_State* lua_state);
    void Push_Player_Collection(lua_State* lua_state);
    void Push_Player(lua_State* lua_state, std::int16_t player_id);
    bool Read_Player(lua_State* lua_state,
        int index,
        std::int16_t* player_id);
    e_player_builtin_trait_result Read_Player_Builtin_Trait(
        std::int16_t player_id,
        const char* trait_id,
        const config::s_generated_enum* metadata,
        bool* has_trait);
    bool Read_Player_Storage_Reference(lua_State* lua_state,
        int index,
        s_lua_storage_reference* reference);
    void Push_Stored_Player(lua_State* lua_state, int16_t player_id);
    void Create_Ship_Metatables(lua_State* lua_state);
    void Push_Ship_Collection(lua_State* lua_state);
    void Push_Ship(lua_State* lua_state, std::int16_t ship_id);
    bool Read_Ship_Storage_Reference(lua_State* lua_state,
        int index,
        s_lua_storage_reference* reference);
    void Push_Stored_Ship(lua_State* lua_state,
        int16_t ship_id,
        uint64_t generation);
    void Push_Stored_Ship(lua_State* lua_state, id_ship key);
    void Create_Planet_System_Metatables(lua_State* lua_state);
    void Advance_Topology_Generation();
    std::uint64_t Topology_Generation();
    void Restore_Topology_Generation(std::uint64_t generation);
    void Push_Planet_Collection(lua_State* lua_state);
    void Push_System_Collection(lua_State* lua_state);
    bool Push_Planet(lua_State* lua_state, id_planet planet_id);
    bool Push_System(lua_State* lua_state, std::int16_t system_id);
    bool Push_System(lua_State* lua_state, id_star system);
    bool Read_Planet(lua_State* lua_state,
        int index,
        id_planet* planet_id, const s_planet_data** record = nullptr);
    bool Read_System(lua_State* lua_state,
        int index,
        std::int16_t* system_id);
    bool Read_System(lua_State* lua_state, int index, id_star* system);
    bool Resolve_Planet_System(
        id_planet planet_id, std::int16_t* system_id);
    void Create_Technology_Metatables(lua_State* lua_state);
    void Push_Technology(lua_State* lua_state);
    bool Resolve_Technology_Application_Id(
        const char* name, std::int16_t* application_id);
    bool Resolve_Technology_Field_Id(
        const char* name, std::int16_t* field_id);
    bool Push_Technology_Field(
        lua_State* lua_state, std::int16_t field_id);
    int Push_Technology_Field_Applications(
        lua_State* lua_state, std::int16_t field_id);
    bool Resolve_Technology_Status(
        std::uint8_t native_status, std::int64_t* status);
    void Create_Leader_Nebula_Metatables(lua_State* lua_state);
    void Reset_Leader_Nebula_Generations();
    void Push_Leader_Collection(lua_State* lua_state);
    void Push_Nebula_Collection(lua_State* lua_state);
    s_ship_destroyed_record Snapshot_Ship_Destroyed_Event(
        const s_ship_destroyed_event* event, std::uint64_t sequence);
    int Build_Ship_Destroyed_Event(lua_State* lua_state, void* user_data);

} // namespace orion2re::lua::game::internal

#endif