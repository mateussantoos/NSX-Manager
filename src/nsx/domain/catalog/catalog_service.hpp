// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "nsx/core/catalog/catalog.hpp"
#include "nsx/core/update/backoff.hpp"
#include "nsx/core/update/manifest_cache.hpp"
#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief Where the content catalogue comes from and how it is cached.
/// @since 0.3.0
struct CatalogConfig
{
    /// @brief The catalogue this application owns.
    /// @details A release asset with a stable name, on the same web-redirect
    ///          path as `update.json` and for the same reason: it is not
    ///          `api.github.com` and so is not subject to the 60-per-hour
    ///          unauthenticated REST budget (ADR-0005).
    std::string catalogUrl{
        "https://github.com/mateussantoos/nsx-catalog/releases/latest/download/catalog.json"};

    /// @brief Orphan-branch mirror, tried only when the primary is unreachable.
    std::string mirrorUrl{
        "https://raw.githubusercontent.com/mateussantoos/nsx-catalog/"
        "catalog-metadata/catalog.json"};

    std::string cacheDir{"/config/nsx-manager/cache"};  ///< Shared with the update check.

    /// @brief How long a cached catalogue is served without asking the network.
    /// @details Longer than the update manifest's six hours. A release is a
    ///          thing the user may be waiting for; the catalogue is a list that
    ///          changes when an upstream project publishes, which is rarer, and
    ///          a stale entry costs nothing because every download is verified
    ///          against the digest the catalogue carries anyway.
    std::int64_t cacheTtlSeconds{24 * 60 * 60};

    core::BackoffPolicy backoff;  ///< Failure throttling, independent of updates.

    /// @brief The cached catalogue document.
    /// @return `<cacheDir>/catalog.json`.
    [[nodiscard]] std::string cachedCatalogPath() const { return cacheDir + "/catalog.json"; }

    /// @brief Metadata describing the cached document.
    /// @return `<cacheDir>/catalog.meta.json`.
    [[nodiscard]] std::string cacheMetadataPath() const { return cacheDir + "/catalog.meta.json"; }

    /// @brief Persisted backoff state for the catalogue endpoint.
    /// @return `<cacheDir>/catalog.backoff.json`.
    /// @details **Separate from the update check's.** A catalogue host that is
    ///          down must not throttle the application's own update checks, and
    ///          a rate-limited update endpoint must not stop the user browsing
    ///          content. One shared counter would couple two unrelated
    ///          failures.
    [[nodiscard]] std::string backoffPath() const { return cacheDir + "/catalog.backoff.json"; }
};

/// @brief The result of fetching the catalogue.
/// @since 0.3.0
struct CatalogOutcome
{
    std::optional<core::Catalog> catalog;  ///< Present whenever one was readable.

    core::FetchPlan plan{core::FetchPlan::Fetch};  ///< What the fetch actually did.
    bool servedFromCache{};                        ///< No fresh document was obtained.
    bool usedMirror{};                             ///< The primary host was unreachable.
    std::int64_t ageSeconds{};                     ///< Age of the catalogue being reported on.
    infra::HttpError transport{infra::HttpError::None};  ///< Set only on a network failure.
    std::string detail;                                  ///< One line for the log and the UI.

    /// @brief Whether there is a catalogue to show.
    /// @return True when one was parsed, from the network or the cache.
    [[nodiscard]] bool available() const { return catalog.has_value(); }
};

/// @brief Fetch, cache and serve the content catalogue.
///
/// @details The same shape as @ref UpdateService::check, deliberately: a fresh
///          cache makes no request, a stale one revalidates with its ETag, a
///          failure falls back to whatever is cached rather than to nothing,
///          and a document this build cannot parse is never written to the
///          cache - the next launch would serve the same failure back from disk
///          for a whole TTL.
///
///          What it does NOT share is the backoff counter. See
///          @ref CatalogConfig::backoffPath.
///
///          The rules all live in `core` and are host-tested in isolation;
///          this sequences them and owns no policy of its own.
///
/// @see docs/architecture/update-pipeline.md
/// @since 0.3.0
class CatalogService
{
public:
    /// @brief Construct the service over its ports.
    /// @param http Network access.
    /// @param files Filesystem access.
    /// @param clock Time source.
    /// @param config Paths, URLs and policy.
    CatalogService(HttpGateway& http, FileStore& files, const Clock& clock, CatalogConfig config);

    /// @brief Get the catalogue, from the cache or the network.
    /// @return The catalogue and how it was obtained.
    [[nodiscard]] CatalogOutcome fetch();

    /// @brief Delete the cached catalogue, forcing the next fetch to the network.
    /// @details For a "refresh" action. Leaves the backoff alone: a user asking
    ///          to refresh is not evidence that a failing endpoint has
    ///          recovered.
    void invalidateCache();

    /// @brief The configuration this service was built with.
    /// @return A reference to the configuration.
    [[nodiscard]] const CatalogConfig& config() const { return m_config; }

    /// @brief Whether to swap primary and fallback mirror URLs.
    void setPreferMirror(bool prefer)
    {
        if (prefer && !m_config.mirrorUrl.empty() && m_config.catalogUrl != m_config.mirrorUrl) {
            std::swap(m_config.catalogUrl, m_config.mirrorUrl);
        }
    }

private:
    struct CachedDocument
    {
        core::CacheMetadata meta;
        std::string document;
    };

    [[nodiscard]] std::optional<CachedDocument> loadCache() const;
    [[nodiscard]] core::BackoffState loadBackoff() const;
    void saveBackoff(const core::BackoffState& state);
    [[nodiscard]] bool storeCache(const std::string& document, const std::string& etag,
                                  const std::string& sourceUrl, std::int64_t nowUnix);
    [[nodiscard]] CatalogOutcome fromDocument(const std::string& document, bool servedFromCache,
                                              std::int64_t ageSeconds, core::FetchPlan plan,
                                              std::string detail) const;

    HttpGateway& m_http;
    FileStore& m_files;
    const Clock& m_clock;
    CatalogConfig m_config;
};

}  // namespace nsx::domain
