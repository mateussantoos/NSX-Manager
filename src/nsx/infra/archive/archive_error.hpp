// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string_view>

namespace nsx::infra {

/// @brief Why an extraction stopped.
///
/// @details Split from the extractor itself for the same reason
///          `http/tls_config.hpp` is split from `http/curl_client.hpp`: this is
///          the vocabulary the domain layer and the UI speak, and it must be
///          available to a host build that has no zlib and no zipper. The
///          extractor needs a console; the words for what it did do not.
/// @since 0.3.0
enum class ArchiveError
{
    CannotOpen,       ///< Not readable, or not a zip.
    Empty,            ///< No entries at all.
    UnsafeEntry,      ///< An entry name was refused by the extraction policy.
    TooLarge,         ///< Declared uncompressed size exceeds the caller's ceiling.
    DirectoryFailed,  ///< Could not create a destination directory.
    WriteFailed,      ///< Could not write a file.
    ReadFailed,       ///< Could not decompress an entry.
    Aborted           ///< Cancelled by the caller.
};

/// @brief A short English description of an extraction failure.
/// @param error The error to describe.
/// @return A description suitable for a log line.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(ArchiveError error);

}  // namespace nsx::infra
