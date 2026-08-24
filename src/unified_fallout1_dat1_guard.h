#ifndef UNIFIED_FALLOUT1_DAT1_GUARD_H
#define UNIFIED_FALLOUT1_DAT1_GUARD_H

#include <cstdarg>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "unified_fallout1_dat1_fixed.h"

namespace fallout {

inline FILE* unifiedFallout1OpenStartupTrace()
{
#if defined(_WIN32)
    char modulePath[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        char* slash = std::strrchr(modulePath, '\\');
        char* forwardSlash = std::strrchr(modulePath, '/');
        if (forwardSlash != nullptr && (slash == nullptr || forwardSlash > slash)) {
            slash = forwardSlash;
        }

        if (slash != nullptr) {
            slash[1] = '\0';
            char logPath[MAX_PATH];
            if (std::snprintf(logPath, sizeof(logPath), "%sunified-startup.log", modulePath) > 0) {
                FILE* stream = std::fopen(logPath, "a");
                if (stream != nullptr) {
                    return stream;
                }
            }
        }
    }
#endif

    return std::fopen("unified-startup.log", "a");
}

inline void unifiedFallout1StartupTrace(const char* format, ...)
{
    FILE* stream = unifiedFallout1OpenStartupTrace();
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
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return dbaseOpen(path);
    }

    // xbaseOpen first probes every path as an archive. For ordinary directories,
    // returning nullptr here is intentional: xbaseOpen then mounts the directory.
    if (path == nullptr || !unifiedFallout1DatPathEndsWith(path, ".dat")) {
#if defined(_WIN32)
        DWORD attributes = path != nullptr ? GetFileAttributesA(path) : INVALID_FILE_ATTRIBUTES;
        bool directoryExists = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        unifiedFallout1StartupTrace(
            "Fallout 1 directory candidate: %s exists=%d; deferring to xbaseOpen directory mount",
            path != nullptr ? path : "<null>",
            directoryExists ? 1 : 0);
#else
        unifiedFallout1StartupTrace(
            "Fallout 1 directory candidate: %s; deferring to xbaseOpen directory mount",
            path != nullptr ? path : "<null>");
#endif
        return nullptr;
    }

    unifiedFallout1StartupTrace("Opening Fallout 1 DAT1: %s", path);

    UnifiedFallout1DatBase* base = unifiedFallout1DatOpenBaseFixed(path);
    if (!unifiedFallout1DatValidateBase(base, path)) {
        delete base;
        return nullptr;
    }

    gUnifiedFallout1DatBases.push_back(base);
    return reinterpret_cast<DBase*>(base);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_DAT1_GUARD_H */
