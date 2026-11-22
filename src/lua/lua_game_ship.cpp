#include "pch.h"
using game::ANTARES_POSITION;
using game::NO_SHIP_LOCATION;

using game::current_state;

#include "lua/lua_game_internal.h"

#include <cstring>

#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_ship_identity.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

using game::Star_Key_At_Row;

using game::Set_Ship_Orbit_At_Row;
using game::Ship_Orbit_Row;

namespace orion2re::lua::game::internal {
    namespace {

        constexpr char SHIP_METATABLE_[] = "orion2re.ship";
        constexpr char SHIP_COLLECTION_METATABLE_[] =
            "orion2re.ship_collection";
        constexpr char DESTROYED_SHIP_METATABLE_[] =
            "orion2re.destroyed_ship";
        constexpr char SHIP_DESTROYED_EVENT_METATABLE_[] =
            "orion2re.ship_destroyed_event";

        struct s_ship_proxy {
            std::int16_t ship_id = -1;
            std::uint64_t generation = 0;
        };

        struct s_ship_collection {
            bool valid = true;
        };

        bool Ship_Reference_Valid_(const s_ship_proxy* proxy) {
            return proxy != nullptr && proxy->ship_id >= 0 &&
                MOX::NUM_SHIPS >= 0 && MOX::NUM_SHIPS <= MAX_SHIPS &&
                proxy->ship_id < MOX::NUM_SHIPS &&
                proxy->ship_id < MAX_SHIPS && MOX::ship != nullptr &&
                proxy->generation != 0 &&
                proxy->generation == Ship_Generation(proxy->ship_id) &&
                MOX::ship[proxy->ship_id].status != 5 &&
                MOX::ship[proxy->ship_id].status != 7 &&
                MOX::ship[proxy->ship_id].status != 8;
        }

        bool Ship_Reference_Usable_(const s_ship_proxy* proxy) {
            return Ship_Reference_Valid_(proxy);
        }

        s_ship_proxy* Check_Ship_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_ship_proxy*>(
                luaL_checkudata(lua_state, index, SHIP_METATABLE_));
            if (!Ship_Reference_Usable_(proxy) ||
                !Game_State_Access_Allowed()) {
                luaL_error(lua_state, "expired ship reference");
            }
            return proxy;
        }

        int Ship_Set_System_Method_(lua_State* lua_state);
        int Ship_Delete_Method_(lua_State* lua_state);

