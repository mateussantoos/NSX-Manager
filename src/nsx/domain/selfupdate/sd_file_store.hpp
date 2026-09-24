// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "nsx/domain/selfupdate/ports.hpp"

namespace nsx::domain {

/// @brief The real SD card, behind @ref FileStore.
///
/// @details Plain C stdio plus `mkdir`, because that is what works identically
///          under the FatFs devoptab libnx installs and under a host libc - the
///          adapter is therefore the same code the tests' fake stands in for,
///          not a second implementation with its own behaviour.
///
///          Two FatFs facts shape everything here: `rename` cannot overwrite an
///          existing file, so every rename clears its destination first; and
///          there is no cheap `fsync`, so `fflush` before the rename is the
///          durability boundary available.
///
/// @since 0.2.0
class SdFileStore final : public FileStore
{
public:
    /// @brief Whether a path exists and can be opened for reading.
    /// @param path Absolute path.
    /// @return True when it exists.
    [[nodiscard]] bool exists(const std::string& path) const override;

    /// @brief Read a whole small file.
    /// @param path Absolute path.
    /// @param maxBytes Refuse anything larger.
    /// @return The contents, or `std::nullopt`.
    [[nodiscard]] std::optional<std::string> readText(const std::string& path,
                                                      std::uint64_t maxBytes) const override;

    /// @brief Write through a temporary and rename into place.
    /// @param path Absolute path.
    /// @param data The contents.
    /// @return True on success.
    [[nodiscard]] bool writeAtomic(const std::string& path, std::string_view data) override;

    /// @brief Delete a file, if it is there.
    /// @param path Absolute path.
    /// @return True when the path is gone afterwards.
    bool remove(const std::string& path) override;

    /// @brief Create a directory and any missing parents.
    /// @param path Absolute directory path.
    /// @return True when the directory exists afterwards.
    [[nodiscard]] bool makeDirectories(const std::string& path) override;

    /// @brief Copy a file in fixed-size chunks.
    /// @param from Source path.
    /// @param to Destination path.
    /// @return True on success.
    [[nodiscard]] bool copyFile(const std::string& from, const std::string& to) override;

    /// @brief Hash a file in fixed-size chunks.
    /// @param path Absolute path.
    /// @return The digest, or `std::nullopt`.
    [[nodiscard]] std::optional<core::Sha256::Digest> digestOf(
        const std::string& path) const override;

    /// @brief Free space on the volume holding a directory.
    /// @param dir Absolute directory path.
    /// @return Free bytes, or `std::nullopt` when `statvfs` is unavailable or fails.
    [[nodiscard]] std::optional<std::uint64_t> freeSpaceBytes(
        const std::string& dir) const override;
};

}  // namespace nsx::domain
