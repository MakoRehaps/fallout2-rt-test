#ifndef DAT_EXTRACTOR_H
#define DAT_EXTRACTOR_H

namespace fallout {

inline constexpr int kDatExtractorNotRequested = -1000;

// Handles the installer's private --extract-dat/--extract-to command mode.
// Returns kDatExtractorNotRequested for an ordinary game launch, otherwise a
// process exit code (zero on success).
int datExtractorTryRun(int argc, char** argv);

} // namespace fallout

#endif /* DAT_EXTRACTOR_H */
