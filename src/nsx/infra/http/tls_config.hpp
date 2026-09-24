// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

namespace nsx::infra {

/// @brief Why a transfer failed, in terms the UI can act on.
/// @since 0.2.0
enum class HttpError
{
    None,
    NoNetwork,      ///< The console reports no connection.
    DnsFailure,     ///< The host could not be resolved.
    ConnectFailed,  ///< Reached DNS but not the host.
    Timeout,
    TlsVerifyFailed,  ///< Certificate rejected. Never downgraded.
    TlsClockSkew,     ///< Certificate dates rejected - almost always the console clock.
    InsecureUrl,      ///< A non-https URL was refused before any request.
    HttpStatus,       ///< A 4xx or 5xx response.
    RateLimited,      ///< 403 or 429; back off and serve the cache.
    TooLarge,         ///< Exceeded the caller's size limit.
    SizeMismatch,     ///< Byte count did not match the manifest.
    DigestMismatch,   ///< SHA-256 did not match the manifest.
    WriteFailed,      ///< Could not write the destination file.
    Aborted,          ///< Cancelled by the caller.
    Internal
};

/// @brief A short English description of a transfer failure.
/// @param error The error to describe.
/// @return Text suitable for a log line.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(HttpError error);

/// @brief Guidance the user can act on, or empty when there is nothing to do.
/// @param error The error to advise on.
/// @return A sentence for the UI, or an empty view.
/// @details @ref HttpError::TlsClockSkew returns the console-clock advice. The
///          Switch RTC is frequently wrong, and a wrong clock makes certificate
///          validation fail - which is very likely why the predecessor disabled
///          verification outright rather than diagnosing it.
/// @since 0.2.0
[[nodiscard]] std::string_view advise(HttpError error);

/// @brief Whether a URL may be requested at all.
/// @param url The URL to check.
/// @return True only for an `https://` URL.
/// @details Checked before the request is built, so a plaintext URL is refused
///          by this application rather than by the server. `tools/lint/forbid_insecure_curl.sh`
///          enforces the same rule at the source level.
/// @since 0.2.0
[[nodiscard]] bool isHttpsUrl(std::string_view url);

}  // namespace nsx::infra
