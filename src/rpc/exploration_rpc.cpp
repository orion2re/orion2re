#include "pch.h"
#include "rpc/exploration_rpc.h"
#include "rpc/rpc_game.h"
#include "config/text_encoding.h"

#include <limits>

namespace exploration_rpc {
    namespace {
        bool Planet_Link(int32_t system, int32_t orbit, int32_t planet) {
            return planet >= 0 && planet < MOX::planet_count && MOX::planet != nullptr &&
                MOX::planet[planet].star_index == system && MOX::planet[planet].orbit == orbit;
        }

        bool Read_Planet(rpc::s_map_observation* observation, int32_t system, int32_t orbit, int32_t ordinal, int32_t index) {
            if (!Planet_Link(system, orbit, index)) return false;
            const auto* planet = &MOX::planet[index];
            rpc::s_planet_observation item;
            item.visible = true;
            item.system = system;
            item.orbit = orbit + 1;
            item.kind = planet->planet_type;
            if (item.kind < PLANET_TYPE_ASTEROID || item.kind > PLANET_TYPE_PLANET) return false;
            if (item.kind == PLANET_TYPE_PLANET) {
                const auto* star = &MOX::star[system];
                if (memchr(star->name, 0, sizeof(star->name)) == nullptr) return false;
                char name[200];
                snprintf(name, sizeof(name), "%s %s", star->name, MOX::roman_literals[ordinal]);
                name[30] = 0; // Native Do_Get_Planet_Name precision, without its shared temp buffer.
                config::Decode_Cp850_To_Utf8(name, strlen(name), &item.name);
                item.size = planet->size;
                item.gravity = planet->gravity_class;
                item.climate = planet->climate;
                item.minerals = planet->mineral_class;
                item.special = planet->planet_special;
                if (item.size < 0 || item.size >= PLANET_SIZE_COUNT || item.gravity < 0 || item.gravity >= PLANET_GRAVITY_COUNT ||
                    item.climate < 0 || item.climate >= PLANET_CLIMATE_COUNT || item.minerals < 0 || item.minerals >= MINERAL_RESOURCE_COUNT ||
                    item.special < 0 || item.special >= SYSTEM_SPECIAL_COUNT) return false;
                if (item.special == SYSTEM_SPECIAL_ORION_STAR) item.special = 0;
                item.food2 = planet->food_per_farmer;
                item.industry = COLCALC::minerals_per_mine[item.minerals];
                item.gravity_penalty = PLNTSUM::Productivity_Penalty(MOX::PLAYER_NUM, (int16_t)item.gravity);
                const int32_t colony_index = planet->colony_index;
                if (colony_index < -1 || colony_index >= MOX::NUM_COLONIES || (colony_index >= 0 && MOX::colony == nullptr)) return false;
                bool own_full = false;
                if (colony_index >= 0) {
                    const auto* colony = &MOX::colony[colony_index];
                    own_full = colony->owner == observation->viewer && colony->outpost_flag == 0;
                    if (own_full) {
                        if (colony->planet != index || colony->n_pops > MAX_COLONY_POPULATION || colony->climate < 0 || colony->climate >= PLANET_CLIMATE_COUNT) return false;
                        for (int32_t pop = 0; pop < colony->n_pops; ++pop) {
                            if (POP::Get_Race(colony->pop[pop]) >= 10) return false;
                        }
                    }
                }
                if (colony_index == -1 || own_full) {
                    item.max_population = COLCALC::Planet_Max_Population_For_Player(&MOX::planet[index], MOX::PLAYER_NUM);
                    item.max_population_available = true;
                }
            }
            observation->planets.push_back(std::move(item));
            return true;
        }
    } // namespace

