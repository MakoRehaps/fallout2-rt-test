#ifndef DB_H
#define DB_H

#include <stddef.h>
#include <stdio.h>

#ifdef LOCAL_COOP_LOADSAVE_META
#include <string.h>

#include <string>

#include "local_coop_character_state.h"
#include "unified_campaign.h"
#include "unified_fallout1_wilderness_state.h"
#include "unified_fallout1_worldmap_globals.h"
#include "unified_fallout1_worldmap_state.h"
#include "unified_world_system.h"
#endif

#include "unified_resource_resolver.h"

namespace fallout {

typedef XFile File;
typedef void FileReadProgressHandler();
typedef char* StrdupProc(const char* string);

int dbOpen(const char* filePath1, int a2, const char* filePath2, int a4);
int db_total();
void dbExit();
int dbGetFileSize(const char* filePath, int* sizePtr);
int dbGetFileContents(const char* filePath, void* ptr);
int fileClose(File* stream);
File* fileOpen(const char* filename, const char* mode);
int filePrintFormatted(File* stream, const char* format, ...);
int fileReadChar(File* stream);
char* fileReadString(char* str, size_t size, File* stream);
int fileWriteString(const char* s, File* stream);
size_t fileRead(void* buf, size_t size, size_t count, File* stream);
size_t fileWrite(const void* buf, size_t size, size_t count, File* stream);
int fileSeek(File* stream, long offset, int origin);
long fileTell(File* stream);
void fileRewind(File* stream);
int fileEof(File* stream);
int fileReadUInt8(File* stream, unsigned char* valuePtr);
int fileReadInt16(File* stream, short* valuePtr);
int fileReadUInt16(File* stream, unsigned short* valuePtr);
int fileReadInt32(File* stream, int* valuePtr);
int fileReadUInt32(File* stream, unsigned int* valuePtr);
int _db_freadInt(File* stream, int* valuePtr);
int fileReadFloat(File* stream, float* valuePtr);
int fileReadBool(File* stream, bool* valuePtr);
int fileWriteUInt8(File* stream, unsigned char value);
int fileWriteInt16(File* stream, short value);
int fileWriteUInt16(File* stream, unsigned short value);
int fileWriteInt32(File* stream, int value);
int _db_fwriteLong(File* stream, int value);
int fileWriteUInt32(File* stream, unsigned int value);
int fileWriteFloat(File* stream, float value);
int fileWriteBool(File* stream, bool value);
int fileReadUInt8List(File* stream, unsigned char* arr, int count);
int fileReadFixedLengthString(File* stream, char* string, int length);
int fileReadInt16List(File* stream, short* arr, int count);
int fileReadUInt16List(File* stream, unsigned short* arr, int count);
int fileReadInt32List(File* stream, int* arr, int count);
int _db_freadIntCount(File* stream, int* arr, int count);
int fileReadUInt32List(File* stream, unsigned int* arr, int count);
int fileWriteUInt8List(File* stream, unsigned char* arr, int count);
int fileWriteFixedLengthString(File* stream, char* string, int length);
int fileWriteInt16List(File* stream, short* arr, int count);
int fileWriteUInt16List(File* stream, unsigned short* arr, int count);
int fileWriteInt32List(File* stream, int* arr, int count);
int _db_fwriteLongCount(File* stream, int* arr, int count);
int fileWriteUInt32List(File* stream, unsigned int* arr, int count);
int fileNameListInit(const char* pattern, char*** fileNames, int a3, int a4);
void fileNameListFree(char*** fileNames, int a2);
int fileGetSize(File* stream);
void fileSetReadProgressHandler(FileReadProgressHandler* handler, int size);

#ifdef LOCAL_COOP_LOADSAVE_META
inline bool localCoopLoadSaveIsSaveDatPath(const char* filename)
{
    if (filename == nullptr) {
        return false;
    }

    size_t length = strlen(filename);
    static constexpr const char* kSaveName = "SAVE.DAT";
    static constexpr size_t kSaveNameLength = 8;
    if (length < kSaveNameLength) {
        return false;
    }

    return strcmp(filename + length - kSaveNameLength, kSaveName) == 0;
}

inline std::string localCoopLoadSaveMetaPath(const char* saveDatPath)
{
    std::string path = saveDatPath != nullptr ? saveDatPath : "";
    if (path.size() >= 8) {
        path.resize(path.size() - 8);
    }
    path.append("COOPMETA.SAV");
    return path;
}

inline void localCoopLoadSaveStageCampaignMeta(const char* saveDatPath)
{
    unifiedCampaignClearPendingSaveHeader();
    unifiedFallout1WorldMapClearPending();
    unifiedFallout1WildernessClearPending();
    unifiedWorldSystemClearPending();
    localCoopCharacterStateClearPending();

    std::string metaPath = localCoopLoadSaveMetaPath(saveDatPath);
    File* meta = fileOpen(metaPath.c_str(), "rb");
    if (meta == nullptr) {
        return;
    }

    UnifiedCampaignSaveHeader header {};
    bool readOk = fileRead(&header, sizeof(header), 1, meta) == 1;
    bool stagedHeader = readOk && unifiedCampaignStageSaveHeader(header);

    if (stagedHeader) {
        UnifiedCampaignMetaChunkHeader chunkHeader {};
        while (fileRead(&chunkHeader, sizeof(chunkHeader), 1, meta) == 1) {
            bool consumed = false;

            if (header.activeGame == static_cast<uint32_t>(UnifiedGameId::Fallout1)
                && unifiedFallout1WorldMapChunkIsSupported(chunkHeader)) {
                UnifiedFallout1WorldMapState state {};
                if (fileRead(&state, sizeof(state), 1, meta) == 1) {
                    unifiedFallout1WorldMapStage(state);
                    consumed = true;
                }
            } else if (
                header.activeGame == static_cast<uint32_t>(UnifiedGameId::Fallout1)
                && unifiedFallout1WildernessChunkIsSupported(chunkHeader)) {
                UnifiedFallout1WildernessState wilderness {};
                if (fileRead(&wilderness, sizeof(wilderness), 1, meta) == 1) {
                    unifiedFallout1WildernessStage(wilderness);
                    consumed = true;
                }
            } else if (unifiedWorldSystemChunkIsSupported(chunkHeader)) {
                UnifiedWorldSystemState world {};
                if (fileRead(&world, sizeof(world), 1, meta) == 1) {
                    unifiedWorldSystemStage(world);
                    consumed = true;
                }
            } else if (localCoopCharacterStateChunkIsSupported(chunkHeader)) {
                LocalCoopCharacterState characters {};
                if (fileRead(&characters, sizeof(characters), 1, meta) == 1) {
                    localCoopCharacterStateStage(characters);
                    consumed = true;
                }
            }

            if (!consumed) {
                if (chunkHeader.payloadSize > 16 * 1024 * 1024
                    || fileSeek(meta, static_cast<long>(chunkHeader.payloadSize), SEEK_CUR) != 0) {
                    break;
                }
            }
        }
    }

    fileClose(meta);
}

inline void localCoopLoadSaveWriteCampaignMeta(const char* saveDatPath)
{
    std::string metaPath = localCoopLoadSaveMetaPath(saveDatPath);
    File* meta = fileOpen(metaPath.c_str(), "wb");
    if (meta == nullptr) {
        return;
    }

    UnifiedCampaignSaveHeader header = unifiedCampaignMakeSaveHeader();
    if (fileWrite(&header, sizeof(header), 1, meta) != 1) {
        fileClose(meta);
        return;
    }

    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        // Keep the sidecar synchronized with F1's original quest/global-driven
        // town discovery immediately before taking the persistent snapshot.
        unifiedFallout1WorldMapSyncFromGlobals();

        UnifiedCampaignMetaChunkHeader chunkHeader = unifiedFallout1WorldMapMakeChunkHeader();
        const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();

        if (fileWrite(&chunkHeader, sizeof(chunkHeader), 1, meta) == 1
            && fileWrite(&state, sizeof(state), 1, meta) == 1) {
            UnifiedCampaignMetaChunkHeader wildernessHeader =
                unifiedFallout1WildernessMakeChunkHeader();
            const UnifiedFallout1WildernessState& wilderness =
                unifiedFallout1WildernessGetStateConst();

            if (fileWrite(&wildernessHeader, sizeof(wildernessHeader), 1, meta) == 1) {
                fileWrite(&wilderness, sizeof(wilderness), 1, meta);
            }
        }
    }

