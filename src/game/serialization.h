#pragma once

namespace SERIAL {
    class Reader_ {
      public:
        // When soft_fail is true, a read past the end of the buffer sets a
        // sticky failure flag (see failed()) and returns zero instead of
        // aborting the process via Fail(). Only callers that read untrusted
        // input (save files) opt in; every other caller keeps the
        // historical abort-on-overrun behaviour.
        Reader_(const void* data, uint32_t len, const char* error_message, bool soft_fail = false);

        // Returns false when the requested bytes are not available. In
        // soft-fail mode this only records the failure; otherwise it
        // aborts via Fail().
        bool Require(size_t size) const;
        void Skip(size_t size);

        char Read_Char();
        uint8_t Read_Uint8();
        int8_t Read_Int8();
        uint16_t Read_Uint16();
        int16_t Read_Int16();
        uint32_t Read_Uint32();
        int32_t Read_Int32();
        uint64_t Read_Uint64();

        void Read_Bytes(void* dest, size_t size);
        uint32_t Length() const;
        uint32_t Remaining() const;
        const uint8_t* Ptr() const;

        // True once any read ran past the end of the buffer. Only set in
        // soft-fail mode; a hard-fail reader aborts instead of setting it.
        bool failed() const;

      private:
        [[noreturn]] void Fail() const;

        const uint8_t* start;
        const uint8_t* ptr;
        uint32_t len;
        const char* error_message;
        bool soft_fail_;
        mutable bool failed_;
    };

    class Writer_ {
      public:
        Writer_(void* data, uint32_t len, const char* error_message);

        void Require(size_t size) const;
        void Skip(size_t size);

        void Write_Char(char value);
        void Write_Uint8(uint8_t value);
        void Write_Int8(int8_t value);
        void Write_Uint16(uint16_t value);
        void Write_Int16(int16_t value);
        void Write_Uint32(uint32_t value);
        void Write_Int32(int32_t value);
        void Write_Uint64(uint64_t value);

        void Write_Bytes(const void* src, size_t size);
        uint32_t Length() const;
        uint8_t* Ptr() const;

      private:
        [[noreturn]] void Fail() const;

        uint8_t* start;
        uint8_t* ptr;
        uint32_t len;
        const char* error_message;
    };
} // namespace SERIAL
