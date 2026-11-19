#include "pch.h"

#include "world.h"

int16_t s_planet_data::Star_Position(const game::s_star_slots& stars) const {
    return stars.Position_Of(parent_star);
}

namespace game {
    s_world_state::s_world_state() {
        colony_order.fill(NO_COLONY);
        star_order.fill(NO_STAR);
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

    s_colony* Find_Colony(s_world_state* world, id_colony id) {
        auto found = world->colonies.find(id);
        return found == world->colonies.end() ? nullptr : &found->second;
    }

    const s_colony* Find_Colony(const s_world_state* world, id_colony id) {
        auto found = world->colonies.find(id);
        return found == world->colonies.end() ? nullptr : &found->second;
    }

    s_colony& Colony(s_world_state* world, id_colony id) {
        auto* colony = Find_Colony(world, id);
        if (colony == nullptr) std::abort();
        return *colony;
    }

    const s_colony& Colony(const s_world_state* world, id_colony id) {
        const auto* colony = Find_Colony(world, id);
        if (colony == nullptr) std::abort();
        return *colony;
    }

    int32_t Colony_Count(const s_world_state* world) {
        return static_cast<int32_t>(world->colonies.size());
    }

    std::optional<id_colony> Create_Colony(s_world_state* world, const s_colony& colony) {
        if (world->colonies.size() >= MAX_COLONIES || world->next_colony_id >= NO_COLONY.id) return std::nullopt;
        const id_colony id{static_cast<uint32_t>(world->next_colony_id)};
        if (!world->colonies.emplace(id, colony).second) std::abort();
        ++world->next_colony_id;
        return id;
    }

    bool Erase_Colony(s_world_state* world, id_colony id) {
        return world->colonies.erase(id) != 0;
    }

    s_star_data* Find_Star(s_world_state* world, id_star id) {
        const auto found = world->stars.find(id);
        return found == world->stars.end() ? nullptr : &found->second;
    }

    const s_star_data* Find_Star(const s_world_state* world, id_star id) {
        const auto found = world->stars.find(id);
        return found == world->stars.end() ? nullptr : &found->second;
    }

    s_star_data& Star(s_world_state* world, id_star id) {
        auto* star = Find_Star(world, id);
        if (star == nullptr) std::abort();
        return *star;
    }

    const s_star_data& Star(const s_world_state* world, id_star id) {
        const auto* star = Find_Star(world, id);
        if (star == nullptr) std::abort();
        return *star;
    }

    int32_t Star_Count(const s_world_state* world) {
        return static_cast<int32_t>(world->stars.size());
    }

    std::optional<id_star> Create_Star(s_world_state* world, const s_star_data& star) {
        if (world->stars.size() == MAX_STARS || world->next_star_id >= NO_STAR.id) return std::nullopt;
        const id_star id{static_cast<uint32_t>(world->next_star_id)};
        if (!world->stars.emplace(id, star).second) std::abort();
        ++world->next_star_id;
        return id;
    }

    s_star_data& s_star_slots::operator[](int32_t position) const {
        if (fixture != nullptr) return fixture[position];
        if (state == nullptr || position < 0 || position > MAX_STARS) std::abort();
        const auto id = Id_At(position);
        if (id != NO_STAR) return Star(state, id);
        if (extent != nullptr && position == *extent) return state->antares;
        if (position == MAX_STARS) std::abort();
        return state->vacant_star_positions.try_emplace(static_cast<int16_t>(position)).first->second;
    }

    id_star s_star_slots::Id_At(int32_t position) const {
        if (position < 0 || position >= MAX_STARS || (extent != nullptr && position == *extent)) return NO_STAR;
        if (fixture != nullptr) return id_star{static_cast<uint32_t>(position)};
        return state != nullptr ? state->star_order[position] : NO_STAR;
    }

    int16_t s_star_slots::Position_Of(id_star id) const {
        if (id == NO_STAR) return -1;
        if (fixture != nullptr) return id.id < MAX_STARS ? static_cast<int16_t>(id.id) : -1;
        if (state == nullptr) return -1;
        const auto found = std::find(state->star_order.begin(), state->star_order.end(), id);
        return found == state->star_order.end() ? -1 : static_cast<int16_t>(found - state->star_order.begin());
    }

    int16_t s_star_slots::Position_Of(const s_star_data* star) const {
        if (star == nullptr) return -1;
        if (fixture != nullptr) {
            const auto first = reinterpret_cast<uintptr_t>(fixture);
            const auto address = reinterpret_cast<uintptr_t>(star);
            if (address < first || (address - first) % sizeof(s_star_data) != 0) return -1;
            const auto position = (address - first) / sizeof(s_star_data);
            return position <= MAX_STARS ? static_cast<int16_t>(position) : -1;
        }
        if (state == nullptr) return -1;
        if (star == &state->antares && extent != nullptr) return *extent;
        for (int16_t position = 0; position < MAX_STARS; ++position) {
            const auto id = state->star_order[position];
            if (id != NO_STAR && Find_Star(state, id) == star) return position;
        }
        return -1;
    }

    bool s_star_slots::Activate(int16_t position) const {
        if (position < 0 || position >= MAX_STARS) return false;
        if (fixture != nullptr) return true;
        if (state == nullptr) return false;
        if (state->star_order[position] != NO_STAR) return true;
        const auto id = Create_Star(state, {});
        if (!id) return false;
        state->star_order[position] = *id;
        state->vacant_star_positions.erase(position);
        return true;
    }

    void s_star_slots::Retire(int16_t position) const {
        if (state == nullptr || position < 0 || position >= MAX_STARS) return;
        const auto id = state->star_order[position];
        if (id == NO_STAR) return;
        state->star_order[position] = NO_STAR;
        state->stars.erase(id);
    }

    void s_star_slots::Reset() const {
        if (fixture != nullptr) {
            memset(fixture, 0, MAX_STARS * sizeof(s_star_data));
        } else if (state != nullptr) {
            state->stars.clear();
            state->star_order.fill(NO_STAR);
            state->vacant_star_positions.clear();
            state->next_star_id = 0;
            state->antares = {};
        }
    }

    s_colony& s_colony_slots::operator[](int32_t position) const {
        if (fixture != nullptr) return fixture[position];
        if (state == nullptr || position < 0 || position >= MAX_COLONIES) std::abort();
        const auto id = state->colony_order[position];
        if (id != NO_COLONY) return Colony(state, id);
        auto [entry, inserted] = state->vacant_colony_positions.try_emplace(static_cast<int16_t>(position));
        if (inserted) {
            entry->second.owner = -1;
            entry->second.allocated_to = -1;
            entry->second.planet = NO_PLANET;
            entry->second.officer = -1;
        }
        return entry->second;
    }

    id_colony s_colony_slots::Id_At(int32_t position) const {
        if (position < 0 || position >= MAX_COLONIES) return NO_COLONY;
        if (fixture != nullptr) return id_colony{static_cast<uint32_t>(position)};
        return state != nullptr ? state->colony_order[position] : NO_COLONY;
    }

    int16_t s_colony_slots::Position_Of(const s_colony* colony) const {
        if (fixture != nullptr) {
            const auto first = reinterpret_cast<uintptr_t>(fixture);
            const auto address = reinterpret_cast<uintptr_t>(colony);
            if (address < first || (address - first) % sizeof(s_colony) != 0) return -1;
            const auto position = (address - first) / sizeof(s_colony);
            return position < MAX_COLONIES ? static_cast<int16_t>(position) : -1;
        }
        if (state != nullptr && colony != nullptr) {
            for (int16_t position = 0; position < MAX_COLONIES; ++position) {
                const auto id = state->colony_order[position];
                if (id != NO_COLONY && Find_Colony(state, id) == colony) return position;
            }
            for (const auto& [position, record] : state->vacant_colony_positions) {
                if (&record == colony) return position;
            }
        }
        return -1;
    }

    bool s_colony_slots::Activate(int16_t position, int16_t player_lane) const {
        if (position < 0 || position >= MAX_COLONIES) return false;
        if (fixture != nullptr) return true;
        if (state == nullptr) return false;
        if (state->colony_order[position] != NO_COLONY) return true;
        if (player_lane >= 0) {
            if (player_lane >= MAX_PLAYERS) return false;
            // Simultaneous orders can create colonies on several peers before
            // the host merges them. Disjoint player lanes avoid ID collisions.
            const auto next = state->next_colony_id;
            const auto adjusted = next + (MAX_PLAYERS + player_lane - next % MAX_PLAYERS) % MAX_PLAYERS;
            if (adjusted >= NO_COLONY.id) return false;
            state->next_colony_id = adjusted;
        }
        const auto id = Create_Colony(state, (*this)[position]);
        if (!id) return false;
        state->colony_order[position] = *id;
        state->vacant_colony_positions.erase(position);
        return true;
    }

    bool s_colony_slots::Install(int16_t position, id_colony id, const s_colony& record) const {
        if (position < 0 || position >= MAX_COLONIES) return false;
        if (fixture != nullptr) {
            fixture[position] = record;
            return true;
        }
        if (state == nullptr) return false;
        if (id == NO_COLONY) {
            if (record.owner >= 0 && record.planet != NO_PLANET) return false;
            Retire(position);
            (*this)[position] = record;
            return true;
        }
        if (id == Id_At(position)) {
            Colony(state, id) = record;
            return true;
        }
        if (Find_Colony(state, id) != nullptr) return false;
        Retire(position);
        state->colonies.emplace(id, record);
        state->colony_order[position] = id;
        state->vacant_colony_positions.erase(position);
        state->next_colony_id = std::max(state->next_colony_id, uint64_t{id.id} + 1);
        return true;
    }

    void s_colony_slots::Retire(int16_t position) const {
        if (state == nullptr || position < 0 || position >= MAX_COLONIES) return;
        const auto id = state->colony_order[position];
        if (id == NO_COLONY) return;
        state->colony_order[position] = NO_COLONY;
        Erase_Colony(state, id);
    }

    void s_colony_slots::Reset() const {
        if (fixture != nullptr) {
            memset(fixture, 0, MAX_COLONIES * sizeof(s_colony));
        } else if (state != nullptr) {
            state->colonies.clear();
            state->colony_order.fill(NO_COLONY);
            state->vacant_colony_positions.clear();
            state->next_colony_id = 0;
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
