#include "pch.h"

#include "lua/lua_game_internal.h"

#include <climits>
#include <cstring>

#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_ship_identity.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace orion2re::lua::game::internal {
    namespace {

        constexpr char PLANET_METATABLE_[] = "orion2re.planet";
        constexpr char PLANET_COLLECTION_METATABLE_[] =
            "orion2re.planet_collection";
        constexpr char SYSTEM_METATABLE_[] = "orion2re.system";
        constexpr char SYSTEM_COLLECTION_METATABLE_[] =
            "orion2re.system_collection";
        constexpr char SYSTEM_PLANET_COLLECTION_METATABLE_[] =
            "orion2re.system_planet_collection";

        struct s_planet_proxy {
            std::int16_t planet_id = -1;
            std::int16_t system_id = -1;
            std::int8_t orbit = -1;
            std::uint64_t topology_generation = 0;
            std::uint64_t slot_generation = 0;
        };

        struct s_system_proxy {
            std::int16_t system_id = -1;
            std::uint64_t topology_generation = 0;
            std::uint64_t slot_generation = 0;
        };

        struct s_collection_proxy {
            std::int16_t system_id = -1;
            std::uint64_t topology_generation = 0;
            std::uint64_t slot_generation = 0;
        };

        std::uint64_t topology_generation_ = 1;
        std::uint64_t planet_generations_[MAX_PLANETS] = {};
        std::uint64_t system_generations_[MAX_STARS] = {};

        std::uint64_t Current_Generation_(std::uint64_t* generations,
            std::int16_t id) {
            if (generations[id] == 0) generations[id] = 1;
            return generations[id];
        }

        void Advance_Generation_(std::uint64_t* generations,
            std::int16_t id) {
            ++generations[id];
            if (generations[id] == 0) generations[id] = 1;
        }

        bool Valid_System_(std::int16_t system_id) {
            return MOX::star != nullptr && MOX::NUM_STARS >= 0 &&
                MOX::NUM_STARS <= MAX_STARS && system_id >= 0 &&
                system_id < MOX::NUM_STARS &&
                MOX::star[system_id].spectral_class >= STAR_CLASS_B &&
                MOX::star[system_id].spectral_class <=
                STAR_CLASS_BLACK_HOLE;
        }

        bool Valid_Planet_Identity_(std::int16_t planet_id,
            std::int16_t system_id,
            std::int8_t orbit) {
            if (MOX::planet == nullptr || MOX::planet_count < 0 ||
                MOX::planet_count > MAX_PLANETS || planet_id < 0 ||
                planet_id >= MOX::planet_count || !Valid_System_(system_id) ||
                orbit < 0 || orbit >= 5) {
                return false;
            }
            const s_planet_data& planet = MOX::planet[planet_id];
            return planet.star_index == system_id && planet.orbit == orbit &&
                MOX::star[system_id].planet_index[orbit] == planet_id;
        }

        bool Valid_Planet_(const s_planet_proxy* proxy) {
            return proxy != nullptr && proxy->planet_id >= 0 &&
                proxy->planet_id < MAX_PLANETS &&
                proxy->topology_generation == topology_generation_ &&
                proxy->slot_generation ==
                Current_Generation_(
                    planet_generations_, proxy->planet_id) &&
                Valid_Planet_Identity_(proxy->planet_id,
                    proxy->system_id,
                    proxy->orbit);
        }

        s_planet_proxy* Check_Planet_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_planet_proxy*>(
                luaL_checkudata(lua_state, index, PLANET_METATABLE_));
            if (!Valid_Planet_(proxy) || !Game_State_Access_Allowed()) {
                luaL_error(lua_state, "expired planet reference");
            }
            return proxy;
        }

        bool Delete_Colony_(::game::s_world_state* world_state, std::int16_t colony_id) {
            if (colony_id < 0 || colony_id >= MOX::NUM_COLONIES) {
                return false;
            }
            const std::int16_t planet_id = MOX::colony[colony_id].planet;
            if (planet_id < 0 || planet_id >= MOX::planet_count ||
                MOX::planet[planet_id].colony_index != colony_id) {
                return false;
            }
            const std::int16_t officer_id = MOX::colony[colony_id].officer;
            if (officer_id >= 0) {
                if (officer_id >= MAX_LEADERS) return false;
                const std::int8_t player_id =
                    ::game::Leader(world_state, officer_id).player_index;
                const std::int16_t star_id =
                    MOX::planet[planet_id].star_index;
                if (player_id >= 0 && player_id < MOX::NUM_PLAYERS &&
                    star_id >= 0 && star_id < MOX::NUM_STARS &&
                    MOX::star[star_id].officer_index[player_id] ==
                        officer_id) {
                    MOX::star[star_id].officer_index[player_id] = -1;
                }
                ::game::Leader(world_state, officer_id).status = 0;
                ::game::Leader(world_state, officer_id).location = -1;
                MOX::colony[colony_id].officer = -1;
            }
            MOX::planet[planet_id].colony_index = -1;
            const std::int16_t last_id = --MOX::NUM_COLONIES;
            if (colony_id != last_id) {
                MOX::colony[colony_id] = MOX::colony[last_id];
                const std::int16_t moved_planet =
                    MOX::colony[colony_id].planet;
                if (moved_planet < 0 || moved_planet >= MOX::planet_count ||
                    MOX::planet[moved_planet].colony_index != last_id) {
                    return false;
                }
                MOX::planet[moved_planet].colony_index = colony_id;
            }
            memset(&MOX::colony[last_id], 0, sizeof(s_colony));
            MOX::colony[last_id].planet = -1;
            MOX::colony[last_id].owner = -1;
            MOX::colony[last_id].officer = -1;
            return true;
        }

        bool Delete_Planet_(std::int16_t planet_id) {
            if (planet_id < 0 || planet_id >= MOX::planet_count) {
                return false;
            }
            for (std::int16_t player_id = 0;
                 player_id < MOX::NUM_PLAYERS;
                 ++player_id) {
                if (MOX::player[player_id].home_planet_id == planet_id) {
                    return false;
                }
            }
            if (MOX::planet[planet_id].colony_index >= 0 &&
                !Delete_Colony_(&::game::world, MOX::planet[planet_id].colony_index)) {
                return false;
            }
            const s_planet_data removed = MOX::planet[planet_id];
            MOX::star[removed.star_index].planet_index[removed.orbit] = -1;
            memset(&MOX::planet[planet_id], 0, sizeof(s_planet_data));
            MOX::planet[planet_id].star_index = -1;
            MOX::planet[planet_id].colony_index = -1;
            Advance_Generation_(planet_generations_, planet_id);
            return true;
        }

        int Planet_Delete_Method_(lua_State* lua_state) {
            const s_planet_proxy* proxy = Check_Planet_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "planet deletion is available only during galaxy generation");
            }
            if (!Delete_Planet_(proxy->planet_id)) {
                return luaL_error(lua_state,
                    "planet cannot be deleted while referenced as a home");
            }
            return 0;
        }

        int Planet_Set_Orbit_Method_(lua_State* lua_state) {
            const s_planet_proxy* proxy = Check_Planet_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "planet topology mutation is available only during galaxy generation");
            }
            std::int16_t system_id = -1;
            if (!Read_System(lua_state, 2, &system_id)) {
                return luaL_error(lua_state, "invalid destination system");
            }
            const lua_Integer orbit = luaL_checkinteger(lua_state, 3);
            if (orbit < 0 || orbit >= 5) {
                return luaL_error(lua_state, "planet orbit is outside its domain");
            }
            s_planet_data& planet = MOX::planet[proxy->planet_id];
            const std::int16_t occupied =
                MOX::star[system_id].planet_index[orbit];
            if (occupied != -1 && occupied != proxy->planet_id) {
                return luaL_error(lua_state,
                    "destination system orbit is occupied");
            }
            MOX::star[planet.star_index].planet_index[planet.orbit] = -1;
            planet.star_index = system_id;
            planet.orbit = static_cast<std::int8_t>(orbit);
            MOX::star[system_id].planet_index[orbit] = proxy->planet_id;
            Advance_Generation_(planet_generations_, proxy->planet_id);
            return 0;
        }

        s_system_proxy* Check_System_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_system_proxy*>(
                luaL_checkudata(lua_state, index, SYSTEM_METATABLE_));
            if (proxy == nullptr ||
                proxy->system_id < 0 || proxy->system_id >= MAX_STARS ||
                proxy->topology_generation != topology_generation_ ||
                proxy->slot_generation != Current_Generation_(system_generations_, proxy->system_id) ||
                !Valid_System_(proxy->system_id) ||
                !Game_State_Access_Allowed()) {
                luaL_error(lua_state, "expired system reference");
            }
            return proxy;
        }

        int Immutable_New_Index_(lua_State* lua_state) {
            return luaL_error(lua_state, "immutable Lua game object");
        }

        int Planet_Set_Special_(lua_State* lua_state) {
            const s_planet_proxy* proxy = Check_Planet_(lua_state, 1);
            if (!Planet_State_Mutation_Allowed() &&
                !Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "planet mutation is not available in this callback");
            }
            const lua_Integer special = luaL_checkinteger(lua_state, 2);
            if (special < SYSTEM_SPECIAL_NO_SPECIAL ||
                special >= SYSTEM_SPECIAL_COUNT) {
                return luaL_error(lua_state,
                    "planet special is outside its domain");
            }
            if (special != SYSTEM_SPECIAL_NO_SPECIAL &&
                special != SYSTEM_SPECIAL_GOLD_DEPOSITS &&
                special != SYSTEM_SPECIAL_GEM_DEPOSITS &&
                special != SYSTEM_SPECIAL_ANCIENT_ARTIFACTS &&
                special != SYSTEM_SPECIAL_ORION_STAR) {
                return luaL_error(lua_state,
                    "special is not valid for a planet");
            }

            s_planet_data& planet = MOX::planet[proxy->planet_id];
            s_star_data& system = MOX::star[proxy->system_id];
            if (system.system_special != SYSTEM_SPECIAL_NO_SPECIAL &&
                system.system_special != planet.planet_special) {
                return luaL_error(lua_state,
                    "planet special conflicts with its system special");
            }
            for (std::int16_t orbit = 0; orbit < 5; ++orbit) {
                const std::int16_t planet_id = system.planet_index[orbit];
                if (planet_id < 0 || planet_id == proxy->planet_id) {
                    continue;
                }
                if (planet_id >= MOX::planet_count ||
                    MOX::planet[planet_id].star_index != proxy->system_id ||
                    MOX::planet[planet_id].orbit != orbit) {
                    return luaL_error(lua_state,
                        "planet system topology is invalid");
                }
                if (MOX::planet[planet_id].planet_special !=
                    SYSTEM_SPECIAL_NO_SPECIAL) {
                    return luaL_error(lua_state,
                        "planet special conflicts with another planet");
                }
            }
            planet.planet_special = static_cast<std::int8_t>(special);
            system.system_special = static_cast<std::int8_t>(special);
            return 0;
        }

        int Planet_New_Index_(lua_State* lua_state) {
            const s_planet_proxy* proxy = Check_Planet_(lua_state, 1);
            if (!Planet_State_Mutation_Allowed() &&
                !Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "planet mutation is not available in this callback");
            }
            const char* key = luaL_checkstring(lua_state, 2);
            const lua_Integer value = luaL_checkinteger(lua_state, 3);
            s_planet_data& planet = MOX::planet[proxy->planet_id];
            if (strcmp(key, "size") == 0) {
                if (value < PLANET_SIZE_TINY || value >= PLANET_SIZE_COUNT) {
                    return luaL_error(lua_state,
                        "planet size is outside its domain");
                }
                planet.size = static_cast<std::int8_t>(value);
                return 0;
            }
            if (strcmp(key, "climate") == 0) {
                if (value < PLANET_CLIMATE_TOXIC ||
                    value >= PLANET_CLIMATE_COUNT) {
                    return luaL_error(lua_state,
                        "planet climate is outside its domain");
                }
                planet.climate = static_cast<std::int8_t>(value);
                planet.food_per_farmer = MOX::food_per_farmer_table[value];
                return 0;
            }
            if (strcmp(key, "minerals") == 0) {
                if (value < MINERAL_RESOURCE_ULTRA_POOR ||
                    value >= MINERAL_RESOURCE_COUNT) {
                    return luaL_error(lua_state,
                        "planet minerals are outside their domain");
                }
                planet.mineral_class = static_cast<std::int8_t>(value);
                return 0;
            }
            if (strcmp(key, "gravity") == 0) {
                if (value < PLANET_GRAVITY_LOW_G ||
                    value >= PLANET_GRAVITY_COUNT) {
                    return luaL_error(lua_state,
                        "planet gravity is outside its domain");
                }
                planet.gravity_class = static_cast<std::int8_t>(value);
                return 0;
            }
            return luaL_error(lua_state, "immutable Lua game object");
        }

        int Planet_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_planet_proxy*>(
                luaL_checkudata(lua_state, 1, PLANET_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "kind") == 0) {
                lua_pushliteral(lua_state, "planet");
                return 1;
            }
            if (strcmp(key, "id") == 0) {
                lua_pushinteger(lua_state, proxy->planet_id);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state, Valid_Planet_(proxy));
                return 1;
            }
            Check_Planet_(lua_state, 1);
            const s_planet_data& planet = MOX::planet[proxy->planet_id];
            if (strcmp(key, "system") == 0) {
                Push_System(lua_state, proxy->system_id);
            } else if (strcmp(key, "orbit") == 0) {
                lua_pushinteger(lua_state, planet.orbit);
            } else if (strcmp(key, "planet_type") == 0) {
                lua_pushinteger(lua_state, planet.planet_type);
            } else if (strcmp(key, "size") == 0) {
                lua_pushinteger(lua_state, planet.size);
            } else if (strcmp(key, "gravity") == 0) {
                lua_pushinteger(lua_state, planet.gravity_class);
            } else if (strcmp(key, "climate") == 0) {
                lua_pushinteger(lua_state, planet.climate);
            } else if (strcmp(key, "minerals") == 0) {
                lua_pushinteger(lua_state, planet.mineral_class);
            } else if (strcmp(key, "special") == 0) {
                lua_pushinteger(lua_state, planet.planet_special);
            } else if (strcmp(key, "set_special") == 0) {
                lua_pushcfunction(lua_state, Planet_Set_Special_);
            } else if (strcmp(key, "set_orbit") == 0) {
                lua_pushcfunction(lua_state, Planet_Set_Orbit_Method_);
            } else if (strcmp(key, "delete") == 0) {
                lua_pushcfunction(lua_state, Planet_Delete_Method_);
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        int Planet_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_planet_proxy*>(
                luaL_testudata(lua_state, 1, PLANET_METATABLE_));
            const auto* right = static_cast<s_planet_proxy*>(
                luaL_testudata(lua_state, 2, PLANET_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->planet_id == right->planet_id &&
                    left->system_id == right->system_id &&
                    left->orbit == right->orbit &&
                    left->topology_generation ==
                        right->topology_generation &&
                    left->slot_generation == right->slot_generation);
            return 1;
        }

        int System_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_system_proxy*>(
                luaL_checkudata(lua_state, 1, SYSTEM_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "kind") == 0) {
                lua_pushliteral(lua_state, "system");
                return 1;
            }
            if (strcmp(key, "id") == 0) {
                lua_pushinteger(lua_state, proxy->system_id);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state,
                    proxy->topology_generation == topology_generation_ &&
                        proxy->slot_generation == Current_Generation_(system_generations_, proxy->system_id) &&
                        Valid_System_(proxy->system_id));
                return 1;
            }
            Check_System_(lua_state, 1);
            const s_star_data& system = MOX::star[proxy->system_id];
            if (strcmp(key, "x") == 0) {
                lua_pushinteger(lua_state, system.x);
            } else if (strcmp(key, "y") == 0) {
                lua_pushinteger(lua_state, system.y);
            } else if (strcmp(key, "spectral_class") == 0) {
                lua_pushinteger(lua_state, system.spectral_class);
            } else if (strcmp(key, "special") == 0) {
                lua_pushinteger(lua_state, system.system_special);
            } else if (strcmp(key, "size") == 0) {
                lua_pushinteger(lua_state, system.size);
            } else if (strcmp(key, "picture") == 0) {
                lua_pushinteger(lua_state, system.pict_type);
            } else if (strcmp(key, "visited") == 0) {
                lua_pushinteger(lua_state, system.visited);
            } else if (strcmp(key, "wormhole") == 0) {
                if (system.wormhole_star_id < 0) {
                    lua_pushnil(lua_state);
                } else {
                    Push_System(lua_state, system.wormhole_star_id);
                }
            } else if (strcmp(key, "planets") == 0) {
                auto* collection = static_cast<s_collection_proxy*>(
                    lua_newuserdatauv(
                        lua_state, sizeof(s_collection_proxy), 0));
                *collection = {
                    proxy->system_id,
                    proxy->topology_generation,
                    proxy->slot_generation,
                };
                luaL_setmetatable(
                    lua_state, SYSTEM_PLANET_COLLECTION_METATABLE_);
            } else {
                lua_pushnil(lua_state);
            }
            return 1;
        }

        int System_New_Index_(lua_State* lua_state) {
            const s_system_proxy* proxy = Check_System_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "system mutation is available only during galaxy generation");
            }
            const char* key = luaL_checkstring(lua_state, 2);
            const lua_Integer value = luaL_checkinteger(lua_state, 3);
            s_star_data& system = MOX::star[proxy->system_id];
            if (strcmp(key, "x") == 0 || strcmp(key, "y") == 0) {
                const std::int16_t maximum = strcmp(key, "x") == 0
                    ? MOX::MAP_MAX_X
                    : MOX::MAP_MAX_Y;
                if (value < 0 || value >= maximum) {
                    return luaL_error(lua_state,
                        "system coordinate is outside the map");
                }
                if (strcmp(key, "x") == 0) {
                    system.x = static_cast<std::int16_t>(value);
                } else {
                    system.y = static_cast<std::int16_t>(value);
                }
                return 0;
            }
            if (strcmp(key, "spectral_class") == 0) {
                if (value < STAR_CLASS_B || value > STAR_CLASS_BLACK_HOLE) {
                    return luaL_error(lua_state,
                        "system spectral class is outside its domain");
                }
                system.spectral_class = static_cast<std::int8_t>(value);
                return 0;
            }
            if (strcmp(key, "special") == 0) {
                if (value < SYSTEM_SPECIAL_NO_SPECIAL ||
                    value >= SYSTEM_SPECIAL_COUNT) {
                    return luaL_error(lua_state,
                        "system special is outside its domain");
                }
                for (std::int16_t orbit = 0; orbit < 5; ++orbit) {
                    const std::int16_t planet_id =
                        system.planet_index[orbit];
                    if (planet_id >= 0 &&
                        MOX::planet[planet_id].planet_special !=
                            SYSTEM_SPECIAL_NO_SPECIAL &&
                        MOX::planet[planet_id].planet_special != value) {
                        return luaL_error(lua_state,
                            "system special conflicts with a planet special");
                    }
                }
                system.system_special = static_cast<std::int8_t>(value);
                return 0;
            }
            if (strcmp(key, "visited") == 0) {
                if (value < 0 || value > UINT8_MAX) {
                    return luaL_error(lua_state,
                        "system visited mask is outside its domain");
                }
                system.visited = static_cast<std::uint8_t>(value);
                return 0;
            }
            return luaL_error(lua_state, "immutable Lua game object");
        }

        int System_Set_Wormhole_Method_(lua_State* lua_state) {
            const s_system_proxy* proxy = Check_System_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "wormhole mutation is available only during galaxy generation");
            }
            std::int16_t destination_id = -1;
            if (!lua_isnoneornil(lua_state, 2) &&
                (!Read_System(lua_state, 2, &destination_id) ||
                    destination_id == proxy->system_id)) {
                return luaL_error(lua_state,
                    "invalid wormhole destination system");
            }
            s_star_data& source = MOX::star[proxy->system_id];
            if (source.wormhole_star_id >= 0) {
                MOX::star[source.wormhole_star_id].wormhole_star_id = -1;
                source.wormhole_star_id = -1;
            }
            if (lua_isnoneornil(lua_state, 2)) {
                return 0;
            }
            s_star_data& destination = MOX::star[destination_id];
            if (destination.wormhole_star_id >= 0) {
                MOX::star[destination.wormhole_star_id].wormhole_star_id = -1;
            }
            source.wormhole_star_id = destination_id;
            destination.wormhole_star_id = proxy->system_id;
            return 0;
        }

        int System_Delete_Method_(::game::s_world_state* world_state, lua_State* lua_state) {
            const s_system_proxy* proxy = Check_System_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "system deletion is available only during galaxy generation");
            }
            const std::int16_t system_id = proxy->system_id;
            for (std::int16_t player_id = 0;
                 player_id < MOX::NUM_PLAYERS;
                 ++player_id) {
                std::int16_t home_system = -1;
                if (!Resolve_Planet_System(
                        MOX::player[player_id].home_planet_id,
                        &home_system) ||
                    home_system == system_id) {
                    return luaL_error(lua_state,
                        "player home system cannot be deleted");
                }
            }
            for (std::int16_t orbit = 4; orbit >= 0; --orbit) {
                const std::int16_t planet_id =
                    MOX::star[system_id].planet_index[orbit];
                if (planet_id >= 0 && !Delete_Planet_(planet_id)) {
                    return luaL_error(lua_state,
                        "system with a colony cannot be deleted");
                }
            }
            s_star_data& removed = MOX::star[system_id];
            if (removed.wormhole_star_id >= 0) {
                MOX::star[removed.wormhole_star_id].wormhole_star_id = -1;
            }
            for (std::int16_t ship_id = 0;
                 ship_id < MOX::NUM_SHIPS;
                 ++ship_id) {
                if (MOX::ship[ship_id].location == system_id) {
                    const std::int16_t officer_id =
                        MOX::ship[ship_id].officer_index;
                    if (officer_id >= 0 && officer_id < MAX_LEADERS) {
                        ::game::Leader(world_state, officer_id).status = 0;
                        ::game::Leader(world_state, officer_id).location = -1;
                    }
                    MOX::ship[ship_id].status = 5;
                    MOX::ship[ship_id].location = -1;
                    MOX::ship[ship_id].officer_index = -1;
                    if (!Advance_Ship_Generation(ship_id)) {
                        return luaL_error(lua_state,
                            "unable to expire deleted ship identity");
                    }
                }
            }
            for (std::int16_t leader_id = 0;
                 leader_id < MAX_LEADERS;
                 ++leader_id) {
                if (::game::Leader(world_state, leader_id).status == 5 &&
                    ::game::Leader(world_state, leader_id).location == system_id) {
                    ::game::Leader(world_state, leader_id).status = -1;
                    ::game::Leader(world_state, leader_id).location = -1;
                } else if (::game::Leader(world_state, leader_id).status == 1 &&
                    ::game::Leader(world_state, leader_id).type == LEADER_TYPE_COLONY &&
                    ::game::Leader(world_state, leader_id).location == system_id) {
                    ::game::Leader(world_state, leader_id).status = 0;
                    ::game::Leader(world_state, leader_id).location = -1;
                }
            }
            memset(&MOX::star[system_id], 0, sizeof(s_star_data));
            MOX::star[system_id].spectral_class = -1;
            MOX::star[system_id].wormhole_star_id = -1;
            memset(MOX::star[system_id].planet_index,
                0xff,
                sizeof(MOX::star[system_id].planet_index));
            Advance_Generation_(system_generations_, system_id);
            return 0;
        }

        int System_Method_Index_(lua_State* lua_state) {
            const auto* proxy = static_cast<s_system_proxy*>(
                luaL_checkudata(lua_state, 1, SYSTEM_METATABLE_));
            const char* key = luaL_checkstring(lua_state, 2);
            if (strcmp(key, "set_wormhole") == 0) {
                lua_pushcfunction(lua_state, System_Set_Wormhole_Method_);
                return 1;
            }
            if (strcmp(key, "delete") == 0) {
                lua_pushcfunction(lua_state, [](lua_State* state) { return System_Delete_Method_(&::game::world, state); });
                return 1;
            }
            return System_Index_(lua_state);
        }

        int System_Equal_(lua_State* lua_state) {
            const auto* left = static_cast<s_system_proxy*>(
                luaL_testudata(lua_state, 1, SYSTEM_METATABLE_));
            const auto* right = static_cast<s_system_proxy*>(
                luaL_testudata(lua_state, 2, SYSTEM_METATABLE_));
            lua_pushboolean(lua_state,
                left != nullptr && right != nullptr &&
                    left->system_id == right->system_id &&
                    left->topology_generation == right->topology_generation &&
                    left->slot_generation == right->slot_generation);
            return 1;
        }

        int Planet_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLANET_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "planet creation is available only during galaxy generation");
            }
            std::int16_t source_id = -1;
            std::int16_t system_id = -1;
            const lua_Integer orbit = luaL_checkinteger(lua_state, 4);
            if (!Read_Planet(lua_state, 2, &source_id) ||
                !Read_System(lua_state, 3, &system_id) || orbit < 0 ||
                orbit >= 5) {
                return luaL_error(lua_state,
                    "invalid planet copy source or destination");
            }
            if (MOX::star[system_id].planet_index[orbit] != -1) {
                return luaL_error(lua_state,
                    "planet capacity exceeded or destination orbit is occupied");
            }
            std::int16_t planet_id = -1;
            for (std::int16_t candidate = 0;
                 candidate < MOX::planet_count;
                 ++candidate) {
                if (MOX::planet[candidate].star_index < 0) {
                    planet_id = candidate;
                    break;
                }
            }
            if (planet_id < 0) {
                if (MOX::planet_count >= MAX_PLANETS) {
                    return luaL_error(lua_state, "planet capacity exceeded");
                }
                planet_id = MOX::planet_count++;
            }
            MOX::planet[planet_id] = MOX::planet[source_id];
            MOX::planet[planet_id].star_index = system_id;
            MOX::planet[planet_id].orbit = static_cast<std::int8_t>(orbit);
            MOX::planet[planet_id].colony_index = -1;
            MOX::star[system_id].planet_index[orbit] = planet_id;
            Push_Planet(lua_state, planet_id);
            return 1;
        }

        int Planet_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLANET_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const lua_Integer planet_id = luaL_checkinteger(lua_state, 2);
            if (planet_id < 0 || planet_id > INT16_MAX) {
                lua_pushnil(lua_state);
            } else {
                Push_Planet(
                    lua_state, static_cast<std::int16_t>(planet_id));
            }
            return 1;
        }

        int System_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SYSTEM_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "system creation is available only during galaxy generation");
            }
            std::int16_t source_id = -1;
            if (!Read_System(lua_state, 2, &source_id)) {
                return luaL_error(lua_state,
                    "invalid system copy source or system capacity exceeded");
            }
            std::int16_t system_id = -1;
            for (std::int16_t candidate = 0;
                 candidate < MOX::NUM_STARS;
                 ++candidate) {
                if (MOX::star[candidate].spectral_class < STAR_CLASS_B ||
                    MOX::star[candidate].spectral_class >
                        STAR_CLASS_BLACK_HOLE) {
                    system_id = candidate;
                    break;
                }
            }
            if (system_id < 0) {
                if (MOX::NUM_STARS >= MAX_STARS) {
                    return luaL_error(lua_state,
                        "system capacity exceeded");
                }
                system_id = MOX::NUM_STARS++;
            }
            s_star_data& system = MOX::star[system_id];
            const s_star_data source = MOX::star[source_id];
            char name[sizeof(system.name)] = {};
            if (!MAPGEN::Copy_Generated_Star_Name(system_id, name, sizeof(name))) {
                return luaL_error(lua_state,
                    "generated star names are unavailable for system copy");
            }
            memset(&system, 0, sizeof(system));
            memcpy(system.name, name, sizeof(name));
            system.x = source.x;
            system.y = source.y;
            system.size = source.size;
            system.pict_type = source.pict_type;
            system.spectral_class = source.spectral_class;
            system.system_special = source.system_special;
            system.visited = source.visited;
            system.owner = -1;
            system.wormhole_star_id = -1;
            system.next_wfi_in_list = -1;
            memset(system.last_planet_selected,
                0xff,
                sizeof(system.last_planet_selected));
            memset(system.planet_index, 0xff, sizeof(system.planet_index));
            memset(system.officer_index, 0xff, sizeof(system.officer_index));
            memset(system.relocate_ship_to,
                0xff,
                sizeof(system.relocate_ship_to));
            memset(system.surrender_to, 0xff, sizeof(system.surrender_to));
            Push_System(lua_state, system_id);
            return 1;
        }

        int Planet_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLANET_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            if (MOX::planet == nullptr || MOX::planet_count < 0 ||
                MOX::planet_count > MAX_PLANETS) {
                return luaL_error(lua_state,
                    "native planet collection is unavailable");
            }
            lua_createtable(lua_state, MOX::planet_count, 0);
            lua_Integer sequence_index = 1;
            for (std::int16_t planet_id = 0;
                 planet_id < MOX::planet_count;
                 ++planet_id) {
                Push_Planet(lua_state, planet_id);
                if (!lua_isnil(lua_state, -1)) {
                    lua_rawseti(lua_state, -2, sequence_index++);
                } else {
                    lua_pop(lua_state, 1);
                }
            }
            return 1;
        }

        int System_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SYSTEM_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const lua_Integer system_id = luaL_checkinteger(lua_state, 2);
            if (system_id < 0 || system_id > INT16_MAX) {
                lua_pushnil(lua_state);
            } else {
                Push_System(
                    lua_state, static_cast<std::int16_t>(system_id));
            }
            return 1;
        }

        int System_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SYSTEM_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            if (MOX::star == nullptr || MOX::NUM_STARS < 0 ||
                MOX::NUM_STARS > MAX_STARS) {
                return luaL_error(lua_state,
                    "native system collection is unavailable");
            }
            lua_createtable(lua_state, MOX::NUM_STARS, 0);
            lua_Integer sequence_index = 1;
            for (std::int16_t system_id = 0;
                 system_id < MOX::NUM_STARS;
                 ++system_id) {
                Push_System(lua_state, system_id);
                if (!lua_isnil(lua_state, -1)) {
                    lua_rawseti(lua_state, -2, sequence_index++);
                } else {
                    lua_pop(lua_state, 1);
                }
            }
            return 1;
        }

        int System_Planet_Collection_All_(lua_State* lua_state) {
            const auto* collection = static_cast<s_collection_proxy*>(
                luaL_checkudata(
                    lua_state, 1, SYSTEM_PLANET_COLLECTION_METATABLE_));
            if (!Game_State_Access_Allowed() ||
                collection->topology_generation != topology_generation_ ||
                collection->system_id < 0 ||
                collection->system_id >= MAX_STARS ||
                collection->slot_generation != Current_Generation_(system_generations_, collection->system_id) ||
                !Valid_System_(collection->system_id)) {
                return luaL_error(lua_state,
                    "expired system planet collection");
            }
            lua_createtable(lua_state, 5, 0);
            lua_Integer sequence_index = 1;
            for (std::int8_t orbit = 0; orbit < 5; ++orbit) {
                const std::int16_t planet_id =
                    MOX::star[collection->system_id].planet_index[orbit];
                if (planet_id < 0) {
                    continue;
                }
                Push_Planet(lua_state, planet_id);
                if (!lua_isnil(lua_state, -1)) {
                    lua_rawseti(lua_state, -2, sequence_index++);
                } else {
                    lua_pop(lua_state, 1);
                }
            }
            return 1;
        }

        int Collection_Index_(lua_State* lua_state) {
            const char* key = luaL_checkstring(lua_state, 2);
            if (luaL_testudata(
                    lua_state, 1, PLANET_COLLECTION_METATABLE_) != nullptr) {
                if (strcmp(key, "get") == 0) {
                    lua_pushcfunction(lua_state, Planet_Collection_Get_);
                    return 1;
                }
                if (strcmp(key, "all") == 0) {
                    lua_pushcfunction(lua_state, Planet_Collection_All_);
                    return 1;
                }
                if (strcmp(key, "copy") == 0) {
                    lua_pushcfunction(lua_state, Planet_Collection_Copy_);
                    return 1;
                }
            } else if (luaL_testudata(
                           lua_state,
                           1,
                           SYSTEM_COLLECTION_METATABLE_) != nullptr) {
                if (strcmp(key, "get") == 0) {
                    lua_pushcfunction(lua_state, System_Collection_Get_);
                    return 1;
                }
                if (strcmp(key, "all") == 0) {
                    lua_pushcfunction(lua_state, System_Collection_All_);
                    return 1;
                }
                if (strcmp(key, "copy") == 0) {
                    lua_pushcfunction(lua_state, System_Collection_Copy_);
                    return 1;
                }
            } else {
                luaL_checkudata(
                    lua_state, 1, SYSTEM_PLANET_COLLECTION_METATABLE_);
                if (strcmp(key, "all") == 0) {
                    lua_pushcfunction(
                        lua_state, System_Planet_Collection_All_);
                    return 1;
                }
            }
            lua_pushnil(lua_state);
            return 1;
        }

        void Create_Metatable_(lua_State* lua_state,
            const char* name,
            lua_CFunction index,
            lua_CFunction equal = nullptr,
            lua_CFunction new_index = Immutable_New_Index_) {
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

    void Create_Planet_System_Metatables(lua_State* lua_state) {
        Create_Metatable_(
            lua_state, PLANET_METATABLE_, Planet_Index_, Planet_Equal_, Planet_New_Index_);
        Create_Metatable_(
            lua_state,
            SYSTEM_METATABLE_,
            System_Method_Index_,
            System_Equal_,
            System_New_Index_);
        Create_Metatable_(
            lua_state, PLANET_COLLECTION_METATABLE_, Collection_Index_);
        Create_Metatable_(
            lua_state, SYSTEM_COLLECTION_METATABLE_, Collection_Index_);
        Create_Metatable_(lua_state,
            SYSTEM_PLANET_COLLECTION_METATABLE_,
            Collection_Index_);
    }

    void Advance_Topology_Generation() {
        ++topology_generation_;
        if (topology_generation_ == 0) {
            topology_generation_ = 1;
        }
        memset(planet_generations_, 0, sizeof(planet_generations_));
        memset(system_generations_, 0, sizeof(system_generations_));
        Reset_Leader_Nebula_Generations();
    }

    std::uint64_t Topology_Generation() {
        return topology_generation_;
    }

    void Restore_Topology_Generation(std::uint64_t generation) {
        topology_generation_ = generation == 0 ? 1 : generation;
        memset(planet_generations_, 0, sizeof(planet_generations_));
        memset(system_generations_, 0, sizeof(system_generations_));
        Reset_Leader_Nebula_Generations();
    }

    void Push_Planet_Collection(lua_State* lua_state) {
        auto* collection = static_cast<s_collection_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_collection_proxy), 0));
        *collection = {};
        luaL_setmetatable(lua_state, PLANET_COLLECTION_METATABLE_);
    }

    void Push_System_Collection(lua_State* lua_state) {
        auto* collection = static_cast<s_collection_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_collection_proxy), 0));
        *collection = {};
        luaL_setmetatable(lua_state, SYSTEM_COLLECTION_METATABLE_);
    }

    bool Push_Planet(lua_State* lua_state, std::int16_t planet_id) {
        std::int16_t system_id = -1;
        if (!Resolve_Planet_System(planet_id, &system_id)) {
            lua_pushnil(lua_state);
            return false;
        }
        const s_planet_data& planet = MOX::planet[planet_id];
        auto* proxy = static_cast<s_planet_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_planet_proxy), 0));
        *proxy = {
            planet_id,
            system_id,
            planet.orbit,
            topology_generation_,
            Current_Generation_(planet_generations_, planet_id),
        };
        luaL_setmetatable(lua_state, PLANET_METATABLE_);
        return true;
    }

    bool Read_Planet(lua_State* lua_state,
        int index,
        std::int16_t* planet_id) {
        const auto* proxy = static_cast<s_planet_proxy*>(
            luaL_testudata(lua_state, index, PLANET_METATABLE_));
        if (!Valid_Planet_(proxy) || planet_id == nullptr) {
            return false;
        }
        *planet_id = proxy->planet_id;
        return true;
    }

    bool Read_System(lua_State* lua_state,
        int index,
        std::int16_t* system_id) {
        const auto* proxy = static_cast<s_system_proxy*>(
            luaL_testudata(lua_state, index, SYSTEM_METATABLE_));
        if (proxy == nullptr ||
            proxy->system_id < 0 || proxy->system_id >= MAX_STARS ||
            proxy->topology_generation != topology_generation_ ||
            proxy->slot_generation != Current_Generation_(system_generations_, proxy->system_id) ||
            !Valid_System_(proxy->system_id) || system_id == nullptr) {
            return false;
        }
        *system_id = proxy->system_id;
        return true;
    }

    bool Push_System(lua_State* lua_state, std::int16_t system_id) {
        if (!Valid_System_(system_id)) {
            lua_pushnil(lua_state);
            return false;
        }
        auto* proxy = static_cast<s_system_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_system_proxy), 0));
        *proxy = {system_id,
            topology_generation_,
            Current_Generation_(system_generations_, system_id)};
        luaL_setmetatable(lua_state, SYSTEM_METATABLE_);
        return true;
    }

    bool Resolve_Planet_System(
        std::int16_t planet_id, std::int16_t* system_id) {
        if (system_id == nullptr || MOX::planet == nullptr ||
            MOX::planet_count < 0 || MOX::planet_count > MAX_PLANETS ||
            planet_id < 0 || planet_id >= MOX::planet_count) {
            return false;
        }
        const s_planet_data& planet = MOX::planet[planet_id];
        if (!Valid_Planet_Identity_(
                planet_id, planet.star_index, planet.orbit)) {
            return false;
        }
        *system_id = planet.star_index;
        return true;
    }

} // namespace orion2re::lua::game::internal