    bool Read_Exploration(rpc::s_map_observation* observation) {
        const int16_t viewer = MOX::PLAYER_NUM;
        const bool omniscient = HAROLD::Player_Is_Omniscient(viewer);
        for (int16_t index = 0; index < MOX::NUM_STARS; ++index) {
            const auto* star = &MOX::star[index];
            rpc::s_system_observation item;
            item.x = star->x;
            item.y = star->y;
            item.visited = HAROLD::Player_Has_Visited(viewer, index) != 0;
            item.spectral_class = star->spectral_class;
            item.size = item.spectral_class == STAR_CLASS_BLACK_HOLE ? 0 : star->size;
            item.explored = SYS::Ok_To_View_System(index, viewer);
            int16_t counts[MAX_PLAYERS] = {};
            bool contact = false;
            int32_t ordinal = 0;
            for (int32_t orbit = 0; orbit < 5; ++orbit) {
                const int32_t planet_index = star->planet_index[orbit];
                if (planet_index == -1) continue;
                ++ordinal;
                // Unexplored malformed/hidden associations must not become an error oracle.
                const bool valid = planet_index >= 0 && planet_index < MOX::planet_count && MOX::planet != nullptr;
                if (!valid) {
                    if (item.explored) return false;
                    continue;
                }
                const int32_t colony_index = MOX::planet[planet_index].colony_index;
                const bool valid_colony = colony_index >= 0 && colony_index < MOX::NUM_COLONIES && MOX::colony != nullptr;
                bool own_full = false;
                if (valid_colony) {
                    const auto* colony = &MOX::colony[colony_index];
                    const int32_t owner = colony->owner;
                    const bool contacted = HAROLD::Contact_With_Player(viewer, (int16_t)owner);
                    contact |= contacted;
                    if (item.spectral_class != STAR_CLASS_BLACK_HOLE && owner >= 0 && owner < MOX::NUM_PLAYERS && (owner == viewer || contacted || omniscient)) {
                        if (colony->planet != planet_index || !Planet_Link(index, orbit, planet_index)) return false;
                        ++counts[owner];
                    }
                    own_full = owner == viewer && colony->outpost_flag == 0;
                }
                if ((item.explored || own_full) && !Read_Planet(observation, index, orbit, ordinal, planet_index)) return false;
            }
            int16_t owner = -1;
            if (counts[viewer] != 0)
                owner = viewer;
            else {
                int16_t maximum = 0;
                for (int16_t player = 0; player < MOX::NUM_PLAYERS; ++player) {
                    if (counts[player] > maximum) {
                        maximum = counts[player];
                        owner = player;
                    }
                }
            }
            item.owner_slot = owner;
            item.name_known = item.spectral_class != STAR_CLASS_BLACK_HOLE && (item.visited || omniscient || (owner >= 0 && contact));
            if (item.name_known) {
                if (memchr(star->name, 0, sizeof(star->name)) == nullptr) return false;
                char name[sizeof(star->name) + 3] = {};
                if (omniscient && viewer != owner && !item.visited)
                    snprintf(name, sizeof(name), HAROLD::s___s__00556ae4, star->name);
                else
                    memcpy(name, star->name, sizeof(star->name));
                int32_t outposts = 0;
                int32_t full = 0;
                for (int32_t orbit = 0; orbit < 5; ++orbit) {
                    const int32_t planet = star->planet_index[orbit];
                    if (planet == -1) continue;
                    if (planet < 0 || planet >= MOX::planet_count || MOX::planet == nullptr) return false;
                    const int32_t colony = MOX::planet[planet].colony_index;
                    if (colony < -1 || colony >= MOX::NUM_COLONIES || (colony >= 0 && MOX::colony == nullptr)) return false;
                    if (colony >= 0) {
                        if (MOX::colony[colony].planet != planet) return false;
                        if (MOX::colony[colony].outpost_flag != 0)
                            ++outposts;
                        else
                            ++full;
                    }
                }
                if (outposts != 0 && full == 0) strings::String_To_Lower(name);
                config::Decode_Cp850_To_Utf8(name, strlen(name), &item.name);
                item.name_available = true;
            }
            if (item.visited || omniscient) {
                const int32_t target = star->wormhole_star_id;
                if (target < -1 || target >= MOX::NUM_STARS) return false;
                item.wormhole_available = target == -1 ? rpc::EXPLORATION_EMPTY : rpc::EXPLORATION_CURRENT;
                item.wormhole_system = target;
            }
            if (item.explored) {
                const int64_t range = (MOX::player[viewer].ship_range * 3 + 1) / 2;
                if (range < 0 || range * range * 900 > std::numeric_limits<int32_t>::max()) return false;
                // The native distance predicate uses int32 arithmetic. Reject malformed
                // authorized geometry before calling it rather than invoking signed UB.
                uint8_t range_players = (uint8_t)(1u << viewer);
                for (int16_t player = 0; player < MOX::NUM_PLAYERS; ++player) {
                    if (MOX::player[viewer].treaty[player] == 2) range_players |= (uint8_t)(1u << player);
                }
                for (int32_t source = 0; source < MOX::NUM_STARS; ++source) {
                    if ((MOX::star[source].has_colony & range_players) == 0) continue;
                    const int32_t targets[] = {index, star->wormhole_star_id};
                    for (int32_t target : targets) {
                        if (target == -1 || (target != index && (MOX::star[target].visited & range_players) == 0)) continue;
                        if (struct_::Test_Bit_Field(MOX::star[source].black_hole_blocks, target)) continue;
                        const int64_t dx = (int64_t)MOX::star[target].x - MOX::star[source].x;
                        const int64_t dy = (int64_t)MOX::star[target].y - MOX::star[source].y;
                        if (dx * dx + dy * dy > std::numeric_limits<int32_t>::max()) return false;
                    }
                }
                item.in_extended_range = SHIPMOVE::Star_In_Extended_Range_Of_Player(&MOX::star[index], viewer);
            }
            observation->systems.push_back(std::move(item));
        }
        for (int16_t index = 0; index < MOX::NUM_NEBULAS; ++index) {
            observation->nebulas.push_back({MOX::nebula[index].x, MOX::nebula[index].y, (uint8_t)MOX::nebula[index].type % 12});
        }
        observation->planets_captured = true;
        return true;
    }
} // namespace exploration_rpc
