#include "pch.h"

#include "world.h"

namespace game {
    s_world_state::s_world_state() {
        Reset_Leaders(this);
    }

    s_world_state world;

    s_planet_data* Find_Planet(s_world_state* world, id_planet id) {
        auto found = world->planets.find(id);
        return found == world->planets.end() ? nullptr : &found->second;
    }

    const s_planet_data* Find_Planet(const s_world_state* world, id_planet id) {
        auto found = world->planets.find(id);
        return found == world->planets.end() ? nullptr : &found->second;
    }

    s_planet_data& Planet(s_world_state* world, id_planet id) {
        auto* planet = Find_Planet(world, id);
        if (planet == nullptr) std::abort();
        return *planet;
    }

    const s_planet_data& Planet(const s_world_state* world, id_planet id) {
        const auto* planet = Find_Planet(world, id);
        if (planet == nullptr) std::abort();
        return *planet;
    }

    int32_t Planet_Count(const s_world_state* world) {
        return static_cast<int32_t>(world->planets.size());
    }

    std::optional<id_planet> Create_Planet(s_world_state* world, const s_planet_data& planet) {
        if (world->planets.size() >= MAX_PLANETS || world->next_planet_id >= UINT32_MAX) return std::nullopt;
        const id_planet id{static_cast<uint32_t>(world->next_planet_id++)};
        if (!world->planets.emplace(id, planet).second) std::abort();
        return id;
    }

    bool Insert_Planet(s_world_state* world, id_planet id, const s_planet_data& planet) {
        if (id == NO_PLANET || world->planets.size() >= MAX_PLANETS || !world->planets.emplace(id, planet).second) return false;
        world->next_planet_id = std::max(world->next_planet_id, uint64_t{id.id} + 1);
        return true;
    }

    bool Erase_Planet(s_world_state* world, id_planet id) {
        return world->planets.erase(id) != 0;
    }

    void Import_Legacy_Planets(s_world_state* world, std::span<const s_planet_data> records) {
        world->planets.clear();
        world->next_planet_id = 0;
        for (uint32_t id = 0; id < records.size(); ++id) {
            if (!Insert_Planet(world, id_planet{id}, records[id])) std::abort();
        }
    }

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
