#include "win32.h"

#include <exception>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#include <DbgHelp.h>
#endif

#include "main.h"
#include "svga.h"
#include "window_manager.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

namespace fallout {

#ifdef _WIN32
// 0x51E444
bool gProgramIsActive = false;

// GNW95MUTEX
HANDLE _GNW95_mutex = nullptr;

static volatile LONG gCrashArtifactsStarted = 0;

static void buildCrashPath(char* out, size_t outSize, const char* fileName)
{
    char modulePath[MAX_PATH] = { 0 };
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        snprintf(out, outSize, "%s", fileName);
        return;
    }

    char* slash = strrchr(modulePath, '\\');
    if (slash != nullptr) {
        slash[1] = '\0';
        snprintf(out, outSize, "%s%s", modulePath, fileName);
    } else {
        snprintf(out, outSize, "%s", fileName);
    }
}

static void writeFullDebugCrashArtifacts(const char* reason, EXCEPTION_POINTERS* exceptionInfo)
{
    // Crash handlers can cascade (for example invalid-parameter -> abort -> SIGABRT).
    // Only the first handler owns the files so the useful report is not overwritten.
    if (InterlockedCompareExchange(&gCrashArtifactsStarted, 1, 0) != 0) {
        return;
    }

    char logPath[MAX_PATH];
    char dumpPath[MAX_PATH];
    buildCrashPath(logPath, sizeof(logPath), "fallout2-ce-crash.log");
    buildCrashPath(dumpPath, sizeof(dumpPath), "fallout2-ce-crash.dmp");

    FILE* log = fopen(logPath, "w");
    if (log != nullptr) {
        DWORD code = exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr
            ? exceptionInfo->ExceptionRecord->ExceptionCode
            : 0;
        void* address = exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr
            ? exceptionInfo->ExceptionRecord->ExceptionAddress
            : nullptr;

        fprintf(log, "Fallout Unified Co-op full debug crash report\n");
        fprintf(log, "Reason=%s\n", reason != nullptr ? reason : "unknown");
        fprintf(log, "ExceptionCode=0x%08lX\n", static_cast<unsigned long>(code));
        fprintf(log, "ExceptionAddress=%p\n", address);
        fprintf(log, "ProcessId=%lu\n", static_cast<unsigned long>(GetCurrentProcessId()));
        fprintf(log, "ThreadId=%lu\n", static_cast<unsigned long>(GetCurrentThreadId()));
#if defined(_M_X64)
        if (exceptionInfo != nullptr && exceptionInfo->ContextRecord != nullptr) {
            CONTEXT* context = exceptionInfo->ContextRecord;
            fprintf(log, "RIP=0x%016llX\n", static_cast<unsigned long long>(context->Rip));
            fprintf(log, "RSP=0x%016llX\n", static_cast<unsigned long long>(context->Rsp));
            fprintf(log, "RBP=0x%016llX\n", static_cast<unsigned long long>(context->Rbp));
            fprintf(log, "RAX=0x%016llX RBX=0x%016llX RCX=0x%016llX RDX=0x%016llX\n",
                static_cast<unsigned long long>(context->Rax),
                static_cast<unsigned long long>(context->Rbx),
                static_cast<unsigned long long>(context->Rcx),
                static_cast<unsigned long long>(context->Rdx));
        }
#endif
        fprintf(log, "MiniDump=%s\n", dumpPath);
        fflush(log);
        fclose(log);
    }

    HMODULE dbghelp = LoadLibraryA("DbgHelp.dll");
    if (dbghelp != nullptr) {
        using MiniDumpWriteDumpProc = BOOL(WINAPI*)(
            HANDLE,
            DWORD,
            HANDLE,
            MINIDUMP_TYPE,
            PMINIDUMP_EXCEPTION_INFORMATION,
            PMINIDUMP_USER_STREAM_INFORMATION,
            PMINIDUMP_CALLBACK_INFORMATION);

        auto miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpProc>(
            GetProcAddress(dbghelp, "MiniDumpWriteDump"));

        if (miniDumpWriteDump != nullptr) {
            HANDLE dumpFile = CreateFileA(
                dumpPath,
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (dumpFile != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo;
                dumpExceptionInfo.ThreadId = GetCurrentThreadId();
                dumpExceptionInfo.ExceptionPointers = exceptionInfo;
                dumpExceptionInfo.ClientPointers = FALSE;

                miniDumpWriteDump(
                    GetCurrentProcess(),
                    GetCurrentProcessId(),
                    dumpFile,
                    static_cast<MINIDUMP_TYPE>(
                        MiniDumpWithDataSegs
                        | MiniDumpWithHandleData
                        | MiniDumpWithThreadInfo
                        | MiniDumpWithIndirectlyReferencedMemory),
                    exceptionInfo != nullptr ? &dumpExceptionInfo : nullptr,
                    nullptr,
                    nullptr);

                CloseHandle(dumpFile);
            }
        }

        FreeLibrary(dbghelp);
    }
}

static LONG WINAPI fullDebugUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    writeFullDebugCrashArtifacts("unhandled-seh", exceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int fullDebugSehFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    writeFullDebugCrashArtifacts("top-level-seh", exceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void fullDebugTerminateHandler()
{
    writeFullDebugCrashArtifacts("std::terminate", nullptr);
    TerminateProcess(GetCurrentProcess(), 0xE0000001);
}

static void __cdecl fullDebugPureCallHandler()
{
    writeFullDebugCrashArtifacts("pure-virtual-call", nullptr);
    TerminateProcess(GetCurrentProcess(), 0xE0000002);
}

static void __cdecl fullDebugInvalidParameterHandler(
    const wchar_t*,
    const wchar_t*,
    const wchar_t*,
    unsigned int,
    uintptr_t)
{
    writeFullDebugCrashArtifacts("crt-invalid-parameter", nullptr);
    TerminateProcess(GetCurrentProcess(), 0xE0000003);
}

static void fullDebugSignalHandler(int signalNumber)
{
    const char* reason = "fatal-signal";
    switch (signalNumber) {
    case SIGABRT:
        reason = "SIGABRT/abort";
        break;
    case SIGFPE:
        reason = "SIGFPE";
        break;
    case SIGILL:
        reason = "SIGILL";
        break;
    case SIGSEGV:
        reason = "SIGSEGV";
        break;
    default:
        break;
    }

    writeFullDebugCrashArtifacts(reason, nullptr);
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(0xE0000100u + (signalNumber & 0xFF)));
}

static void installFullDebugCrashHandlers()
{
    SetUnhandledExceptionFilter(fullDebugUnhandledExceptionFilter);
    std::set_terminate(fullDebugTerminateHandler);
    _set_purecall_handler(fullDebugPureCallHandler);
    _set_invalid_parameter_handler(fullDebugInvalidParameterHandler);
    signal(SIGABRT, fullDebugSignalHandler);
    signal(SIGFPE, fullDebugSignalHandler);
    signal(SIGILL, fullDebugSignalHandler);
    signal(SIGSEGV, fullDebugSignalHandler);
}

// 0x4DE700
int main(int argc, char* argv[])
{
    installFullDebugCrashHandlers();

    _GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() == ERROR_SUCCESS) {
        SDL_ShowCursor(SDL_DISABLE);

        gProgramIsActive = true;

        __try {
            falloutMain(argc, argv);
        } __except (fullDebugSehFilter(GetExceptionInformation())) {
            // Crash artifacts were written by fullDebugSehFilter.
        }

        CloseHandle(_GNW95_mutex);
    }
    return 0;
}
#else
bool gProgramIsActive = false;

int main(int argc, char* argv[])
{
#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
#endif

#if __APPLE__ && TARGET_OS_OSX
    char* basePath = SDL_GetBasePath();
    chdir(basePath);
    SDL_free(basePath);
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
#endif

    SDL_ShowCursor(SDL_DISABLE);
    gProgramIsActive = true;
    return falloutMain(argc, argv);
}
#endif

} // namespace fallout

int main(int argc, char* argv[])
{
    return fallout::main(argc, argv);
}
