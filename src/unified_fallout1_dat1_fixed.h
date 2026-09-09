#ifndef UNIFIED_FALLOUT1_DAT1_FIXED_H
#define UNIFIED_FALLOUT1_DAT1_FIXED_H

#include "unified_fallout1_dfile_adapter.h"

namespace fallout {

// Fallout 1 DAT1 is not an assoc_array dump at the file header. The first
// four big-endian words are directory count, allocation hint, reserved, and a
// timestamp/unused value. This is followed immediately by one Pascal string
// per directory. In particular, MASTER.DAT's first directory is ".", which
// owns COLOR.PAL and FONT0.FON..FONTx.FON.
//
// The original unified adapter incorrectly treated the third header word as a
// per-directory payload size and skipped bytes after every directory name.
// Since that field is normally zero in DAT1, valid Fallout 1 archives were
// rejected before the root font entries could ever be indexed.
inline UnifiedFallout1DatBase* unifiedFallout1DatOpenBaseFixed(const char* path)
{
    FILE* stream = compat_fopen(path, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    int directoryCount = 0;
    int allocationHint = 0;
    int reserved = 0;
    int timestamp = 0;
    if (!unifiedFallout1DatReadBe32(stream, &directoryCount)
        || !unifiedFallout1DatReadBe32(stream, &allocationHint)
        || !unifiedFallout1DatReadBe32(stream, &reserved)
        || !unifiedFallout1DatReadBe32(stream, &timestamp)
        || directoryCount <= 0
        || directoryCount > 65536
        || allocationHint < directoryCount) {
        std::fclose(stream);
        return nullptr;
    }

    std::vector<std::string> directories;
    directories.reserve(static_cast<size_t>(directoryCount));
    for (int index = 0; index < directoryCount; index++) {
        std::string directory;
        if (!unifiedFallout1DatReadName(stream, &directory)) {
            std::fclose(stream);
            return nullptr;
        }
        directories.push_back(unifiedFallout1DatNormalizePath(directory.c_str()));
    }

    auto* base = new UnifiedFallout1DatBase();
    base->path = path;

    for (int directoryIndex = 0; directoryIndex < directoryCount; directoryIndex++) {
        int fileCount = 0;
        int fileAllocationHint = 0;
        int fixedMetadataSize = 0;
        int directoryTimestamp = 0;
        if (!unifiedFallout1DatReadBe32(stream, &fileCount)
            || !unifiedFallout1DatReadBe32(stream, &fileAllocationHint)
            || !unifiedFallout1DatReadBe32(stream, &fixedMetadataSize)
            || !unifiedFallout1DatReadBe32(stream, &directoryTimestamp)
            || fileCount < 0
            || fileCount > 1000000
            || fileAllocationHint < fileCount
            || fixedMetadataSize != 16) {
            delete base;
            std::fclose(stream);
            return nullptr;
        }

        for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
            std::string name;
            UnifiedFallout1DatEntry entry;
            if (!unifiedFallout1DatReadName(stream, &name)
                || !unifiedFallout1DatReadBe32(stream, &entry.flags)
                || !unifiedFallout1DatReadBe32(stream, &entry.offset)
                || !unifiedFallout1DatReadBe32(stream, &entry.length)
                || !unifiedFallout1DatReadBe32(stream, &entry.compressedLength)
                || entry.offset < 0
                || entry.length < 0
                || entry.compressedLength < 0) {
                delete base;
                std::fclose(stream);
                return nullptr;
            }

            std::string normalizedName = unifiedFallout1DatNormalizePath(name.c_str());
            const std::string& directory = directories[static_cast<size_t>(directoryIndex)];
            if (!directory.empty() && directory != ".") {
                entry.path = directory + "\\" + normalizedName;
            } else {
                entry.path = normalizedName;
            }
            base->entries.push_back(std::move(entry));
        }
    }

    std::fclose(stream);
    if (base->entries.empty()) {
        delete base;
        return nullptr;
    }

    // FONT0.FON is a useful structural sanity check for MASTER.DAT, but do not
    // require it here because the same parser is also used for CRITTER.DAT.
    return base;
}

inline DBase* unifiedDbaseOpenFixed(const char* path)
{
    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        UnifiedFallout1DatBase* base = unifiedFallout1DatOpenBaseFixed(path);
        if (base != nullptr) {
            gUnifiedFallout1DatBases.push_back(base);
            return reinterpret_cast<DBase*>(base);
        }
    }

    return dbaseOpen(path);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_DAT1_FIXED_H */
