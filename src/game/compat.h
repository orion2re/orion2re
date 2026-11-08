#pragma once

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#if !defined(HAVE_STRLCPY) && !defined(strlcpy)
#define strlcpy SDL_strlcpy
#endif

#if !defined(HAVE_STRLCAT) && !defined(strlcat)
#define strlcat SDL_strlcat
#endif

#if !defined(_MSC_VER) && !defined(__cdecl)
#define __cdecl
#endif

#if !defined(_MSC_VER) && !defined(__stdcall)
#define __stdcall
#endif

#if !defined(_MSC_VER) && !defined(__thiscall)
#define __thiscall
#endif

using legacy_qsort_compare = int(__cdecl*)(const void*, const void*);
void Legacy_Qsort(void* base, size_t count, size_t width, legacy_qsort_compare compare);
using legacy_qsort_context_compare = int(__cdecl*)(void*, const void*, const void*);
void Legacy_Qsort(void* base, size_t count, size_t width,
    legacy_qsort_context_compare compare, void* context);

#define qsort Legacy_Qsort

#if !defined(GUID_DEFINED)
#define GUID_DEFINED
typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID;
#endif

#if !defined(ORION2RE_S_MEMORYSTATUS_DEFINED)
#define ORION2RE_S_MEMORYSTATUS_DEFINED
struct s_memorystatus {
    uint32_t dwLength;
    uint32_t dwMemoryLoad;
    uint32_t dwTotalPhys;
    uint32_t dwAvailPhys;
    uint32_t dwTotalPageFile;
    uint32_t dwAvailPageFile;
    uint32_t dwTotalVirtual;
    uint32_t dwAvailVirtual;
};
#endif

bool File_Exists(const char* path);
bool Cleanup_Path(const char* input_path, char* output_path, size_t output_path_size, bool ensure_trailing_separator = false);
bool Resolve_File_Path_Case(const char* path, char* resolved_path, size_t resolved_path_size);
FILE* fopen_case(const char* path, const char* mode);
void Query_Memory_Status(s_memorystatus* memory_status);
int Show_Message_Box(void* window_handle, const char* text, const char* caption, bool is_error = false);

#if defined(_WIN32)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#define itoa SDL_itoa
#define _itoa SDL_itoa
#endif

inline char* strlwr(char* text) {
    return SDL_strlwr(text);
}

inline char* strupr(char* text) {
    return SDL_strupr(text);
}

#if defined(_WIN32)
#define _strlwr strlwr
#define _strupr strupr
#endif