        int Ship_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_ship_proxy*>(
                luaL_checkudata(lua_state, 1, SHIP_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "kind") == 0) {
                lua_pushliteral(lua_state, "ship");
                return 1;
            }
            if (strcmp(key, "id") == 0) {
                lua_pushinteger(lua_state, proxy->ship_id);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state, Ship_Reference_Usable_(proxy));
                return 1;
            }
            if (strcmp(key, "owner") == 0) {
                Check_Ship_(lua_state, 1);
                Push_Player(lua_state, MOX::ship[proxy->ship_id].owner);
                return 1;
            }
            Check_Ship_(lua_state, 1);
            const s_ship_data& ship = MOX::ship[proxy->ship_id];
            if (strcmp(key, "owner_id") == 0) {
                lua_pushinteger(lua_state, ship.owner);
                return 1;
            }
            if (strcmp(key, "status") == 0) {
                lua_pushinteger(lua_state, ship.status);
                return 1;
            }
            if (strcmp(key, "system") == 0) {
                Push_System(lua_state, Ship_Orbit_Row(current_state, ship.position));
                return 1;
            }
            if (strcmp(key, "x") == 0) {
                lua_pushinteger(lua_state, ship.x);
                return 1;
            }
            if (strcmp(key, "y") == 0) {
                lua_pushinteger(lua_state, ship.y);
                return 1;
            }
            if (strcmp(key, "set_system") == 0) {
                lua_pushcfunction(lua_state, Ship_Set_System_Method_);
                return 1;
            }
            if (strcmp(key, "delete") == 0) {
                lua_pushcfunction(lua_state, Ship_Delete_Method_);
                return 1;
            }
            lua_pushnil(lua_state);
            return 1;
        }

        int Ship_Set_System_Method_(lua_State* lua_state) {
            const s_ship_proxy* proxy = Check_Ship_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "ship placement is available only during galaxy generation");
            }
            std::int16_t system_id = -1;
            if (!Read_System(lua_state, 2, &system_id)) {
                return luaL_error(lua_state, "invalid ship system reference");
            }
            s_ship_data& ship = MOX::ship[proxy->ship_id];
            Set_Ship_Orbit_At_Row(current_state, ship.position, system_id, ANTARES_POSITION);
            ship.x = current_state.stars.at(Star_Key_At_Row(current_state, system_id)).x;
            ship.y = current_state.stars.at(Star_Key_At_Row(current_state, system_id)).y;
            return 0;
        }

        int Ship_Delete_Method_(lua_State* lua_state) {
            const s_ship_proxy* proxy = Check_Ship_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "ship deletion is available only during galaxy generation");
            }
            s_ship_data& ship = MOX::ship[proxy->ship_id];
            ship.status = 5;
            ship.position.Set(NO_SHIP_LOCATION);
            ship.officer_index = -1;
            if (!Advance_Ship_Generation(proxy->ship_id)) {
                return luaL_error(lua_state,
                    "unable to expire deleted ship identity");
            }
            return 0;
        }

        int Ship_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "ship creation is available only during galaxy generation");
            }
            std::int16_t source_id = -1;
            std::int16_t system_id = -1;
            if (!Read_Ship(lua_state, 2, &source_id) ||
                !Read_System(lua_state, 3, &system_id)) {
                return luaL_error(lua_state,
                    "invalid ship copy source or destination");
            }
            if (MOX::NUM_SHIPS >= MAX_SHIPS) {
                return luaL_error(lua_state, "ship capacity exceeded");
            }
            const std::int16_t ship_id = MOX::NUM_SHIPS++;
            MOX::ship[ship_id] = MOX::ship[source_id];
            Set_Ship_Orbit_At_Row(current_state, MOX::ship[ship_id].position, system_id, ANTARES_POSITION);
            MOX::ship[ship_id].x = current_state.stars.at(Star_Key_At_Row(current_state, system_id)).x;
            MOX::ship[ship_id].y = current_state.stars.at(Star_Key_At_Row(current_state, system_id)).y;
            MOX::ship[ship_id].officer_index = -1;
            if (!Advance_Ship_Generation(ship_id)) {
                --MOX::NUM_SHIPS;
                return luaL_error(lua_state,
                    "unable to allocate stable ship identity");
            }
            Push_Ship(lua_state, ship_id);
            return 1;
        }

        void Push_Combat_Ship_(lua_State* lua_state,
            std::int16_t combat_ship,
            std::int16_t ship_id) {
            if (combat_ship < 0 || ship_id < 0 ||
                MOX::combat_data == nullptr ||
                combat_ship >= MOX::combat_total_ship_count ||
                MOX::combat_data[combat_ship].unit_status != 0 ||
                MOX::combat_data[combat_ship].ship_idx != ship_id ||
                Ship_Generation(ship_id) == 0) {
                lua_pushnil(lua_state);
                return;
            }
            Push_Ship(lua_state, ship_id);
        }

        int Destroyed_Ship_Index_(lua_State* lua_state) {
            const auto* record = static_cast<s_destroyed_ship_record*>(
                luaL_checkudata(
                    lua_state, 1, DESTROYED_SHIP_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "id") == 0) {
                if (record->ship_id < 0) {
                    lua_pushnil(lua_state);
                } else {
                    lua_pushinteger(lua_state, record->ship_id);
                }
                return 1;
            }
            if (strcmp(key, "owner") == 0) {
                Push_Player(lua_state, record->owner_id);
                return 1;
            }
            if (strcmp(key, "cost") == 0) {
                lua_pushinteger(lua_state, record->cost);
                return 1;
            }
            if (strcmp(key, "cause") == 0) {
                lua_pushinteger(lua_state, record->cause);
                return 1;
            }
            if (strcmp(key, "name") == 0) {
                lua_pushstring(lua_state, record->name);
                return 1;
            }
            lua_pushnil(lua_state);
            return 1;
        }

        int Immutable_New_Index_(lua_State* lua_state) {
            return luaL_error(lua_state, "immutable Lua game object");
        }

        int Ship_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_ship_proxy*>(
                luaL_testudata(lua_state, 1, SHIP_METATABLE_));
            const auto* right = static_cast<s_ship_proxy*>(
                luaL_testudata(lua_state, 2, SHIP_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->ship_id == right->ship_id &&
                    left->generation == right->generation);
            return 1;
        }

        int Ship_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const lua_Integer ship_id = luaL_checkinteger(lua_state, 2);
            if (ship_id < 0 || ship_id > INT16_MAX) {
                lua_pushnil(lua_state);
                return 1;
            }
            Push_Ship(lua_state, static_cast<std::int16_t>(ship_id));
            return 1;
        }

        int Ship_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            if (MOX::ship == nullptr || MOX::NUM_SHIPS < 0 ||
                MOX::NUM_SHIPS > MAX_SHIPS) {
                return luaL_error(lua_state,
                    "native ship collection is unavailable");
            }
            lua_createtable(lua_state, MOX::NUM_SHIPS, 0);
            lua_Integer sequence_index = 1;
            for (std::int16_t ship_id = 0;
                 ship_id < MOX::NUM_SHIPS;
                 ++ship_id) {
                Push_Ship(lua_state, ship_id);
                if (!lua_isnil(lua_state, -1)) {
                    lua_rawseti(lua_state, -2, sequence_index++);
                } else {
                    lua_pop(lua_state, 1);
                }
            }
            return 1;
        }

        int Ship_Collection_Index_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "get") == 0) {
                lua_pushcfunction(lua_state, Ship_Collection_Get_);
                return 1;
            }
            if (strcmp(key, "all") == 0) {
                lua_pushcfunction(lua_state, Ship_Collection_All_);
                return 1;
            }
            if (strcmp(key, "copy") == 0) {
                lua_pushcfunction(lua_state, Ship_Collection_Copy_);
                return 1;
            }
            lua_pushnil(lua_state);
            return 1;
        }

        int Event_Index_(lua_State* lua_state) {
            const auto* event = static_cast<s_ship_destroyed_record*>(
                luaL_checkudata(
                    lua_state, 1, SHIP_DESTROYED_EVENT_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "destroyed_ship") == 0) {
                auto* record = static_cast<s_destroyed_ship_record*>(
                    lua_newuserdatauv(
                        lua_state, sizeof(s_destroyed_ship_record), 0));
                *record = event->destroyed_ship;
                luaL_setmetatable(lua_state, DESTROYED_SHIP_METATABLE_);
                return 1;
            }
            if (strcmp(key, "killer") == 0) {
                Push_Combat_Ship_(lua_state,
                    event->killer_combat_ship,
                    event->killer_ship_id);
                return 1;
            }
            if (strcmp(key, "phase") == 0) {
                lua_pushliteral(lua_state, "combat");
                return 1;
            }
            if (strcmp(key, "sequence") == 0) {
                lua_pushinteger(lua_state,
                    static_cast<lua_Integer>(event->sequence));
                return 1;
            }
            lua_pushnil(lua_state);
            return 1;
        }

        void Create_Metatable_(lua_State* lua_state,
            const char* name,
            lua_CFunction index) {
            if (luaL_newmetatable(lua_state, name) != 0) {
                lua_pushcfunction(lua_state, index);
                lua_setfield(lua_state, -2, "__index");
                lua_pushcfunction(lua_state, Immutable_New_Index_);
                lua_setfield(lua_state, -2, "__newindex");
                if (strcmp(name, SHIP_METATABLE_) == 0) {
                    lua_pushcfunction(lua_state, Ship_Equal_);
                    lua_setfield(lua_state, -2, "__eq");
                }
                lua_pushliteral(lua_state, "locked");
                lua_setfield(lua_state, -2, "__metatable");
            }
            lua_pop(lua_state, 1);
        }

    } // namespace

    s_ship_destroyed_record Snapshot_Ship_Destroyed_Event(
        const s_ship_destroyed_event* source, std::uint64_t sequence) {
        s_ship_destroyed_record event;
        event.sequence = sequence;
        const s_combat_data& destroyed =
            MOX::combat_data[source->destroyed_combat_ship];
        event.destroyed_ship.ship_id = destroyed.ship_idx;
        event.destroyed_ship.owner_id = destroyed.owner;
        event.destroyed_ship.cause = source->destruction_cause;
        memcpy(event.destroyed_ship.name,
            destroyed.name,
            sizeof(event.destroyed_ship.name));
        event.destroyed_ship.name[sizeof(event.destroyed_ship.name) - 1] = '\0';
        if (destroyed.ship_idx >= 0 && MOX::ship != nullptr &&
            destroyed.ship_idx < MOX::NUM_SHIPS) {
            event.destroyed_ship.cost =
                MOX::ship[destroyed.ship_idx].d.cost;
        }

        if (source->killer_combat_ship >= 0 &&
            source->killer_combat_ship < MOX::combat_total_ship_count) {
            const s_combat_data& killer =
                MOX::combat_data[source->killer_combat_ship];
            if (killer.ship_idx >= 0 && MOX::ship != nullptr &&
                killer.ship_idx < MOX::NUM_SHIPS) {
                event.killer_combat_ship = source->killer_combat_ship;
                event.killer_ship_id = killer.ship_idx;
            }
        }
        return event;
    }

    int Build_Ship_Destroyed_Event(lua_State* lua_state, void* user_data) {
        Create_Player_Metatables(lua_state);
        Create_Ship_Metatables(lua_state);
        auto* event = static_cast<s_ship_destroyed_record*>(
            lua_newuserdatauv(
                lua_state, sizeof(s_ship_destroyed_record), 0));
        *event = *static_cast<s_ship_destroyed_record*>(user_data);
        luaL_setmetatable(lua_state, SHIP_DESTROYED_EVENT_METATABLE_);
        return 1;
    }

    void Create_Ship_Metatables(lua_State* lua_state) {
        Create_Metatable_(lua_state, SHIP_METATABLE_, Ship_Index_);
        Create_Metatable_(
            lua_state, SHIP_COLLECTION_METATABLE_, Ship_Collection_Index_);
        Create_Metatable_(lua_state,
            DESTROYED_SHIP_METATABLE_,
            Destroyed_Ship_Index_);
        Create_Metatable_(lua_state,
            SHIP_DESTROYED_EVENT_METATABLE_,
            Event_Index_);
    }

    void Push_Ship_Collection(lua_State* lua_state) {
        auto* collection = static_cast<s_ship_collection*>(
            lua_newuserdatauv(lua_state, sizeof(s_ship_collection), 0));
        *collection = {};
        luaL_setmetatable(lua_state, SHIP_COLLECTION_METATABLE_);
    }

    void Push_Ship(lua_State* lua_state, std::int16_t ship_id) {
        const std::uint64_t generation = Ship_Generation(ship_id);
        const s_ship_proxy identity = {ship_id, generation};
        if (!Ship_Reference_Valid_(&identity)) {
            lua_pushnil(lua_state);
            return;
        }
        auto* proxy = static_cast<s_ship_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_ship_proxy), 0));
        *proxy = identity;
        luaL_setmetatable(lua_state, SHIP_METATABLE_);
    }

    bool Read_Ship(lua_State* lua_state,
        int index,
        std::int16_t* ship_id) {
        const auto* proxy = static_cast<s_ship_proxy*>(
            luaL_testudata(lua_state, index, SHIP_METATABLE_));
        if (!Ship_Reference_Usable_(proxy) || ship_id == nullptr) {
            return false;
        }
        *ship_id = proxy->ship_id;
        return true;
    }

    bool Read_Ship_Storage_Reference(lua_State* lua_state,
        int index,
        s_lua_storage_reference* reference) {
        const auto* proxy = static_cast<s_ship_proxy*>(
            luaL_testudata(lua_state, index, SHIP_METATABLE_));
        if (proxy == nullptr || reference == nullptr ||
            proxy->generation == 0) {
            return false;
        }
        reference->id = proxy->ship_id;
        reference->generation = proxy->generation;
        return true;
    }

    void Push_Stored_Ship(lua_State* lua_state,
        int16_t ship_id,
        uint64_t generation) {
        Create_Ship_Metatables(lua_state);
        auto* proxy = static_cast<s_ship_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_ship_proxy), 0));
        *proxy = {ship_id, generation};
        luaL_setmetatable(lua_state, SHIP_METATABLE_);
    }

} // namespace orion2re::lua::game::internal
