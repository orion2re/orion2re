#include "pch.h"

#include "game/serialization.h"

namespace SERIAL {
    Reader_::Reader_(const void* data, uint32_t len, const char* error_message, bool soft_fail)
        : start(static_cast<const uint8_t*>(data)),
          ptr(static_cast<const uint8_t*>(data)),
          len(len),
          error_message(error_message),
          soft_fail_(soft_fail),
          failed_(false) {
        if (start == nullptr) {
            if (soft_fail_) {
                failed_ = true;
            } else {
                Fail();
            }
        }
    }

    bool Reader_::Require(size_t size) const {
        if (failed_) {
            return false;
        }
        if (ptr == nullptr || ptr < start ||
            static_cast<size_t>(ptr - start) > len ||
            size > len - static_cast<size_t>(ptr - start)) {
            if (soft_fail_) {
                failed_ = true;
                return false;
            }
            Fail();
        }
        return true;
    }

    void Reader_::Skip(size_t size) {
        if (!Require(size)) {
            return;
        }
        ptr += size;
    }

    char Reader_::Read_Char() {
        return static_cast<char>(Read_Uint8());
    }

    uint8_t Reader_::Read_Uint8() {
        if (!Require(sizeof(uint8_t))) {
            return 0;
        }
        uint8_t value = *ptr;
        ptr += sizeof(uint8_t);
        return value;
    }

    int8_t Reader_::Read_Int8() {
        return static_cast<int8_t>(Read_Uint8());
    }

    uint16_t Reader_::Read_Uint16() {
        uint16_t value;
        Read_Bytes(&value, sizeof(value));
        return value;
    }

    int16_t Reader_::Read_Int16() {
        int16_t value;
        Read_Bytes(&value, sizeof(value));
        return value;
    }

    uint32_t Reader_::Read_Uint32() {
        uint32_t value;
        Read_Bytes(&value, sizeof(value));
        return value;
    }

    int32_t Reader_::Read_Int32() {
        int32_t value;
        Read_Bytes(&value, sizeof(value));
        return value;
    }

    uint64_t Reader_::Read_Uint64() {
        uint64_t value;
        Read_Bytes(&value, sizeof(value));
        return value;
    }

    void Reader_::Read_Bytes(void* dest, size_t size) {
        if (!Require(size)) {
            // Leave the destination deterministic instead of untouched:
            // callers that read into stack buffers must not observe
            // uninitialized bytes after a truncated read.
            if (dest != nullptr) {
                memset(dest, 0, size);
            }
            return;
        }
        memcpy(dest, ptr, size);
        ptr += size;
    }

    uint32_t Reader_::Length() const {
        return static_cast<uint32_t>(ptr - start);
    }

    uint32_t Reader_::Remaining() const {
        return len - Length();
    }

    const uint8_t* Reader_::Ptr() const {
        return ptr;
    }

    bool Reader_::failed() const {
        return failed_;
    }

    [[noreturn]] void Reader_::Fail() const {
        exitm::Exit_With_Message(error_message);
    }

    Writer_::Writer_(void* data, uint32_t len, const char* error_message)
        : start(static_cast<uint8_t*>(data)),
          ptr(static_cast<uint8_t*>(data)),
          len(len),
          error_message(error_message) {
        if (start == nullptr) {
            Fail();
        }
    }

    void Writer_::Require(size_t size) const {
        if (ptr == nullptr || ptr < start ||
            static_cast<size_t>(ptr - start) > len ||
            size > len - static_cast<size_t>(ptr - start)) {
            Fail();
        }
    }

    void Writer_::Skip(size_t size) {
        Require(size);
        ptr += size;
    }

    void Writer_::Write_Char(char value) {
        Write_Uint8(static_cast<uint8_t>(value));
    }

    void Writer_::Write_Uint8(uint8_t value) {
        Require(sizeof(uint8_t));
        *ptr = value;
        ptr += sizeof(uint8_t);
    }

    void Writer_::Write_Int8(int8_t value) {
        Write_Uint8(static_cast<uint8_t>(value));
    }

    void Writer_::Write_Uint16(uint16_t value) {
        Write_Bytes(&value, sizeof(value));
    }

    void Writer_::Write_Int16(int16_t value) {
        Write_Bytes(&value, sizeof(value));
    }

    void Writer_::Write_Uint32(uint32_t value) {
        Write_Bytes(&value, sizeof(value));
    }

    void Writer_::Write_Int32(int32_t value) {
        Write_Bytes(&value, sizeof(value));
    }

    void Writer_::Write_Uint64(uint64_t value) {
        Write_Bytes(&value, sizeof(value));
    }

    void Writer_::Write_Bytes(const void* src, size_t size) {
        Require(size);
        memcpy(ptr, src, size);
        ptr += size;
    }

    uint32_t Writer_::Length() const {
        return static_cast<uint32_t>(ptr - start);
    }

    uint8_t* Writer_::Ptr() const {
        return ptr;
    }

    [[noreturn]] void Writer_::Fail() const {
        exitm::Exit_With_Message(error_message);
    }
} // namespace SERIAL