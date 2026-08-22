#ifndef UNIFIED_FALLOUT1_DAT1_GUARD_H
#define UNIFIED_FALLOUT1_DAT1_GUARD_H

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "unified_fallout1_dat1_fixed.h"

namespace fallout {

inline std::string unifiedDebugLogPath()
{
#if defined(_WIN32)
    char modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length != 0 && length < MAX_PATH) {
        char* slash = std::strrchr(modulePath, '\\');
        if (slash != nullptr) {
            slash[1] = '\0';
            return std::string(modulePath) + "unified-debug.log";
        }
    }
#endif
    return "unified-debug.log";
}

inline void unifiedFallout1StartupTrace(const char* format, ...)
{
    std::string logPath = unifiedDebugLogPath();
    FILE* stream = std::fopen(logPath.c_str(), "a");
    if (stream == nullptr) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vfprintf(stream, format, args);
    va_end(args);
    std::fputc('\n', stream);
    std::fflush(stream);
    std::fclose(stream);
}

#if defined(_WIN32)
inline LONG WINAPI unifiedDebugUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    DWORD code = 0;
    void* address = nullptr;
    if (exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr) {
        code = exceptionInfo->ExceptionRecord->ExceptionCode;
        address = exceptionInfo->ExceptionRecord->ExceptionAddress;
    }

    unifiedFallout1StartupTrace("*** UNHANDLED EXCEPTION code=0x%08lX address=%p thread=%lu ***",
        static_cast<unsigned long>(code), address, static_cast<unsigned long>(GetCurrentThreadId()));
    return EXCEPTION_EXECUTE_HANDLER;
}

inline bool unifiedDebugInstallCrashHandler()
{
    SetUnhandledExceptionFilter(unifiedDebugUnhandledExceptionFilter);
    unifiedFallout1StartupTrace("=== unified debug session begin ===");
    return true;
}

inline bool gUnifiedDebugCrashHandlerInstalled = unifiedDebugInstallCrashHandler();
#endif

inline bool unifiedFallout1DatPathEndsWith(const char* path, const char* suffix)
{
    if (path == nullptr || suffix == nullptr) {
        return false;
    }

    size_t pathLength = std::strlen(path);
    size_t suffixLength = std::strlen(suffix);
    if (pathLength < suffixLength) {
        return false;
    }

    return compat_stricmp(path + pathLength - suffixLength, suffix) == 0;
}

inline bool unifiedFallout1DatValidateBase(UnifiedFallout1DatBase* base, const char* path)
{
    if (base == nullptr) {
        unifiedFallout1StartupTrace("DAT1 open failed: %s", path != nullptr ? path : "<null>");
        return false;
    }

    FILE* archive = compat_fopen(path, "rb");
    if (archive == nullptr) {
        unifiedFallout1StartupTrace("DAT1 validation could not reopen: %s", path);
        return false;
    }

    std::fseek(archive, 0, SEEK_END);
    long archiveSize = std::ftell(archive);
    std::fclose(archive);

    if (archiveSize <= 0) {
        unifiedFallout1StartupTrace("DAT1 invalid archive size: %s size=%ld", path, archiveSize);
        return false;
    }

    bool hasFont0 = false;
    for (const UnifiedFallout1DatEntry& entry : base->entries) {
        if (entry.path == "FONT0.FON") {
            hasFont0 = true;
        }

        if (entry.offset < 0 || entry.length < 0 || entry.compressedLength < 0
            || entry.offset >= archiveSize) {
            unifiedFallout1StartupTrace(
                "DAT1 invalid entry: archive=%s entry=%s offset=%d length=%d packed=%d size=%ld",
                path,
                entry.path.c_str(),
                entry.offset,
                entry.length,
                entry.compressedLength,
                archiveSize);
            return false;
        }

        int encoding = entry.flags == 0 ? 0x10 : (entry.flags & 0xF0);
        long storedLength = encoding == 0x20 ? entry.length : entry.compressedLength;
        if (storedLength > 0 && static_cast<long long>(entry.offset) + storedLength > archiveSize) {
            unifiedFallout1StartupTrace(
                "DAT1 entry exceeds archive: archive=%s entry=%s encoding=%02X offset=%d stored=%ld size=%ld",
                path,
                entry.path.c_str(),
                encoding,
                entry.offset,
                storedLength,
                archiveSize);
            return false;
        }
    }

    if (unifiedFallout1DatPathEndsWith(path, "master.dat") && !hasFont0) {
        unifiedFallout1StartupTrace(
            "DAT1 MASTER parsed but FONT0.FON was not indexed: %s entries=%zu",
            path,
            base->entries.size());
        return false;
    }

    unifiedFallout1StartupTrace(
        "DAT1 validated: %s entries=%zu size=%ld font0=%d",
        path,
        base->entries.size(),
        archiveSize,
        hasFont0 ? 1 : 0);
    return true;
}

inline DBase* unifiedDbaseOpenGuarded(const char* path)
{
    unifiedFallout1StartupTrace("DBASE open request activeGame=%u path=%s",
        static_cast<unsigned int>(unifiedCampaignGetActiveGame()), path != nullptr ? path : "<null>");

    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        DBase* result = dbaseOpen(path);
        unifiedFallout1StartupTrace("DBASE DAT2 result=%p path=%s", result, path != nullptr ? path : "<null>");
        return result;
    }

    UnifiedFallout1DatBase* base = unifiedFallout1DatOpenBaseFixed(path);
    if (!unifiedFallout1DatValidateBase(base, path)) {
        delete base;
        unifiedFallout1StartupTrace("DBASE DAT1 rejected path=%s", path != nullptr ? path : "<null>");
        return nullptr;
    }

    gUnifiedFallout1DatBases.push_back(base);
    unifiedFallout1StartupTrace("DBASE DAT1 mounted handle=%p path=%s", base, path != nullptr ? path : "<null>");
    return reinterpret_cast<DBase*>(base);
}

inline DFile* unifiedDfileOpenDebug(DBase* dbase, const char* filename, const char* mode)
{
    unifiedFallout1StartupTrace("DFILE open begin base=%p file=%s mode=%s",
        dbase,
        filename != nullptr ? filename : "<null>",
        mode != nullptr ? mode : "<null>");
    DFile* result = unifiedDfileOpen(dbase, filename, mode);
    if (result != nullptr) {
        unifiedFallout1StartupTrace("DFILE open OK file=%s handle=%p size=%ld",
            filename != nullptr ? filename : "<null>",
            result,
            unifiedDfileGetSize(result));
    } else {
        unifiedFallout1StartupTrace("DFILE open FAIL file=%s", filename != nullptr ? filename : "<null>");
    }
    return result;
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_DAT1_GUARD_H */