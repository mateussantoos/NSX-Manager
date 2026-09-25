// SPDX-License-Identifier: GPL-3.0-only
//
// The catalogue service orchestrator.
//
// Same shape as selfupdate_test.cpp: the clock, the SD card and the network are
// all fakes, and the whole fetch/cache/backoff/mirror pipeline is exercised
// end-to-end. Every test below maps to a row in
// docs/architecture/update-pipeline.md, applied to the catalogue endpoint.

#include "nsx/domain/catalog/catalog_service.hpp"

#include <deque>
#include <map>
#include <set>
#include <string>

#include <doctest.h>

using namespace nsx::domain;
namespace core = nsx::core;
namespace infra = nsx::infra;

namespace {

constexpr std::int64_t kNow = 1'760'000'000;
constexpr std::int64_t kHour = 60 * 60;
constexpr std::int64_t kDay = 24 * kHour;

// ---------------------------------------------------------------------------
// Fakes - identical to the ones in selfupdate_test.cpp. Extracting a shared
// fake library is a deliberate non-goal at this scale (ADR-0014): one more
// file in tests/unit/ is cheaper than an abstraction two suites depend on.
// ---------------------------------------------------------------------------

class FakeClock final : public Clock
{
public:
    std::int64_t now{kNow};

    [[nodiscard]] std::int64_t nowUnix() const override { return now; }
};

class FakeFileStore final : public FileStore
{
public:
    std::map<std::string, std::string> files;
    std::set<std::string> dirs{"/", "/config", "/switch"};
    std::set<std::string> writeFailures;
    bool refuseDirectories{};

    [[nodiscard]] bool exists(const std::string& path) const override
    {
        return files.count(path) != 0;
    }

    [[nodiscard]] std::optional<std::string> readText(const std::string& path,
                                                      std::uint64_t maxBytes) const override
    {
        const auto it = files.find(path);
        if (it == files.end() || it->second.size() > maxBytes) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] bool writeAtomic(const std::string& path, std::string_view data) override
    {
        if (writeFailures.count(path) != 0) {
            return false;
        }
        const std::string::size_type slash = path.find_last_of('/');
        if (slash != std::string::npos && dirs.count(path.substr(0, slash)) == 0) {
            return false;
        }
        files[path] = std::string(data);
        return true;
    }

    bool remove(const std::string& path) override
    {
        files.erase(path);
        return true;
    }

    [[nodiscard]] bool makeDirectories(const std::string& path) override
    {
        if (refuseDirectories) {
            return false;
        }
        std::string partial;
        for (const char c : path) {
            if (c == '/' && !partial.empty()) {
                dirs.insert(partial);
            }
            partial.push_back(c);
        }
        dirs.insert(path);
        return true;
    }

    [[nodiscard]] bool copyFile(const std::string& from, const std::string& to) override
    {
        const auto it = files.find(from);
        if (it == files.end()) {
            return false;
        }
        return writeAtomic(to, it->second);
    }

    [[nodiscard]] std::optional<core::Sha256::Digest> digestOf(
        const std::string& path) const override
    {
        const auto it = files.find(path);
        if (it == files.end()) {
            return std::nullopt;
        }
        return core::Sha256::of(it->second);
    }

    [[nodiscard]] std::optional<std::uint64_t> freeSpaceBytes(const std::string&) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] bool rename(const std::string& from, const std::string& to) override
    {
        const auto it = files.find(from);
        if (it == files.end() || writeFailures.count(to) != 0) {
            return false;
        }
        const std::string data = it->second;
        files.erase(it);
        files[to] = data;
        return true;
    }

