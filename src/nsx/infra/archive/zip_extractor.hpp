// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "nsx/core/paths/extraction_policy.hpp"
#include "nsx/core/result/result.hpp"
#include "nsx/infra/archive/archive_error.hpp"

namespace nsx::infra {

/// @brief Progress through an archive.
/// @since 0.3.0
struct ExtractionProgress
{
    std::size_t entriesDone{};     ///< Entries handled so far.
    std::size_t entriesTotal{};    ///< Entries in the archive.
    std::uint64_t bytesWritten{};  ///< Uncompressed bytes written so far.
    std::string currentEntry;      ///< The entry being handled.
};

/// @brief Called as extraction proceeds.
/// @return False to abort.
using ExtractionCallback = std::function<bool(const ExtractionProgress&)>;

/// @brief What an extraction did.
/// @since 0.3.0
struct ExtractionReport
{
    std::size_t filesWritten{};        ///< Files created or replaced.
    std::size_t directoriesCreated{};  ///< Directories created.
    std::size_t skippedPreserved{};    ///< Left alone because of `preserve.txt`.
    std::uint64_t bytesWritten{};      ///< Total uncompressed bytes written.

    /// @brief Entry names the policy refused, with the reason.
    /// @details Populated on an `ArchiveError::UnsafeEntry` failure so the
    ///          log can name what was wrong rather than only that something was.
    std::vector<std::string> rejected;
};

/// @brief Extract a zip archive under a destination, one entry at a time.
///
/// @details **Never calls `Unzipper::extract()`.** That function takes a
///          destination and resolves every entry name itself, which is the
///          zip-slip surface in its entirety - the archive would be choosing
///          where its contents land. Instead this enumerates entries, hands
///          each name to @ref nsx::core::ExtractionPolicy, and streams the
///          bytes to the path the policy returned. zipper decompresses; it
///          never decides a destination.
///
///          A refused entry **fails the whole archive** rather than being
///          skipped. An archive containing one traversal entry is not a
///          well-formed archive with a bad file in it; it is an archive that
///          tried something, and extracting the rest of it would mean trusting
///          a source that just demonstrated it should not be trusted.
///
///          Not transactional. A failure part-way leaves what was already
///          written in place, because a Switch has no rename-based way to make
///          a multi-thousand-file extraction atomic. Callers extract to a
///          staging directory and merge, which is what makes that acceptable -
///          see `docs/architecture/update-pipeline.md`.
///
/// @param archivePath The zip file on disk.
/// @param policy Decides, per entry, where it goes or why it does not.
/// @param maxUncompressedBytes Refuse an archive whose declared uncompressed
///        size exceeds this. Guards against a decompression bomb filling the
///        card; the declared size is read from the directory before any byte is
///        written.
/// @param onProgress Optional; returning false aborts.
/// @return What was done, or why it stopped.
/// @since 0.3.0
[[nodiscard]] core::Result<ExtractionReport, ArchiveError> extractZip(
    const std::string& archivePath, const core::ExtractionPolicy& policy,
    std::uint64_t maxUncompressedBytes, const ExtractionCallback& onProgress = {});

}  // namespace nsx::infra
