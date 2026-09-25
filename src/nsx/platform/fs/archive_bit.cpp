// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/platform/fs/archive_bit.hpp"

#include <filesystem>
#include <system_error>

namespace nsx::platform {

namespace fs = std::filesystem;

namespace {

void processDirectory(const fs::path& dir, ArchiveBitReport& report,
                      const std::function<bool(std::string_view)>& onProgress)
{
    std::error_code ec;
    if (!fs::is_directory(dir, ec) || ec) {
        return;
    }

    report.directoriesScanned++;
    if (onProgress && !onProgress(dir.string())) {
        return;
    }

    // Recurse into subdirectories first (depth-first traversal).
    for (const auto& entry :
         fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory(ec)) {
            processDirectory(entry.path(), report, onProgress);
        }
    }

    // Recreate directory entry to reset any FAT archive bit (0x20).
    const fs::path tempDir = dir.string() + ".nsx_tmp";
    fs::rename(dir, tempDir, ec);
    if (ec) {
        report.errors++;
        return;
    }

    fs::create_directories(dir, ec);
    if (ec) {
        std::error_code rollbackEc;
        fs::rename(tempDir, dir, rollbackEc);
        report.errors++;
        return;
    }

    bool moveFailed = false;
    for (const auto& entry :
         fs::directory_iterator(tempDir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            moveFailed = true;
            break;
        }
        const fs::path dest = dir / entry.path().filename();
        fs::rename(entry.path(), dest, ec);
        if (ec) {
            moveFailed = true;
            break;
        }
    }

    if (!moveFailed) {
        fs::remove(tempDir, ec);
        report.directoriesFixed++;
    }
    else {
        report.errors++;
    }
}

}  // namespace

ArchiveBitReport fixArchiveBit(std::string_view rootPath,
                               const std::function<bool(std::string_view currentDir)>& onProgress)
{
    ArchiveBitReport report;
    std::error_code ec;
    const fs::path root(rootPath);
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return report;
    }

    processDirectory(root, report, onProgress);
    return report;
}

ArchiveBitReport fixAllCommonDirectories(
    const std::function<bool(std::string_view currentDir)>& onProgress)
{
    ArchiveBitReport total;
    const char* targets[] = {"/atmosphere", "/switch"};
    for (const char* target : targets) {
        const ArchiveBitReport rep = fixArchiveBit(target, onProgress);
        total.directoriesScanned += rep.directoriesScanned;
        total.directoriesFixed += rep.directoriesFixed;
        total.errors += rep.errors;
    }
    return total;
}

}  // namespace nsx::platform
