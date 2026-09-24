// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "nsx/core/hash/sha256.hpp"
#include "nsx/core/result/result.hpp"
#include "nsx/infra/http/curl_client.hpp"
#include "nsx/infra/http/tls_config.hpp"

/// @brief Use-cases. Orchestrates core rules over infra adapters.
namespace nsx::domain {

/// @brief Where the current time comes from.
///
/// @details A port rather than a direct `std::time` call because every rule in
///          the update check is time-dependent - cache freshness, backoff
///          deadlines, handoff timestamps - and a test that cannot move the
///          clock cannot exercise any of them.
/// @since 0.2.0
class Clock
{
public:
    Clock() = default;
    virtual ~Clock() = default;

    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;

    /// @brief The current time.
    /// @return Seconds since the Unix epoch, as the console reports them.
    /// @note On a Switch this is frequently wrong. Every rule that consumes it
    ///       is written to tolerate that - see @ref nsx::core::mayAttempt and
    ///       @ref nsx::core::cacheIsFresh.
    [[nodiscard]] virtual std::int64_t nowUnix() const = 0;
};

/// @brief The real clock.
/// @since 0.2.0
class SystemClock final : public Clock
{
public:
    /// @brief The current time from the C library.
    /// @return Seconds since the Unix epoch.
    [[nodiscard]] std::int64_t nowUnix() const override;
};

/// @brief Every filesystem operation the update flow performs.
///
/// @details Narrow on purpose. These seven operations are the complete list of
///          what staging an update does to the SD card, which makes the blast
///          radius of this feature readable in one place - and lets the whole
///          orchestration run against an in-memory fake on a host.
/// @since 0.2.0
class FileStore
{
public:
    FileStore() = default;
    virtual ~FileStore() = default;

    FileStore(const FileStore&) = delete;
    FileStore& operator=(const FileStore&) = delete;

    /// @brief Whether a path exists and can be opened for reading.
    /// @param path Absolute path.
    /// @return True when it exists.
    [[nodiscard]] virtual bool exists(const std::string& path) const = 0;

    /// @brief Read a whole small file.
    /// @param path Absolute path.
    /// @param maxBytes Refuse anything larger; these are manifests and counters,
    ///        never binaries.
    /// @return The contents, or `std::nullopt` when absent, unreadable or too big.
    [[nodiscard]] virtual std::optional<std::string> readText(const std::string& path,
                                                              std::uint64_t maxBytes) const = 0;

    /// @brief Write a file so a reader never sees it half-written.
    /// @param path Absolute path.
    /// @param data The contents.
    /// @return True on success.
    /// @details Writes a temporary and renames it into place. A half-written
    ///          handoff is indistinguishable from no handoff, which would
    ///          silently abandon an update that was actually in flight.
    [[nodiscard]] virtual bool writeAtomic(const std::string& path, std::string_view data) = 0;

    /// @brief Delete a file, if it is there.
    /// @param path Absolute path.
    /// @return True when the path is gone afterwards.
    virtual bool remove(const std::string& path) = 0;

    /// @brief Create a directory and any missing parents.
    /// @param path Absolute directory path.
    /// @return True when the directory exists afterwards.
    [[nodiscard]] virtual bool makeDirectories(const std::string& path) = 0;

    /// @brief Copy a file, streaming rather than loading it whole.
    /// @param from Source path.
    /// @param to Destination path.
    /// @return True on success.
    [[nodiscard]] virtual bool copyFile(const std::string& from, const std::string& to) = 0;

    /// @brief Hash a file without reading it into memory.
    /// @param path Absolute path.
    /// @return The digest, or `std::nullopt` when the file could not be read.
    [[nodiscard]] virtual std::optional<core::Sha256::Digest> digestOf(
        const std::string& path) const = 0;

    /// @brief Free space on the volume holding a directory.
    /// @param dir Absolute directory path.
    /// @return Free bytes, or `std::nullopt` when it could not be determined.
    /// @note Callers must treat `std::nullopt` as "unknown", never as "zero".
    ///       Refusing to update because free space could not be measured would
    ///       turn a missing statistic into a stranded device.
    [[nodiscard]] virtual std::optional<std::uint64_t> freeSpaceBytes(
        const std::string& dir) const = 0;
};

/// @brief The network operations the update flow performs.
///
/// @details Expressed in the vocabulary of @ref nsx::infra::CurlClient rather
///          than a parallel set of types, so the production adapter is a
///          forwarding shim with nothing to get wrong, and a fake can serve
///          canned bytes through the identical contract.
/// @since 0.2.0
class HttpGateway
{
public:
    HttpGateway() = default;
    virtual ~HttpGateway() = default;

    HttpGateway(const HttpGateway&) = delete;
    HttpGateway& operator=(const HttpGateway&) = delete;

    /// @brief Fetch a small document.
    /// @param url Must be https.
    /// @param ifNoneMatch ETag for a conditional request; empty for none.
    /// @return The response, or why it failed.
    [[nodiscard]] virtual core::Result<infra::Response, infra::HttpError> fetch(
        const std::string& url, std::string_view ifNoneMatch) = 0;

    /// @brief Download and verify a file.
    /// @param url Must be https.
    /// @param destPath Where the verified file should end up.
    /// @param expected Size and digest the content must match.
    /// @param onProgress Optional; returning false aborts.
    /// @return Nothing on success, or why it failed.
    /// @note The implementation must not leave an unverified file at @p destPath
    ///       under any outcome.
    [[nodiscard]] virtual core::Result<std::monostate, infra::HttpError> download(
        const std::string& url, const std::string& destPath,
        const infra::ExpectedArtifact& expected, const infra::ProgressCallback& onProgress) = 0;
};

}  // namespace nsx::domain
