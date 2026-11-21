#include "pch.h"

#include "game/legacy_ids.h"

#include "lua/lua_game.h"

#include <climits>
#include <string>

#include "lua/lua_game_internal.h"
#include "lua/lua_mod_runtime.h"
#include "game/techinit.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace orion2re::lua::game {
    namespace {

        std::uint64_t next_event_sequence_ = 1;
        thread_local std::uint32_t authoritative_population_growth_depth_ = 0;

        bool Setup_Callback_Allowed_() {
            return MOX::game_type != 3;
        }

        int Reject_Rule_Context_Write_(lua_State* lua_state) {
            return luaL_error(lua_state,
                "population.growth context is immutable");
        }

        int Reject_Technology_Setup_Event_Write_(lua_State* lua_state) {
            return luaL_error(lua_state,
                "new_game.technology_setup event payload is immutable");
        }

        int Build_Technology_Setup_Event_(lua_State* lua_state,
            void* user_data) {
            const auto* event =
                static_cast<const s_technology_setup_event*>(user_data);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            lua_pushinteger(lua_state,
                static_cast<lua_Integer>(event->setup_sequence));
            lua_setfield(lua_state, values, "setup_sequence");
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(
                lua_state, Reject_Technology_Setup_Event_Write_);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua event object");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
            return 1;
        }

        int Reject_Combat_Beam_Damage_Context_Write_(lua_State* lua_state) {
            return luaL_error(lua_state,
                "combat.beam_damage context is immutable");
        }

        int Reject_Player_Setup_Event_Write_(lua_State* lua_state) {
            return luaL_error(lua_state,
                "new_game.player_setup event payload is immutable");
        }

        int Reject_Home_System_Generated_Event_Write_(lua_State* lua_state) {
            return luaL_error(lua_state,
                "new_game.home_system_generated event payload is immutable");
        }

        int Reject_Planet_Generated_Event_Write_(lua_State* lua_state) {
            return luaL_error(lua_state,
                "planet.generated event payload is immutable");
        }

        int Build_Player_Setup_Event_(lua_State* lua_state,
            void* user_data) {
            const auto* event =
                static_cast<const s_player_setup_event*>(user_data);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            lua_pushinteger(lua_state, event->player_id);
            lua_setfield(lua_state, values, "player_id");
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(lua_state, Reject_Player_Setup_Event_Write_);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua event object");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
            return 1;
        }

        int Build_Planet_Generated_Event_(lua_State* lua_state,
            void* user_data) {
            const auto* event =
                static_cast<const s_planet_generated_event*>(user_data);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            lua_pushinteger(lua_state, event->planet_id.id);
            lua_setfield(lua_state, values, "planet_id");
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(lua_state, Reject_Planet_Generated_Event_Write_);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua event object");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
            return 1;
        }

        int Build_Home_System_Generated_Event_(lua_State* lua_state,
            void* user_data) {
            const auto* event =
                static_cast<const s_home_system_generated_event*>(user_data);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            lua_pushinteger(lua_state, event->player_id);
            lua_setfield(lua_state, values, "player_id");
            lua_pushinteger(lua_state, event->system_id);
            lua_setfield(lua_state, values, "system_id");
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(
                lua_state, Reject_Home_System_Generated_Event_Write_);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua event object");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
            return 1;
        }

        int Build_Population_Growth_Context_(lua_State* lua_state,
            void* user_data) {
            const auto* context =
                static_cast<const s_population_growth_context*>(user_data);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            const auto set_integer = [&](const char* name, std::int16_t value) {
                lua_pushinteger(lua_state, value);
                lua_setfield(lua_state, values, name);
            };
            set_integer("colony_id", context->colony_id);
            set_integer("planet_id", context->planet_id.id);
            set_integer("owner_id", context->owner_id);
            set_integer("race_id", context->race_id);
            set_integer("race_population", context->race_population);
            set_integer("total_population", context->total_population);
            set_integer("population_limit", context->population_limit);
            set_integer("climate", context->climate);
            lua_pushboolean(lua_state, context->housing);
            lua_setfield(lua_state, values, "housing");
            lua_pushboolean(lua_state, context->population_boom);
            lua_setfield(lua_state, values, "population_boom");
            lua_pushboolean(lua_state, context->plague);
            lua_setfield(lua_state, values, "plague");
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(lua_state, Reject_Rule_Context_Write_);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua rule context");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
            return 1;
        }

        int Build_Combat_Beam_Damage_Context_(lua_State* lua_state,
            void* user_data) {
            const auto* context =
                static_cast<const s_combat_beam_damage_context*>(user_data);
            lua_newtable(lua_state);
            const int object = lua_gettop(lua_state);
            lua_newtable(lua_state);
            const int values = lua_gettop(lua_state);
            const auto set_integer = [&](const char* name, std::int64_t value) {
                lua_pushinteger(lua_state, value);
                lua_setfield(lua_state, values, name);
            };
            set_integer("attacker_player_id", context->attacker_player_id);
            set_integer("target_player_id", context->target_player_id);
            set_integer("weapon_id", context->weapon_id);
            set_integer("beam_ordinal", context->beam_ordinal);
            set_integer("beam_count", context->beam_count);
            lua_newtable(lua_state);
            lua_pushvalue(lua_state, values);
            lua_setfield(lua_state, -2, "__index");
            lua_pushcfunction(
                lua_state, Reject_Combat_Beam_Damage_Context_Write_);
            lua_setfield(lua_state, -2, "__newindex");
            lua_pushliteral(lua_state, "immutable Lua rule context");
            lua_setfield(lua_state, -2, "__metatable");
            lua_setmetatable(lua_state, object);
            lua_remove(lua_state, values);
            return 1;
        }

    } // namespace

    s_authoritative_population_growth_scope::
        s_authoritative_population_growth_scope(bool active_value)
        : active(active_value) {
        if (active_value) {
            ++authoritative_population_growth_depth_;
        }
    }

    s_authoritative_population_growth_scope::
        ~s_authoritative_population_growth_scope() {
        if (active) {
            --authoritative_population_growth_depth_;
        }
    }

    void Reset_Semantic_State() {
        next_event_sequence_ = 1;
        internal::Advance_Topology_Generation();
    }

    void Begin_Semantic_State_Replacement(
        s_semantic_state_replacement* replacement) {
        if (replacement == nullptr || replacement->active) {
            return;
        }
        replacement->prior_event_sequence = next_event_sequence_;
        replacement->prior_topology_generation =
            internal::Topology_Generation();
        replacement->active = true;
        Reset_Semantic_State();
    }

    void Commit_Semantic_State_Replacement(
        s_semantic_state_replacement* replacement) {
        if (replacement != nullptr) {
            replacement->active = false;
        }
    }

    void Planet_Generated(const s_planet_generated_event* event) {
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count == 0 || !Setup_Callback_Allowed_()) {
            return;
        }
        if (!event) {
            exitm::Exit_With_Message("invalid planet.generated Lua event");
        }
        int16_t system;
        if (!internal::Resolve_Planet_System(event->planet_id, &system)) {
            exitm::Exit_With_Message("invalid planet.generated planet");
        }

        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "planet.generated",
                    Build_Planet_Generated_Event_,
                    const_cast<s_planet_generated_event*>(event),
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "lua: package=%s event=planet.generated error=%s",
                    package_id == nullptr ? "" : package_id,
                    result.diagnostic.c_str());
                const std::string message =
                    "Lua planet.generated callback failed: " +
                    result.diagnostic;
                exitm::Exit_With_Message(message.c_str());
            }
        }
    }

    void Abort_Semantic_State_Replacement(
        s_semantic_state_replacement* replacement) {
        if (replacement == nullptr || !replacement->active) {
            return;
        }
        next_event_sequence_ = replacement->prior_event_sequence;
        internal::Restore_Topology_Generation(
            replacement->prior_topology_generation);
        replacement->active = false;
    }

    bool Technology_Setup(const s_technology_setup_event* event,
        std::string* diagnostic) {
        if (diagnostic == nullptr) {
            return false;
        }
        diagnostic->clear();
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count == 0 || !Setup_Callback_Allowed_()) {
            return true;
        }
        if (event == nullptr || event->setup_sequence == 0) {
            *diagnostic = "invalid new_game.technology_setup Lua event";
            return false;
        }

        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "new_game.technology_setup",
                    Build_Technology_Setup_Event_,
                    const_cast<s_technology_setup_event*>(event),
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                *diagnostic = "package=" +
                    std::string(package_id == nullptr ? "" : package_id) +
                    " event=new_game.technology_setup error=" +
                    result.diagnostic;
                return false;
            }
        }
        return true;
    }

    void Player_Setup(const s_player_setup_event* event) {
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count == 0 || !Setup_Callback_Allowed_()) {
            return;
        }
        if (event == nullptr || MOX::player == nullptr ||
            MOX::NUM_PLAYERS < 0 || MOX::NUM_PLAYERS > MAX_PLAYERS ||
            event->player_id < 0 ||
            event->player_id >= MOX::NUM_PLAYERS) {
            exitm::Exit_With_Message(
                "invalid new_game.player_setup Lua event");
        }

        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "new_game.player_setup",
                    Build_Player_Setup_Event_,
                    const_cast<s_player_setup_event*>(event),
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "lua: package=%s event=new_game.player_setup error=%s",
                    package_id == nullptr ? "" : package_id,
                    result.diagnostic.c_str());
                const std::string message =
                    "Lua new_game.player_setup callback failed: " +
                    result.diagnostic;
                exitm::Exit_With_Message(message.c_str());
            }
        }
    }

    void Home_System_Generated(const s_home_system_generated_event* event) {
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count == 0 || !Setup_Callback_Allowed_()) {
            return;
        }
        if (event == nullptr || MOX::player == nullptr ||
            MOX::star == nullptr ||
            MOX::NUM_PLAYERS < 0 || MOX::NUM_PLAYERS > MAX_PLAYERS ||
            MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS ||
            event->player_id < 0 ||
            event->player_id >= MOX::NUM_PLAYERS || event->system_id < 0 ||
            event->system_id >= MOX::NUM_STARS) {
            exitm::Exit_With_Message(
                "invalid new_game.home_system_generated Lua event");
        }
        const auto home = MOX::player[event->player_id].home_planet_id.get();
        int16_t system;
        if (!internal::Resolve_Planet_System(home, &system) || system != event->system_id) {
            exitm::Exit_With_Message(
                "invalid new_game.home_system_generated home system");
        }

        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "new_game.home_system_generated",
                    Build_Home_System_Generated_Event_,
                    const_cast<s_home_system_generated_event*>(event),
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "lua: package=%s event=new_game.home_system_generated error=%s",
                    package_id == nullptr ? "" : package_id,
                    result.diagnostic.c_str());
                const std::string message =
                    "Lua new_game.home_system_generated callback failed: " +
                    result.diagnostic;
                exitm::Exit_With_Message(message.c_str());
            }
        }
    }

    void Ship_Destroyed(const s_ship_destroyed_event* event) {
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count == 0) {
            return;
        }
        if (MOX::game_type == 2 || MOX::game_type == 3) {
            return;
        }
        if (event == nullptr || MOX::combat_data == nullptr ||
            event->destroyed_combat_ship < 0 ||
            event->destroyed_combat_ship >= MOX::combat_total_ship_count) {
            exitm::Exit_With_Message("invalid ship.destroyed Lua event");
        }

        internal::s_ship_destroyed_record record =
            internal::Snapshot_Ship_Destroyed_Event(
                event, next_event_sequence_++);
        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Event(runtime,
                    "ship.destroyed",
                    internal::Build_Ship_Destroyed_Event,
                    &record,
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "lua: package=%s event=ship.destroyed error=%s",
                    package_id == nullptr ? "" : package_id,
                    result.diagnostic.c_str());
                const std::string message =
                    "Lua ship.destroyed callback failed: " +
                    result.diagnostic;
                exitm::Exit_With_Message(message.c_str());
            }
        }
    }

    std::int16_t Modify_Population_Growth(
        const s_population_growth_context* context, std::int64_t growth) {
        if (growth < INT16_MIN || growth > INT16_MAX) {
            exitm::Exit_With_Message(
                "native population growth exceeds its signed 16-bit field");
        }
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        const bool network_game =
            MOX::game_type == 2 || MOX::game_type == 3;
        if (runtime_count == 0 ||
            (network_game && authoritative_population_growth_depth_ == 0)) {
            return growth;
        }
        if (context == nullptr || context->colony_id < 0 ||
            context->colony_id >= MAX_COLONIES || context->planet_id == NO_PLANET || context->owner_id < 0 ||
            context->owner_id >= MAX_PLAYERS || context->race_id < 0 ||
            context->race_id >= MAX_PLAYERS ||
            context->race_population < 0 ||
            context->total_population < context->race_population ||
            context->population_limit < 0 ||
            context->climate < 0 || context->climate >= PLANET_CLIMATE_COUNT) {
            exitm::Exit_With_Message("invalid population.growth Lua context");
        }

        std::int64_t value = growth;
        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Rule(runtime,
                    "population.growth",
                    Build_Population_Growth_Context_,
                    const_cast<s_population_growth_context*>(context),
                    INT16_MIN,
                    INT16_MAX,
                    &value,
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "lua: package=%s rule=population.growth error=%s",
                    package_id == nullptr ? "" : package_id,
                    result.diagnostic.c_str());
                const std::string message =
                    "Lua population.growth modifier failed: " +
                    result.diagnostic;
                exitm::Exit_With_Message(message.c_str());
            }
        }
        return static_cast<std::int16_t>(value);
    }

    std::int16_t Modify_Combat_Beam_Damage(
        const s_combat_beam_damage_context* context, std::int64_t damage) {
        if (damage < 0 || damage > INT16_MAX) {
            exitm::Exit_With_Message(
                "native combat beam damage exceeds its signed 16-bit field");
        }
        const s_lua_mod_runtime_set* runtimes = Get_Active_Mod_Runtimes();
        const std::size_t runtime_count = Get_Mod_Runtime_Count(runtimes);
        if (runtime_count == 0 || MOX::game_type == 2 ||
            MOX::game_type == 3) {
            return static_cast<std::int16_t>(damage);
        }
        constexpr std::int16_t maximum_combat_player_id = 14;
        if (context == nullptr || context->attacker_player_id < 0 ||
            context->attacker_player_id > maximum_combat_player_id ||
            context->target_player_id < 0 ||
            context->target_player_id > maximum_combat_player_id ||
            context->weapon_id <= WEAPON_NO_WEAPONS ||
            context->weapon_id >= WEAPON_COUNT || context->beam_ordinal == 0 ||
            context->beam_ordinal > context->beam_count) {
            exitm::Exit_With_Message(
                "invalid combat.beam_damage Lua context");
        }

        std::int64_t value = damage;
        for (std::size_t index = 0; index < runtime_count; ++index) {
            auto* runtime = const_cast<s_lua_mod_runtime*>(
                Get_Mod_Runtime(runtimes, index));
            s_lua_result result;
            if (!Dispatch_Mod_Rule(runtime,
                    "combat.beam_damage",
                    Build_Combat_Beam_Damage_Context_,
                    const_cast<s_combat_beam_damage_context*>(context),
                    0,
                    INT16_MAX,
                    &value,
                    &result)) {
                const char* package_id = Get_Mod_Runtime_Package_Id(runtime);
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                    "lua: package=%s rule=combat.beam_damage error=%s",
                    package_id == nullptr ? "" : package_id,
                    result.diagnostic.c_str());
                const std::string message =
                    "Lua combat.beam_damage modifier failed: " +
                    result.diagnostic;
                exitm::Exit_With_Message(message.c_str());
            }
        }
        return static_cast<std::int16_t>(value);
    }

} // namespace orion2re::lua::game