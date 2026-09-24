// SPDX-License-Identifier: GPL-3.0-only
//
// The manifest cache and the decision of whether to touch the network at all.
//
// This is the module that keeps startup off the network, so the matrix it
// implements - fresh, stale, backed off, with and without an ETag - is the
// thing to pin. It is pure, so the whole matrix is reachable here without a
// clock, a socket or a filesystem.

#include "nsx/core/update/manifest_cache.hpp"

#include "nsx/core/hash/sha256.hpp"

#include <doctest.h>

using namespace nsx::core;

namespace {

constexpr std::int64_t kNow = 1'760'000'000;
constexpr std::int64_t kHour = 60 * 60;

CacheMetadata metaFor(const std::string& document, std::int64_t fetchedAt,
                      const std::string& etag = "\"abc123\"")
{
    CacheMetadata m;
    m.fetchedAtUnix = fetchedAt;
    m.etag = etag;
    m.documentSha256 = Sha256::hexOf(document);
    m.documentSize = document.size();
    m.sourceUrl = "https://github.com/mateussantoos/nsx-manager/releases/latest/download/"
                  "update.json";
    return m;
}

}  // namespace

// ---------------------------------------------------------------------------
// Metadata parsing
// ---------------------------------------------------------------------------

TEST_CASE("metadata round-trips")
{
    const std::string document = R"({"schema_version":1})";
    const CacheMetadata original = metaFor(document, kNow);

    const Result<CacheMetadata, CacheError> back =
        parseCacheMetadata(serializeCacheMetadata(original));
    REQUIRE(back.hasValue());

    CHECK(back.value().fetchedAtUnix == original.fetchedAtUnix);
    CHECK(back.value().etag == original.etag);
    CHECK(back.value().documentSha256 == original.documentSha256);
    CHECK(back.value().documentSize == original.documentSize);
    CHECK(back.value().sourceUrl == original.sourceUrl);
}

TEST_CASE("malformed metadata is rejected, never thrown from")
{
    CHECK_NOTHROW((void)parseCacheMetadata("{ truncated"));
    CHECK(parseCacheMetadata("{ truncated").error() == CacheError::NotJson);
    CHECK(parseCacheMetadata("[]").error() == CacheError::NotObject);
    CHECK(parseCacheMetadata("{}").error() == CacheError::MissingField);
    CHECK(parseCacheMetadata(R"({"schema_version": 2})").error() ==
          CacheError::UnsupportedSchemaVersion);
}

TEST_CASE("the recorded digest must be 64 lowercase hex characters")
{
    const std::string good = Sha256::hexOf("x");
    for (const std::string bad : {std::string(63, 'a'), std::string(64, 'A'), std::string("")}) {
        CAPTURE(bad);
        const std::string doc = R"({"schema_version":1,"fetched_at_unix":1,"document_sha256":")" +
                                bad + R"(","document_size":1})";
        CHECK(parseCacheMetadata(doc).error() == CacheError::InvalidDigest);
    }
    const std::string ok = R"({"schema_version":1,"fetched_at_unix":1,"document_sha256":")" + good +
                           R"(","document_size":1})";
    CHECK(parseCacheMetadata(ok).hasValue());
}

TEST_CASE("a negative fetch time is rejected")
{
    const std::string doc = R"({"schema_version":1,"fetched_at_unix":-1,"document_sha256":")" +
                            Sha256::hexOf("x") + R"(","document_size":1})";
    CHECK(parseCacheMetadata(doc).error() == CacheError::InvalidTimestamp);
}

TEST_CASE("every cache error has a description")
{
    for (const CacheError e :
         {CacheError::NotJson, CacheError::NotObject, CacheError::UnsupportedSchemaVersion,
          CacheError::MissingField, CacheError::WrongType, CacheError::InvalidDigest,
          CacheError::InvalidTimestamp}) {
        CHECK_FALSE(describe(e).empty());
        CHECK(describe(e) != "unknown cache error");
    }
}

// ---------------------------------------------------------------------------
// The document and its metadata must agree
// ---------------------------------------------------------------------------

TEST_CASE("metadata matches the document it describes")
{
    const std::string document = R"({"schema_version":1,"version":"0.2.0"})";
    CHECK(cacheMatchesDocument(metaFor(document, kNow), document));
}

TEST_CASE("a torn write is detected")
{
    // The document and the metadata are two files. A power cut between them
    // leaves metadata describing the previous document - which must read as
    // "no cache", never as a manifest to act on.
    const std::string document = R"({"schema_version":1,"version":"0.2.0"})";
    const CacheMetadata meta = metaFor(document, kNow);

    CHECK_FALSE(cacheMatchesDocument(meta, R"({"schema_version":1,"version":"0.3.0"})"));
    CHECK_FALSE(cacheMatchesDocument(meta, document.substr(0, document.size() - 3)));
    CHECK_FALSE(cacheMatchesDocument(meta, ""));
}

TEST_CASE("an edited cache is detected even at the same length")
{
    const std::string document = R"({"schema_version":1,"version":"0.2.0"})";
    std::string tampered = document;
    tampered[tampered.size() - 3] = '9';

    REQUIRE(tampered.size() == document.size());
    CHECK_FALSE(cacheMatchesDocument(metaFor(document, kNow), tampered));
}

