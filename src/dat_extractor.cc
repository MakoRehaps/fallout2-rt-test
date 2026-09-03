#include "dat_extractor.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "dfile.h"
#include "unified_campaign.h"
#include "unified_fallout1_dat1_guard.h"

namespace fallout {

namespace {

const char* findArgumentValue(int argc, char** argv, const char* prefix)
{
    const size_t prefixLength = std::strlen(prefix);
    for (int index = 1; index < argc; index++) {
        const char* argument = argv[index];
        if (argument != nullptr && std::strncmp(argument, prefix, prefixLength) == 0) {
            return argument + prefixLength;
        }
    }
    return nullptr;
}

bool makeSafeRelativePath(const char* archivePath, std::filesystem::path* result)
{
    if (archivePath == nullptr || *archivePath == '\0' || result == nullptr) {
        return false;
    }

    std::string normalized = archivePath;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (normalized.empty() || normalized.front() == '/' || normalized.find(':') != std::string::npos) {
        return false;
    }

    std::filesystem::path relative;
    size_t start = 0;
    while (start <= normalized.size()) {
        size_t end = normalized.find('/', start);
        std::string component = normalized.substr(
            start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!component.empty() && component != ".") {
            if (component == "..") {
                return false;
            }
            relative /= component;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    *result = relative.lexically_normal();
    return true;
}

bool extractEntry(DBase* archive,
    const char* entryName,
    const std::filesystem::path& outputRoot,
    std::ofstream& log)
{
    std::filesystem::path relative;
    if (!makeSafeRelativePath(entryName, &relative)) {
        log << "Rejected unsafe archive path: " << (entryName != nullptr ? entryName : "<null>") << '\n';
        return false;
    }

    DFile* input = unifiedDfileOpen(archive, entryName, "rb");
    if (input == nullptr) {
        log << "Could not open archive entry: " << entryName << '\n';
        return false;
    }

    long expectedSize = unifiedDfileGetSize(input);
    if (expectedSize < 0) {
        unifiedDfileClose(input);
        log << "Invalid archive entry size: " << entryName << '\n';
        return false;
    }

    std::filesystem::path destination = outputRoot / relative;
    std::filesystem::path temporary = destination;
    temporary += ".extracting";

    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) {
        unifiedDfileClose(input);
        log << "Could not create directory for: " << destination.string() << '\n';
        return false;
    }

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        unifiedDfileClose(input);
        log << "Could not create extracted file: " << temporary.string() << '\n';
        return false;
    }

    std::array<char, 64 * 1024> buffer {};
    long written = 0;
    bool ok = true;
    while (written < expectedSize) {
        size_t wanted = static_cast<size_t>(std::min<long>(
            static_cast<long>(buffer.size()),
            expectedSize - written));
        size_t received = unifiedDfileRead(buffer.data(), 1, wanted, input);
        if (received == 0) {
            ok = false;
            break;
        }
        output.write(buffer.data(), static_cast<std::streamsize>(received));
        if (!output) {
            ok = false;
            break;
        }
        written += static_cast<long>(received);
    }

    output.close();
    unifiedDfileClose(input);
    if (!ok || written != expectedSize) {
        std::filesystem::remove(temporary, error);
        log << "Incomplete archive entry: " << entryName
            << " expected=" << expectedSize << " wrote=" << written << '\n';
        return false;
    }

    // Extraction order provides archive priority. A fully written temporary
    // file replaces the prior layer only after its size has been verified.
    error.clear();
    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        log << "Could not finalize extracted file: " << destination.string() << '\n';
        return false;
    }
    return true;
}

int extractArchive(const char* archivePath, const char* outputPath)
{
    std::filesystem::path outputRoot(outputPath);
    std::error_code error;
    std::filesystem::create_directories(outputRoot, error);
    if (error) {
        return 3;
    }

    std::ofstream log(outputRoot / "dat-extract.log", std::ios::app);
    log << "Extracting " << archivePath << " -> " << outputRoot.string() << '\n';

    DBase* archive = unifiedDbaseOpenGuarded(archivePath);
    if (archive == nullptr) {
        log << "Could not parse DAT archive.\n";
        return 4;
    }

    DFileFindData found {};
    int extracted = 0;
    bool ok = true;
    if (unifiedDbaseFindFirstEntry(archive, &found, "*")) {
        do {
            if (!extractEntry(archive, found.fileName, outputRoot, log)) {
                ok = false;
                break;
            }
            extracted++;
        } while (unifiedDbaseFindNextEntry(archive, &found));
        unifiedDbaseFindClose(archive, &found);
    } else {
        ok = false;
        log << "Archive contained no files.\n";
    }

    unifiedDbaseClose(archive);
    log << (ok ? "Completed" : "Failed") << ": files=" << extracted << '\n';
    return ok ? 0 : 5;
}

} // namespace

int datExtractorTryRun(int argc, char** argv)
{
    const char* archivePath = findArgumentValue(argc, argv, "--extract-dat=");
    const char* outputPath = findArgumentValue(argc, argv, "--extract-to=");
    if (archivePath == nullptr && outputPath == nullptr) {
        return kDatExtractorNotRequested;
    }
    if (archivePath == nullptr || *archivePath == '\0'
        || outputPath == nullptr || *outputPath == '\0') {
        return 2;
    }

    // The installer supplies both roots. This lets the guarded archive opener
    // select Fallout 1 DAT1 or Fallout 2 DAT2 by the archive's actual origin.
    unifiedCampaignConfigureFromArgs(argc, argv);
    return extractArchive(archivePath, outputPath);
}

} // namespace fallout
