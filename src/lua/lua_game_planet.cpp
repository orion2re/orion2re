#include "pch.h"
using game::ANTARES_POSITION;
using game::NO_STAR;
using game::NO_SHIP_LOCATION;

#include "lua/lua_game_internal.h"
#include "game/legacy_ids.h"

#include <climits>
#include <cstring>

#include "lua/lua_mod_runtime.h"
#include "lua/lua_game_ship_identity.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

using game::current_state;
using game::id_star;

using game::Leader_Assignment_Row;
using game::Star_Key_At_Row;
using game::Star_Row;

using game::Create_Planet;
using game::Create_Star;
using game::Erase_Planet;
using game::Replace_Draft_Star;
using game::e_ship_location;
using game::e_ship_target;

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
            id_planet planet_id{};
            id_star system_key = NO_STAR;
            std::int8_t orbit = -1;
            std::uint64_t topology_generation = 0;
            std::uint64_t slot_generation = 0;
        };

        struct s_system_proxy {
            id_star key = NO_STAR;
            std::uint64_t topology_generation = 0;
        };

        struct s_collection_proxy {
            id_star system_key = NO_STAR;
            std::uint64_t topology_generation = 0;
        };

        std::uint64_t topology_generation_ = 1;
        std::unordered_map<id_planet, std::uint64_t> planet_generations_;

        std::uint64_t Planet_Generation_(id_planet planet_id) {
            const auto it = planet_generations_.find(planet_id);
            return it == planet_generations_.end() ? 1 : it->second;
        }

        void Advance_Planet_Generation_(id_planet planet_id) {
            auto it = planet_generations_.try_emplace(planet_id, 1).first;
            if (++it->second == 0) it->second = 1;
        }

        bool Valid_System_(id_star key) {
            if (key == NO_STAR || current_state.stars.size() > MAX_STARS) return false;
            const auto star = current_state.stars.find(key);
            return star != current_state.stars.end() && star->second.spectral_class >= STAR_CLASS_B &&
                star->second.spectral_class <= STAR_CLASS_BLACK_HOLE;
        }

        bool Valid_System_(std::int16_t position) {
            return Valid_System_(Star_Key_At_Row(current_state, position));
        }

        s_planet_data* Resolve_Planet_Record_(id_planet id) {
            auto& planets = (galaxy_planet_candidate ? *galaxy_planet_candidate : current_state).planets;
            if (planets.size() > MAX_PLANETS) return nullptr;
            const auto it = planets.find(id);
            return it == planets.end() ? nullptr : &it->second;
        }

        bool Valid_Planet_Identity_(id_planet id, const s_planet_data& planet,
            id_star system_key,
            std::int8_t orbit) {
            return Valid_System_(system_key) && orbit >= 0 && orbit < 5 &&
                planet.star_id == system_key && planet.orbit == orbit &&
                current_state.stars.at(system_key).orbit_planet_ids[orbit] == id;
        }

        std::array<id_planet, 5> System_Orbits_(id_star key) {
            std::array<id_planet, 5> orbits;
            for (size_t orbit = 0; orbit < orbits.size(); ++orbit) {
                orbits[orbit] = current_state.stars.at(key).orbit_planet_ids[orbit];
            }
            return orbits;
        }

        bool Valid_Planet_(const s_planet_proxy* proxy) {
            if (proxy == nullptr || proxy->topology_generation != topology_generation_) return false;
            const auto* planet = Resolve_Planet_Record_(proxy->planet_id);
            return planet &&
                proxy->slot_generation == Planet_Generation_(proxy->planet_id) &&
                Valid_Planet_Identity_(proxy->planet_id, *planet,
                    proxy->system_key,
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

        bool Delete_Colony_(id_colony colony_id) {
            if (!current_state.colonies.contains(colony_id)) {
                return false;
            }
            auto& colony = current_state.colonies.at(colony_id);
            const auto planet_id = colony.planet_id;
            auto* planet = Resolve_Planet_Record_(planet_id);
            if (!planet || planet->colony_id != colony_id) {
                return false;
            }
            const auto officer_id = colony.officer;
            if (officer_id >= 0) {
                if (officer_id >= MAX_LEADERS) return false;
                const std::int8_t player_id =
                    legacy_world::Encode_Player_Slot(current_state.leaders.at(officer_id).employer);
                const auto star = current_state.stars.find(planet->star_id);
                if (player_id >= 0 && player_id < ::game::current_state.player_count &&
                    star != current_state.stars.end() && star->second.officer_index[player_id] == officer_id) {
                    star->second.officer_index[player_id] = -1;
                }
                auto& leader = current_state.leaders.at(officer_id);
                leader.status = 0;
                leader.Clear_Location();
            }
            planet->colony_id = NO_COLONY;
            current_state.colonies.erase(colony_id);
            return true;
        }

        bool Delete_Planet_(id_planet id) {
            auto* planet = Resolve_Planet_Record_(id);
            if (!planet) {
                return false;
            }
            for (std::int16_t player_id = 0;
                 player_id < ::game::current_state.player_count;
                 ++player_id) {
                if (::game::current_state.players[player_id].home_planet_id == id) {
                    return false;
                }
            }
            if (planet->colony_id != NO_COLONY && !Delete_Colony_(planet->colony_id)) {
                return false;
            }
            current_state.stars.at(planet->star_id).orbit_planet_ids[planet->orbit] = NO_PLANET;
            Erase_Planet((galaxy_planet_candidate ? *galaxy_planet_candidate : current_state), id);
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
            id_star system_id = NO_STAR;
            if (!Read_System(lua_state, 2, &system_id)) {
                return luaL_error(lua_state, "invalid destination system");
            }
            const lua_Integer orbit = luaL_checkinteger(lua_state, 3);
            if (orbit < 0 || orbit >= 5) {
                return luaL_error(lua_state, "planet orbit is outside its domain");
            }
            s_planet_data& planet = *Resolve_Planet_Record_(proxy->planet_id);
            const auto occupied = System_Orbits_(system_id)[orbit];
            if (occupied != NO_PLANET && occupied != proxy->planet_id) {
                return luaL_error(lua_state,
                    "destination system orbit is occupied");
            }
            current_state.stars.at(planet.star_id).orbit_planet_ids[planet.orbit] = NO_PLANET;
            current_state.stars.at(system_id).orbit_planet_ids[orbit] = proxy->planet_id;
            planet.star_id = system_id;
            planet.orbit = static_cast<std::int8_t>(orbit);
            Advance_Planet_Generation_(proxy->planet_id);
            return 0;
        }

        s_system_proxy* Check_System_(lua_State* lua_state, int index) {
            auto* proxy = static_cast<s_system_proxy*>(
                luaL_checkudata(lua_state, index, SYSTEM_METATABLE_));
            if (proxy == nullptr ||
                proxy->topology_generation != topology_generation_ ||
                !Valid_System_(proxy->key) ||
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

            s_planet_data& planet = *Resolve_Planet_Record_(proxy->planet_id);
            s_star_data& system = current_state.stars.at(proxy->system_key);
            if (system.system_special != SYSTEM_SPECIAL_NO_SPECIAL &&
                system.system_special != planet.planet_special) {
                return luaL_error(lua_state,
                    "planet special conflicts with its system special");
            }
            const auto orbits = System_Orbits_(proxy->system_key);
            for (int8_t orbit = 0; orbit < 5; ++orbit) {
                const auto planet_id = orbits[orbit];
                if (planet_id == NO_PLANET || planet_id == proxy->planet_id) {
                    continue;
                }
                const auto* sibling = Resolve_Planet_Record_(planet_id);
                if (!sibling || sibling->star_id != proxy->system_key || sibling->orbit != orbit) {
                    return luaL_error(lua_state,
                        "planet system topology is invalid");
                }
                if (sibling->planet_special !=
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
            s_planet_data& planet = *Resolve_Planet_Record_(proxy->planet_id);
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
                lua_pushinteger(lua_state, proxy->planet_id.id);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state, Valid_Planet_(proxy));
                return 1;
            }
            Check_Planet_(lua_state, 1);
            const s_planet_data& planet = *Resolve_Planet_Record_(proxy->planet_id);
            if (strcmp(key, "system") == 0) {
                Push_System(lua_state, proxy->system_key);
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
                    left->system_key == right->system_key &&
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
                lua_pushinteger(lua_state, proxy->key.id);
                return 1;
            }
            if (strcmp(key, "valid") == 0) {
                lua_pushboolean(lua_state,
                    proxy->topology_generation == topology_generation_ && Valid_System_(proxy->key));
                return 1;
            }
            Check_System_(lua_state, 1);
            const s_star_data& system = current_state.stars.at(proxy->key);
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
                lua_pushinteger(lua_state, system.visited.To_Byte());
            } else if (strcmp(key, "wormhole") == 0) {
                Push_System(lua_state, system.wormhole_star_id);
            } else if (strcmp(key, "planets") == 0) {
                auto* collection = static_cast<s_collection_proxy*>(
                    lua_newuserdatauv(
                        lua_state, sizeof(s_collection_proxy), 0));
                *collection = {
                    proxy->key,
                    proxy->topology_generation,
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
            s_star_data& system = current_state.stars.at(proxy->key);
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
                for (const auto planet_id : System_Orbits_(proxy->key)) {
                    if (planet_id == NO_PLANET) continue;
                    const auto* planet = Resolve_Planet_Record_(planet_id);
                    if (!planet || (planet->planet_special != SYSTEM_SPECIAL_NO_SPECIAL &&
                        planet->planet_special != value)) {
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
                system.visited = ::game::player_bitmap::From_Byte(static_cast<std::uint8_t>(value));
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
            id_star destination_id = NO_STAR;
            if (!lua_isnoneornil(lua_state, 2) &&
                (!Read_System(lua_state, 2, &destination_id) ||
                    destination_id == proxy->key)) {
                return luaL_error(lua_state,
                    "invalid wormhole destination system");
            }
            s_star_data& source = current_state.stars.at(proxy->key);
            if (const auto exit = current_state.stars.find(source.wormhole_star_id); exit != current_state.stars.end()) {
                exit->second.wormhole_star_id = NO_STAR;
            }
            source.wormhole_star_id = destination_id;
            if (destination_id == NO_STAR) return 0;
            s_star_data& destination = current_state.stars.at(destination_id);
            if (const auto exit = current_state.stars.find(destination.wormhole_star_id); exit != current_state.stars.end()) {
                exit->second.wormhole_star_id = NO_STAR;
            }
            destination.wormhole_star_id = proxy->key;
            return 0;
        }

        int System_Delete_Method_(lua_State* lua_state) {
            const s_system_proxy* proxy = Check_System_(lua_state, 1);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "system deletion is available only during galaxy generation");
            }
            const auto system_id = Star_Row(current_state, proxy->key);
            for (std::int16_t player_id = 0;
                 player_id < ::game::current_state.player_count;
                 ++player_id) {
                std::int16_t home_system = -1;
                if (!Resolve_Planet_System(
                        ::game::current_state.players[player_id].home_planet_id,
                        &home_system) ||
                    home_system == system_id) {
                    return luaL_error(lua_state,
                        "player home system cannot be deleted");
                }
            }
            const auto orbits = System_Orbits_(proxy->key);
            for (auto it = orbits.rbegin(); it != orbits.rend(); ++it) {
                if (*it != NO_PLANET && !Delete_Planet_(*it)) {
                    return luaL_error(lua_state,
                        "system with a colony cannot be deleted");
                }
            }
            s_star_data& removed = current_state.stars.at(proxy->key);
            if (const auto exit = current_state.stars.find(removed.wormhole_star_id); exit != current_state.stars.end()) {
                exit->second.wormhole_star_id = NO_STAR;
            }
            for (std::int16_t ship_id = 0;
                 ship_id < static_cast<int16_t>(current_state.ship_slots.size());
                 ++ship_id) {
                const auto location = ship_slots::At(current_state, ship_id).position.Get();
                if (location.kind == e_ship_location::at_star && location.target == e_ship_target::star && location.star_id == proxy->key) {
                    const std::int16_t officer_id =
                        ship_slots::At(current_state, ship_id).officer_index;
                    if (officer_id >= 0 && officer_id < MAX_LEADERS) {
                        auto& leader = current_state.leaders.at(officer_id);
                        leader.status = 0;
                        leader.Clear_Location();
                    }
                    ship_slots::At(current_state, ship_id).status = 5;
                    ship_slots::At(current_state, ship_id).position.Set(NO_SHIP_LOCATION);
                    ship_slots::At(current_state, ship_id).officer_index = -1;
                    if (!Advance_Ship_Generation(ship_id)) {
                        return luaL_error(lua_state,
                            "unable to expire deleted ship identity");
                    }
                }
            }
            for (auto& leader : current_state.leaders) {
                if (leader.status == 5 && Leader_Assignment_Row(current_state, leader) == system_id) {
                    leader.status = -1;
                    leader.Clear_Location();
                } else if (leader.status == 1 && leader.type == LEADER_TYPE_COLONY && Leader_Assignment_Row(current_state, leader) == system_id) {
                    leader.status = 0;
                    leader.Clear_Location();
                }
            }
            removed = {.id = proxy->key};
            removed.spectral_class = -1;
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
                lua_pushcfunction(lua_state, System_Delete_Method_);
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
                    left->key == right->key &&
                    left->topology_generation == right->topology_generation);
            return 1;
        }

        int Planet_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLANET_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "planet creation is available only during galaxy generation");
            }
            id_planet source_id{};
            id_star system_id = NO_STAR;
            const lua_Integer orbit = luaL_checkinteger(lua_state, 4);
            if (!Read_Planet(lua_state, 2, &source_id) ||
                !Read_System(lua_state, 3, &system_id) || orbit < 0 ||
                orbit >= 5) {
                return luaL_error(lua_state,
                    "invalid planet copy source or destination");
            }
            if (System_Orbits_(system_id)[orbit] != NO_PLANET) {
                return luaL_error(lua_state,
                    "planet capacity exceeded or destination orbit is occupied");
            }
            s_planet_data copy = *Resolve_Planet_Record_(source_id);
            copy.star_id = system_id;
            copy.orbit = static_cast<std::int8_t>(orbit);
            copy.colony_id = NO_COLONY;
            const auto id = Create_Planet((galaxy_planet_candidate ? *galaxy_planet_candidate : current_state), copy);
            if (!id) return luaL_error(lua_state, "planet capacity exceeded");
            current_state.stars.at(system_id).orbit_planet_ids[orbit] = *id;
            Push_Planet(lua_state, *id);
            return 1;
        }

        int Planet_Collection_Get_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLANET_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const lua_Integer planet_id = luaL_checkinteger(lua_state, 2);
            if (planet_id < 0 || planet_id > UINT32_MAX) {
                lua_pushnil(lua_state);
            } else {
                Push_Planet(
                    lua_state, id_planet{static_cast<uint32_t>(planet_id)});
            }
            return 1;
        }

        int System_Collection_Copy_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SYSTEM_COLLECTION_METATABLE_);
            if (!Galaxy_State_Mutation_Allowed()) {
                return luaL_error(lua_state,
                    "system creation is available only during galaxy generation");
            }
            id_star source_id = NO_STAR;
            if (!Read_System(lua_state, 2, &source_id)) {
                return luaL_error(lua_state,
                    "invalid system copy source or system capacity exceeded");
            }
            int16_t system_id = 0;
            while (system_id < std::ssize(current_state.stars) && Valid_System_(system_id)) ++system_id;
            std::optional<id_star> created;
            if (system_id < std::ssize(current_state.stars)) {
                created = Replace_Draft_Star(current_state, system_id);
            } else if (std::ssize(current_state.stars) < MAX_STARS) {
                created = Create_Star(current_state);
            }
            if (!created) return luaL_error(lua_state, "system capacity exceeded or identity allocation failed");
            s_star_data& system = current_state.stars.at(*created);
            const s_star_data source = current_state.stars.at(source_id);
            char name[sizeof(system.name)] = {};
            if (!MAPGEN::Copy_Generated_Star_Name(system_id, name, sizeof(name))) {
                return luaL_error(lua_state,
                    "generated star names are unavailable for system copy");
            }
            system = {.id = *created};
            memcpy(system.name, name, sizeof(name));
            system.x = source.x;
            system.y = source.y;
            system.size = source.size;
            system.pict_type = source.pict_type;
            system.spectral_class = source.spectral_class;
            system.system_special = source.system_special;
            system.visited = source.visited;
            system.owner = factions::From_Legacy(-1);
            system.wormhole_star_id = NO_STAR;
            system.next_wfi_in_list = NO_STAR;
            memset(system.last_planet_selected,
                0xff,
                sizeof(system.last_planet_selected));
            for (auto& link : system.orbit_planet_ids) link = NO_PLANET;
            memset(system.officer_index, 0xff, sizeof(system.officer_index));
            memset(system.relocate_ship_to,
                0xff,
                sizeof(system.relocate_ship_to));
            memset(system.surrender_to, 0xff, sizeof(system.surrender_to));
            Push_System(lua_state, *created);
            return 1;
        }

        int Planet_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, PLANET_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            const auto& order = (galaxy_planet_candidate ? *galaxy_planet_candidate : current_state).planet_order;
            if (order.size() > MAX_PLANETS) return luaL_error(lua_state, "native planet collection is unavailable");
            lua_createtable(lua_state, static_cast<int>(order.size()), 0);
            lua_Integer position = 1;
            for (const auto id : order) {
                if (Push_Planet(lua_state, id)) lua_rawseti(lua_state, -2, position++);
                else lua_pop(lua_state, 1);
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
            if (system_id < 0 || system_id >= UINT32_MAX) {
                lua_pushnil(lua_state);
            } else {
                Push_System(lua_state, id_star{static_cast<uint32_t>(system_id)});
            }
            return 1;
        }

        int System_Collection_All_(lua_State* lua_state) {
            luaL_checkudata(lua_state, 1, SYSTEM_COLLECTION_METATABLE_);
            if (!Game_State_Access_Allowed()) {
                return luaL_error(lua_state,
                    "game state is unavailable outside a callback");
            }
            if (current_state.stars.size() > MAX_STARS) {
                return luaL_error(lua_state,
                    "native system collection is unavailable");
            }
            lua_createtable(lua_state, std::ssize(current_state.stars), 0);
            lua_Integer sequence_index = 1;
            for (const auto system : current_state.star_order) {
                Push_System(lua_state, system);
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
                !Valid_System_(collection->system_key)) {
                return luaL_error(lua_state,
                    "expired system planet collection");
            }
            lua_createtable(lua_state, 5, 0);
            lua_Integer sequence_index = 1;
            for (const auto planet_id : System_Orbits_(collection->system_key)) {
                if (planet_id == NO_PLANET) {
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
        planet_generations_.clear();
        Reset_Leader_Nebula_Generations();
    }

    std::uint64_t Topology_Generation() {
        return topology_generation_;
    }

    void Restore_Topology_Generation(std::uint64_t generation) {
        topology_generation_ = generation == 0 ? 1 : generation;
        planet_generations_.clear();
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

    bool Push_Planet(lua_State* lua_state, id_planet planet_id) {
        std::int16_t system_id = -1;
        if (!Resolve_Planet_System(planet_id, &system_id)) {
            lua_pushnil(lua_state);
            return false;
        }
        const s_planet_data& planet = *Resolve_Planet_Record_(planet_id);
        auto* proxy = static_cast<s_planet_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_planet_proxy), 0));
        *proxy = {
            planet_id,
            planet.star_id,
            planet.orbit,
            topology_generation_,
            Planet_Generation_(planet_id),
        };
        luaL_setmetatable(lua_state, PLANET_METATABLE_);
        return true;
    }

    bool Read_Planet(lua_State* lua_state,
        int index,
        id_planet* planet_id, const s_planet_data** record) {
        const auto* proxy = static_cast<s_planet_proxy*>(
            luaL_testudata(lua_state, index, PLANET_METATABLE_));
        if (!Valid_Planet_(proxy) || planet_id == nullptr) {
            return false;
        }
        *planet_id = proxy->planet_id;
        if (record) *record = Resolve_Planet_Record_(*planet_id);
        return true;
    }

    bool Read_System(lua_State* lua_state, int index, id_star* system_id) {
        const auto* proxy = static_cast<s_system_proxy*>(
            luaL_testudata(lua_state, index, SYSTEM_METATABLE_));
        if (proxy == nullptr ||
            proxy->topology_generation != topology_generation_ ||
            !Valid_System_(proxy->key) || system_id == nullptr) {
            return false;
        }
        *system_id = proxy->key;
        return true;
    }

    bool Read_System(lua_State* lua_state, int index, std::int16_t* system_id) {
        id_star key;
        if (!system_id || !Read_System(lua_state, index, &key)) return false;
        *system_id = Star_Row(current_state, key);
        return *system_id >= 0;
    }

    bool Push_System(lua_State* lua_state, std::int16_t system_id) {
        return Push_System(lua_state, Star_Key_At_Row(current_state, system_id));
    }

    bool Push_System(lua_State* lua_state, id_star system_id) {
        if (!Valid_System_(system_id)) {
            lua_pushnil(lua_state);
            return false;
        }
        auto* proxy = static_cast<s_system_proxy*>(
            lua_newuserdatauv(lua_state, sizeof(s_system_proxy), 0));
        *proxy = {system_id, topology_generation_};
        luaL_setmetatable(lua_state, SYSTEM_METATABLE_);
        return true;
    }

    bool Resolve_Planet_System(
        id_planet planet_id, std::int16_t* system_id) {
        const auto* planet = Resolve_Planet_Record_(planet_id);
        if (!system_id || !planet || !Valid_Planet_Identity_(planet_id, *planet, planet->star_id, planet->orbit)) {
            return false;
        }
        *system_id = Star_Row(current_state, planet->star_id);
        return true;
    }

} // namespace orion2re::lua::game::internal
