#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    std::optional<game::id_colony> Colony_Id(const game::s_view* world, const s_colony* colony) {
        if (world == nullptr || world->colonies == nullptr || world->colony_count == nullptr || *world->colony_count < 0) return std::nullopt;
        const auto index = world->colonies.Position_Of(colony);
        if (index < 0 || index >= *world->colony_count) return std::nullopt;
        return Colony_Id((uint32_t)index);
    }
} // namespace legacy_world
