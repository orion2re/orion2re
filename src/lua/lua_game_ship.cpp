#include "pch.h"
#include "game/ship_slots.h"
using game::NO_SHIP_LOCATION;
using game::NO_SHIP;
using game::id_ship;

using game::current_state;

#include "lua/lua_game_internal.h"
#include "game/legacy_ids.h"
#include "game/ship_ids.h"

#include <cstring>

#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_ship_identity.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

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
            id_ship key = NO_SHIP;
            std::int16_t legacy_row = -1;
            std::uint64_t generation = 0;
            std::uint64_t world = Ship_World();
        };

        struct s_ship_collection {
            bool valid = true;
        };

        bool Ship_Roster_Valid_() {
            return current_state.ship_slots.size() <= MAX_SHIPS;
        }

        bool Ship_Active_(const s_ship_data& ship) {
            const auto status = ship.status;
            return status != 5 && status != 7 && status != 8;
        }

        s_ship_data* Resolve_Ship_(const s_ship_proxy* proxy) {
            if (!proxy || proxy->world != Ship_World() || !Ship_Roster_Valid_()) return nullptr;
            const auto ship_entry = current_state.ships.find(proxy->key);
            auto* ship = ship_entry == current_state.ships.end() ? nullptr : &ship_entry->second;
            return ship && Ship_Active_(*ship) ? ship : nullptr;
        }

        const s_ship_proxy* Test_Ship_Proxy_(lua_State* lua_state, int index) {
            return static_cast<const s_ship_proxy*>(luaL_testudata(lua_state, index, SHIP_METATABLE_));
        }

        void Push_Ship_Proxy_(lua_State* lua_state, s_ship_proxy value) {
            auto* proxy = static_cast<s_ship_proxy*>(lua_newuserdatauv(lua_state, sizeof(s_ship_proxy), 0));
            *proxy = value;
            luaL_setmetatable(lua_state, SHIP_METATABLE_);
        }

        void Push_Active_Ship_Key_(lua_State* lua_state, id_ship key) {
            if (const auto* ship = [&] { const auto entry = current_state.ships.find(key); return entry == current_state.ships.end() ? nullptr : &entry->second; }(); Ship_Roster_Valid_() && ship && Ship_Active_(*ship)) {
                Push_Ship_Proxy_(lua_state, {key});
            } else {
                lua_pushnil(lua_state);
            }
        }

        s_ship_data& Check_Ship_(lua_State* lua_state, int index) {
            const auto* proxy = static_cast<const s_ship_proxy*>(luaL_checkudata(lua_state, index, SHIP_METATABLE_));
            auto* ship = Resolve_Ship_(proxy);
            if (!ship || !Game_State_Access_Allowed()) luaL_error(lua_state, "expired ship reference");
            return *ship;
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
                lua_pushinteger(lua_state, proxy->key != NO_SHIP ? lua_Integer{proxy->key.id} : proxy->legacy_row);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state, Resolve_Ship_(proxy) != nullptr);
                return 1;
            }
            const auto& ship = Check_Ship_(lua_state, 1);
            if (strcmp(key, "owner") == 0) {
                Push_Player(lua_state, factions::Player(ship.owner));
                return 1;
            }
            if (strcmp(key, "owner_id") == 0) {
                lua_pushinteger(lua_state, factions::Legacy_Code(ship.owner));
                return 1;
            }
            if (strcmp(key, "status") == 0) {
                lua_pushinteger(lua_state, ship.status);
                return 1;
            }
            if (strcmp(key, "system") == 0) {
                Push_System(lua_state, ship.position.kind == ::game::e_ship_location::at_star && ship.position.target == ::game::e_ship_target::star
                    ? ship.position.star_id.get() : ::game::NO_STAR);
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
            auto& ship = Check_Ship_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "ship placement is available only during galaxy generation");
            }
            id_star system_id = ::game::NO_STAR;
            if (!Read_System(lua_state, 2, &system_id)) {
                return luaL_error(lua_state, "invalid ship system reference");
            }
            const auto& star = current_state.stars.at(system_id);
            ship.position.Set({::game::e_ship_location::at_star, system_id});
            ship.x = star.x;
            ship.y = star.y;
            return 0;
        }

        int Ship_Delete_Method_(lua_State* lua_state) {
            auto& ship = Check_Ship_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "ship deletion is available only during galaxy generation");
            }
            const auto row = ship_slots::Row_Of(current_state, ship);
            if (row < 0 || !Advance_Ship_Generation(row)) return luaL_error(lua_state, "unable to expire deleted ship identity");
            ship.status = 5;
            ship.position.Set(NO_SHIP_LOCATION);
            ship.officer_id = ::game::NO_LEADER;
            return 0;
        }

        int Ship_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "ship creation is available only during galaxy generation");
            }
            const auto* source = Resolve_Ship_(Test_Ship_Proxy_(lua_state, 2));
            id_star system_id = ::game::NO_STAR;
            if (!source ||
                !Read_System(lua_state, 3, &system_id)) {
                return luaL_error(lua_state,
                    "invalid ship copy source or destination");
            }
            if (static_cast<int16_t>(current_state.ship_slots.size()) >= MAX_SHIPS) {
                return luaL_error(lua_state, "ship capacity exceeded");
            }
            const auto id = ship_slots::Allocate_Key(current_state,
            MOX::game_type == 2 || MOX::game_type == 3 ? std::optional{MOX::local_player} : std::nullopt);
            if (id == NO_SHIP) return luaL_error(lua_state, "ship identity capacity exhausted");
            const auto row = static_cast<int16_t>(current_state.ship_slots.size());
            auto copy = *source;
            copy.id = id;
            const auto& star = current_state.stars.at(system_id);
            copy.position.Set({::game::e_ship_location::at_star, system_id});
            copy.x = star.x;
            copy.y = star.y;
            copy.officer_id = ::game::NO_LEADER;
            if (!Advance_Ship_Generation(row)) {
                return luaL_error(lua_state,
                    "unable to allocate stable ship identity");
            }
            ship_slots::Grow(current_state, row + 1);
            ship_slots::Put(current_state, row, copy);
            Push_Ship_Proxy_(lua_state, {id});
            return 1;
        }

        void Push_Combat_Ship_(lua_State* lua_state,
            std::int16_t combat_ship,
            id_ship key) {
            if (combat_ship < 0 ||
                MOX::combat_data == nullptr ||
                combat_ship >= MOX::combat_total_ship_count ||
                MOX::combat_data[combat_ship].unit_status != 0 ||
                MOX::combat_data[combat_ship].ship_id != key) {
                lua_pushnil(lua_state);
                return;
            }
            Push_Active_Ship_Key_(lua_state, key);
        }

        int Destroyed_Ship_Index_(lua_State* lua_state) {
            const auto* record = static_cast<s_destroyed_ship_record*>(
                luaL_checkudata(
                    lua_state, 1, DESTROYED_SHIP_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "id") == 0) {
                if (record->ship_id == NO_SHIP) {
                    lua_pushnil(lua_state);
                } else {
                    lua_pushinteger(lua_state, record->ship_id.id);
                }
                return 1;
            }
            if (strcmp(key, "owner") == 0) {
                Push_Player(lua_state, factions::Player(record->owner));
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
                    left->world == right->world &&
                    (left->key != NO_SHIP ? left->key == right->key
                        : right->key == NO_SHIP && left->legacy_row == right->legacy_row && left->generation == right->generation));
            return 1;
        }

        int Ship_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const lua_Integer ship_id = luaL_checkinteger(lua_state, 2);
            if (ship_id < 0 || ship_id >= UINT32_MAX || !Ship_Roster_Valid_()) {
                lua_pushnil(lua_state);
                return 1;
            }
            Push_Active_Ship_Key_(lua_state, id_ship{static_cast<uint32_t>(ship_id)});
            return 1;
        }

        int Ship_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SHIP_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            if (!Ship_Roster_Valid_()) {
                return luaL_error(lua_state,
                    "native ship collection is unavailable");
            }
            lua_createtable(lua_state, static_cast<int16_t>(current_state.ship_slots.size()), 0);
            lua_Integer sequence_index = 1;
            for (std::int16_t ship_id = 0;
                 ship_id < static_cast<int16_t>(current_state.ship_slots.size());
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
        event.destroyed_ship.ship_id = destroyed.ship_id;
        event.destroyed_ship.owner = destroyed.owner;
        event.destroyed_ship.cause = source->destruction_cause;
        memcpy(event.destroyed_ship.name,
            destroyed.name,
            sizeof(event.destroyed_ship.name));
        event.destroyed_ship.name[sizeof(event.destroyed_ship.name) - 1] = '\0';
        auto& world = current_state;
        if (const auto ship_entry = world.ships.find(destroyed.ship_id); ship_entry != world.ships.end()) {
            const auto* ship = &ship_entry->second;
            event.destroyed_ship.cost = ship->d.cost;
        }

        if (source->killer_combat_ship >= 0 &&
            source->killer_combat_ship < MOX::combat_total_ship_count) {
            const s_combat_data& killer =
                MOX::combat_data[source->killer_combat_ship];
            if (world.ships.contains(killer.ship_id)) {
                event.killer_combat_ship = source->killer_combat_ship;
                event.killer_ship_id = killer.ship_id;
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
        Push_Active_Ship_Key_(lua_state, ::game::Ship_Key_At_Row(current_state, ship_id));
    }

    bool Read_Ship_Storage_Reference(lua_State* lua_state,
        int index,
        s_lua_storage_reference* reference) {
        const auto* proxy = static_cast<s_ship_proxy*>(
            luaL_testudata(lua_state, index, SHIP_METATABLE_));
        if (proxy == nullptr || reference == nullptr || proxy->world != Ship_World() ||
            (proxy->key == NO_SHIP && proxy->generation == 0)) {
            return false;
        }
        reference->id = proxy->key == NO_SHIP ? int64_t{proxy->legacy_row} : int64_t{proxy->key.id};
        reference->generation = proxy->generation;
        return true;
    }

    void Push_Stored_Ship(lua_State* lua_state,
        int16_t ship_id,
        uint64_t generation) {
        Create_Ship_Metatables(lua_state);
        const auto* ship = ::game::Find_Legacy_Ship_At_Row(current_state, ship_id);
        if (ship && ship->id != NO_SHIP && Ship_Active_(*ship) &&
            generation != 0 && Ship_Generation(ship_id) == generation) {
            Push_Ship_Proxy_(lua_state, {ship->id});
            return;
        }
        auto* proxy = static_cast<s_ship_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_ship_proxy), 0));
        *proxy = {NO_SHIP, ship_id, generation};
        luaL_setmetatable(lua_state, SHIP_METATABLE_);
    }

    void Push_Stored_Ship(lua_State* lua_state, id_ship key) {
        Create_Ship_Metatables(lua_state);
        auto* proxy = static_cast<s_ship_proxy*>(lua_newuserdatauv(lua_state, sizeof(s_ship_proxy), 0));
        *proxy = {key};
        luaL_setmetatable(lua_state, SHIP_METATABLE_);
    }

} // namespace orion2re::lua::game::internal