    [[nodiscard]] std::vector<std::string> listFilesRecursive(const std::string& dir) const override
    {
        const std::string prefix = dir.empty() || dir.back() == '/' ? dir : dir + "/";
        std::vector<std::string> out;
        for (const auto& [path, unused] : files) {
            if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0) {
                out.push_back(path.substr(prefix.size()));
            }
        }
        return out;
    }

    bool removeTree(const std::string& dir) override
    {
        const std::string prefix = dir.empty() || dir.back() == '/' ? dir : dir + "/";
        for (auto it = files.begin(); it != files.end();) {
            const bool inside = it->first.size() > prefix.size() &&
                                it->first.compare(0, prefix.size(), prefix) == 0;
            it = (inside || it->first == dir) ? files.erase(it) : std::next(it);
        }
        return true;
    }
};

struct FetchCall
{
    std::string url;
    std::string ifNoneMatch;
};

class FakeGateway final : public HttpGateway
{
public:
    explicit FakeGateway(FakeFileStore& files) : m_files(files) {}

    std::deque<core::Result<infra::Response, infra::HttpError>> fetchResults;
    std::vector<FetchCall> fetches;
    std::vector<std::string> downloads;

    [[nodiscard]] core::Result<infra::Response, infra::HttpError> fetch(
        const std::string& url, std::string_view ifNoneMatch) override
    {
        fetches.push_back({url, std::string(ifNoneMatch)});
        if (fetchResults.empty()) {
            return core::Result<infra::Response, infra::HttpError>::err(infra::HttpError::Internal);
        }
        auto result = fetchResults.front();
        fetchResults.pop_front();
        return result;
    }

    [[nodiscard]] core::Result<std::monostate, infra::HttpError> download(
        const std::string& url, const std::string& destPath,
        const infra::ExpectedArtifact& expected, const infra::ProgressCallback&) override
    {
        (void)url;
        (void)destPath;
        (void)expected;
        downloads.push_back(url);
        return core::Result<std::monostate, infra::HttpError>::err(infra::HttpError::Internal);
    }

private:
    FakeFileStore& m_files;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// A minimal valid catalogue with one CFW pack and one firmware entry.
const std::string kValidCatalog = R"({
  "schema_version": 1,
  "updated_at": "2026-09-24T12:00:00Z",
  "items": [
    {
      "id": "atmosphere-pack",
      "name": "Atmosphere CFW Pack",
      "summary": "Atmosphere, Hekate and the usual sysmodules",
      "kind": "cfw-pack",
      "target": "sd-root",
      "version": "1.7.1",
      "url": "https://github.com/mateussantoos/nsx-catalog/releases/download/packs/atmosphere-1.7.1.zip",
      "size": 48210944,
      "sha256": "bfd5a8ed7357ec4de9597c2858902db1a0e3050438fcb5f2b37c1de0e4496b49"
    },
    {
      "id": "firmware-19.0.1",
      "name": "Firmware 19.0.1",
      "kind": "firmware",
      "target": "firmware-staging",
      "version": "19.0.1",
      "url": "https://example.invalid/firmware/19.0.1.zip",
      "size": 419430400,
      "sha256": "c3bf47ea1f4a4a605470313cacb3a44f4a461f68c6faeab07e737610cb5ac835"
    }
  ]
})";

infra::Response okResponse(const std::string& body, const std::string& etag = "\"cat1\"")
{
    infra::Response r;
    r.status = 200;
    r.body = body;
    r.etag = etag;
    return r;
}

core::Result<infra::Response, infra::HttpError> ok(const std::string& body,
                                                   const std::string& etag = "\"cat1\"")
{
    return core::Result<infra::Response, infra::HttpError>::ok(okResponse(body, etag));
}

core::Result<infra::Response, infra::HttpError> notModified()
{
    infra::Response r;
    r.status = 304;
    return core::Result<infra::Response, infra::HttpError>::ok(r);
}

core::Result<infra::Response, infra::HttpError> fail(infra::HttpError error)
{
    return core::Result<infra::Response, infra::HttpError>::err(error);
}

