#include "pch.h"

#include <stdexcept>

namespace exitm {
    void __cdecl Exit_With_Message(const char* message) {
        throw std::runtime_error(message == nullptr ? "serialization failure" : message);
    }
} // namespace exitm
