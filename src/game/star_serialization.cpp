#include "pch.h"

#include "record_serialization.h"
#include "star_serialization.h"

namespace SAVEGAME {
    namespace {
        s_star_data Unused_Star() {
            s_star_data star{};
            star.owner = -1;
            star.colonize_player = -1;
            star.wormhole_star_id = game::NO_STAR;
            star.next_wfi_in_list = game::NO_STAR;
            memset(star.officer_index, 0xff, sizeof(star.officer_index));
            memset(star.planet_index, 0xff, sizeof(star.planet_index));
            memset(star.relocate_ship_to, 0xff, sizeof(star.relocate_ship_to));
            memset(star.surrender_to, 0xff, sizeof(star.surrender_to));
            return star;
        }
    }

    void Write_Ordered_Stars(const game::s_star_slots& stars, SERIAL::Writer_* writer,
        int16_t extent, bool include_antares) {
        writer->Write_Int16(extent);
        if (extent < 0 || extent > MAX_STARS) return;
        writer->Write_Uint64(stars.state != nullptr ? stars.state->next_star_id : static_cast<uint64_t>(extent));
        for (int16_t i = 0; i < extent; ++i) {
            writer->Write_Uint32(stars.Id_At(i).id);
            records::Write_Star(writer, &stars[i]);
        }
        if (include_antares) records::Write_Star(writer, &stars[extent]);
    }

    bool Read_Ordered_Stars(const game::s_star_slots& stars, SERIAL::Reader_* reader,
        uint16_t version, int16_t* extent, bool include_antares) {
        const auto count = reader->Read_Int16();
        const bool classic = version == records::LEGACY_VERSION;
        if (reader->Failed() || count < 0 || count > (classic ? ORIGINAL_MAX_STARS : MAX_STARS)) return false;
        const auto next = version >= 8 ? reader->Read_Uint64() : static_cast<uint64_t>(count);
        if (reader->Failed() || next > game::NO_STAR.id) return false;

        std::array<game::id_star, MAX_STARS> order;
        order.fill(game::NO_STAR);
        std::map<game::id_star, s_star_data> live;
        std::map<int16_t, s_star_data> vacant;
        const auto unused = Unused_Star();
        auto antares = unused;
        const int16_t record_count = classic ? ORIGINAL_MAX_STARS : count;
        std::vector<s_star_data> fixture_records;
        if (stars.fixture != nullptr) fixture_records.resize(include_antares ? count + 1 : MAX_STARS, unused);
        for (int16_t i = 0; i < record_count; ++i) {
            const auto id = version >= 8 ? game::id_star{reader->Read_Uint32()} : game::id_star{static_cast<uint32_t>(i)};
            s_star_data star{};
            records::Read_Star(reader, &star, version);
            if (reader->Failed()) return false;
            if (stars.fixture != nullptr) fixture_records[i] = star;
            if (i == count) antares = star;
            if (i >= count) continue;
            if (id == game::NO_STAR) {
                if (star.spectral_class != -1) return false;
                vacant.emplace(i, star);
            } else {
                if (id.id >= next || !live.emplace(id, star).second) return false;
                order[i] = id;
            }
        }
        if (include_antares) {
            records::Read_Star(reader, &antares, version);
            if (reader->Failed()) return false;
            if (stars.fixture != nullptr) fixture_records[count] = antares;
        }
        if (stars.state != nullptr) {
            stars.state->star_order = order;
            stars.state->stars.swap(live);
            stars.state->vacant_star_positions.swap(vacant);
            stars.state->next_star_id = next;
            stars.state->antares = antares;
        } else if (stars.fixture != nullptr) {
            std::copy(fixture_records.begin(), fixture_records.end(), stars.fixture);
        }
        *extent = count;
        return true;
    }
}
