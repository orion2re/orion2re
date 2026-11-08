#include "pch.h"

namespace struct_ {
    void __cdecl Set_Memory(void* dest, uint32_t size, uint8_t fill) {
        memset(dest, fill, size);
    }

    void __cdecl Clear_Structure(void* dest, uint32_t size) {
        memset(dest, 0, size);
    }

    void __cdecl Copy_Structure_Aligned(void* dest, void* src, uint32_t size) {
        memcpy(dest, src, (size >> 2) * sizeof(uint32_t));
    }

    void __cdecl Copy_Structure(void* dest, void* src, uint32_t size) {
        memmove(dest, src, size);
    }

    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        return Read_Bit_Field(bit_field, bit_index);
    }

    void __cdecl Set_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bit_field_bytes = (uint8_t*)bit_field;
        bit_field_bytes[bit_index >> 3] |= (uint8_t)(1 << (bit_index & 0x7));
    }

    void __cdecl Clear_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bit_field_bytes = (uint8_t*)bit_field;

        uint32_t byte_offset = bit_index >> 3;
        uint32_t bit_in_byte = bit_index & 7;

        bit_field_bytes[byte_offset] &= ~((uint8_t)(1 << bit_in_byte));
    }

    void __cdecl Swap_Structure(void* left_ptr, void* right_ptr, uint32_t byte_count) {
        uint8_t* left_bytes = (uint8_t*)left_ptr;
        uint8_t* right_bytes = (uint8_t*)right_ptr;

        if (((std::uintptr_t)right_ptr & 1U) != 0) {
            if (((std::uintptr_t)left_ptr & 1U) == 0) {
                uint32_t count = byte_count;
                do {
                    uint8_t swapped_byte = *right_bytes;
                    *right_bytes++ = *left_bytes;
                    *left_bytes++ = swapped_byte;
                } while (--count != 0);
                return;
            }

            {
                uint8_t swapped_byte = *right_bytes;
                *right_bytes++ = *left_bytes;
                *left_bytes++ = swapped_byte;
            }

            byte_count -= 1;

            uint32_t dword_count = byte_count >> 2;
            if (dword_count != 0) {
                do {
                    uint32_t swapped_dword;
                    memcpy(&swapped_dword, right_bytes, sizeof(swapped_dword));
                    memcpy(right_bytes, left_bytes, sizeof(swapped_dword));
                    memcpy(left_bytes, &swapped_dword, sizeof(swapped_dword));
                    right_bytes += sizeof(swapped_dword);
                    left_bytes += sizeof(swapped_dword);
                } while (--dword_count != 0);
            }

            uint32_t tail_count = byte_count & 3;
            if (tail_count != 0) {
                do {
                    uint8_t swapped_byte = *right_bytes;
                    *right_bytes++ = *left_bytes;
                    *left_bytes++ = swapped_byte;
                } while (--tail_count != 0);
            }

            return;
        }

        if (((std::uintptr_t)left_ptr & 1U) != 0) {
            uint32_t count = byte_count;
            do {
                uint8_t swapped_byte = *right_bytes;
                *right_bytes++ = *left_bytes;
                *left_bytes++ = swapped_byte;
            } while (--count != 0);
            return;
        }

        uint32_t dword_count = byte_count >> 2;
        if (dword_count != 0) {
            do {
                uint32_t swapped_dword;
                memcpy(&swapped_dword, right_bytes, sizeof(swapped_dword));
                memcpy(right_bytes, left_bytes, sizeof(swapped_dword));
                memcpy(left_bytes, &swapped_dword, sizeof(swapped_dword));
                right_bytes += sizeof(swapped_dword);
                left_bytes += sizeof(swapped_dword);
            } while (--dword_count != 0);
        }

        uint32_t tail_count = byte_count & 3;
        if (tail_count != 0) {
            do {
                uint8_t swapped_byte = *right_bytes;
                *right_bytes++ = *left_bytes;
                *left_bytes++ = swapped_byte;
            } while (--tail_count != 0);
        }
    }
} // namespace struct_