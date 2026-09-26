// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/update/manifest_cache.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "nsx/core/hash/sha256.hpp"

namespace nsx::core {

namespace detail {
namespace {

using Json = nlohmann::json;

bool isLowercaseHexDigest(std::string_view text)
{
    if (text.size() != Sha256::kDigestSize * 2) {
        return false;
    }
    return std::ranges::all_of(
        text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

std::string humanDuration(std::int64_t seconds)
{
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }
    if (seconds < 60 * 60) {
        return std::to_string(seconds / 60) + "m";
    }
    return std::to_string(seconds / (60 * 60)) + "h";
}

}  // namespace
}  // namespace detail

Result<CacheMetadata, CacheError> parseCacheMetadata(std::string_view json)
{
    using R = Result<CacheMetadata, CacheError>;

    const detail::Json doc = detail::Json::parse(json, nullptr, false);
    if (doc.is_discarded()) {
        return R::err(CacheError::NotJson);
    }
    if (!doc.is_object()) {
        return R::err(CacheError::NotObject);
    }

    if (!doc.contains("schema_version")) {
        return R::err(CacheError::MissingField);
    }
    if (!doc["schema_version"].is_number_integer()) {
        return R::err(CacheError::WrongType);
    }
    if (doc["schema_version"].get<int>() != kSupportedCacheSchema) {
        return R::err(CacheError::UnsupportedSchemaVersion);
    }

    for (const char* key : {"fetched_at_unix", "document_sha256", "document_size"}) {
        if (!doc.contains(key)) {
            return R::err(CacheError::MissingField);
        }
    }

    CacheMetadata out;
    out.schemaVersion = kSupportedCacheSchema;

    if (!doc["fetched_at_unix"].is_number_integer()) {
        return R::err(CacheError::WrongType);
    }
    out.fetchedAtUnix = doc["fetched_at_unix"].get<std::int64_t>();
    if (out.fetchedAtUnix < 0) {
        return R::err(CacheError::InvalidTimestamp);
    }

    if (!doc["document_sha256"].is_string()) {
        return R::err(CacheError::WrongType);
    }
    out.documentSha256 = doc["document_sha256"].get<std::string>();
    if (!detail::isLowercaseHexDigest(out.documentSha256)) {
        return R::err(CacheError::InvalidDigest);
    }

    if (!doc["document_size"].is_number_unsigned()) {
        return R::err(CacheError::WrongType);
    }
    out.documentSize = doc["document_size"].get<std::uint64_t>();

    // Optional: a cache that has lost its ETag simply fetches unconditionally,
    // and one that has lost its source URL is still perfectly serveable.
    if (doc.contains("etag")) {
        if (!doc["etag"].is_string()) {
            return R::err(CacheError::WrongType);
        }
        out.etag = doc["etag"].get<std::string>();
    }
    if (doc.contains("source_url")) {
        if (!doc["source_url"].is_string()) {
            return R::err(CacheError::WrongType);
        }
        out.sourceUrl = doc["source_url"].get<std::string>();
    }

    return R::ok(std::move(out));
}

std::string serializeCacheMetadata(const CacheMetadata& meta)
{
    detail::Json doc;
    doc["schema_version"] = kSupportedCacheSchema;
    doc["fetched_at_unix"] = meta.fetchedAtUnix;
    doc["etag"] = meta.etag;
    doc["document_sha256"] = meta.documentSha256;
    doc["document_size"] = meta.documentSize;
    doc["source_url"] = meta.sourceUrl;
    return doc.dump(2) + "\n";
}

std::string_view describe(CacheError error)
{
    switch (error) {
        case CacheError::NotJson:
            return "cache metadata is not valid JSON";
        case CacheError::NotObject:
            return "cache metadata top level is not an object";
        case CacheError::UnsupportedSchemaVersion:
            return "cache metadata was written by a newer build";
        case CacheError::MissingField:
            return "cache metadata is missing a required field";
        case CacheError::WrongType:
            return "a cache metadata field has the wrong type";
        case CacheError::InvalidDigest:
            return "the cached document digest is not 64 lowercase hex";
        case CacheError::InvalidTimestamp:
            return "the cache timestamp is negative";
    }
    return "unknown cache error";
}

bool cacheMatchesDocument(const CacheMetadata& meta, std::string_view document)
{
    if (meta.documentSize != document.size()) {
        return false;
    }
    return Sha256::hexOf(document) == meta.documentSha256;
}

std::int64_t cacheAgeSeconds(const CacheMetadata& meta, std::int64_t nowUnix)
{
    if (meta.fetchedAtUnix >= nowUnix) {
        return 0;
    }
    return nowUnix - meta.fetchedAtUnix;
}

bool cacheIsFresh(const CacheMetadata& meta, std::int64_t nowUnix, std::int64_t ttlSeconds)
{
    if (ttlSeconds <= 0) {
        return false;
    }
    // Stamped in the future: not fresh, deliberately. See the header.
    if (meta.fetchedAtUnix > nowUnix) {
        return false;
    }
    return (nowUnix - meta.fetchedAtUnix) < ttlSeconds;
}

FetchDecision planManifestFetch(const std::optional<CacheMetadata>& meta,
                                const BackoffState& backoff, std::int64_t nowUnix,
                                std::int64_t ttlSeconds, const BackoffPolicy& policy)
{
    FetchDecision out;
    out.cacheAgeSeconds = meta.has_value() ? cacheAgeSeconds(*meta, nowUnix) : 0;

    // 1. A fresh cache makes no request, so throttling has nothing to govern.
    if (meta.has_value() && cacheIsFresh(*meta, nowUnix, ttlSeconds)) {
        out.plan = FetchPlan::ServeCache;
        out.reason = "cached manifest is " + detail::humanDuration(out.cacheAgeSeconds) + " old";
        return out;
    }

    // 2. Still backing off from earlier failures.
    if (const std::int64_t wait = secondsUntilNextAttempt(backoff, nowUnix, policy); wait > 0) {
        out.retryAfterSeconds = wait;
        if (meta.has_value()) {
            out.plan = FetchPlan::DeferToCache;
            out.reason = "backing off after " + std::to_string(backoff.consecutiveFailures) +
                         " failure(s); retrying in " + detail::humanDuration(wait) +
                         ", serving a cache " + detail::humanDuration(out.cacheAgeSeconds) + " old";
        }
        else {
            out.plan = FetchPlan::DeferNoCache;
            out.reason = "backing off after " + std::to_string(backoff.consecutiveFailures) +
                         " failure(s); retrying in " + detail::humanDuration(wait);
        }
        return out;
    }

    // 3. Stale with an ETag: ask whether it changed rather than re-downloading.
    if (meta.has_value() && !meta->etag.empty()) {
        out.plan = FetchPlan::Revalidate;
        out.ifNoneMatch = meta->etag;
        out.reason = "cached manifest is " + detail::humanDuration(out.cacheAgeSeconds) +
                     " old; revalidating";
        return out;
    }

    // 4. Nothing usable to build on.
    out.plan = FetchPlan::Fetch;
    out.reason = meta.has_value() ? "cached manifest has no etag; fetching" : "no cached manifest";
    return out;
}

}  // namespace nsx::core
