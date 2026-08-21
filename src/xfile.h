#ifndef XFILE_H
#define XFILE_H

#include <stdio.h>

#include <zlib.h>

#include "dfile.h"
#include "platform_compat.h"
#include "unified_fallout1_dfile_adapter.h"
#include "unified_fallout1_dat1_fixed.h"

// xfile.cc remains the common resource layer. Route only its DBase/DFile calls
// through profile-aware wrappers. Fallout 1 uses the corrected DAT1 header and
// directory parser while Fallout 2 keeps the untouched DAT2 implementation.
#define dbaseOpen unifiedDbaseOpenFixed
#define dbaseClose unifiedDbaseClose
#define dbaseFindFirstEntry unifiedDbaseFindFirstEntry
#define dbaseFindNextEntry unifiedDbaseFindNextEntry
#define dbaseFindClose unifiedDbaseFindClose
#define dfileGetSize unifiedDfileGetSize
#define dfileClose unifiedDfileClose
#define dfileOpen unifiedDfileOpen
#define dfilePrintFormattedArgs unifiedDfilePrintFormattedArgs
#define dfileReadChar unifiedDfileReadChar
#define dfileReadString unifiedDfileReadString
#define dfileWriteChar unifiedDfileWriteChar
#define dfileWriteString unifiedDfileWriteString
#define dfileRead unifiedDfileRead
#define dfileWrite unifiedDfileWrite
#define dfileSeek unifiedDfileSeek
#define dfileTell unifiedDfileTell
#define dfileRewind unifiedDfileRewind
#define dfileEof unifiedDfileEof

namespace fallout {

typedef enum XFileType {
    XFILE_TYPE_FILE,
    XFILE_TYPE_DFILE,
    XFILE_TYPE_GZFILE,
} XFileType;

// A universal database of files.
typedef struct XBase {
    // The path to directory or .DAT file that this xbase represents.
    char* path;

    // The [DBase] instance that this xbase represents.
    DBase* dbase;

    // A flag used to denote that this xbase represents .DAT file (true), or
    // a directory (false).
    //
    // NOTE: Original type is 1 byte, likely unsigned char.
    bool isDbase;

    // Next [XBase] in linked list.
    struct XBase* next;
} XBase;

typedef struct XFile {
    XFileType type;
    union {
        FILE* file;
        DFile* dfile;
        gzFile gzfile;
    };
} XFile;

typedef struct XList {
    int fileNamesLength;
    char** fileNames;
} XList;

int xfileClose(XFile* stream);
XFile* xfileOpen(const char* filename, const char* mode);
int xfilePrintFormatted(XFile* xfile, const char* format, ...);
int xfilePrintFormattedArgs(XFile* stream, const char* format, va_list args);
int xfileReadChar(XFile* stream);
char* xfileReadString(char* string, int size, XFile* stream);
int xfileWriteChar(int ch, XFile* stream);
int xfileWriteString(const char* s, XFile* stream);
size_t xfileRead(void* ptr, size_t size, size_t count, XFile* stream);
size_t xfileWrite(const void* buf, size_t size, size_t count, XFile* stream);
int xfileSeek(XFile* stream, long offset, int origin);
long xfileTell(XFile* stream);
void xfileRewind(XFile* stream);
int xfileEof(XFile* stream);
long xfileGetSize(XFile* stream);
bool xbaseReopenAll(char* paths);
bool xbaseOpen(const char* path);
bool xlistInit(const char* pattern, XList* xlist);
void xlistFree(XList* xlist);

} // namespace fallout

#endif /* XFILE_H */
