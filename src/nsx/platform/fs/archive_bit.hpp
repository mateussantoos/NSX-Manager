// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace nsx::platform {

/// @brief Statistics from an archive bit repair run.
/// @since 0.4.0
struct ArchiveBitReport
{
    std::size_t directoriesScanned{};  ///< Total directories inspected.
    std::size_t directoriesFixed{};    ///< Directories that were recreated cleanly.
    std::size_t errors{};              ///< Directories that could not be processed.
};

/// @brief Recursively clear the FAT archive bit from directories.
///
/// @details macOS and some transfer tools set the FAT archive attribute (0x20)
///          on directories. Horizon OS treats directories with the archive bit
///          as ConcatenationFiles (split files), causing Atmosphere crashes,
///          mod loading failures, or invisible homebrew in `/switch`.
///          This function cleans directory entries on the SD card so Horizon
///          treats them strictly as directories.
///
/// @param rootPath Root directory to fix (e.g. "/atmosphere" or "/switch").
/// @param onProgress Callback invoked per directory; returns false to abort.
/// @return Report summarizing scanned and repaired directory counts.
/// @since 0.4.0
ArchiveBitReport fixArchiveBit(
    std::string_view rootPath,
    const std::function<bool(std::string_view currentDir)>& onProgress = {});

/// @brief Fix archive bits across the common problematic directories:
///        "/atmosphere" and "/switch".
/// @param onProgress Callback invoked per directory; returns false to abort.
/// @return Combined report.
/// @since 0.4.0
ArchiveBitReport fixAllCommonDirectories(
    const std::function<bool(std::string_view currentDir)>& onProgress = {});

}  // namespace nsx::platform
