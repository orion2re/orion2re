#include "pch.h"

#include "lua/lua_game_internal.h"

#include <cstring>

#include "lua/lua_mod_runtime.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

using game::current_state;

namespace orion2re::lua::game::internal {
    namespace {

        constexpr char LEADER_METATABLE_[] = "orion2re.leader";
        constexpr char LEADER_COLLECTION_METATABLE_[] =
            "orion2re.leader_collection";
        constexpr char NEBULA_METATABLE_[] = "orion2re.nebula";
        constexpr char NEBULA_COLLECTION_METATABLE_[] =
            "orion2re.nebula_collection";

        struct s_leader_proxy {
            std::int16_t leader_id = -1;
            std::uint64_t generation = 0;
            std::uint64_t topology_generation = 0;
        };

        struct s_nebula_proxy {
            std::int16_t nebula_id = -1;
            std::uint64_t topology_generation = 0;
            std::uint64_t slot_generation = 0;
        };

        struct s_collection_proxy {
            bool valid = true;
        };

        std::uint64_t leader_generations_[MAX_LEADERS] = {};
        std::uint64_t nebula_generations_[MAX_NEBULAS] = {};

        std::uint64_t Leader_Generation_(std::int16_t leader_id) {
            if (leader_generations_[leader_id] == 0) {
                leader_generations_[leader_id] = 1;
            }
            return leader_generations_[leader_id];
        }

        void Advance_Leader_Generation_(std::int16_t leader_id) {
            ++leader_generations_[leader_id];
            if (leader_generations_[leader_id] == 0) {
                leader_generations_[leader_id] = 1;
            }
        }

        std::uint64_t Nebula_Generation_(std::int16_t nebula_id) {
            if (nebula_generations_[nebula_id] == 0) {
                nebula_generations_[nebula_id] = 1;
            }
            return nebula_generations_[nebula_id];
        }

        void Advance_Nebula_Generation_(std::int16_t nebula_id) {
            ++nebula_generations_[nebula_id];
            if (nebula_generations_[nebula_id] == 0) {
                nebula_generations_[nebula_id] = 1;
            }
        }

        int Immutable_New_Index_(lua_State* lua_state) {
            return luaL_error(lua_state, "immutable Lua game object");
        }

        s_leader_proxy* Check_Leader_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_leader_proxy*>(
                luaL_checkudata(lua_state, index, LEADER_METATABLE_));
            if (proxy->leader_id < 0 || proxy->leader_id >= MAX_LEADERS ||
                proxy->generation != Leader_Generation_(proxy->leader_id) ||
                proxy->topology_generation != Topology_Generation() ||
                !Game_State_Access_Allowed()) {
                luaL_error(lua_state, "expired leader reference");
            }
            return proxy;
        }

