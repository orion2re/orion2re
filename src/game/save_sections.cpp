#include "pch.h"

#include "game/save_sections.h"

#include "game/serialization.h"

namespace SAVEGAME {

    uint32_t Extension_Size(uint32_t payload_size, uint32_t section_count) {
        const uint64_t size = static_cast<uint64_t>(EXTENSION_HEADER_SIZE) +
            static_cast<uint64_t>(SECTION_HEADER_SIZE) * section_count +
            payload_size;
        return section_count <= MAX_EXTENSION_SECTIONS &&
                size <= std::numeric_limits<uint32_t>::max()
            ? static_cast<uint32_t>(size)
            : 0;
    }

    void Write_Extension_Header(SERIAL::Writer_* writer,
        uint32_t payload_size,
        uint32_t section_count) {
        writer->Write_Uint32(EXTENSION_MAGIC);
        writer->Write_Uint16(EXTENSION_FORMAT_VERSION);
        writer->Write_Uint16(EXTENSION_HEADER_SIZE);
        writer->Write_Uint32(payload_size + SECTION_HEADER_SIZE * section_count);
        writer->Write_Uint32(section_count);
    }

    void Write_Section_Header(SERIAL::Writer_* writer,
        uint32_t type,
        uint16_t version,
        uint16_t flags,
        uint32_t payload_size) {
        writer->Write_Uint32(type);
        writer->Write_Uint16(version);
        writer->Write_Uint16(flags);
        writer->Write_Uint32(payload_size);
    }

    bool Read_Extension(SERIAL::Reader_* reader,
        s_section_view* sections,
        uint32_t section_capacity,
        uint32_t* section_count_out,
        std::string* diagnostic) {
        if (reader == nullptr || sections == nullptr ||
            section_count_out == nullptr || diagnostic == nullptr) {
            return false;
        }
        *section_count_out = 0;
        diagnostic->clear();
        if (reader->Remaining() < EXTENSION_HEADER_SIZE ||
            reader->Read_Uint32() != EXTENSION_MAGIC ||
            reader->Read_Uint16() != EXTENSION_FORMAT_VERSION ||
            reader->Read_Uint16() != EXTENSION_HEADER_SIZE) {
            *diagnostic = "invalid save extension header";
            return false;
        }
        const uint32_t payload_size = reader->Read_Uint32();
        const uint32_t section_count = reader->Read_Uint32();
        if (section_count > MAX_EXTENSION_SECTIONS ||
            section_count > section_capacity ||
            payload_size > reader->Remaining()) {
            *diagnostic = "invalid save extension size or section count";
            return false;
        }

        SERIAL::Reader_ payload(reader->Ptr(), payload_size, "invalid save extension payload");
        for (uint32_t index = 0; index < section_count; ++index) {
            if (payload.Remaining() < SECTION_HEADER_SIZE) {
                *diagnostic = "truncated save extension section header";
                return false;
            }
            s_section_view& section = sections[index];
            section.type = payload.Read_Uint32();
            section.version = payload.Read_Uint16();
            section.flags = payload.Read_Uint16();
            section.size = payload.Read_Uint32();
            if ((section.flags & ~SECTION_FLAG_REQUIRED) != 0 ||
                section.size > payload.Remaining()) {
                *diagnostic = "invalid save extension section";
                return false;
            }
            section.data = payload.Ptr();
            payload.Skip(section.size);
        }
        if (payload.Remaining() != 0) {
            *diagnostic = "trailing save extension payload data";
            return false;
        }
        reader->Skip(payload_size);
        *section_count_out = section_count;
        return true;
    }

} // namespace SAVEGAME