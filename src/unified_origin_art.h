#ifndef UNIFIED_ORIGIN_ART_H
#define UNIFIED_ORIGIN_ART_H

#include <cstdio>
#include <cstring>

#include "art.h"
#include "memory.h"
#include "unified_resource_origin.h"

namespace fallout {

inline const char* unifiedOriginArtFolderName(int objectType)
{
    static const char* const names[OBJ_TYPE_COUNT] = {
        "items",
        "critters",
        "scenery",
        "walls",
        "tiles",
        "misc",
        "intrface",
        "inven",
        "heads",
        "backgrnd",
        "skilldex",
    };

    return objectType >= 0 && objectType < OBJ_TYPE_COUNT ? names[objectType] : nullptr;
}

inline bool unifiedOriginArtReadListEntry(UnifiedGameId game, int objectType, int index, char* name, size_t nameSize)
{
    if (name == nullptr || nameSize == 0 || index < 0) {
        return false;
    }

    const char* folder = unifiedOriginArtFolderName(objectType);
    if (folder == nullptr) {
        return false;
    }

    UnifiedResourceOriginScope origin(game);

    char listPath[COMPAT_MAX_PATH];
    std::snprintf(listPath, sizeof(listPath), "art\\%s\\%s.lst", folder, folder);
    File* stream = fileOpen(listPath, "rt");
    if (stream == nullptr) {
        return false;
    }

    bool found = false;
    char line[256];
    for (int current = 0; current <= index; current++) {
        if (fileReadString(line, sizeof(line), stream) == nullptr) {
            break;
        }

        if (current == index) {
            char* separator = std::strpbrk(line, " ,;\r\t\n");
            if (separator != nullptr) {
                *separator = '\0';
            }

            if (line[0] != '\0') {
                std::snprintf(name, nameSize, "%s", line);
                found = true;
            }
            break;
        }
    }

    fileClose(stream);
    return found;
}

// Small direct loader for legacy art whose numeric FID belongs to one original
// game's LST namespace. It deliberately bypasses the global F2-engine UI LST and
// resolves the original index -> original filename while an explicit dataset
// origin scope is active. This is used for F1-only world/town/endgame visuals;
// normal engine art continues through the cached FrmImage path.
class UnifiedOriginFrmImage {
public:
    UnifiedOriginFrmImage() = default;
    ~UnifiedOriginFrmImage() { unlock(); }

    UnifiedOriginFrmImage(const UnifiedOriginFrmImage&) = delete;
    UnifiedOriginFrmImage& operator=(const UnifiedOriginFrmImage&) = delete;

    bool isLocked() const { return _art != nullptr; }

    bool lock(UnifiedGameId game, int objectType, int originalIndex)
    {
        if (isLocked()) {
            return false;
        }

        char fileName[64];
        if (!unifiedOriginArtReadListEntry(game, objectType, originalIndex, fileName, sizeof(fileName))) {
            return false;
        }

        const char* folder = unifiedOriginArtFolderName(objectType);
        if (folder == nullptr) {
            return false;
        }

        char artPath[COMPAT_MAX_PATH];
        std::snprintf(artPath, sizeof(artPath), "art\\%s\\%s", folder, fileName);

        UnifiedResourceOriginScope origin(game);
        _art = artLoad(artPath);
        if (_art == nullptr) {
            return false;
        }

        _data = artGetFrameData(_art, 0, 0);
        _width = artGetWidth(_art, 0, 0);
        _height = artGetHeight(_art, 0, 0);
        if (_data == nullptr || _width <= 0 || _height <= 0) {
            unlock();
            return false;
        }

        return true;
    }

    void unlock()
    {
        if (_art != nullptr) {
            internal_free(_art);
        }
        _art = nullptr;
        _data = nullptr;
        _width = 0;
        _height = 0;
    }

    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    unsigned char* getData() const { return _data; }

private:
    Art* _art = nullptr;
    unsigned char* _data = nullptr;
    int _width = 0;
    int _height = 0;
};

} // namespace fallout

#endif /* UNIFIED_ORIGIN_ART_H */
