#include "pch.h"

#include "world.h"

namespace game {
    s_world_state::s_world_state() {
        Reset_Leaders(this);
    }

    s_world_state world;

    s_leader_data* Find_Leader(s_world_state* world, id_leader id) {
        auto found = world->leaders.find(id);
        return found == world->leaders.end() ? nullptr : &found->second;
    }

    const s_leader_data* Find_Leader(const s_world_state* world, id_leader id) {
        auto found = world->leaders.find(id);
        return found == world->leaders.end() ? nullptr : &found->second;
    }

    s_leader_data& Leader(s_world_state* world, int32_t id) {
        if (id < 0) std::abort();
        return Leader(world, id_leader{static_cast<uint32_t>(id)});
    }

    const s_leader_data& Leader(const s_world_state* world, int32_t id) {
        if (id < 0) std::abort();
        return Leader(world, id_leader{static_cast<uint32_t>(id)});
    }

    s_leader_data& Leader(s_world_state* world, id_leader id) {
        auto* record = Find_Leader(world, id);
        if (record == nullptr) std::abort();
        return *record;
    }

    const s_leader_data& Leader(const s_world_state* world, id_leader id) {
        const auto* record = Find_Leader(world, id);
        if (record == nullptr) std::abort();
        return *record;
    }

    void Reset_Leaders(s_world_state* world) {
        world->leaders.clear();
        for (uint32_t id = 0; id < MAX_LEADERS; ++id) {
            world->leaders.emplace(id_leader{id}, s_leader_data{});
        }
    }

    void Import_Leaders(s_world_state* world, std::span<const s_leader_data> records) {
        world->leaders.clear();
        for (uint32_t id = 0; id < records.size(); ++id) {
            world->leaders.emplace(id_leader{id}, records[id]);
        }
    }

    std::vector<s_leader_data> Export_Leaders(const s_world_state* world) {
        std::vector<s_leader_data> records(MAX_LEADERS);
        for (const auto& [id, leader] : world->leaders) {
            if (id.id >= records.size()) std::abort();
            records[id.id] = leader;
        }
        return records;
    }
} // namespace game
