#pragma once

#include <cstdint>
#include <cstddef>

namespace allocate {
    uint32_t Global_Screen_Space();

#pragma pack(push, 1)
    struct s_alloc_header {
        uint32_t current;
        uint32_t size;
        uint32_t reserved2;
    };

    struct s_mark_stack_entry {
        void* block;
        uint32_t current;
    };
#pragma pack(pop)

    /// Allocates header-backed storage for a payload buffer.
    /// @param size Requested payload size in bytes.
    /// @return Pointer to the payload bytes after the internal header.
    void* __cdecl Allocate_Space(uint32_t size);

    /// Allocates and clears the main global data buffers used during startup.
    void Allocate_Data_Space();

    /// Returns the unused payload space remaining in a block.
    /// @param block Pointer previously returned by this allocator.
    int32_t __cdecl Get_Remaining_Block_Space(void* block);

    /// Returns the total payload capacity recorded for a block.
    /// @param block Pointer previously returned by this allocator.
    uint32_t __cdecl Get_Max_Block_Space(void* block);

    /// Exits with an allocation failure message.
    /// @param requested_bytes Number of payload bytes that could not be allocated.
    void __cdecl Allocation_Error(int32_t requested_bytes);

    extern int32_t mark_stack_pos[2];

    extern s_mark_stack_entry mark_stack[MARK_STACK_COUNT];

    /// Saves the current allocation offset for later restoration.
    /// @param block Block whose current offset should be pushed.
    void __cdecl Push_Block(void* block);

    /// Restores the most recently pushed allocation offset.
    /// @param unused_block Unused legacy parameter kept for call compatibility.
    void __cdecl Pop_Block(void* unused_block);

    /// Saves the current allocation offset in a block header.
    /// @param block_ptr Block whose current offset should be marked.
    void __cdecl Mark_Block(void* block_ptr);

    /// Restores a block to the offset previously saved by Mark_Block.
    /// @param block Block whose saved offset should be restored.
    void __cdecl Release_Block(void* block);

    /// Exits with a detailed sub-allocation failure message.
    /// @param requested_size Requested payload size in bytes.
    /// @param bytes_short Number of bytes the pool was short.
    /// @param block_capacity Total payload capacity of the pool.
    void __cdecl Reallocation_Error(uint32_t requested_size, int32_t bytes_short, int32_t block_capacity);

    /// Allocates the next header-backed sub-block from a pool.
    /// @param block Pool base pointer previously returned by this allocator.
    /// @param size Requested payload size in bytes.
    /// @return Pointer to the sub-block payload after its header.
    void* __cdecl Allocate_Next_Block(void* block, uint32_t size);

    /// Allocates a header-backed sub-block with an aligned payload.
    /// Padding precedes the sub-block header, preserving the normal block API contract.
    /// @param block Pool base pointer previously returned by this allocator.
    /// @param size Requested payload size in bytes.
    /// @param alignment Nonzero power-of-two payload alignment in bytes.
    /// @return Aligned payload pointer immediately after its header; exits on failure.
    void* __cdecl Allocate_Next_Block_Aligned(void* block, uint32_t size, size_t alignment);

    /// Allocates header-backed storage through the DOS-space path.
    /// @param size Requested payload size in bytes.
    /// @return Pointer to the payload bytes after the internal header.
    void* __cdecl Allocate_Dos_Space(uint32_t size);

    /// Allocates raw heap storage and aborts on failure.
    /// @param size Requested allocation size in bytes.
    /// @return Pointer to the allocated storage.
    void* __cdecl Allocate_Far_Space(uint32_t size);

    /// Allocates raw storage without attaching an outer allocation header.
    /// @param size Requested allocation size in bytes.
    /// @return Pointer to the allocated storage.
    void* __cdecl Allocate_Space_No_Header(uint32_t size);

    /// Resets a block pool and allocates its first no-header sub-block.
    /// @param block_ptr Pool base pointer previously returned by this allocator.
    /// @param size Requested payload size in bytes.
    /// @return Pointer to the start of the allocated sub-block.
    void* __cdecl Allocate_First_Block_No_Header(void* block_ptr, uint32_t size);

    /// Allocates the next no-header sub-block from a pool.
    /// @param block_ptr Pool base pointer previously returned by this allocator.
    /// @param size Requested payload size in bytes.
    /// @return Pointer to the start of the allocated sub-block, or nullptr on overflow.
    void* __cdecl Allocate_Next_Block_No_Header(void* block_ptr, uint32_t size);

    /// Resets a block pool and allocates its first header-backed sub-block.
    /// @param block Pool base pointer previously returned by this allocator.
    /// @param size Requested payload size in bytes.
    /// @return Pointer to the first sub-block payload after its header.
    void* __cdecl Allocate_First_Block(void* block, uint32_t size);
    void* Allocate_First_Block_Aligned(void* block, uint32_t size, size_t alignment);

    /// Returns the payload size needed when a header-backed sub-block will later be reused
    /// as a no-header pool for allocation mode 1 reloads.
    /// @param size Requested no-header payload size in bytes.
    /// @return Padded payload size that can satisfy one Allocate_First_Block_No_Header request.
    constexpr uint32_t Nested_No_Header_Pool_Size(uint32_t size) {
        return (size & 1u) != 0 ? ((size + 3u) & ~3u) : size;
    }

    /// Allocates a header-backed sub-block sized for later reuse as a no-header pool.
    /// @param block Pool base pointer previously returned by this allocator.
    /// @param size Requested no-header payload size in bytes.
    /// @return Pointer to the first sub-block payload after its header.
    inline void* Allocate_Next_Block_For_No_Header_Pool(void* block, uint32_t size) {
        return Allocate_Next_Block(block, Nested_No_Header_Pool_Size(size));
    }
} // namespace allocate