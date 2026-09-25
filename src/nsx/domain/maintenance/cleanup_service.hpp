// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief Configuration paths for maintenance cleanup.
/// @since 0.4.0
struct CleanupConfig
{
    std::string stagingDir{
        "/config/nsx-manager/staging"};  ///< Staged downloads and unzipped files.
    std::string cacheDir{
        "/config/nsx-manager/cache"};  ///< Cached manifests and backoff states (PROTECTED).
};

/// @brief Summary of a cleanup operation.
/// @since 0.4.0
struct CleanupReport
{
    std::size_t filesRemoved{};  ///< Number of temporary files deleted.
    bool success{true};          ///< Whether the operation completed without error.
    std::string detail;          ///< One line suitable for UI or logs.
};

/// @brief Safely removes staging leftovers while protecting cache and settings.
///
/// @details Large downloads (firmware zips ~400MB, CFW packs ~50MB) and uncompressed
///          trees live in `/config/nsx-manager/staging/`. If an install or extraction
///          is aborted or leaves partials, this service purges them to recover SD space.
///          Crucially, it verifies that `stagingDir` does NOT intersect `cacheDir`
///          so update/catalogue cache and backoff state are never touched.
///
/// @since 0.4.0
class CleanupService
{
public:
    /// @brief Construct the cleanup service.
    /// @param files FileStore interface.
    /// @param config Staging and cache paths.
    CleanupService(FileStore& files, CleanupConfig config = {});

    /// @brief Purge all files and trees in the staging directory.
    /// @return Report of deleted files.
    [[nodiscard]] CleanupReport cleanStaging();

    /// @brief Check whether any leftover staging files exist.
    /// @return Number of files found in staging.
    [[nodiscard]] std::size_t countStagedFiles() const;

    /// @brief The configuration this service was built with.
    [[nodiscard]] const CleanupConfig& config() const { return m_config; }

private:
    FileStore& m_files;
    CleanupConfig m_config;
};

}  // namespace nsx::domain