    UnifiedCampaignMetaChunkHeader worldHeader =
        unifiedWorldSystemMakeChunkHeader();
    const UnifiedWorldSystemState& world = unifiedWorldSystemGetStateConst();
    if (fileWrite(&worldHeader, sizeof(worldHeader), 1, meta) == 1) {
        fileWrite(&world, sizeof(world), 1, meta);
    }

    UnifiedCampaignMetaChunkHeader charactersHeader =
        localCoopCharacterStateMakeChunkHeader();
    const LocalCoopCharacterState& characters =
        localCoopCharacterStateGetConst();
    if (fileWrite(&charactersHeader, sizeof(charactersHeader), 1, meta) == 1) {
        fileWrite(&characters, sizeof(characters), 1, meta);
    }

    fileClose(meta);
}

inline File* localCoopLoadSaveFileOpen(const char* filename, const char* mode)
{
    bool isSaveDat = localCoopLoadSaveIsSaveDatPath(filename);

    if (isSaveDat && mode != nullptr && strcmp(mode, "rb") == 0) {
        // Stage only. Slot-list/header scans also open SAVE.DAT; the metadata is
        // not applied until loadsave.cc reaches its real _PrepLoad -> gameReset
        // path. That keeps simply browsing the load menu from switching games.
        localCoopLoadSaveStageCampaignMeta(filename);
    }

    File* stream = fileOpen(filename, mode);

    if (stream != nullptr
        && isSaveDat
        && mode != nullptr
        && strcmp(mode, "wb") == 0) {
        // The slot directory already exists by the time loadsave.cc creates
        // SAVE.DAT, so the engine metadata sidecar can safely be written.
        localCoopLoadSaveWriteCampaignMeta(filename);
    }

    return stream;
}

#define fileOpen localCoopLoadSaveFileOpen
#endif

} // namespace fallout

#endif /* DB_H */
