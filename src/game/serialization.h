#pragma once

namespace SERIAL {
    class Reader_ {
      public:
        // Recoverable parsers opt in and check Failed() before accepting the result.
        Reader_(const void* data, uint32_t len, const char* error_message, bool soft_fail = false);

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
        bool Failed() const;

      private:
        [[noreturn]] void Fail() const;

        const uint8_t* start;
        const uint8_t* ptr;
        uint32_t len;
        const char* error_message;
        bool soft_fail;
        mutable bool failed = false;
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