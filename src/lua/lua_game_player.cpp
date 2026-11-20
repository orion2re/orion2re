#include "pch.h"

#include "game/legacy_ids.h"

#include "lua/lua_game_internal.h"

#include <climits>
#include <cstring>

#include "config/generated/config_enums.h"
#include "game/raceopt.h"
#include "game/techdata.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_traits.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace orion2re::lua::game {
    namespace {

        constexpr char PLAYER_METATABLE_[] = "orion2re.player";
        constexpr char PLAYER_COLLECTION_METATABLE_[] =
            "orion2re.player_collection";
        constexpr char PLAYER_TECHNOLOGIES_METATABLE_[] =
            "orion2re.player_technologies";
        constexpr char BUILTIN_TRAIT_ENUM_[] = "race_special_choice";

        struct s_player_proxy {
            std::int16_t player_id = -1;
        };

        struct s_player_collection {
            bool valid = true;
        };

        struct s_player_technologies_proxy {
            std::int16_t player_id = -1;
        };

        bool Valid_Player_(std::int16_t player_id) {
            return MOX::player != nullptr && MOX::NUM_PLAYERS >= 0 &&
                MOX::NUM_PLAYERS <= MAX_PLAYERS && player_id >= 0 &&
                player_id < MOX::NUM_PLAYERS && player_id < MAX_PLAYERS;
        }

        bool Player_Reference_Valid_(const s_player_proxy* proxy) {
            return proxy != nullptr && Valid_Player_(proxy->player_id);
        }

        bool Player_Reference_Usable_(const s_player_proxy* proxy) {
            return Player_Reference_Valid_(proxy);
        }

        s_player_proxy* Check_Player_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_player_proxy*>(
                luaL_checkudata(lua_state, index, PLAYER_METATABLE_));
            if (!Player_Reference_Usable_(proxy) ||
                !Game_State_Access_Allowed()) {
                luaL_error(lua_state, "expired player reference");
            }
            return proxy;
        }

        s_player_technologies_proxy* Check_Player_Technologies_(
            lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_player_technologies_proxy*>(
                luaL_checkudata(
                    lua_state, index, PLAYER_TECHNOLOGIES_METATABLE_));
            if (proxy == nullptr || !Valid_Player_(proxy->player_id) ||
                !Game_State_Access_Allowed()) {
                luaL_error(lua_state,
                    "expired player technology reference");
            }
            return proxy;
        }

        std::int16_t Check_Technology_Application_Id_(
            lua_State* lua_state, int index) {
            luaL_checktype(lua_state, index, LUA_TSTRING);
            const char* name = lua_tostring(lua_state, index);
            std::int16_t application_id = -1;
            if (!internal::Resolve_Technology_Application_Id(
                    name, &application_id)) {
                luaL_error(lua_state,
                    "unknown technology application '%s'",
                    name);
            }
            return application_id;
        }

        std::uint8_t Check_Player_Technology_Status_(lua_State* lua_state,
            const s_player_technologies_proxy* proxy,
            std::int16_t application_id,
            const char* application_name,
            std::int64_t* semantic_status) {
            const std::uint8_t native_status =
                MOX::player[proxy->player_id]
                    .tech_applications[application_id];
            std::int64_t resolved_status = 0;
            if (!internal::Resolve_Technology_Status(
                    native_status, &resolved_status)) {
                luaL_error(lua_state,
                    "player %d has invalid technology status %d for application '%s'",
                    proxy->player_id,
                    static_cast<int>(native_status),
                    application_name);
            }
            if (semantic_status != nullptr) {
                *semantic_status = resolved_status;
            }
            return native_status;
        }

        void Check_Player_Technology_Field_Status_(lua_State* lua_state,
            const s_player_technologies_proxy* proxy,
            std::int16_t field_id,
            const char* field_name) {
            const std::uint8_t status =
                MOX::player[proxy->player_id].tech_fields[field_id];
            if (status > 3) {
                luaL_error(lua_state,
                    "player %d has invalid technology field status %d for field '%s'",
                    proxy->player_id,
                    static_cast<int>(status),
                    field_name);
            }
        }

        int Player_Technology_Status_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 2);
            std::int64_t status = 0;
            Check_Player_Technology_Status_(lua_state,
                proxy,
                application_id,
                lua_tostring(lua_state, 2),
                &status);
            lua_pushinteger(lua_state, status);
            return 1;
        }

        int Player_Technology_Is_Available_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 2);
            const std::uint8_t status = Check_Player_Technology_Status_(
                lua_state,
                proxy,
                application_id,
                lua_tostring(lua_state, 2),
                nullptr);
            lua_pushboolean(
                lua_state, status == TECH_RESEARCH_STATUS_AVAILABLE);
            return 1;
        }

        int Player_Technology_Is_Researched_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 2);
            const std::uint8_t status = Check_Player_Technology_Status_(
                lua_state,
                proxy,
                application_id,
                lua_tostring(lua_state, 2),
                nullptr);
            lua_pushboolean(
                lua_state, status == TECH_RESEARCH_STATUS_RESEARCHED);
            return 1;
        }

        int Player_Technology_Field_Of_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 2);
            Check_Player_Technology_Status_(lua_state,
                proxy,
                application_id,
                lua_tostring(lua_state, 2),
                nullptr);
            const std::int16_t field_id =
                TECHDATA::technology_applications[application_id]
                    .tech_field_id;
            if (field_id == TECH_FIELD_INVALID) {
                lua_pushnil(lua_state);
                return 1;
            }
            if (!internal::Push_Technology_Field(lua_state, field_id)) {
                return luaL_error(lua_state,
                    "technology application '%s' has invalid field %d",
                    lua_tostring(lua_state, 2),
                    field_id);
            }
            Check_Player_Technology_Field_Status_(lua_state,
                proxy,
                field_id,
                lua_tostring(lua_state, 2));
            return 1;
        }

        int Player_Technology_Alternatives_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            luaL_checktype(lua_state, 2, LUA_TSTRING);
            const char* name = lua_tostring(lua_state, 2);
            std::int16_t field_id = -1;
            if (!internal::Resolve_Technology_Field_Id(name, &field_id)) {
                return luaL_error(lua_state,
                    "unknown technology field '%s'",
                    name);
            }
            Check_Player_Technology_Field_Status_(
                lua_state, proxy, field_id, name);
            return internal::Push_Technology_Field_Applications(
                lua_state, field_id);
        }

        int Player_Technology_Set_Available_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            if (!Game_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "current Lua callback cannot mutate player technology state");
            }
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 2);
            luaL_checktype(lua_state, 3, LUA_TBOOLEAN);
            const std::uint8_t status = Check_Player_Technology_Status_(
                lua_state,
                proxy,
                application_id,
                lua_tostring(lua_state, 2),
                nullptr);
            if (application_id == TECH_APP_NO_TECH) {
                return luaL_error(lua_state,
                    "no-tech application availability cannot be changed");
            }
            if (status == TECH_RESEARCH_STATUS_RESEARCHED) {
                return luaL_error(lua_state,
                    "researched technology '%s' availability cannot be changed",
                    lua_tostring(lua_state, 2));
            }
            MOX::player[proxy->player_id]
                .tech_applications[application_id] =
                lua_toboolean(lua_state, 3) != 0
                ? TECH_RESEARCH_STATUS_AVAILABLE
                : TECH_RESEARCH_STATUS_UNAVAILABLE;
            return 0;
        }

        int Player_Technology_Set_Status_Method_(lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            if (!Player_Setup_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "exact technology status replacement is available only during new-game player setup");
            }
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 2);
            const lua_Integer semantic_status =
                luaL_checkinteger(lua_state, 3);
            if (application_id == TECH_APP_NO_TECH) {
                return luaL_error(lua_state,
                    "no-tech application status cannot be changed");
            }
            std::uint8_t native_status = TECH_RESEARCH_STATUS_UNAVAILABLE;
            if (semantic_status == 1) {
                native_status = TECH_RESEARCH_STATUS_AVAILABLE;
            } else if (semantic_status == 2) {
                native_status = TECH_RESEARCH_STATUS_RESEARCHED;
            } else if (semantic_status != 0) {
                return luaL_error(lua_state,
                    "technology status is outside its semantic domain");
            }
            Check_Player_Technology_Status_(lua_state,
                proxy,
                application_id,
                lua_tostring(lua_state, 2),
                nullptr);
            MOX::player[proxy->player_id]
                .tech_applications[application_id] =
                native_status;
            return 0;
        }

        int Player_Technology_Replace_Available_Method_(
            lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            if (!Player_Setup_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "technology replacement is available only during new-game player setup");
            }
            const std::int16_t removed_id =
                Check_Technology_Application_Id_(lua_state, 2);
            const std::int16_t first_id =
                Check_Technology_Application_Id_(lua_state, 3);
            const std::int16_t second_id =
                Check_Technology_Application_Id_(lua_state, 4);
            luaL_checktype(lua_state, 5, LUA_TBOOLEAN);
            if (removed_id == TECH_APP_NO_TECH ||
                first_id == TECH_APP_NO_TECH ||
                second_id == TECH_APP_NO_TECH) {
                return luaL_error(lua_state,
                    "no-tech application cannot participate in technology replacement");
            }
            if (removed_id == first_id || removed_id == second_id ||
                first_id == second_id) {
                return luaL_error(lua_state,
                    "technology replacement applications must be distinct");
            }
            const std::int16_t application_ids[] = {
                removed_id,
                first_id,
                second_id,
            };
            std::uint8_t statuses[3] = {};
            for (int index = 0; index < 3; ++index) {
                statuses[index] = Check_Player_Technology_Status_(lua_state,
                    proxy,
                    application_ids[index],
                    lua_tostring(lua_state, index + 2),
                    nullptr);
            }
            if (statuses[0] != TECH_RESEARCH_STATUS_AVAILABLE) {
                return luaL_error(lua_state,
                    "replaced technology '%s' is not available",
                    lua_tostring(lua_state, 2));
            }

            s_player& player = MOX::player[proxy->player_id];
            const bool choose_first = lua_toboolean(lua_state, 5) != 0;
            player.tech_applications[removed_id] =
                TECH_RESEARCH_STATUS_UNAVAILABLE;
            player.tech_applications[first_id] = choose_first
                ? TECH_RESEARCH_STATUS_AVAILABLE
                : TECH_RESEARCH_STATUS_UNAVAILABLE;
            player.tech_applications[second_id] = choose_first
                ? TECH_RESEARCH_STATUS_UNAVAILABLE
                : TECH_RESEARCH_STATUS_AVAILABLE;
            return 0;
        }

        int Player_Set_Home_Planet_Method_(lua_State* lua_state) {
            const s_player_proxy* proxy = Check_Player_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "player home assignment is available only during galaxy generation");
            }
            id_planet planet_id{};
            if (!internal::Read_Planet(lua_state, 2, &planet_id)) {
                return luaL_error(lua_state, "invalid home planet reference");
            }
            const auto index = legacy_world::Index(planet_id);
            if (!index) return luaL_error(lua_state, "home planet cannot be represented in the legacy player record");
            s_planet_data& planet = MOX::planet[*index];
            if (planet.planet_type != PLANET_TYPE_PLANET) {
                return luaL_error(lua_state,
                    "player home must be a colonizable planet");
            }
            if (planet.colony_index < 0 ||
                planet.colony_index >= MOX::NUM_COLONIES ||
                MOX::colony[planet.colony_index].planet != *index ||
                MOX::colony[planet.colony_index].owner != proxy->player_id) {
                return luaL_error(lua_state,
                    "player home must be an existing colony owned by that player");
            }
            MOX::player[proxy->player_id].home_planet_id = planet_id;
            return 0;
        }

        int Player_Technology_Set_Field_Application_Method_(
            lua_State* lua_state) {
            const s_player_technologies_proxy* proxy =
                Check_Player_Technologies_(lua_state, 1);
            if (!Game_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "current Lua callback cannot mutate player technology state");
            }
            luaL_checktype(lua_state, 2, LUA_TSTRING);
            const char* field_name = lua_tostring(lua_state, 2);
            std::int16_t field_id = -1;
            if (!internal::Resolve_Technology_Field_Id(
                    field_name, &field_id)) {
                return luaL_error(lua_state,
                    "unknown technology field '%s'",
                    field_name);
            }
            const std::int16_t application_id =
                Check_Technology_Application_Id_(lua_state, 3);
            if (TECHDATA::technology_applications[application_id]
                    .tech_field_id != field_id) {
                return luaL_error(lua_state,
                    "technology application '%s' does not belong to field '%s'",
                    lua_tostring(lua_state, 3),
                    field_name);
            }
            s_player& player = MOX::player[proxy->player_id];
            if (player.tech_fields[field_id] > 3) {
                return luaL_error(lua_state,
                    "player %d has invalid technology field status %d for field '%s'",
                    proxy->player_id,
                    static_cast<int>(player.tech_fields[field_id]),
                    field_name);
            }
            if (player.tech_applications[application_id] ==
                TECH_RESEARCH_STATUS_RESEARCHED) {
                return luaL_error(lua_state,
                    "researched technology '%s' cannot become a field choice",
                    lua_tostring(lua_state, 3));
            }
            for (std::int16_t candidate_id = 1;
                 candidate_id < TECH_APP_COUNT;
                 ++candidate_id) {
                if (TECHDATA::technology_applications[candidate_id]
                        .tech_app_id != candidate_id) {
                    return luaL_error(lua_state,
                        "invalid native technology application %d",
                        candidate_id);
                }
                if (TECHDATA::technology_applications[candidate_id]
                        .tech_field_id != field_id) {
                    continue;
                }
                const std::uint8_t status =
                    player.tech_applications[candidate_id];
                std::int64_t semantic_status = 0;
                if (!internal::Resolve_Technology_Status(
                        status, &semantic_status)) {
                    return luaL_error(lua_state,
                        "player %d has invalid technology status %d for application %d",
                        proxy->player_id,
                        static_cast<int>(status),
                        candidate_id);
                }
            }
            for (std::int16_t candidate_id = 1;
                 candidate_id < TECH_APP_COUNT;
                 ++candidate_id) {
                if (TECHDATA::technology_applications[candidate_id]
                            .tech_field_id == field_id &&
                    player.tech_applications[candidate_id] ==
                        TECH_RESEARCH_STATUS_AVAILABLE) {
                    player.tech_applications[candidate_id] =
                        TECH_RESEARCH_STATUS_UNAVAILABLE;
                }
            }
            player.tech_applications[application_id] =
                TECH_RESEARCH_STATUS_AVAILABLE;
            return 0;
        }

        int Player_Technologies_Index_(lua_State* lua_state) {
            Check_Player_Technologies_(lua_state, 1);
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "status") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Status_Method_);
            } else if (strcmp(key, "is_available") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Is_Available_Method_);
            } else if (strcmp(key, "is_researched") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Is_Researched_Method_);
            } else if (strcmp(key, "field_of") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Field_Of_Method_);
            } else if (strcmp(key, "alternatives") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Alternatives_Method_);
            } else if (strcmp(key, "set_available") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Set_Available_Method_);
            } else if (strcmp(key, "set_status") == 0) {
                lua_pushcfunction(
                    lua_state, Player_Technology_Set_Status_Method_);
            } else if (strcmp(key, "replace_available") == 0) {
                lua_pushcfunction(lua_state,
                    Player_Technology_Replace_Available_Method_);
            } else if (strcmp(key, "set_field_application") == 0) {
                lua_pushcfunction(lua_state,
                    Player_Technology_Set_Field_Application_Method_);
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        int Player_Technologies_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_player_technologies_proxy*>(
                luaL_testudata(
                    lua_state, 1, PLAYER_TECHNOLOGIES_METATABLE_));
            const auto* right = static_cast<s_player_technologies_proxy*>(
                luaL_testudata(
                    lua_state, 2, PLAYER_TECHNOLOGIES_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->player_id == right->player_id);
            return 1;
        }

        int Player_Has_Trait_Method_(lua_State* lua_state) {
            const s_player_proxy* proxy = Check_Player_(lua_state, 1);
            const char* trait_id = luaL_checkstring(lua_state, 2);
            const bool lua_defined = Find_Race_Trait_Prototype(
                Get_Active_Game_Traits(), trait_id, nullptr);
            bool builtin = false;
            const internal::e_player_builtin_trait_result builtin_result =
                internal::Read_Player_Builtin_Trait(proxy->player_id,
                    trait_id,
                    config::Find_Generated_Enum(BUILTIN_TRAIT_ENUM_),
                    &builtin);
            if (builtin_result ==
                internal::e_player_builtin_trait_result::invalid) {
                return luaL_error(lua_state,
                    "player %d has invalid native race trait state",
                    proxy->player_id);
            }
            if (builtin_result ==
                    internal::e_player_builtin_trait_result::unknown &&
                !lua_defined) {
                return luaL_error(lua_state,
                    "unknown race trait '%s'",
                    trait_id);
            }
            lua_pushboolean(lua_state,
                builtin ||
                    (lua_defined && Has_Player_Race_Trait_Assignment(Get_Active_Game_Traits(), proxy->player_id, trait_id)));
            return 1;
        }

        int Player_Add_Research_Method_(lua_State* lua_state) {
            const s_player_proxy* proxy = Check_Player_(lua_state, 1);
            if (!Game_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "current Lua callback cannot mutate player state");
            }
            const lua_Integer amount = luaL_checkinteger(lua_state, 2);
            if (amount < 0 || amount > INT32_MAX) {
                return luaL_error(lua_state,
                    "research award must be between 0 and %d",
                    INT32_MAX);
            }
            s_player& player = MOX::player[proxy->player_id];
            if (player.research_accumulated < 0 ||
                amount > INT32_MAX - player.research_accumulated) {
                return luaL_error(lua_state,
                    "research award exceeds the signed 32-bit accumulator");
            }
            player.research_accumulated += static_cast<std::int32_t>(amount);
            return 0;
        }

        int Player_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_player_proxy*>(
                luaL_checkudata(lua_state, 1, PLAYER_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "kind") == 0) {
                lua_pushliteral(lua_state, "player");
                return 1;
            }
            if (strcmp(key, "id") == 0) {
                lua_pushinteger(lua_state, proxy->player_id);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state, Player_Reference_Usable_(proxy));
                return 1;
            }
            Check_Player_(lua_state, 1);
            const s_player& player = MOX::player[proxy->player_id];
            if (strcmp(key, "is_human") == 0) {
                lua_pushboolean(
                    lua_state, player.objectives == PLAYER_OBJECTIVE_HUMAN);
                return 1;
            }
            if (strcmp(key, "government") == 0) {
                const std::uint8_t government =
                    player.traits[TRAIT_CURRENT_GOVERNMENT];
                if (government >= GOVERNMENT_COUNT) {
                    return luaL_error(lua_state,
                        "player %d has invalid government %u",
                        proxy->player_id,
                        government);
                }
                lua_pushinteger(lua_state, government);
                return 1;
            }
            if (strcmp(key, "home_planet") == 0) {
                const auto home = player.home_planet_id.get();
                if (home == NO_PLANET) {
                    lua_pushnil(lua_state);
                    return 1;
                }
                if (!internal::Push_Planet(
                        lua_state, home)) {
                    return luaL_error(lua_state,
                        "player %d has invalid home planet %u",
                        proxy->player_id,
                        home.id);
                }
                return 1;
            }
            if (strcmp(key, "home_system") == 0) {
                const auto home = player.home_planet_id.get();
                if (home == NO_PLANET) {
                    lua_pushnil(lua_state);
                    return 1;
                }
                std::int16_t system_id = -1;
                if (!internal::Resolve_Planet_System(
                        home, &system_id) ||
                    !internal::Push_System(lua_state, system_id)) {
                    return luaL_error(lua_state,
                        "player %d has invalid home planet %u",
                        proxy->player_id,
                        home.id);
                }
                return 1;
            }
            if (strcmp(key, "technologies") == 0) {
                auto* technologies =
                    static_cast<s_player_technologies_proxy*>(
                        lua_newuserdatauv(lua_state,
                            sizeof(s_player_technologies_proxy),
                            0));
                *technologies = {proxy->player_id};
                luaL_setmetatable(
                    lua_state, PLAYER_TECHNOLOGIES_METATABLE_);
                return 1;
            }
            if (strcmp(key, "has_trait") == 0) {
                lua_pushcfunction(lua_state, Player_Has_Trait_Method_);
                return 1;
            }
            if (strcmp(key, "add_research") == 0) {
                lua_pushcfunction(lua_state, Player_Add_Research_Method_);
                return 1;
            }
            if (strcmp(key, "set_home_planet") == 0) {
                lua_pushcfunction(lua_state, Player_Set_Home_Planet_Method_);
                return 1;
            }
            lua_pushnil(lua_state);
            return 1;
        }

        int Immutable_New_Index_(lua_State* lua_state) {
            return luaL_error(lua_state, "immutable Lua game object");
        }

        int Player_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_player_proxy*>(
                luaL_testudata(lua_state, 1, PLAYER_METATABLE_));
            const auto* right = static_cast<s_player_proxy*>(
                luaL_testudata(lua_state, 2, PLAYER_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->player_id == right->player_id);
            return 1;
        }

        int Player_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLAYER_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const lua_Integer player_id = luaL_checkinteger(lua_state, 2);
            if (player_id < 0 || player_id > INT16_MAX) {
                lua_pushnil(lua_state);
                return 1;
            }
            internal::Push_Player(
                lua_state, static_cast<std::int16_t>(player_id));
            return 1;
        }

        int Player_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLAYER_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            if (MOX::player == nullptr || MOX::NUM_PLAYERS < 0 ||
                MOX::NUM_PLAYERS > MAX_PLAYERS) {
                return luaL_error(lua_state,
                    "native player collection is unavailable");
            }
            lua_createtable(lua_state, MOX::NUM_PLAYERS, 0);
            for (std::int16_t player_id = 0;
                 player_id < MOX::NUM_PLAYERS;
                 ++player_id) {
                internal::Push_Player(lua_state, player_id);
                lua_rawseti(lua_state, -2, player_id + 1);
            }
            return 1;
        }

        int Player_Collection_Index_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLAYER_COLLECTION_METATABLE_);
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "get") == 0) {
                lua_pushcfunction(lua_state, Player_Collection_Get_);
                return 1;
            }
            if (strcmp(key, "all") == 0) {
                lua_pushcfunction(lua_state, Player_Collection_All_);
                return 1;
            }
            lua_pushnil(lua_state);
            return 1;
        }

    } // namespace

    namespace internal {

        e_player_builtin_trait_result Read_Player_Builtin_Trait(
            std::int16_t player_id,
            const char* trait_id,
            const config::s_generated_enum* metadata,
            bool* has_trait) {
            if (has_trait == nullptr) {
                return e_player_builtin_trait_result::invalid;
            }
            *has_trait = false;
            const config::s_generated_enum* canonical =
                config::Find_Generated_Enum(BUILTIN_TRAIT_ENUM_);
            if (!Valid_Player_(player_id) || trait_id == nullptr ||
                canonical == nullptr || canonical->name == nullptr ||
                canonical->values == nullptr ||
                canonical->value_count != raceopt::SPECIAL_PICK_COUNT ||
                metadata == nullptr || metadata->name == nullptr ||
                strcmp(metadata->name, BUILTIN_TRAIT_ENUM_) != 0 ||
                metadata->values == nullptr ||
                metadata->value_count != raceopt::SPECIAL_PICK_COUNT) {
                return e_player_builtin_trait_result::invalid;
            }

            for (std::size_t index = 0;
                 index < metadata->value_count;
                 ++index) {
                const config::s_generated_enum_symbol& symbol =
                    metadata->values[index];
                const config::s_generated_enum_symbol& canonical_symbol =
                    canonical->values[index];
                if (symbol.name == nullptr || canonical_symbol.name == nullptr ||
                    symbol.value != static_cast<std::int64_t>(index)) {
                    return e_player_builtin_trait_result::invalid;
                }
                if (canonical_symbol.value != symbol.value ||
                    strcmp(canonical_symbol.name, symbol.name) != 0) {
                    return e_player_builtin_trait_result::invalid;
                }
                const raceopt::s_special_pick_identity& identity =
                    raceopt::special_pick_identities[index];
                if (identity.trait < TRAIT_LOW_G_WORLD ||
                    identity.trait >= TRAIT_COUNT ||
                    (identity.trait == TRAIT_RICH_HOME_WORLD
                            ? identity.selected_value != -1 &&
                                identity.selected_value != 1
                            : identity.selected_value != 1)) {
                    return e_player_builtin_trait_result::invalid;
                }
                const std::int8_t native_value =
                    MOX::player[player_id].traits[identity.trait];
                if (identity.trait == TRAIT_RICH_HOME_WORLD) {
                    if (native_value < -1 || native_value > 1) {
                        return e_player_builtin_trait_result::invalid;
                    }
                } else if (native_value < 0 || native_value > 1) {
                    return e_player_builtin_trait_result::invalid;
                }
            }

            const config::s_generated_enum_symbol* symbol = nullptr;
            for (std::size_t index = 0;
                 index < metadata->value_count;
                 ++index) {
                if (strcmp(metadata->values[index].name, trait_id) == 0) {
                    symbol = &metadata->values[index];
                    break;
                }
            }
            if (symbol == nullptr) {
                return e_player_builtin_trait_result::unknown;
            }

            const raceopt::s_special_pick_identity& identity =
                raceopt::special_pick_identities[symbol->value];
            const std::int8_t native_value =
                MOX::player[player_id].traits[identity.trait];
            *has_trait = native_value == identity.selected_value;
            return e_player_builtin_trait_result::valid;
        }

        void Create_Player_Metatables(lua_State* lua_state) {
            if (luaL_newmetatable(lua_state, PLAYER_METATABLE_) != 0) {
                lua_pushcfunction(lua_state, Player_Index_);
                lua_setfield(lua_state, -2, "__index");
                lua_pushcfunction(lua_state, Immutable_New_Index_);
                lua_setfield(lua_state, -2, "__newindex");
                lua_pushcfunction(lua_state, Player_Equal_);
                lua_setfield(lua_state, -2, "__eq");
                lua_pushliteral(lua_state, "locked");
                lua_setfield(lua_state, -2, "__metatable");
            }
            lua_pop(lua_state, 1);
            if (luaL_newmetatable(
                    lua_state, PLAYER_COLLECTION_METATABLE_) != 0) {
                lua_pushcfunction(lua_state, Player_Collection_Index_);
                lua_setfield(lua_state, -2, "__index");
                lua_pushcfunction(lua_state, Immutable_New_Index_);
                lua_setfield(lua_state, -2, "__newindex");
                lua_pushliteral(lua_state, "locked");
                lua_setfield(lua_state, -2, "__metatable");
            }
            lua_pop(lua_state, 1);
            if (luaL_newmetatable(
                    lua_state, PLAYER_TECHNOLOGIES_METATABLE_) != 0) {
                lua_pushcfunction(lua_state, Player_Technologies_Index_);
                lua_setfield(lua_state, -2, "__index");
                lua_pushcfunction(lua_state, Immutable_New_Index_);
                lua_setfield(lua_state, -2, "__newindex");
                lua_pushcfunction(lua_state, Player_Technologies_Equal_);
                lua_setfield(lua_state, -2, "__eq");
                lua_pushliteral(lua_state, "locked");
                lua_setfield(lua_state, -2, "__metatable");
            }
            lua_pop(lua_state, 1);
        }

        void Push_Player_Collection(lua_State* lua_state) {
            auto* collection = static_cast<s_player_collection*>(
                lua_newuserdatauv(
                    lua_state, sizeof(s_player_collection), 0));
            *collection = {};
            luaL_setmetatable(lua_state, PLAYER_COLLECTION_METATABLE_);
        }

        void Push_Player(lua_State* lua_state, std::int16_t player_id) {
            if (!Valid_Player_(player_id)) {
                lua_pushnil(lua_state);
                return;
            }
            auto* proxy = static_cast<s_player_proxy*>(
                lua_newuserdatauv(lua_state, sizeof(s_player_proxy), 0));
            *proxy = {player_id};
            luaL_setmetatable(lua_state, PLAYER_METATABLE_);
        }

        bool Read_Player(lua_State* lua_state,
            int index,
            std::int16_t* player_id) {
            const auto* proxy = static_cast<s_player_proxy*>(
                luaL_testudata(lua_state, index, PLAYER_METATABLE_));
            if (!Player_Reference_Usable_(proxy) || player_id == nullptr) {
                return false;
            }
            *player_id = proxy->player_id;
            return true;
        }

        bool Read_Player_Storage_Reference(lua_State* lua_state,
            int index,
            s_lua_storage_reference* reference) {
            const auto* proxy = static_cast<s_player_proxy*>(
                luaL_testudata(lua_state, index, PLAYER_METATABLE_));
            if (proxy == nullptr || reference == nullptr) {
                return false;
            }
            reference->id = proxy->player_id;
            reference->generation = 0;
            return true;
        }

        void Push_Stored_Player(lua_State* lua_state,
            int16_t player_id) {
            Create_Player_Metatables(lua_state);
            auto* proxy = static_cast<s_player_proxy*>(
                lua_newuserdatauv(lua_state, sizeof(s_player_proxy), 0));
            *proxy = {player_id};
            luaL_setmetatable(lua_state, PLAYER_METATABLE_);
        }

    } // namespace internal

    bool Set_Player_Race_Trait(
        std::int16_t player_id, const char* trait_id, bool enabled) {
        if (!Valid_Player_(player_id) || trait_id == nullptr ||
            !Find_Race_Trait_Prototype(
                Get_Active_Game_Traits(), trait_id, nullptr)) {
            return false;
        }
        return Set_Player_Race_Trait_Assignment(
            Get_Active_Game_Traits(), player_id, trait_id, enabled);
    }

} // namespace orion2re::lua::game