CatalogConfig testConfig()
{
    CatalogConfig cfg;
    cfg.cacheTtlSeconds = kDay;
    cfg.backoff.baseSeconds = 60;
    cfg.backoff.capSeconds = kHour;
    return cfg;
}

}  // namespace

// ---------------------------------------------------------------------------
// First fetch from a cold start
// ---------------------------------------------------------------------------

TEST_CASE("first fetch returns the catalogue and caches it")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(ok(kValidCatalog));

    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK(out.catalog->items.size() == 2);
    CHECK_FALSE(out.servedFromCache);
    CHECK(out.plan == core::FetchPlan::Fetch);

    // Cached document was written.
    CHECK(files.exists(cfg.cachedCatalogPath()));
    CHECK(files.exists(cfg.cacheMetadataPath()));
}

TEST_CASE("CFW and firmware items are reachable by kind")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogService svc(http, files, clock, testConfig());

    http.fetchResults.push_back(ok(kValidCatalog));

    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK(out.catalog->ofKind(core::ContentKind::CfwPack).size() == 1);
    CHECK(out.catalog->ofKind(core::ContentKind::Firmware).size() == 1);
    CHECK(out.catalog->ofKind(core::ContentKind::Tool).empty());
}

// ---------------------------------------------------------------------------
// Cache TTL
// ---------------------------------------------------------------------------

TEST_CASE("a fresh cache is served without a request")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    // Prime the cache.
    http.fetchResults.push_back(ok(kValidCatalog));
    const CatalogOutcome first = svc.fetch();
    REQUIRE(first.available());
    CHECK(http.fetches.size() == 1);

    // Second call within the TTL - no network.
    clock.now += kHour;
    const CatalogOutcome second = svc.fetch();
    REQUIRE(second.available());
    CHECK(second.servedFromCache);
    CHECK(second.plan == core::FetchPlan::ServeCache);
    CHECK(http.fetches.size() == 1);  // no new request
}

TEST_CASE("a stale cache with an etag sends a conditional request")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(ok(kValidCatalog, "\"etag1\""));
    (void)svc.fetch();

    // Advance past TTL.
    clock.now += cfg.cacheTtlSeconds + 1;
    http.fetchResults.push_back(notModified());

    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK(out.servedFromCache);
    CHECK(http.fetches.back().ifNoneMatch == "\"etag1\"");
}

// ---------------------------------------------------------------------------
// Network failure with a stale cache
// ---------------------------------------------------------------------------

TEST_CASE("a stale cache is served when the network fails")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(ok(kValidCatalog));
    (void)svc.fetch();

    clock.now += cfg.cacheTtlSeconds + 1;
    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));
    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));  // mirror too

    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK(out.servedFromCache);
    CHECK(out.transport == infra::HttpError::ConnectFailed);
}

TEST_CASE("no cache and no network reports unavailable")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogService svc(http, files, clock, testConfig());

    http.fetchResults.push_back(fail(infra::HttpError::DnsFailure));
    http.fetchResults.push_back(fail(infra::HttpError::DnsFailure));  // mirror too

    const CatalogOutcome out = svc.fetch();
    CHECK_FALSE(out.available());
    CHECK(out.transport == infra::HttpError::DnsFailure);
}

// ---------------------------------------------------------------------------
// Mirror fallback
// ---------------------------------------------------------------------------

TEST_CASE("the mirror is tried when the primary host is unreachable")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(fail(infra::HttpError::DnsFailure));
    http.fetchResults.push_back(ok(kValidCatalog));

    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK(out.usedMirror);
    CHECK(http.fetches.size() == 2);
    CHECK(http.fetches[0].url == cfg.catalogUrl);
    CHECK(http.fetches[1].url == cfg.mirrorUrl);
}

TEST_CASE("a reachable primary that returns a bad status does not fall through to the mirror")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogService svc(http, files, clock, testConfig());

    // 403 is "we reached it and it answered"; the mirror would answer the same.
    http.fetchResults.push_back(fail(infra::HttpError::RateLimited));

    const CatalogOutcome out = svc.fetch();
    CHECK_FALSE(out.available());
    CHECK_FALSE(out.usedMirror);
    CHECK(http.fetches.size() == 1);
}