// ---------------------------------------------------------------------------
// Freshness
// ---------------------------------------------------------------------------

TEST_CASE("freshness follows the six-hour TTL")
{
    const CacheMetadata m = metaFor("x", kNow - kHour);
    CHECK(cacheIsFresh(m, kNow));
    CHECK(cacheAgeSeconds(m, kNow) == kHour);

    CHECK(cacheIsFresh(metaFor("x", kNow - (6 * kHour) + 1), kNow));
    CHECK_FALSE(cacheIsFresh(metaFor("x", kNow - (6 * kHour)), kNow));
    CHECK_FALSE(cacheIsFresh(metaFor("x", kNow - (24 * kHour)), kNow));
}

TEST_CASE("a cache stamped in the future is never fresh")
{
    // One bad RTC reading must not suppress update checks until that timestamp
    // arrives. Treating it as stale costs a request; treating it as fresh could
    // strand the device.
    const CacheMetadata m = metaFor("x", kNow + (10 * 365 * 24 * kHour));
    CHECK_FALSE(cacheIsFresh(m, kNow));
    CHECK(cacheAgeSeconds(m, kNow) == 0);
}

// ---------------------------------------------------------------------------
// The plan
// ---------------------------------------------------------------------------

TEST_CASE("a fresh cache makes no request at all")
{
    const FetchDecision d = planManifestFetch(metaFor("x", kNow - kHour), BackoffState{}, kNow);
    CHECK(d.plan == FetchPlan::ServeCache);
    CHECK_FALSE(d.requestsNetwork());
    CHECK(d.cacheAgeSeconds == kHour);
}

TEST_CASE("a fresh cache is served even while backing off")
{
    // Ordering is the contract: throttling governs requests, and this plan
    // makes none. A backoff must not stop us reading a file we already have.
    BackoffState backoff;
    backoff.consecutiveFailures = 5;
    backoff.nextAttemptUnix = kNow + kHour;

    const FetchDecision d = planManifestFetch(metaFor("x", kNow - 60), backoff, kNow);
    CHECK(d.plan == FetchPlan::ServeCache);
}

TEST_CASE("a stale cache with an etag revalidates")
{
    const FetchDecision d =
        planManifestFetch(metaFor("x", kNow - (7 * kHour), "\"etag-1\""), BackoffState{}, kNow);
    CHECK(d.plan == FetchPlan::Revalidate);
    CHECK(d.ifNoneMatch == "\"etag-1\"");
    CHECK(d.requestsNetwork());
}

TEST_CASE("a stale cache with no etag fetches unconditionally")
{
    const FetchDecision d =
        planManifestFetch(metaFor("x", kNow - (7 * kHour), ""), BackoffState{}, kNow);
    CHECK(d.plan == FetchPlan::Fetch);
    CHECK(d.ifNoneMatch.empty());
}

TEST_CASE("no cache at all fetches")
{
    const FetchDecision d = planManifestFetch(std::nullopt, BackoffState{}, kNow);
    CHECK(d.plan == FetchPlan::Fetch);
    CHECK(d.cacheAgeSeconds == 0);
}

TEST_CASE("a stale cache under backoff is served, with the wait reported")
{
    BackoffState backoff;
    backoff.consecutiveFailures = 3;
    backoff.nextAttemptUnix = kNow + 240;

    const FetchDecision d = planManifestFetch(metaFor("x", kNow - (7 * kHour)), backoff, kNow);
    CHECK(d.plan == FetchPlan::DeferToCache);
    CHECK_FALSE(d.requestsNetwork());
    CHECK(d.retryAfterSeconds == 240);
    CHECK(d.cacheAgeSeconds == 7 * kHour);
}

TEST_CASE("backing off with nothing cached reports unavailable, not up to date")
{
    BackoffState backoff;
    backoff.consecutiveFailures = 1;
    backoff.nextAttemptUnix = kNow + 60;

    const FetchDecision d = planManifestFetch(std::nullopt, backoff, kNow);
    CHECK(d.plan == FetchPlan::DeferNoCache);
    CHECK_FALSE(d.requestsNetwork());
    CHECK(d.retryAfterSeconds == 60);
}

TEST_CASE("an expired backoff no longer defers")
{
    BackoffState backoff;
    backoff.consecutiveFailures = 3;
    backoff.nextAttemptUnix = kNow - 1;

    CHECK(planManifestFetch(std::nullopt, backoff, kNow).plan == FetchPlan::Fetch);
}

TEST_CASE("every plan explains itself")
{
    BackoffState deferring;
    deferring.nextAttemptUnix = kNow + 60;

    CHECK_FALSE(planManifestFetch(metaFor("x", kNow), BackoffState{}, kNow).reason.empty());
    CHECK_FALSE(
        planManifestFetch(metaFor("x", kNow - (7 * kHour)), BackoffState{}, kNow).reason.empty());
    CHECK_FALSE(planManifestFetch(std::nullopt, BackoffState{}, kNow).reason.empty());
    CHECK_FALSE(planManifestFetch(std::nullopt, deferring, kNow).reason.empty());
    CHECK_FALSE(
        planManifestFetch(metaFor("x", kNow - (7 * kHour)), deferring, kNow).reason.empty());
}
