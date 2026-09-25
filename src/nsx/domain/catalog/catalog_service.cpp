// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/catalog/catalog_service.hpp"

#include <utility>

namespace nsx::domain {

namespace {

using core::BackoffState;
using core::CacheMetadata;
using core::FetchPlan;
using infra::HttpError;

/// A catalogue is a few kilobytes of JSON. Anything claiming to be one and
/// running to a megabyte is not a catalogue, and reading it into the console's
/// heap to find out is not necessary.
constexpr std::uint64_t kMaxDocumentBytes = 1u << 20u;

/// Small JSON side-files: backoff counters, cache metadata.
constexpr std::uint64_t kMaxSidecarBytes = 64u * 1024u;

/// A failure of the host rather than of the response - the case where a mirror
/// is worth trying. A 404 or a rate limit means we reached GitHub and GitHub
/// answered, so the mirror would very likely answer the same way.
bool hostUnreachable(HttpError error)
{
    switch (error) {
        case HttpError::DnsFailure:
        case HttpError::ConnectFailed:
        case HttpError::Timeout:
            return true;
        default:
            return false;
    }
}

std::string describeTransport(HttpError error)
{
    std::string text(infra::describe(error));
    const std::string_view advice = infra::advise(error);
    if (!advice.empty()) {
        text += " - ";
        text += advice;
    }
    return text;
}

}  // namespace

CatalogService::CatalogService(HttpGateway& http, FileStore& files, const Clock& clock,
                               CatalogConfig config)
    : m_http(http), m_files(files), m_clock(clock), m_config(std::move(config))
{
}

// ---------------------------------------------------------------------------
// Cache and backoff persistence
// ---------------------------------------------------------------------------

std::optional<CatalogService::CachedDocument> CatalogService::loadCache() const
{
    const std::optional<std::string> rawMeta =
        m_files.readText(m_config.cacheMetadataPath(), kMaxSidecarBytes);
    if (!rawMeta.has_value()) {
        return std::nullopt;
    }

    const core::Result<CacheMetadata, core::CacheError> parsed = core::parseCacheMetadata(*rawMeta);
    if (!parsed.hasValue()) {
        return std::nullopt;
    }

    const std::optional<std::string> document =
        m_files.readText(m_config.cachedCatalogPath(), kMaxDocumentBytes);
    if (!document.has_value()) {
        return std::nullopt;
    }

    // The two files are written separately, so they can disagree - a power cut
    // between them is exactly the case this catches. A cache that does not
    // describe itself is treated as no cache at all.
    if (!core::cacheMatchesDocument(parsed.value(), *document)) {
        return std::nullopt;
    }

    return CachedDocument{parsed.value(), *document};
}

BackoffState CatalogService::loadBackoff() const
{
    const std::optional<std::string> raw =
        m_files.readText(m_config.backoffPath(), kMaxSidecarBytes);
    if (!raw.has_value()) {
        return BackoffState{};
    }
    return core::parseBackoff(*raw).value_or(BackoffState{});
}

void CatalogService::saveBackoff(const BackoffState& state)
{
    // Best effort. Failing to persist a throttling hint must not fail a catalog
    // fetch that otherwise succeeded.
    (void)m_files.makeDirectories(m_config.cacheDir);
    (void)m_files.writeAtomic(m_config.backoffPath(), core::serializeBackoff(state));
}

bool CatalogService::storeCache(const std::string& document, const std::string& etag,
                                const std::string& sourceUrl, std::int64_t nowUnix)
{
    if (!m_files.makeDirectories(m_config.cacheDir)) {
        return false;
    }

    CacheMetadata meta;
    meta.fetchedAtUnix = nowUnix;
    meta.etag = etag;
    meta.documentSha256 = core::Sha256::hexOf(document);
    meta.documentSize = document.size();
    meta.sourceUrl = sourceUrl;

    // Document first, metadata second. Interrupted between the two, the old
    // metadata no longer describes the file on disk, cacheMatchesDocument says
    // so, and the cache is ignored rather than half-trusted.
    if (!m_files.writeAtomic(m_config.cachedCatalogPath(), document)) {
        return false;
    }
    return m_files.writeAtomic(m_config.cacheMetadataPath(), serializeCacheMetadata(meta));
}

// ---------------------------------------------------------------------------
// fetch()
// ---------------------------------------------------------------------------

CatalogOutcome CatalogService::fromDocument(const std::string& document, bool servedFromCache,
                                            std::int64_t ageSeconds, FetchPlan plan,
                                            std::string detail) const
{
    CatalogOutcome out;
    out.plan = plan;
    out.servedFromCache = servedFromCache;
    out.ageSeconds = ageSeconds;
    out.detail = std::move(detail);

    const core::Result<core::Catalog, core::CatalogError> parsed = core::parseCatalog(document);
    if (!parsed.hasValue()) {
        out.detail = std::string(core::describe(parsed.error()));
        return out;
    }

    out.catalog = parsed.value();
    return out;
}

CatalogOutcome CatalogService::fetch()
{
    const std::int64_t now = m_clock.nowUnix();
    const std::optional<CachedDocument> cached = loadCache();
    const BackoffState backoff = loadBackoff();

    std::optional<CacheMetadata> meta;
    if (cached.has_value()) {
        meta = cached->meta;
    }

    const core::FetchDecision plan =
        core::planManifestFetch(meta, backoff, now, m_config.cacheTtlSeconds, m_config.backoff);

    switch (plan.plan) {
        case FetchPlan::ServeCache:
        case FetchPlan::DeferToCache:
            return fromDocument(cached->document, true, plan.cacheAgeSeconds, plan.plan,
                                plan.reason);

        case FetchPlan::DeferNoCache: {
            CatalogOutcome out;
            out.plan = plan.plan;
            out.detail = plan.reason;
            return out;
        }

        case FetchPlan::Revalidate:
        case FetchPlan::Fetch:
            break;
    }

    // --- network path -----------------------------------------------------
    bool usedMirror = false;
    core::Result<infra::Response, HttpError> response =
        m_http.fetch(m_config.catalogUrl, plan.ifNoneMatch);

    if (!response.hasValue() && hostUnreachable(response.error()) && !m_config.mirrorUrl.empty()) {
        // The mirror is a different document with a different ETag history, so
        // the conditional header is dropped rather than sent somewhere it
        // cannot mean anything.
        usedMirror = true;
        response = m_http.fetch(m_config.mirrorUrl, {});
    }

    const std::string sourceUrl = usedMirror ? m_config.mirrorUrl : m_config.catalogUrl;

    if (!response.hasValue()) {
        const HttpError error = response.error();
        saveBackoff(
            core::afterFailure(backoff, now, m_config.backoff, static_cast<std::uint64_t>(now)));

        if (cached.has_value()) {
            CatalogOutcome out =
                fromDocument(cached->document, true, plan.cacheAgeSeconds, plan.plan,
                             "catalogue fetch failed (" + describeTransport(error) +
                                 "); showing the last known catalogue");
            out.transport = error;
            out.usedMirror = usedMirror;
            return out;
        }

        CatalogOutcome out;
        out.plan = plan.plan;
        out.transport = error;
        out.usedMirror = usedMirror;
        out.detail = describeTransport(error);
        return out;
    }

    // 304: the cache is still current. Re-stamp it so the TTL restarts rather
    // than asking again on the next launch.
    if (response.value().status == 304 && cached.has_value()) {
        (void)storeCache(cached->document,
                         cached->meta.etag.empty() ? plan.ifNoneMatch : cached->meta.etag,
                         cached->meta.sourceUrl.empty() ? sourceUrl : cached->meta.sourceUrl, now);
        saveBackoff(core::afterSuccess(backoff, now));

        CatalogOutcome out =
            fromDocument(cached->document, true, 0, plan.plan, "not modified since last check");
        out.usedMirror = usedMirror;
        return out;
    }

    CatalogOutcome out =
        fromDocument(response.value().body, false, 0, plan.plan, "fetched a fresh catalogue");
    out.usedMirror = usedMirror;

    if (!out.available()) {
        // Never cache a document this build cannot read: the next launch would
        // serve it straight back from disk and fail identically for a whole TTL.
        saveBackoff(
            core::afterFailure(backoff, now, m_config.backoff, static_cast<std::uint64_t>(now)));

        if (cached.has_value()) {
            CatalogOutcome fallback =
                fromDocument(cached->document, true, plan.cacheAgeSeconds, plan.plan,
                             "the published catalogue could not be read (" + out.detail +
                                 "); showing the last known catalogue");
            fallback.usedMirror = usedMirror;
            return fallback;
        }
        return out;
    }

    (void)storeCache(response.value().body, response.value().etag, sourceUrl, now);
    saveBackoff(core::afterSuccess(backoff, now));
    return out;
}

void CatalogService::invalidateCache()
{
    (void)m_files.remove(m_config.cachedCatalogPath());
    (void)m_files.remove(m_config.cacheMetadataPath());
}

}  // namespace nsx::domain