        s_nebula_proxy* Check_Nebula_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_nebula_proxy*>(
                luaL_checkudata(lua_state, index, NEBULA_METATABLE_));
            if (proxy->nebula_id < 0 ||
                proxy->nebula_id >= MOX::NUM_NEBULAS ||
                proxy->topology_generation != Topology_Generation() ||
                proxy->slot_generation !=
                    Nebula_Generation_(proxy->nebula_id) ||
                MOX::nebula == nullptr ||
                MOX::nebula[proxy->nebula_id].type < 0 ||
                !Game_State_Access_Allowed()) {
                luaL_error(lua_state, "expired nebula reference");
            }
            return proxy;
        }

        void Push_Leader_(lua_State* lua_state, std::int16_t leader_id) {
            if (leader_id < 0 ||
                leader_id >= MAX_LEADERS) {
                lua_pushnil(lua_state);
                return;
            }
            auto* proxy = static_cast<s_leader_proxy*>(
                lua_newuserdatauv(lua_state, sizeof(s_leader_proxy), 0));
            *proxy = {
                leader_id,
                Leader_Generation_(leader_id),
                Topology_Generation(),
            };
            luaL_setmetatable(lua_state, LEADER_METATABLE_);
        }

        void Push_Nebula_(lua_State* lua_state, std::int16_t nebula_id) {
            if (MOX::nebula == nullptr || nebula_id < 0 ||
                nebula_id >= MOX::NUM_NEBULAS ||
                MOX::nebula[nebula_id].type < 0) {
                lua_pushnil(lua_state);
                return;
            }
            auto* proxy = static_cast<s_nebula_proxy*>(
                lua_newuserdatauv(lua_state, sizeof(s_nebula_proxy), 0));
            *proxy = {nebula_id,
                Topology_Generation(),
                Nebula_Generation_(nebula_id)};
            luaL_setmetatable(lua_state, NEBULA_METATABLE_);
        }

        int Leader_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_leader_proxy*>(
                luaL_checkudata(lua_state, 1, LEADER_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state,
                    proxy->leader_id >= 0 &&
                        proxy->leader_id < MAX_LEADERS &&
                        proxy->generation ==
                            Leader_Generation_(proxy->leader_id) &&
                        proxy->topology_generation == Topology_Generation());
                return 1;
            }
            Check_Leader_(lua_state, 1);
            const auto& leader = current_state.leaders.at(proxy->leader_id);
            if (strcmp(key, "kind") == 0) {
                lua_pushliteral(lua_state, "leader");
            } else if (strcmp(key, "id") == 0) {
                lua_pushinteger(lua_state, proxy->leader_id);
            } else if (strcmp(key, "name") == 0) {
                lua_pushstring(lua_state, leader.name);
            } else if (strcmp(key, "status") == 0) {
                lua_pushinteger(lua_state, leader.status);
            } else if (strcmp(key, "system") == 0 && leader.status == 5) {
                Push_System(lua_state, leader.Location_Position());
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        int Leader_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_leader_proxy*>(
                luaL_testudata(lua_state, 1, LEADER_METATABLE_));
            const auto* right = static_cast<s_leader_proxy*>(
                luaL_testudata(lua_state, 2, LEADER_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->leader_id == right->leader_id &&
                    left->generation == right->generation &&
                    left->topology_generation == right->topology_generation);
            return 1;
        }

        int Leader_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, LEADER_COLLECTION_METATABLE_);
            const lua_Integer leader_id = luaL_checkinteger(lua_state, 2);
            if (leader_id < 0 || leader_id >= MAX_LEADERS) {
                lua_pushnil(lua_state);
            } else {
                Push_Leader_(lua_state,
                    static_cast<std::int16_t>(leader_id));
            }
            return 1;
        }

        int Leader_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, LEADER_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "native leader collection is unavailable");
            }
            lua_createtable(lua_state, MAX_LEADERS, 0);
            for (std::int16_t leader_id = 0; leader_id < MAX_LEADERS;
                 ++leader_id) {
                Push_Leader_(lua_state, leader_id);
                lua_rawseti(lua_state, -2, leader_id + 1);
            }
            return 1;
        }

        int Leader_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, LEADER_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "leader placement is available only during galaxy generation");
            }
            const s_leader_proxy* source = Check_Leader_(lua_state, 2);
            const s_leader_proxy* target = Check_Leader_(lua_state, 3);
            const auto& source_leader = current_state.leaders.at(source->leader_id);
            auto& target_leader = current_state.leaders.at(target->leader_id);
            std::int16_t system_id = -1;
            if (!Read_System(lua_state, 4, &system_id) ||
                source_leader.status != 5 || target_leader.status != -1) {
                return luaL_error(lua_state,
                    "leader copy requires a marooned source, unused target, and valid system");
            }
            Advance_Leader_Generation_(target->leader_id);
            target_leader = source_leader;
            target_leader.Set_Star_Location(current_state.Star_Id_At_Position(system_id));
            target_leader.player_index = -1;
            Push_Leader_(lua_state, target->leader_id);
            return 1;
        }

        int Leader_Collection_Index_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, LEADER_COLLECTION_METATABLE_);
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "get") == 0) {
                lua_pushcfunction(lua_state, Leader_Collection_Get_);
            } else if (strcmp(key, "all") == 0) {
                lua_pushcfunction(lua_state, Leader_Collection_All_);
            } else if (strcmp(key, "copy") == 0) {
                lua_pushcfunction(lua_state, Leader_Collection_Copy_);
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        int Nebula_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_nebula_proxy*>(
                luaL_checkudata(lua_state, 1, NEBULA_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state,
                    proxy->nebula_id >= 0 &&
                        proxy->nebula_id < MOX::NUM_NEBULAS &&
                        proxy->topology_generation ==
                            Topology_Generation() &&
                        proxy->slot_generation ==
                            Nebula_Generation_(proxy->nebula_id) &&
                        MOX::nebula != nullptr &&
                        MOX::nebula[proxy->nebula_id].type >= 0);
                return 1;
            }
            Check_Nebula_(lua_state, 1);
            const s_nebula& nebula = MOX::nebula[proxy->nebula_id];
            if (strcmp(key, "kind") == 0) {
                lua_pushliteral(lua_state, "nebula");
            } else if (strcmp(key, "id") == 0) {
                lua_pushinteger(lua_state, proxy->nebula_id);
            } else if (strcmp(key, "x") == 0) {
                lua_pushinteger(lua_state, nebula.x);
            } else if (strcmp(key, "y") == 0) {
                lua_pushinteger(lua_state, nebula.y);
            } else if (strcmp(key, "type") == 0) {
                lua_pushinteger(lua_state, nebula.type);
            } else if (strcmp(key, "delete") == 0) {
                lua_pushcfunction(lua_state, [](lua_State* state) -> int {
                    const s_nebula_proxy* selected = Check_Nebula_(state, 1);
                    if (!Galaxy_State_Mutation_Allowed()) {
                        return luaL_error(state,
                            "nebula deletion is available only during galaxy generation");
                    }
                    const std::int16_t id = selected->nebula_id;
                    MOX::nebula[id].type = -1;
                    Advance_Nebula_Generation_(id);
                    return 0;
                });
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        int Nebula_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_nebula_proxy*>(
                luaL_testudata(lua_state, 1, NEBULA_METATABLE_));
            const auto* right = static_cast<s_nebula_proxy*>(
                luaL_testudata(lua_state, 2, NEBULA_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->nebula_id == right->nebula_id &&
                    left->topology_generation ==
                        right->topology_generation &&
                    left->slot_generation == right->slot_generation);
            return 1;
        }

        int Nebula_New_Index_(lua_State* lua_state) {
            const s_nebula_proxy* proxy = Check_Nebula_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "nebula mutation is available only during galaxy generation");
            }
            const char* key = luaL_checkstring(lua_state, 2);
            const lua_Integer value = luaL_checkinteger(lua_state, 3);
            s_nebula& nebula = MOX::nebula[proxy->nebula_id];
            if (strcmp(key, "x") == 0 || strcmp(key, "y") == 0) {
                const std::int16_t maximum = strcmp(key, "x") == 0
                    ? MOX::MAP_MAX_X
                    : MOX::MAP_MAX_Y;
                if (value < 0 || value >= maximum) {
                    return luaL_error(lua_state,
                        "nebula coordinate is outside the map");
                }
                if (strcmp(key, "x") == 0) {
                    nebula.x = static_cast<std::int16_t>(value);
                } else {
                    nebula.y = static_cast<std::int16_t>(value);
                }
                return 0;
            }
            if (strcmp(key, "type") == 0) {
                if (value < 0 || value >= 12) {
                    return luaL_error(lua_state,
                        "nebula type is outside its domain");
                }
                nebula.type = static_cast<std::int8_t>(value);
                return 0;
            }
            return luaL_error(lua_state, "immutable Lua game object");
        }

        int Nebula_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, NEBULA_COLLECTION_METATABLE_);
            const lua_Integer nebula_id = luaL_checkinteger(lua_state, 2);
            if (nebula_id < 0 || nebula_id > INT16_MAX) {
                lua_pushnil(lua_state);
            } else {
                Push_Nebula_(lua_state,
                    static_cast<std::int16_t>(nebula_id));
            }
            return 1;
        }

        int Nebula_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, NEBULA_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed() || MOX::nebula == nullptr ||
                MOX::NUM_NEBULAS > MAX_NEBULAS) {
                return luaL_error(lua_state,
                    "native nebula collection is unavailable");
            }
            lua_createtable(lua_state, MOX::NUM_NEBULAS, 0);
            lua_Integer sequence_index = 1;
            for (std::int16_t nebula_id = 0;
                 nebula_id < MOX::NUM_NEBULAS;
                 ++nebula_id) {
                Push_Nebula_(lua_state, nebula_id);
                if (!lua_isnil(lua_state, -1)) {
                    lua_rawseti(lua_state, -2, sequence_index++);
                } else {
                    lua_pop(lua_state, 1);
                }
            }
            return 1;
        }

        int Nebula_Collection_Create_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, NEBULA_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "nebula creation is available only during galaxy generation");
            }
            const lua_Integer x = luaL_checkinteger(lua_state, 2);
            const lua_Integer y = luaL_checkinteger(lua_state, 3);
            const lua_Integer type = luaL_checkinteger(lua_state, 4);
            if (x < 0 || x >= MOX::MAP_MAX_X || y < 0 ||
                y >= MOX::MAP_MAX_Y ||
                type < 0 || type >= 12) {
                return luaL_error(lua_state,
                    "invalid nebula or nebula capacity exceeded");
            }
            std::int16_t nebula_id = -1;
            for (std::int16_t candidate = 0;
                 candidate < MOX::NUM_NEBULAS;
                 ++candidate) {
                if (MOX::nebula[candidate].type < 0) {
                    nebula_id = candidate;
                    break;
                }
            }
            if (nebula_id < 0) {
                if (MOX::NUM_NEBULAS >= MAX_NEBULAS) {
                    return luaL_error(lua_state, "nebula capacity exceeded");
                }
                nebula_id = MOX::NUM_NEBULAS++;
            }
            MOX::nebula[nebula_id] = {
                static_cast<std::int16_t>(x),
                static_cast<std::int16_t>(y),
                static_cast<std::int8_t>(type),
            };
            Push_Nebula_(lua_state, nebula_id);
            return 1;
        }

        int Nebula_Collection_Index_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, NEBULA_COLLECTION_METATABLE_);
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "get") == 0) {
                lua_pushcfunction(lua_state, Nebula_Collection_Get_);
            } else if (strcmp(key, "all") == 0) {
                lua_pushcfunction(lua_state, Nebula_Collection_All_);
            } else if (strcmp(key, "create") == 0) {
                lua_pushcfunction(lua_state, Nebula_Collection_Create_);
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        void Create_Metatable_(lua_State* lua_state,
            const char* name,
            lua_CFunction index,
            lua_CFunction new_index = Immutable_New_Index_,
            lua_CFunction equal = nullptr) {
            if (luaL_newmetatable(lua_state, name) != 0) {
                lua_pushcfunction(lua_state, index);
                lua_setfield(lua_state, -2, "__index");
                lua_pushcfunction(lua_state, new_index);
                lua_setfield(lua_state, -2, "__newindex");
                if (equal != nullptr) {
                    lua_pushcfunction(lua_state, equal);
                    lua_setfield(lua_state, -2, "__eq");
                }
                lua_pushliteral(lua_state, "locked");
                lua_setfield(lua_state, -2, "__metatable");
            }
            lua_pop(lua_state, 1);
        }

    } // namespace

    void Create_Leader_Nebula_Metatables(lua_State* lua_state) {
        Create_Metatable_(lua_state,
            LEADER_METATABLE_,
            Leader_Index_,
            Immutable_New_Index_,
            Leader_Equal_);
        Create_Metatable_(lua_state,
            LEADER_COLLECTION_METATABLE_,
            Leader_Collection_Index_);
        Create_Metatable_(lua_state,
            NEBULA_METATABLE_,
            Nebula_Index_,
            Nebula_New_Index_,
            Nebula_Equal_);
        Create_Metatable_(lua_state,
            NEBULA_COLLECTION_METATABLE_,
            Nebula_Collection_Index_);
    }

    void Reset_Leader_Nebula_Generations() {
        memset(leader_generations_, 0, sizeof(leader_generations_));
        memset(nebula_generations_, 0, sizeof(nebula_generations_));
    }

    void Push_Leader_Collection(lua_State* lua_state) {
        auto* collection = static_cast<s_collection_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_collection_proxy), 0));
        *collection = {};
        luaL_setmetatable(lua_state, LEADER_COLLECTION_METATABLE_);
    }

    void Push_Nebula_Collection(lua_State* lua_state) {
        auto* collection = static_cast<s_collection_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_collection_proxy), 0));
        *collection = {};
        luaL_setmetatable(lua_state, NEBULA_COLLECTION_METATABLE_);
    }

} // namespace orion2re::lua::game::internal
