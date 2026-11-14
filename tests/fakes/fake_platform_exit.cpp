#include "pch.h"

#include <cstdlib>

// Minimal platform exit stubs for tests that link fake_exitm without the
// real platform: the exit path is never taken in these tests, so aborting
// on an unexpected call is the honest failure.
namespace platform {
    void __cdecl Request_Fatal_Exit(const char*) {
        std::abort();
    }

    void __cdecl Request_Exit(const char*) {
        std::abort();
    }

    void __cdecl Check_Exit() {
    }
} // namespace platform
