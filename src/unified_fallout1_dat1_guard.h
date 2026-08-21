#ifndef UNIFIED_FALLOUT1_DAT1_GUARD_H
#define UNIFIED_FALLOUT1_DAT1_GUARD_H

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "unified_fallout1_dat1_fixed.h"

namespace fallout {

inline void unifiedFallout1StartupTrace(const char* format, ...)
{
    FILE* stream = std::fopen("unified-startup.log", "a");
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

    unifiedFallout1StartupTrace("Opening Fallout 1 DAT1: %s", path != nullptr ? path : "<null>");

    UnifiedFallout1DatBase* base = unifiedFallout1DatOpenBaseFixed(path);
    if (!unifiedFallout1DatValidateBase(base, path)) {
        delete base;
        // Fail closed. Never feed a Fallout 1 archive to Fallout 2's DAT2 reader.
        return nullptr;
    }

    gUnifiedFallout1DatBases.push_back(base);
    return reinterpret_cast<DBase*>(base);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_DAT1_GUARD_H */
