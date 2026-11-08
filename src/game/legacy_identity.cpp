#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    std::optional<uint32_t> Colony_Id(const game_world::s_view* world, const s_colony* colony) {
        if (world == nullptr || world->colonies == nullptr || world->colony_count == nullptr || *world->colony_count < 0) return std::nullopt;
        const uintptr_t first = reinterpret_cast<uintptr_t>(world->colonies);
        const uintptr_t address = reinterpret_cast<uintptr_t>(colony);
        if (address < first || (address - first) % sizeof(s_colony) != 0) return std::nullopt;
        const uint64_t index = (address - first) / sizeof(s_colony);
        if (index >= (uint32_t)*world->colony_count) return std::nullopt;
        return (uint32_t)index;
    }
} // namespace legacy_world
