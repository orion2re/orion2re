#pragma once

#include "pch.h"
#include "game/world.h"

namespace test {
    inline s_planet_data& Planet(game::s_world_state* world, game::id_planet id) {
        auto [entry, inserted] = world->planets.try_emplace(id, s_planet_data{});
        world->next_planet_id = std::max(world->next_planet_id, uint64_t{id.id} + 1);
        return entry->second;
    }

    inline void Resize_Planets(game::s_world_state* world, int32_t count) {
        for (auto entry = world->planets.begin(); entry != world->planets.end();) {
            if (entry->first.id >= static_cast<uint32_t>(count)) entry = world->planets.erase(entry);
            else ++entry;
        }
        for (int32_t id = 0; id < count; ++id) Planet(world, game::id_planet{static_cast<uint32_t>(id)});
    }

    inline bool Planets_Equal(const std::map<game::id_planet, s_planet_data>& left,
        const std::map<game::id_planet, s_planet_data>& right) {
        if (left.size() != right.size()) return false;
        auto a = left.begin();
        auto b = right.begin();
        for (; a != left.end(); ++a, ++b) {
            if (a->first != b->first || memcmp(&a->second, &b->second, sizeof(s_planet_data)) != 0) return false;
        }
        return true;
    }
} // namespace test
