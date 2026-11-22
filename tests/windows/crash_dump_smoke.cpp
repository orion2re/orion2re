#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "windows_crash_dump.h"

int main(int argc, char**) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX);
    if (!crash_dump::Install_Windows_Handler()) {
        return 1;
    }
    if (argc > 1) {
        RaiseException(0xE0420001, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
    return 0;
}