// ---------------------------------------------------------------------------
// Backoff
// ---------------------------------------------------------------------------

TEST_CASE("a failed fetch arms the backoff")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));
    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));  // mirror too
    (void)svc.fetch();

    // A second call within the base backoff should not make a request.
    clock.now += cfg.backoff.baseSeconds / 2;
    const CatalogOutcome out = svc.fetch();
    CHECK_FALSE(out.available());
    CHECK(out.plan == core::FetchPlan::DeferNoCache);
    CHECK(http.fetches.size() == 2);  // the initial primary + mirror, no new request
}

TEST_CASE("a successful fetch clears the backoff")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    // Fail once to arm.
    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));
    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));
    (void)svc.fetch();

    // Advance past the backoff and succeed.
    clock.now += cfg.backoff.capSeconds + 1;
    http.fetchResults.push_back(ok(kValidCatalog));
    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());

    // Advance past the cache TTL - should revalidate, not defer.
    clock.now += cfg.cacheTtlSeconds + 1;
    http.fetchResults.push_back(ok(kValidCatalog));
    const CatalogOutcome fresh = svc.fetch();
    REQUIRE(fresh.available());
    CHECK(fresh.plan == core::FetchPlan::Revalidate);
    CHECK(core::parseBackoff(*files.readText(cfg.backoffPath(), 1024))
              .value_or(core::BackoffState{})
              .consecutiveFailures == 0);
}

// ---------------------------------------------------------------------------
// Invalid documents
// ---------------------------------------------------------------------------

TEST_CASE("an unparseable document is not cached")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(ok("not json"));

    const CatalogOutcome out = svc.fetch();
    CHECK_FALSE(out.available());
    CHECK_FALSE(files.exists(cfg.cachedCatalogPath()));
}

TEST_CASE("an unparseable document falls back to the stale cache")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    // Prime the cache.
    http.fetchResults.push_back(ok(kValidCatalog));
    (void)svc.fetch();

    // Advance past the TTL and serve garbage.
    clock.now += cfg.cacheTtlSeconds + 1;
    http.fetchResults.push_back(ok("not json"));

    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK(out.servedFromCache);
}

// ---------------------------------------------------------------------------
// Invalidation
// ---------------------------------------------------------------------------

TEST_CASE("invalidateCache forces a fresh fetch")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    // Prime.
    http.fetchResults.push_back(ok(kValidCatalog));
    (void)svc.fetch();
    CHECK(http.fetches.size() == 1);

    // Invalidate, then fetch again within the original TTL.
    svc.invalidateCache();
    CHECK_FALSE(files.exists(cfg.cachedCatalogPath()));

    http.fetchResults.push_back(ok(kValidCatalog));
    const CatalogOutcome out = svc.fetch();
    REQUIRE(out.available());
    CHECK_FALSE(out.servedFromCache);
    CHECK(http.fetches.size() == 2);  // second network request
}

// ---------------------------------------------------------------------------
// The backoff is independent of the update check
// ---------------------------------------------------------------------------

TEST_CASE("the backoff state is stored at catalog.backoff.json")
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http(files);
    CatalogConfig cfg = testConfig();
    CatalogService svc(http, files, clock, cfg);

    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));
    http.fetchResults.push_back(fail(infra::HttpError::ConnectFailed));
    (void)svc.fetch();

    CHECK(files.exists(cfg.backoffPath()));
    // The update check's backoff is at /config/nsx-manager/cache/backoff.json.
    // The catalogue's is at /config/nsx-manager/cache/catalog.backoff.json.
    // They must not be the same path.
    CHECK(cfg.backoffPath() != "/config/nsx-manager/cache/backoff.json");
}
