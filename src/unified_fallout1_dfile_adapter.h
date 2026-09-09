#ifndef UNIFIED_FALLOUT1_DFILE_ADAPTER_H
#define UNIFIED_FALLOUT1_DFILE_ADAPTER_H

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fpattern.h>

#include "dfile.h"
#include "platform_compat.h"
#include "unified_campaign.h"

namespace fallout {

// Fallout 1 and Fallout 2 use different DAT container formats. Fallout 2 CE's
// DBase reader understands the later footer/table + zlib format. Fallout 1's
// MASTER.DAT/CRITTER.DAT instead begin with nested big-endian associative
// tables and use the original LZSS/chunk encodings. Keep the stock DBase ABI
// intact and adapt F1 archives only at xfile's DBase call boundary.

struct UnifiedFallout1DatEntry {
    std::string path;
    int flags = 0;
    int offset = 0;
    int length = 0;
    int compressedLength = 0;
};

struct UnifiedFallout1DatBase {
    std::string path;
    std::vector<UnifiedFallout1DatEntry> entries;
};

struct UnifiedFallout1DatFile {
    std::vector<unsigned char> data;
    size_t position = 0;
    bool text = false;
};

inline std::vector<UnifiedFallout1DatBase*> gUnifiedFallout1DatBases;
inline std::vector<UnifiedFallout1DatFile*> gUnifiedFallout1DatFiles;

inline bool unifiedFallout1DatReadBe32(FILE* stream, int* value)
{
    unsigned char bytes[4];
    if (std::fread(bytes, 1, sizeof(bytes), stream) != sizeof(bytes)) {
        return false;
    }

    *value = (static_cast<int>(bytes[0]) << 24)
        | (static_cast<int>(bytes[1]) << 16)
        | (static_cast<int>(bytes[2]) << 8)
        | static_cast<int>(bytes[3]);
    return true;
}

inline bool unifiedFallout1DatReadName(FILE* stream, std::string* value)
{
    int length = std::fgetc(stream);
    if (length < 0 || length > 255) {
        return false;
    }

    value->assign(static_cast<size_t>(length), '\0');
    if (length != 0
        && std::fread(value->data(), 1, static_cast<size_t>(length), stream) != static_cast<size_t>(length)) {
        return false;
    }
    return true;
}

inline std::string unifiedFallout1DatNormalizePath(const char* path)
{
    std::string normalized = path != nullptr ? path : "";
    std::replace(normalized.begin(), normalized.end(), '/', '\\');

    while (normalized.size() >= 2 && normalized[0] == '.' && normalized[1] == '\\') {
        normalized.erase(0, 2);
    }
    while (!normalized.empty() && normalized.front() == '\\') {
        normalized.erase(normalized.begin());
    }

    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return normalized;
}

inline UnifiedFallout1DatBase* unifiedFallout1DatAsBase(DBase* base)
{
    for (UnifiedFallout1DatBase* candidate : gUnifiedFallout1DatBases) {
        if (reinterpret_cast<DBase*>(candidate) == base) {
            return candidate;
        }
    }
    return nullptr;
}

inline UnifiedFallout1DatFile* unifiedFallout1DatAsFile(DFile* file)
{
    for (UnifiedFallout1DatFile* candidate : gUnifiedFallout1DatFiles) {
        if (reinterpret_cast<DFile*>(candidate) == file) {
            return candidate;
        }
    }
    return nullptr;
}

inline UnifiedFallout1DatBase* unifiedFallout1DatOpenBase(const char* path)
{
    FILE* stream = compat_fopen(path, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    int rootSize = 0;
    int rootMax = 0;
    int rootDataSize = 0;
    int ignoredPointer = 0;
    if (!unifiedFallout1DatReadBe32(stream, &rootSize)
        || !unifiedFallout1DatReadBe32(stream, &rootMax)
        || !unifiedFallout1DatReadBe32(stream, &rootDataSize)
        || !unifiedFallout1DatReadBe32(stream, &ignoredPointer)
        || rootSize <= 0
        || rootSize > 65536
        || rootMax < rootSize
        || rootDataSize <= 0
        || rootDataSize > 1024) {
        std::fclose(stream);
        return nullptr;
    }

    std::vector<std::string> directories;
    directories.reserve(static_cast<size_t>(rootSize));
    for (int index = 0; index < rootSize; index++) {
        std::string directory;
        if (!unifiedFallout1DatReadName(stream, &directory)) {
            std::fclose(stream);
            return nullptr;
        }
        directories.push_back(unifiedFallout1DatNormalizePath(directory.c_str()));
        if (std::fseek(stream, rootDataSize, SEEK_CUR) != 0) {
            std::fclose(stream);
            return nullptr;
        }
    }

    auto* base = new UnifiedFallout1DatBase();
    base->path = path;

    for (int directoryIndex = 0; directoryIndex < rootSize; directoryIndex++) {
        int entryCount = 0;
        int entryMax = 0;
        int entryDataSize = 0;
        if (!unifiedFallout1DatReadBe32(stream, &entryCount)
            || !unifiedFallout1DatReadBe32(stream, &entryMax)
            || !unifiedFallout1DatReadBe32(stream, &entryDataSize)
            || !unifiedFallout1DatReadBe32(stream, &ignoredPointer)
            || entryCount < 0
            || entryCount > 1000000
            || entryMax < entryCount
            || entryDataSize != 16) {
            delete base;
            std::fclose(stream);
            return nullptr;
        }

        for (int entryIndex = 0; entryIndex < entryCount; entryIndex++) {
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

    return base;
}

inline void unifiedFallout1DatLzssDecode(const unsigned char* source,
    size_t sourceLength,
    std::vector<unsigned char>* destination,
    size_t outputLimit = static_cast<size_t>(-1))
{
    unsigned char ring[4096];
    std::memset(ring, ' ', 4078);
    std::memset(ring + 4078, 0, sizeof(ring) - 4078);
    int ringIndex = 4078;
    size_t pos = 0;

    while (pos < sourceLength && destination->size() < outputLimit) {
        unsigned char control = source[pos++];
        for (int bit = 0; bit < 8 && pos < sourceLength && destination->size() < outputLimit; bit++) {
            if ((control & (1 << bit)) != 0) {
                unsigned char value = source[pos++];
                destination->push_back(value);
                ring[ringIndex] = value;
                ringIndex = (ringIndex + 1) & 0xFFF;
            } else {
                if (pos + 1 >= sourceLength) {
                    return;
                }
                unsigned char low = source[pos++];
                unsigned char high = source[pos++];
                int dictionaryOffset = low | ((high & 0xF0) << 4);
                int chunkLength = (high & 0x0F) + 3;
                for (int index = 0; index < chunkLength && destination->size() < outputLimit; index++) {
                    unsigned char value = ring[(dictionaryOffset + index) & 0xFFF];
                    destination->push_back(value);
                    ring[ringIndex] = value;
                    ringIndex = (ringIndex + 1) & 0xFFF;
                }
            }
        }
    }
}

inline bool unifiedFallout1DatLoadEntry(UnifiedFallout1DatBase* base,
    const UnifiedFallout1DatEntry& entry,
    std::vector<unsigned char>* data)
{
    FILE* stream = compat_fopen(base->path.c_str(), "rb");
    if (stream == nullptr || std::fseek(stream, entry.offset, SEEK_SET) != 0) {
        if (stream != nullptr) {
            std::fclose(stream);
        }
        return false;
    }

    int encoding = entry.flags == 0 ? 0x10 : (entry.flags & 0xF0);
    bool ok = true;
    data->clear();
    data->reserve(static_cast<size_t>(entry.length));

    if (encoding == 0x20) {
        data->resize(static_cast<size_t>(entry.length));
        ok = entry.length == 0
            || std::fread(data->data(), 1, static_cast<size_t>(entry.length), stream) == static_cast<size_t>(entry.length);
    } else if (encoding == 0x10) {
        std::vector<unsigned char> compressed(static_cast<size_t>(entry.compressedLength));
        ok = entry.compressedLength == 0
            || std::fread(compressed.data(), 1, compressed.size(), stream) == compressed.size();
        if (ok) {
            unifiedFallout1DatLzssDecode(compressed.data(), compressed.size(), data, static_cast<size_t>(entry.length));
            ok = data->size() == static_cast<size_t>(entry.length);
        }
    } else if (encoding == 0x40) {
        while (ok && data->size() < static_cast<size_t>(entry.length)) {
            int high = std::fgetc(stream);
            int low = std::fgetc(stream);
            if (high < 0 || low < 0) {
                ok = false;
                break;
            }

            unsigned int chunkSize = (static_cast<unsigned int>(high) << 8) | static_cast<unsigned int>(low);
            bool raw = (chunkSize & 0x8000U) != 0;
            chunkSize &= 0x7FFFU;
            if (chunkSize == 0) {
                ok = false;
                break;
            }

            std::vector<unsigned char> chunk(chunkSize);
            if (std::fread(chunk.data(), 1, chunk.size(), stream) != chunk.size()) {
                ok = false;
                break;
            }

            if (raw) {
                size_t remaining = static_cast<size_t>(entry.length) - data->size();
                size_t amount = std::min(remaining, chunk.size());
                data->insert(data->end(), chunk.begin(), chunk.begin() + amount);
            } else {
                unifiedFallout1DatLzssDecode(chunk.data(), chunk.size(), data, static_cast<size_t>(entry.length));
            }
        }
        ok = ok && data->size() == static_cast<size_t>(entry.length);
    } else {
        ok = false;
    }

    std::fclose(stream);
    return ok;
}

inline const UnifiedFallout1DatEntry* unifiedFallout1DatFindEntry(UnifiedFallout1DatBase* base, const char* filename)
{
    std::string wanted = unifiedFallout1DatNormalizePath(filename);
    for (const UnifiedFallout1DatEntry& entry : base->entries) {
        if (entry.path == wanted) {
            return &entry;
        }
    }
    return nullptr;
}

inline DBase* unifiedDbaseOpen(const char* path)
{
    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        UnifiedFallout1DatBase* base = unifiedFallout1DatOpenBase(path);
        if (base != nullptr) {
            gUnifiedFallout1DatBases.push_back(base);
            return reinterpret_cast<DBase*>(base);
        }
    }
    return dbaseOpen(path);
}

inline bool unifiedDbaseClose(DBase* dbase)
{
    UnifiedFallout1DatBase* base = unifiedFallout1DatAsBase(dbase);
    if (base == nullptr) {
        return dbaseClose(dbase);
    }

    gUnifiedFallout1DatBases.erase(
        std::remove(gUnifiedFallout1DatBases.begin(), gUnifiedFallout1DatBases.end(), base),
        gUnifiedFallout1DatBases.end());
    delete base;
    return true;
}

inline bool unifiedDbaseFindFirstEntry(DBase* dbase, DFileFindData* findFileData, const char* pattern)
{
    UnifiedFallout1DatBase* base = unifiedFallout1DatAsBase(dbase);
    if (base == nullptr) {
        return dbaseFindFirstEntry(dbase, findFileData, pattern);
    }

    std::string normalizedPattern = unifiedFallout1DatNormalizePath(pattern);
    for (size_t index = 0; index < base->entries.size(); index++) {
        if (fpattern_match(normalizedPattern.c_str(), base->entries[index].path.c_str())) {
            std::snprintf(findFileData->fileName, sizeof(findFileData->fileName), "%s", base->entries[index].path.c_str());
            std::snprintf(findFileData->pattern, sizeof(findFileData->pattern), "%s", normalizedPattern.c_str());
            findFileData->index = static_cast<int>(index);
            return true;
        }
    }
    return false;
}

inline bool unifiedDbaseFindNextEntry(DBase* dbase, DFileFindData* findFileData)
{
    UnifiedFallout1DatBase* base = unifiedFallout1DatAsBase(dbase);
    if (base == nullptr) {
        return dbaseFindNextEntry(dbase, findFileData);
    }

    for (size_t index = static_cast<size_t>(findFileData->index + 1); index < base->entries.size(); index++) {
        if (fpattern_match(findFileData->pattern, base->entries[index].path.c_str())) {
            std::snprintf(findFileData->fileName, sizeof(findFileData->fileName), "%s", base->entries[index].path.c_str());
            findFileData->index = static_cast<int>(index);
            return true;
        }
    }
    return false;
}

inline bool unifiedDbaseFindClose(DBase* dbase, DFileFindData* findFileData)
{
    if (unifiedFallout1DatAsBase(dbase) != nullptr) {
        return true;
    }
    return dbaseFindClose(dbase, findFileData);
}

inline DFile* unifiedDfileOpen(DBase* dbase, const char* filename, const char* mode)
{
    UnifiedFallout1DatBase* base = unifiedFallout1DatAsBase(dbase);
    if (base == nullptr) {
        return dfileOpen(dbase, filename, mode);
    }

    if (mode == nullptr || std::strchr(mode, 'r') == nullptr
        || std::strchr(mode, 'w') != nullptr
        || std::strchr(mode, 'a') != nullptr
        || std::strchr(mode, '+') != nullptr) {
        return nullptr;
    }

    const UnifiedFallout1DatEntry* entry = unifiedFallout1DatFindEntry(base, filename);
    if (entry == nullptr) {
        return nullptr;
    }

    auto* file = new UnifiedFallout1DatFile();
    file->text = std::strchr(mode, 'b') == nullptr;
    if (!unifiedFallout1DatLoadEntry(base, *entry, &file->data)) {
        delete file;
        return nullptr;
    }

    gUnifiedFallout1DatFiles.push_back(file);
    return reinterpret_cast<DFile*>(file);
}

inline int unifiedDfileClose(DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    if (file == nullptr) {
        return dfileClose(stream);
    }

    gUnifiedFallout1DatFiles.erase(
        std::remove(gUnifiedFallout1DatFiles.begin(), gUnifiedFallout1DatFiles.end(), file),
        gUnifiedFallout1DatFiles.end());
    delete file;
    return 0;
}

inline long unifiedDfileGetSize(DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    return file != nullptr ? static_cast<long>(file->data.size()) : dfileGetSize(stream);
}

inline int unifiedFallout1DatReadChar(UnifiedFallout1DatFile* file)
{
    if (file->position >= file->data.size()) {
        return -1;
    }

    int ch = file->data[file->position++];
    if (file->text && ch == '\r' && file->position < file->data.size() && file->data[file->position] == '\n') {
        ch = '\n';
        file->position++;
    }
    return ch;
}

inline int unifiedDfileReadChar(DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    return file != nullptr ? unifiedFallout1DatReadChar(file) : dfileReadChar(stream);
}

inline char* unifiedDfileReadString(char* str, int size, DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    if (file == nullptr) {
        return dfileReadString(str, size, stream);
    }
    if (str == nullptr || size <= 0 || file->position >= file->data.size()) {
        return nullptr;
    }

    int index = 0;
    while (index < size - 1) {
        int ch = unifiedFallout1DatReadChar(file);
        if (ch < 0) {
            break;
        }
        str[index++] = static_cast<char>(ch);
        if (ch == '\n') {
            break;
        }
    }
    str[index] = '\0';
    return index != 0 ? str : nullptr;
}

inline size_t unifiedDfileRead(void* ptr, size_t size, size_t count, DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    if (file == nullptr) {
        return dfileRead(ptr, size, count, stream);
    }
    if (ptr == nullptr || size == 0 || count == 0) {
        return 0;
    }

    size_t bytesRequested = size * count;
    size_t remaining = file->data.size() - std::min(file->position, file->data.size());
    size_t bytesToCopy = std::min(bytesRequested, remaining);
    bytesToCopy -= bytesToCopy % size;
    if (bytesToCopy != 0) {
        std::memcpy(ptr, file->data.data() + file->position, bytesToCopy);
        file->position += bytesToCopy;
    }
    return bytesToCopy / size;
}

inline int unifiedDfileSeek(DFile* stream, long offset, int origin)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    if (file == nullptr) {
        return dfileSeek(stream, offset, origin);
    }

    long long base = 0;
    if (origin == SEEK_CUR) {
        base = static_cast<long long>(file->position);
    } else if (origin == SEEK_END) {
        base = static_cast<long long>(file->data.size());
    } else if (origin != SEEK_SET) {
        return -1;
    }

    long long next = base + static_cast<long long>(offset);
    if (next < 0 || next > static_cast<long long>(file->data.size())) {
        return -1;
    }
    file->position = static_cast<size_t>(next);
    return 0;
}

inline long unifiedDfileTell(DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    return file != nullptr ? static_cast<long>(file->position) : dfileTell(stream);
}

inline void unifiedDfileRewind(DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    if (file != nullptr) {
        file->position = 0;
    } else {
        dfileRewind(stream);
    }
}

inline int unifiedDfileEof(DFile* stream)
{
    UnifiedFallout1DatFile* file = unifiedFallout1DatAsFile(stream);
    return file != nullptr ? (file->position >= file->data.size() ? 1 : 0) : dfileEof(stream);
}

inline int unifiedDfilePrintFormattedArgs(DFile* stream, const char* format, va_list args)
{
    if (unifiedFallout1DatAsFile(stream) != nullptr) {
        return -1;
    }
    return dfilePrintFormattedArgs(stream, format, args);
}

inline int unifiedDfileWriteChar(int ch, DFile* stream)
{
    return unifiedFallout1DatAsFile(stream) != nullptr ? -1 : dfileWriteChar(ch, stream);
}

inline int unifiedDfileWriteString(const char* str, DFile* stream)
{
    return unifiedFallout1DatAsFile(stream) != nullptr ? -1 : dfileWriteString(str, stream);
}

inline size_t unifiedDfileWrite(const void* ptr, size_t size, size_t count, DFile* stream)
{
    return unifiedFallout1DatAsFile(stream) != nullptr ? 0 : dfileWrite(ptr, size, count, stream);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_DFILE_ADAPTER_H */
