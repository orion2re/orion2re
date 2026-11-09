#include "pch.h"

namespace {
    uint32_t Bitmap_Bytes(uint32_t bit_count) {
        return (bit_count + 7) / 8;
    }

    uint32_t Good_Star_Bytes(uint32_t star_count) {
        const uint32_t good_star_size =
            Bitmap_Bytes(star_count) + star_count * sizeof(uint32_t) + sizeof(uint16_t);
        return star_count * good_star_size;
    }

    uint32_t Advanced_Civilization_Bytes(uint32_t star_count,
        uint32_t planet_count) {
        const uint32_t player_size =
            4 * sizeof(int16_t) + planet_count +
            star_count * sizeof(s_adv_civ_player_star) +
            planet_count * sizeof(s_adv_civ_player_planet);
        return MAX_PLAYERS * player_size +
            star_count * star_count * sizeof(int32_t) +
            planet_count + star_count;
    }

    uint32_t Map_Generation_Bytes(uint32_t star_count,
        uint32_t planet_count) {
        const uint32_t fixed_star_size =
            sizeof(s_star_data) - BITMAP(MAX_STARS);
        const uint32_t star_size = fixed_star_size + Bitmap_Bytes(star_count);
        return planet_count * sizeof(s_planet_data) +
            star_count * star_size +
            planet_count * sizeof(int16_t);
    }

    uint32_t Planet_Summary_Bytes(uint32_t star_count,
        uint32_t planet_count,
        uint32_t ship_count) {
        return 2 * planet_count * sizeof(s_plntsum_planet) +
            ship_count * sizeof(s_plntsum_ship) +
            star_count * sizeof(int16_t);
    }

    uint32_t Fleet_Screen_Bytes(uint32_t ship_count) {
        const uint32_t list_count = ship_count + 1;
        return list_count *
            (sizeof(s_big_icon) + sizeof(int8_t) + sizeof(int16_t));
    }

    uint32_t Score_Population_Bytes(uint32_t colony_count) {
        uint32_t population_count = colony_count * MAX_COLONY_POPULATION;
        if (population_count > INT16_MAX) population_count = INT16_MAX;
        return population_count * sizeof(s_hiscore_tmp_pop);
    }

    uint32_t Capacity_Dependent_Screen_Bytes(uint32_t star_count,
        uint32_t planet_count,
        uint32_t colony_count,
        uint32_t ship_count) {
        return Good_Star_Bytes(star_count) +
            Advanced_Civilization_Bytes(star_count, planet_count) +
            Map_Generation_Bytes(star_count, planet_count) +
            Planet_Summary_Bytes(star_count, planet_count, ship_count) +
            Fleet_Screen_Bytes(ship_count) +
            Score_Population_Bytes(colony_count);
    }
} // namespace

namespace allocate {
    uint32_t Global_Screen_Space() {
        uint32_t size = 0x1B5030 +
            Capacity_Dependent_Screen_Bytes(MAX_STARS, MAX_PLANETS, MAX_COLONIES, MAX_SHIPS) -
            Capacity_Dependent_Screen_Bytes(ORIGINAL_MAX_STARS, ORIGINAL_MAX_PLANETS, ORIGINAL_MAX_COLONIES, ORIGINAL_MAX_SHIPS) +
            NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES - 0xE1000u;

        return (size + 3u) & ~3u;
    }

    int32_t mark_stack_pos[2];

    s_mark_stack_entry mark_stack[MARK_STACK_COUNT];

    void* __cdecl Allocate_Space(uint32_t size) {
        if (size > INT32_MAX - sizeof(s_alloc_header) - 3u) {
            Allocation_Error(INT32_MAX);
        }
        uint32_t padded_size = size;

        // Alignment logic: If size is odd, align it to the next 4-byte boundary.
        // Note: Even sizes (e.g., 2, 6) are not aligned to 4-byte boundaries here.
        if (size & 1) {
            padded_size = (size & 0xFFFFFFFC) + 4;
        }

        uint32_t total_alloc_size = padded_size + sizeof(s_alloc_header);

        auto* header = (s_alloc_header*)Allocate_Far_Space(total_alloc_size);
        if (header == nullptr) {
            header = (s_alloc_header*)Allocate_Dos_Space(total_alloc_size);

            if (header == nullptr) {
                Allocation_Error(size);
            }
        }

        header->current = 0;
        header->size = size; // Stores the original requested size, not the padded size
        header->reserved2 = 0;

        // Return pointer to the data region following the header
        return header + 1;
    }

    void __cdecl Allocation_Error(int32_t requested_bytes) {
        char message_buffer[100];

        snprintf(message_buffer, sizeof(message_buffer), "Couldn't Allocate %d Increase Virtual", requested_bytes);

        exitm::Exit_With_Message(message_buffer);
    }

    int32_t __cdecl Get_Remaining_Block_Space(void* block) {
        const auto* header = (s_alloc_header*)block - 1;
        return header->size - header->current;
    }

    uint32_t __cdecl Get_Max_Block_Space(void* block) {
        return ((s_alloc_header*)block - 1)->size;
    }

    void __cdecl Reallocation_Error(uint32_t requested_size, int32_t bytes_short, int32_t block_capacity) {
        char buffer[100];

        snprintf(buffer, sizeof(buffer), "Unable to Allocate %d bytes (short by %d bytes) in a block size of %d!\n\n", requested_size, bytes_short, block_capacity);

        exitm::Exit_With_Message(buffer);
    }

