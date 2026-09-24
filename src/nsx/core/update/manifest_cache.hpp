// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "nsx/core/result/result.hpp"
#include "nsx/core/update/backoff.hpp"

namespace nsx::core {

/// @brief How long a cached manifest is served without asking the network.
/// @details Six hours, per `docs/architecture/update-pipeline.md`. Startup never
///          blocks on the network; within the TTL it never touches it at all.
inline constexpr std::int64_t kManifestCacheTtlSeconds = 6 * 60 * 60;

/// @brief The cache metadata schema version this build writes and understands.
inline constexpr int kSupportedCacheSchema = 1;

/// @brief Why cache metadata was rejected.
/// @since 0.2.0
enum class CacheError
{
    NotJson,                   ///< Not parseable as JSON.
    NotObject,                 ///< Top level was not an object.
    UnsupportedSchemaVersion,  ///< Written by a newer build.
    MissingField,              ///< A required field was absent.
    WrongType,                 ///< A field had the wrong JSON type.
    InvalidDigest,             ///< Not 64 lowercase hex characters.
    InvalidTimestamp           ///< A negative fetch time.
};

/// @brief What is known about the cached manifest document.
///
/// @details Kept in a **separate file** from the document itself, written
///          **after** it, and carrying that document's digest and length. The
///          pair is only trusted when they agree, which is what makes a torn
///          write - power cut between the two - detectable instead of silently
///          serving half a manifest with a plausible-looking ETag.
///
/// @see docs/reference/sd-layout.md
/// @since 0.2.0
struct CacheMetadata
{
    int schemaVersion{kSupportedCacheSchema};  ///< Always the supported version once parsed.
    std::int64_t fetchedAtUnix{};              ///< When the document was last confirmed current.
    std::string etag;                          ///< For `If-None-Match`; empty when unknown.
    std::string documentSha256;                ///< Digest of the cached document, lowercase hex.
    std::uint64_t documentSize{};              ///< Length of the cached document, in bytes.
    std::string sourceUrl;                     ///< Where it came from; primary or mirror.
};

/// @brief Parse cache metadata.
/// @param json The raw document.
/// @return The metadata, or why it was rejected.
/// @note **Never throws.**
/// @since 0.2.0
[[nodiscard]] Result<CacheMetadata, CacheError> parseCacheMetadata(std::string_view json);

/// @brief Render cache metadata as JSON.
/// @param meta The metadata to render.
/// @return A document that @ref parseCacheMetadata accepts.
/// @since 0.2.0
[[nodiscard]] std::string serializeCacheMetadata(const CacheMetadata& meta);

/// @brief A short English description of a cache rejection.
/// @param error The error to describe.
/// @return A description suitable for a log line.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(CacheError error);

/// @brief Whether the metadata actually describes the document beside it.
/// @param meta The metadata read from disk.
/// @param document The document read from disk.
/// @return True when the length and digest both agree.
/// @details Length is checked first because it is free. A disagreement means a
///          torn write or an edited cache, and the only safe response is to
///          treat the cache as absent.
/// @since 0.2.0
[[nodiscard]] bool cacheMatchesDocument(const CacheMetadata& meta, std::string_view document);

/// @brief How old the cached document is.
/// @param meta The cache metadata.
/// @param nowUnix The current time.
/// @return Age in seconds; zero when the recorded time is in the future.
/// @since 0.2.0
[[nodiscard]] std::int64_t cacheAgeSeconds(const CacheMetadata& meta, std::int64_t nowUnix);

/// @brief Whether the cache may be served without contacting the network.
/// @param meta The cache metadata.
/// @param nowUnix The current time.
/// @param ttlSeconds How long a document stays fresh.
/// @return True while the document is within its TTL.
/// @details A document stamped in the **future** is never fresh. Treating it as
///          fresh would let one bad clock reading suppress update checks until
///          that timestamp passed, which on a console whose RTC resets to 2000
///          or jumps forward is a real way to strand a device on an old build.
/// @since 0.2.0
[[nodiscard]] bool cacheIsFresh(const CacheMetadata& meta, std::int64_t nowUnix,
                                std::int64_t ttlSeconds = kManifestCacheTtlSeconds);

/// @brief What the update check should do before making any request.
/// @since 0.2.0
enum class FetchPlan
{
    ServeCache,    ///< Within the TTL. No request at all.
    Revalidate,    ///< Stale, but we have an ETag: conditional request.
    Fetch,         ///< Stale or absent with no ETag: full request.
    DeferToCache,  ///< Backing off; serve the stale cache and say how old it is.
    DeferNoCache   ///< Backing off with nothing cached; report unavailable.
};

/// @brief The plan, with everything the caller needs to act on it.
/// @since 0.2.0
struct FetchDecision
{
    FetchPlan plan{FetchPlan::Fetch};  ///< What to do.
    std::string ifNoneMatch;           ///< ETag to send; empty unless revalidating.
    std::int64_t retryAfterSeconds{};  ///< When deferring, how long is left.
    std::int64_t cacheAgeSeconds{};    ///< Age of the cached document, when there is one.
    std::string reason;                ///< Why, in English.

    /// @brief Whether this plan makes a network request.
    /// @return True for @ref FetchPlan::Revalidate and @ref FetchPlan::Fetch.
    [[nodiscard]] bool requestsNetwork() const
    {
        return plan == FetchPlan::Revalidate || plan == FetchPlan::Fetch;
    }
};

/// @brief Decide whether to use the cache, revalidate, fetch, or stand down.
///
/// @details The ordering is the contract:
///
///          1. a **fresh** cache is served without consulting the backoff at
///             all - throttling governs requests, and this plan makes none;
///          2. a backoff still in force defers, serving whatever cache exists;
///          3. a stale cache with an ETag revalidates;
///          4. anything else fetches.
///
///          Pure, so the whole matrix is testable without a network, a clock or
///          a filesystem.
///
/// @param meta The cache metadata, or `std::nullopt` when there is no usable cache.
/// @param backoff The persisted backoff state.
/// @param nowUnix The current time.
/// @param ttlSeconds How long a cached document stays fresh.
/// @param policy The backoff policy, for the clock-jump bound in @ref mayAttempt.
/// @return The plan and why.
/// @since 0.2.0
[[nodiscard]] FetchDecision planManifestFetch(const std::optional<CacheMetadata>& meta,
                                              const BackoffState& backoff, std::int64_t nowUnix,
                                              std::int64_t ttlSeconds = kManifestCacheTtlSeconds,
                                              const BackoffPolicy& policy = {});

}  // namespace nsx::core
