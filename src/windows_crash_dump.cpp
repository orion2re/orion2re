#include "windows_crash_dump.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

namespace crash_dump {
    namespace {
        constexpr wchar_t directory_name[] = L"crashes";
        constexpr wchar_t filename_template[] = L"0000-00-00-00-00-00.dmp";
        constexpr DWORD dump_timeout_ms = 10'000;

        wchar_t dump_directory[MAX_PATH];
        DWORD dump_directory_length;
        HANDLE dump_requested;
        HANDLE dump_thread;
        DWORD dump_thread_id;
        volatile LONG dump_started;
        MINIDUMP_EXCEPTION_INFORMATION exception_info;

        void Write_Digits_(wchar_t* output, unsigned value, unsigned digits) {
            while (digits != 0) {
                output[--digits] = static_cast<wchar_t>(L'0' + value % 10);
                value /= 10;
            }
        }

        DWORD WINAPI Write_Dump_(void*) {
            if (WaitForSingleObject(dump_requested, INFINITE) != WAIT_OBJECT_0) {
                return 0;
            }
            if (!CreateDirectoryW(dump_directory, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
                return 0;
            }

            SYSTEMTIME now;
            GetLocalTime(&now);
            wchar_t path[MAX_PATH];
            for (DWORD i = 0; i < dump_directory_length; ++i) {
                path[i] = dump_directory[i];
            }
            path[dump_directory_length] = L'\\';
            wchar_t* filename = path + dump_directory_length + 1;
            for (unsigned i = 0; i < sizeof(filename_template) / sizeof(wchar_t); ++i) {
                filename[i] = filename_template[i];
            }
            Write_Digits_(filename, now.wYear, 4);
            Write_Digits_(filename + 5, now.wMonth, 2);
            Write_Digits_(filename + 8, now.wDay, 2);
            Write_Digits_(filename + 11, now.wHour, 2);
            Write_Digits_(filename + 14, now.wMinute, 2);
            Write_Digits_(filename + 17, now.wSecond, 2);

            // Keep an existing report if two game instances crash in the same second.
            const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) {
                return 0;
            }

            const BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                MiniDumpNormal, &exception_info, nullptr, nullptr);
            CloseHandle(file);
            if (!written) {
                DeleteFileW(path);
            }
            return 0;
        }

        LONG WINAPI Handle_Crash_(EXCEPTION_POINTERS* exception) {
            const DWORD thread_id = GetCurrentThreadId();
            if (thread_id == dump_thread_id) {
                return EXCEPTION_CONTINUE_SEARCH;
            }
            if (InterlockedCompareExchange(&dump_started, 1, 0) == 0) {
                exception_info.ThreadId = thread_id;
                exception_info.ExceptionPointers = exception;
                exception_info.ClientPointers = FALSE; // The writer runs in this process.
                SetEvent(dump_requested);
            }
            // Keep the exception context alive for the writer, but don't hang on a deadlock.
            WaitForSingleObject(dump_thread, dump_timeout_ms);
            return EXCEPTION_CONTINUE_SEARCH;
        }
    } // namespace

    bool Install_Windows_Handler() {
        const DWORD exe_path_length = GetModuleFileNameW(nullptr, dump_directory, MAX_PATH);
        if (exe_path_length == 0 || exe_path_length >= MAX_PATH) {
            return false;
        }
        DWORD parent_length = exe_path_length;
        while (parent_length != 0 && dump_directory[parent_length - 1] != L'\\') {
            --parent_length;
        }
        constexpr DWORD directory_length = sizeof(directory_name) / sizeof(wchar_t) - 1;
        constexpr DWORD filename_length_with_null = sizeof(filename_template) / sizeof(wchar_t);
        if (parent_length == 0 || parent_length + directory_length + 1 + filename_length_with_null > MAX_PATH) {
            return false;
        }
        for (DWORD i = 0; i <= directory_length; ++i) {
            dump_directory[parent_length + i] = directory_name[i];
        }
        dump_directory_length = parent_length + directory_length;

        dump_requested = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (dump_requested == nullptr) {
            return false;
        }
        dump_thread = CreateThread(nullptr, 0, Write_Dump_, nullptr, 0, &dump_thread_id);
        if (dump_thread == nullptr) {
            CloseHandle(dump_requested);
            return false;
        }
        // This filter, event, and writer belong to the process for its lifetime.
        SetUnhandledExceptionFilter(Handle_Crash_);
        return true;
    }
} // namespace crash_dump