    void* __cdecl Allocate_Next_Block(void* block, uint32_t size) {
        return Allocate_Next_Block_Aligned(block, size, 1);
    }

    void* __cdecl Allocate_Next_Block_Aligned(void* block, uint32_t size, size_t alignment) {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            exitm::Exit_With_Message("Invalid Block Alignment");
        }
        const uint64_t padded = (size & 1u) != 0 ? ((uint64_t)size + 3u) & ~UINT64_C(3) : size;
        if (padded + sizeof(s_alloc_header) > INT32_MAX) {
            Reallocation_Error(size, INT32_MAX, Get_Max_Block_Space(block));
        }
        const uint32_t aligned_size = (uint32_t)padded;

        s_alloc_header* pool_header = (s_alloc_header*)block - 1;

        uint32_t current_offset = pool_header->current;
        int32_t capacity = (int32_t)pool_header->size;

        // Pad before the header so the aligned payload remains a normal block.
        const uintptr_t payload_address = (uintptr_t)block + current_offset + sizeof(s_alloc_header);
        const size_t padding = (alignment - (payload_address & (alignment - 1))) & (alignment - 1);
        const uint64_t total_required =
            (uint64_t)current_offset + padding + aligned_size + sizeof(s_alloc_header);

        if (total_required > pool_header->size) {
            Reallocation_Error(
                size,
                (int32_t)std::min<uint64_t>(total_required - pool_header->size, INT32_MAX),
                capacity);
        }

        // Pointer to the start of the new sub-block (header starts here)
        uint8_t* pool_base = (uint8_t*)block;
        s_alloc_header* sub_block_header = (s_alloc_header*)(pool_base + current_offset + padding);

        // Advance the pool's allocation pointer for future allocations
        pool_header->current = (uint32_t)total_required;

        // Initialize the sub-block header.
        sub_block_header->current = 0;
        sub_block_header->size = size;
        sub_block_header->reserved2 = 0;

        // Return a pointer to the sub-block's data area, skipping the header.
        return sub_block_header + 1;
    }

    void* __cdecl Allocate_First_Block(void* block, uint32_t size) {
        ((s_alloc_header*)block - 1)->current = 0;

        return Allocate_Next_Block(block, size);
    }

    void* __cdecl Allocate_Dos_Space(uint32_t size) {
        uint32_t allocation_size = size;

        if ((size & 1) != 0) {
            allocation_size = (size & 0xFFFFFFFC) + 4;
        }

        s_alloc_header* header = (s_alloc_header*)Allocate_Far_Space(
            allocation_size + sizeof(s_alloc_header));

        if (header == nullptr) {
            Allocation_Error(size);
        }

        header->current = 0;
        header->size = size;
        header->reserved2 = 0;

        return header + 1;
    }

    void* __cdecl Allocate_Far_Space(uint32_t size) {
        void* ptr = ::malloc(size);

        if (ptr == nullptr) {
            exitm::Exit_With_Message("Null Pointer");
        }

        return ptr;
    }

    void __cdecl Push_Block(void* block) {
        if (mark_stack_pos[0] >= MARK_STACK_COUNT) {
            exitm::Exit_With_Message("Mark Block Stack Size Exceeded");
        }

        int32_t pos = mark_stack_pos[0];
        mark_stack[pos].block = block;

        mark_stack[pos].current = ((s_alloc_header*)block - 1)->current;

        mark_stack_pos[0]++;
    }

    void __cdecl Pop_Block(void* unused_block) {
        (void)unused_block;

        if (mark_stack_pos[0] < 1) {
            exitm::Exit_With_Message("Mark Block Stack Empty");
        }

        mark_stack_pos[0]--;
        int32_t index = mark_stack_pos[0];

        void* target_base = mark_stack[index].block;
        uint32_t original_value = mark_stack[index].current;

        ((s_alloc_header*)target_base - 1)->current = original_value;
    }

    void* __cdecl Allocate_Space_No_Header(uint32_t size) {
        void* ptr = Allocate_Far_Space(size);

        if (ptr == nullptr) {
            ptr = Allocate_Dos_Space(size);

            if (ptr == nullptr) {
                Allocation_Error(size);
            }
        }

        return ptr;
    }

    void* __cdecl Allocate_First_Block_No_Header(void* block_ptr, uint32_t size) {
        ((s_alloc_header*)block_ptr - 1)->current = 0;

        return Allocate_Next_Block_No_Header(block_ptr, size);
    }

    void __cdecl Mark_Block(void* block_ptr) {
        auto* header = (s_alloc_header*)block_ptr - 1;
        header->reserved2 = header->current;
    }

    void* __cdecl Allocate_Next_Block_No_Header(void* block_ptr, uint32_t size) {
        auto* pool_header = (s_alloc_header*)block_ptr - 1;

        uint64_t aligned_size = size;

        // Alignment logic: if size is odd, align it to the next 4-byte boundary.
        // Even sizes are used as-is.
        if ((size & 1) != 0) {
            aligned_size = ((uint64_t)size + 3u) & ~UINT64_C(3);
        }

        uint32_t current_offset = pool_header->current;
        uint64_t next_offset = (uint64_t)current_offset + aligned_size;

        if (next_offset > pool_header->size) {
            return nullptr;
        }

        void* new_block_ptr = (uint8_t*)block_ptr + current_offset;

        pool_header->current = (uint32_t)next_offset;

        return new_block_ptr;
    }

    void __cdecl Release_Block(void* block) {
        auto* header = (s_alloc_header*)block - 1;
        header->current = header->reserved2;
    }
} // namespace allocate