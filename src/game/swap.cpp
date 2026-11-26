#include "pch.h"

namespace swap {

    int32_t swap_block_size;
    uint8_t* swap_block_address;

    void __cdecl Save_Block_To_Drive(uint8_t* block_address, int32_t block_size) {
        auto* block_header = (allocate::s_alloc_header*)block_address - 1;
        int32_t saved_block_size = block_size + (int32_t)sizeof(*block_header);
        FILE* swap_file;

        swap::swap_block_address = block_address;
        swap::swap_block_size = saved_block_size + (int32_t)sizeof(*block_header);

        if ((int32_t)block_header->size < swap::swap_block_size) {
            exitm::Exit_With_Message("Drive swap reqd too large for block");
        }

        if (dos::Disk_Free() < swap::swap_block_size) {
            exitm::Exit_With_Message("Insufficient disk space for swap");
        }

        swap_file = fopen_case("TEMP.TMP", "wb");
        if (swap_file == nullptr) {
            exitm::Exit_With_Message("Error in swap file");
        }

        fwrite(block_header, swap::swap_block_size, 1, swap_file);
        fclose(swap_file);

        block_header->size = (uint32_t)saved_block_size;
    }

    void __cdecl Restore_Block_From_Drive() {
        uint8_t* swap_block_address = swap::swap_block_address;
        if (swap_block_address == nullptr) {
            return;
        }
        auto* block_header = (allocate::s_alloc_header*)swap_block_address - 1;
        FILE* swap_file = fopen_case("TEMP.TMP", "rb");

        if (swap_file == nullptr) {
            exitm::Exit_With_Message("Error in swap file");
        }

        const bool restored = fread(block_header, (uint32_t)swap::swap_block_size, 1, swap_file) == 1;
        fclose(swap_file);
        if (!restored) {
            exitm::Exit_With_Message("Error reading swap file");
        }
        swap::swap_block_address = nullptr;
        swap::swap_block_size = 0;
    }

} // namespace swap