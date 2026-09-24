// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "nsx/core/hash/sha256.hpp"
#include "nsx/core/result/result.hpp"
#include "nsx/infra/http/tls_config.hpp"

namespace nsx::infra {

/// @brief Progress during a transfer.
/// @since 0.2.0
struct Progress
{
    std::uint64_t received{};  ///< Bytes received so far.
    std::uint64_t total{};     ///< Expected total; zero when the server did not say.
};

/// @brief Called periodically during a transfer.
/// @return False to abort the transfer.
using ProgressCallback = std::function<bool(const Progress&)>;

/// @brief A completed small response held in memory.
/// @since 0.2.0
struct Response
{
    long status{};     ///< HTTP status code.
    std::string body;  ///< The response body.
    std::string etag;  ///< Entity tag; empty when the server sent none.
};

/// @brief What a verified download must match.
/// @details Both fields are required. A download with no expected digest cannot
///          be verified, and this client will not perform one.
/// @since 0.2.0
struct ExpectedArtifact
{
    std::uint64_t size{};           ///< Exact byte count the content must have.
    core::Sha256::Digest sha256{};  ///< Digest the content must match.
};

/// @brief libcurl, configured so it cannot be used insecurely.
///
/// @details Every request gets peer **and** host verification, TLS 1.2 or
///          better, and https-only for both the initial request and any
///          redirect. This class exists so there is exactly one place that
///          decides how a connection is secured, and therefore exactly one
///          place to audit.
///
///          devkitPro ships curl 7.69.1 built against libnx's `ssl` service -
///          the firmware's own TLS stack - so verification runs against the
///          trust store the console itself uses. A pinned Mozilla bundle is
///          compiled into the binary and handed over automatically if a future
///          devkitPro curl accepts `CURLOPT_CAINFO_BLOB` (7.77.0+); the guard
///          around it is a version gate, not a fallback, and verification is
///          enabled identically either way.
///
///          The predecessor set `CURLOPT_SSL_VERIFYPEER` and
///          `CURLOPT_SSL_VERIFYHOST` to `0` on every request
///          (`download.cpp:140-141`, `:198-199`, `:353-354`, `:472-473`) and
///          then extracted the unverified result to the SD card root.
///
/// @see ADR-0016 (which supersedes ADR-0006), docs/architecture/threat-model.md
/// @since 0.2.0
class CurlClient
{
public:
    CurlClient();
    ~CurlClient();

    CurlClient(const CurlClient&) = delete;
    CurlClient& operator=(const CurlClient&) = delete;

    /// @brief Fetch a small document into memory.
    /// @param url Must be https.
    /// @param maxBytes Refuse anything larger; guards against a hostile server
    ///        streaming until the heap is exhausted.
    /// @param ifNoneMatch Optional ETag for a conditional request.
    /// @return The response, or why it failed.
    /// @since 0.2.0
    [[nodiscard]] core::Result<Response, HttpError> get(const std::string& url,
                                                        std::uint64_t maxBytes = 1u << 20u,
                                                        std::string_view ifNoneMatch = {});

    /// @brief Download a file, verifying it before it is usable.
    ///
    /// @details The bytes land in `destPath + ".part"` and are hashed **while**
    ///          they are written - one pass, never a re-read. The temporary is
    ///          promoted to @p destPath only when the byte count and the digest
    ///          both match. A `.part` file is therefore never something that
    ///          could be executed, extracted or chainloaded.
    ///
    ///          The size is checked first because it is free and catches a
    ///          truncated transfer without hashing megabytes to find out.
    ///
    /// @param url Must be https.
    /// @param destPath Final path; the caller owns the directory.
    /// @param expected Size and digest the content must match.
    /// @param onProgress Optional; returning false aborts.
    /// @return Nothing on success, or why it failed.
    /// @since 0.2.0
    [[nodiscard]] core::Result<std::monostate, HttpError> download(
        const std::string& url, const std::string& destPath, const ExpectedArtifact& expected,
        const ProgressCallback& onProgress = {});

private:
    struct Impl;
    Impl* m_impl;
};

}  // namespace nsx::infra
