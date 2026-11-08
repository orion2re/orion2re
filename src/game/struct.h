#pragma once

namespace struct_ {
    /**
     * Clears a buffer or structure to zero.
     * @param dest Buffer to clear.
     * @param size Number of bytes to clear.
     */
    void __cdecl Clear_Structure(void* dest, uint32_t size);

    /**
     * Fills a buffer with a repeated byte value.
     * @param dest Buffer to fill.
     * @param size Number of bytes to write.
     * @param fill Byte value to store.
     */
    void __cdecl Set_Memory(void* dest, uint32_t size, uint8_t fill);

    /**
     * Copies a byte range from one buffer to another.
     * @param dest Destination buffer.
     * @param src Source buffer.
     * @param size Number of bytes to copy.
     */
    void __cdecl Copy_Structure(void* dest, void* src, uint32_t size);

    /**
     * Copies only the complete 4-byte chunks from one buffer to another.
     * @param dest Destination buffer.
     * @param src Source buffer.
     * @param size Source byte count to round down to 4-byte chunks.
     */
    void __cdecl Copy_Structure_Aligned(void* dest, void* src, uint32_t size);

    /**
     * Returns whether the indexed bit is set in a packed bit field.
     * @param bit_field Packed bit-field storage.
     * @param bit_index Zero-based bit index to test.
     */
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index);
    inline int16_t Test_Bit_Field(const void* bit_field, uint16_t bit_index) {
        return Test_Bit_Field(const_cast<void*>(bit_field), bit_index);
    }

    /**
     * Sets the indexed bit in a packed bit field.
     * @param bit_field Packed bit-field storage.
     * @param bit_index Zero-based bit index to set.
     */
    void __cdecl Set_Bit_Field(void* bit_field, uint16_t bit_index);

    /**
     * Clears the indexed bit in a packed bit field.
     * @param bit_field Packed bit-field storage.
     * @param bit_index Zero-based bit index to clear.
     */
    void __cdecl Clear_Bit_Field(void* bit_field, uint16_t bit_index);

    /**
     * Swaps the contents of two buffers byte-for-byte.
     * @param left_ptr First buffer.
     * @param right_ptr Second buffer.
     * @param byte_count Number of bytes to swap.
     */
    void __cdecl Swap_Structure(void* left_ptr, void* right_ptr, uint32_t byte_count);
} // namespace struct_