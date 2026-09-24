// SPDX-License-Identifier: GPL-3.0-only
//
// The self-update orchestrator, end to end.
//
// Everything the update flow touches - the clock, the SD card, the network -
// arrives through a port, so the whole sequence runs here: cache decision,
// conditional request, manifest parse, policy, pre-flight, verified download,
// handoff. The last test closes the loop by feeding the handoff this service
// produces into the state machine the forwarder actually runs.
//
// See docs/architecture/update-pipeline.md.

#include "nsx/domain/selfupdate/update_service.hpp"

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

// ---------------------------------------------------------------------------
// Fakes
// ---------------------------------------------------------------------------

class FakeClock final : public Clock
{
public:
    std::int64_t now{kNow};
    [[nodiscard]] std::int64_t nowUnix() const override { return now; }
};

/// An in-memory SD card. Directories are tracked separately because the real
/// store fails a write into a directory that was never created, and a fake that
/// quietly accepted it would hide that.
class FakeFileStore final : public FileStore
{
public:
    std::map<std::string, std::string> files;
    std::set<std::string> dirs{"/", "/config", "/switch"};
    std::optional<std::uint64_t> freeSpace;  // nullopt = "could not measure"
    std::set<std::string> writeFailures;     // paths that refuse to be written
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
        return freeSpace;
    }
};

struct FetchCall
{
    std::string url;
    std::string ifNoneMatch;
};

/// Serves canned responses in order. The download path reproduces the
/// CurlClient contract deliberately - size first, then digest, and nothing
/// written unless both match - because the service's retry logic is built on
/// exactly those distinctions.
class FakeGateway final : public HttpGateway
{
public:
    explicit FakeGateway(FakeFileStore& files) : m_files(files) {}

    std::deque<core::Result<infra::Response, infra::HttpError>> fetchResults;
    std::deque<std::string> downloadBodies;  // what the server will send, in order
    std::optional<infra::HttpError> downloadError;  // when set, every download fails with it
    std::vector<FetchCall> fetches;
    std::vector<std::string> downloads;

    [[nodiscard]] core::Result<infra::Response, infra::HttpError> fetch(
        const std::string& url, std::string_view ifNoneMatch) override
    {
        fetches.push_back({url, std::string(ifNoneMatch)});
        if (fetchResults.empty()) {
            return core::Result<infra::Response, infra::HttpError>::err(
                infra::HttpError::Internal);
        }
        auto result = fetchResults.front();
        fetchResults.pop_front();
        return result;
    }

    [[nodiscard]] core::Result<std::monostate, infra::HttpError> download(
        const std::string& url, const std::string& destPath,
        const infra::ExpectedArtifact& expected, const infra::ProgressCallback&) override
    {
        using R = core::Result<std::monostate, infra::HttpError>;
        downloads.push_back(url);

        if (downloadError.has_value()) {
            return R::err(*downloadError);
        }
        if (downloadBodies.empty()) {
            return R::err(infra::HttpError::ConnectFailed);
        }
        const std::string body = downloadBodies.front();
        downloadBodies.pop_front();

        if (body.size() != expected.size) {
            return R::err(infra::HttpError::SizeMismatch);
        }
        if (!core::digestsEqual(core::Sha256::of(body), expected.sha256)) {
            return R::err(infra::HttpError::DigestMismatch);
        }
        if (!m_files.writeAtomic(destPath, body)) {
            return R::err(infra::HttpError::WriteFailed);
        }
        return R::ok(std::monostate{});
    }

private:
    FakeFileStore& m_files;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const std::string kAppBody = "a plausible nro payload";

/// Build a manifest whose asset genuinely describes `body`, so the fixture can
/// never drift from the bytes the fake serves.
std::string manifestFor(const std::string& version, const std::string& body,
                        const std::string& channel = "stable", bool mandatory = false,
                        const std::string& minSupported = "0.1.0")
{
    return std::string(R"({
  "schema_version": 1,
  "product": "nsx-manager",
  "version": ")") + version + R"(",
  "tag": "v)" + version + R"(",
  "published_at": "2026-10-01T12:00:00Z",
  "channel": ")" + channel + R"(",
  "mandatory": )" + (mandatory ? "true" : "false") + R"(,
  "min_supported": ")" + minSupported + R"(",
  "changelog_url": "https://github.com/mateussantoos/nsx-manager/releases/tag/v)" + version +
           R"(",
  "assets": [
    {
      "name": "nsx-manager-)" + version + R"(.nro",
      "kind": "app-nro",
      "url": "https://github.com/mateussantoos/nsx-manager/releases/download/v)" + version +
           R"(/nsx-manager-)" + version + R"(.nro",
      "size": )" + std::to_string(body.size()) + R"(,
      "sha256": ")" + core::Sha256::hexOf(body) + R"("
    }
  ]
})";
}

infra::Response okResponse(const std::string& body, const std::string& etag = "\"v2\"")
{
    infra::Response r;
    r.status = 200;
    r.body = body;
    r.etag = etag;
    return r;
}

core::Result<infra::Response, infra::HttpError> ok(const std::string& body,
                                                   const std::string& etag = "\"v2\"")
{
    return core::Result<infra::Response, infra::HttpError>::ok(okResponse(body, etag));
}

core::Result<infra::Response, infra::HttpError> failure(infra::HttpError e)
{
    return core::Result<infra::Response, infra::HttpError>::err(e);
}

SelfUpdateConfig configFor(const std::string& installed = "0.1.0")
{
    SelfUpdateConfig c;
    c.installed = core::parseSemVer(installed).value();
    return c;
}

/// The whole rig, so each test reads as a scenario rather than as wiring.
struct Rig
{
    FakeClock clock;
    FakeFileStore files;
    FakeGateway http{files};
    SelfUpdateConfig config{configFor()};

    /// A plausible SD card. Tests that care about space say so explicitly, and
    /// "could not measure" is its own case rather than the default.
    Rig() { files.freeSpace = 64ULL * 1024 * 1024 * 1024; }

    UpdateService service() { return UpdateService(http, files, clock, config); }

    /// Less free space than the payload needs.
    void freeSpaceShort() { files.freeSpace = 1; }

    /// Make the asset download fail the way a rate-limited CDN would.
    void rateLimitDownloads() { http.downloadError = infra::HttpError::RateLimited; }

    /// Put a forwarder in romfs, which is where a real first run finds one.
    void withForwarderInRomfs()
    {
        files.files[config.forwarderSource] = "forwarder binary";
    }

    /// Seed a cache the service will accept: document and metadata agreeing.
    void withCache(const std::string& document, std::int64_t fetchedAt,
                   const std::string& etag = "\"v1\"")
    {
        core::CacheMetadata meta;
        meta.fetchedAtUnix = fetchedAt;
        meta.etag = etag;
        meta.documentSha256 = core::Sha256::hexOf(document);
        meta.documentSize = document.size();
        meta.sourceUrl = config.manifestUrl;

        (void)files.makeDirectories(config.cacheDir);
        files.files[config.cachedManifestPath()] = document;
        files.files[config.cacheMetadataPath()] = core::serializeCacheMetadata(meta);
    }

    void withBackoff(const core::BackoffState& state)
    {
        (void)files.makeDirectories(config.cacheDir);
        files.files[config.backoffPath()] = core::serializeBackoff(state);
    }

    [[nodiscard]] core::BackoffState storedBackoff() const
    {
        const auto it = files.files.find(config.backoffPath());
        if (it == files.files.end()) {
            return core::BackoffState{};
        }
        return core::parseBackoff(it->second).value_or(core::BackoffState{});
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// check() - the cache
// ---------------------------------------------------------------------------

TEST_CASE("a fresh cache is served without touching the network")
{
    Rig rig;
    rig.withCache(manifestFor("0.2.0", kAppBody), kNow - kHour);

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(rig.http.fetches.empty());
    CHECK(outcome.servedFromCache);
    CHECK(outcome.plan == core::FetchPlan::ServeCache);
    CHECK(outcome.decision.action == core::UpdateAction::Optional);
    REQUIRE(outcome.manifest.has_value());
    CHECK(outcome.manifest->version.toString() == "0.2.0");
}

TEST_CASE("a stale cache revalidates with its etag")
{
    Rig rig;
    rig.withCache(manifestFor("0.2.0", kAppBody), kNow - (7 * kHour), "\"etag-1\"");
    rig.http.fetchResults.push_back(
        core::Result<infra::Response, infra::HttpError>::ok([] {
            infra::Response r;
            r.status = 304;
            return r;
        }()));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    REQUIRE(rig.http.fetches.size() == 1);
    CHECK(rig.http.fetches[0].ifNoneMatch == "\"etag-1\"");
    CHECK(outcome.decision.action == core::UpdateAction::Optional);

    // A 304 re-stamps the cache, so the next launch stays off the network.
    UpdateService again = rig.service();
    const CheckOutcome second = again.check();
    CHECK(rig.http.fetches.size() == 1);
    CHECK(second.plan == core::FetchPlan::ServeCache);
}

TEST_CASE("a torn cache is ignored rather than half-trusted")
{
    Rig rig;
    rig.withCache(manifestFor("0.2.0", kAppBody), kNow - kHour);
    // Metadata now describes a document that is no longer there - the state a
    // power cut between the two writes leaves behind.
    rig.files.files[rig.config.cachedManifestPath()] = manifestFor("0.9.9", kAppBody);
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    REQUIRE(rig.http.fetches.size() == 1);
    CHECK_FALSE(outcome.servedFromCache);
    CHECK(outcome.manifest->version.toString() == "0.2.0");
}

TEST_CASE("a fetched manifest is cached, and the next launch reads it")
{
    Rig rig;
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody), "\"fresh\""));

    UpdateService first = rig.service();
    CHECK(first.check().decision.action == core::UpdateAction::Optional);

    CHECK(rig.files.exists(rig.config.cachedManifestPath()));
    CHECK(rig.files.exists(rig.config.cacheMetadataPath()));

    UpdateService second = rig.service();
    const CheckOutcome outcome = second.check();
    CHECK(rig.http.fetches.size() == 1);  // no second request
    CHECK(outcome.plan == core::FetchPlan::ServeCache);
}

// ---------------------------------------------------------------------------
// check() - failure and backoff
// ---------------------------------------------------------------------------

TEST_CASE("a failed check is never reported as up to date")
{
    // The predecessor swallowed a 404 into an empty string and told every user
    // they were current, for its entire life.
    Rig rig;
    rig.http.fetchResults.push_back(failure(infra::HttpError::HttpStatus));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(outcome.decision.action == core::UpdateAction::ManifestUnreadable);
    CHECK(outcome.transport == infra::HttpError::HttpStatus);
    CHECK_FALSE(outcome.detail.empty());
}

TEST_CASE("a failure is persisted so the next launch does not retry immediately")
{
    Rig rig;
    rig.http.fetchResults.push_back(failure(infra::HttpError::HttpStatus));

    UpdateService service = rig.service();
    (void)service.check();

    const core::BackoffState stored = rig.storedBackoff();
    CHECK(stored.consecutiveFailures == 1);
    CHECK(stored.nextAttemptUnix > kNow);
    CHECK_FALSE(core::mayAttempt(stored, kNow));
}

TEST_CASE("a check inside the backoff window makes no request")
{
    Rig rig;
    core::BackoffState backoff;
    backoff.consecutiveFailures = 2;
    backoff.nextAttemptUnix = kNow + 120;
    rig.withBackoff(backoff);

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(rig.http.fetches.empty());
    CHECK(outcome.plan == core::FetchPlan::DeferNoCache);
    CHECK(outcome.decision.action == core::UpdateAction::ManifestUnreadable);
}

TEST_CASE("backing off still serves the last known release")
{
    Rig rig;
    rig.withCache(manifestFor("0.2.0", kAppBody), kNow - (12 * kHour));
    core::BackoffState backoff;
    backoff.consecutiveFailures = 3;
    backoff.nextAttemptUnix = kNow + 600;
    rig.withBackoff(backoff);

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(rig.http.fetches.empty());
    CHECK(outcome.plan == core::FetchPlan::DeferToCache);
    CHECK(outcome.decision.action == core::UpdateAction::Optional);
    CHECK(outcome.manifestAgeSeconds == 12 * kHour);
}

TEST_CASE("a success clears an accumulated backoff")
{
    Rig rig;
    core::BackoffState backoff;
    backoff.consecutiveFailures = 4;
    backoff.nextAttemptUnix = kNow - 1;  // expired
    rig.withBackoff(backoff);
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));

    UpdateService service = rig.service();
    (void)service.check();

    CHECK(rig.storedBackoff().consecutiveFailures == 0);
    CHECK(rig.storedBackoff().lastSuccessUnix == kNow);
}

TEST_CASE("a network failure falls back to the stale cache")
{
    Rig rig;
    rig.withCache(manifestFor("0.2.0", kAppBody), kNow - (7 * kHour));
    rig.http.fetchResults.push_back(failure(infra::HttpError::HttpStatus));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(outcome.servedFromCache);
    CHECK(outcome.decision.action == core::UpdateAction::Optional);
    CHECK(outcome.transport == infra::HttpError::HttpStatus);
    CHECK(outcome.detail.find("last known release") != std::string::npos);
}

TEST_CASE("an unreachable host falls back to the mirror")
{
    Rig rig;
    rig.http.fetchResults.push_back(failure(infra::HttpError::DnsFailure));
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    REQUIRE(rig.http.fetches.size() == 2);
    CHECK(rig.http.fetches[0].url == rig.config.manifestUrl);
    CHECK(rig.http.fetches[1].url == rig.config.mirrorUrl);
    CHECK(outcome.usedMirror);
    CHECK(outcome.decision.action == core::UpdateAction::Optional);
}

TEST_CASE("a host that answered is not second-guessed by the mirror")
{
    // A 404 or a rate limit means GitHub was reached and GitHub answered. The
    // mirror would answer the same way, and asking it just doubles the traffic.
    for (const infra::HttpError e : {infra::HttpError::HttpStatus, infra::HttpError::RateLimited,
                                     infra::HttpError::TlsVerifyFailed}) {
        Rig rig;
        rig.http.fetchResults.push_back(failure(e));

        UpdateService service = rig.service();
        (void)service.check();
        CHECK(rig.http.fetches.size() == 1);
    }
}

TEST_CASE("a manifest this build cannot read is never written to the cache")
{
    // Caching it would serve the same unreadable document straight back from
    // disk on every launch for the next six hours.
    Rig rig;
    rig.http.fetchResults.push_back(ok(R"({"schema_version": 99})"));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(outcome.decision.action == core::UpdateAction::ManifestUnreadable);
    CHECK_FALSE(rig.files.exists(rig.config.cachedManifestPath()));
    CHECK(rig.storedBackoff().consecutiveFailures == 1);
}

TEST_CASE("an unreadable published manifest still shows the last known release")
{
    Rig rig;
    rig.withCache(manifestFor("0.2.0", kAppBody), kNow - (7 * kHour));
    rig.http.fetchResults.push_back(ok("{ truncated"));

    UpdateService service = rig.service();
    const CheckOutcome outcome = service.check();

    CHECK(outcome.servedFromCache);
    CHECK(outcome.decision.action == core::UpdateAction::Optional);
    CHECK(rig.files.files[rig.config.cachedManifestPath()] != "{ truncated");
}

// ---------------------------------------------------------------------------
// check() - the policy comes through unchanged
// ---------------------------------------------------------------------------

TEST_CASE("the installed version decides what is offered")
{
    Rig rig;
    rig.config = configFor("0.2.0");
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));

    UpdateService service = rig.service();
    CHECK(service.check().decision.action == core::UpdateAction::UpToDate);
}

TEST_CASE("a mandatory release is reported as mandatory")
{
    Rig rig;
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody, "stable", true)));

    UpdateService service = rig.service();
    CHECK(service.check().decision.action == core::UpdateAction::Mandatory);
}

TEST_CASE("a beta manifest is ignored on the stable channel")
{
    Rig rig;
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody, "beta")));

    UpdateService service = rig.service();
    CHECK(service.check().decision.action == core::UpdateAction::WrongChannel);
}

TEST_CASE("a version below min_supported is sent to the zip")
{
    Rig rig;
    rig.http.fetchResults.push_back(ok(manifestFor("0.9.0", kAppBody, "stable", false, "0.5.0")));

    UpdateService service = rig.service();
    CHECK(service.check().decision.action == core::UpdateAction::UnsupportedPath);
}

// ---------------------------------------------------------------------------
// stage()
// ---------------------------------------------------------------------------

TEST_CASE("staging downloads, verifies and records a handoff")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    REQUIRE(check.offersUpdate());

    const StageOutcome staged = service.stage(*check.manifest);
    REQUIRE(staged.result == StageResult::Staged);
    CHECK(staged.readyToChainload());

    CHECK(rig.files.exists(rig.config.stagedNroPath()));
    CHECK(rig.files.exists(rig.config.handoffPath()));
    CHECK(rig.files.files[rig.config.stagedNroPath()] == kAppBody);

    REQUIRE(staged.handoff.has_value());
    CHECK(staged.handoff->fromVersion == "0.1.0");
    CHECK(staged.handoff->toVersion == "0.2.0");
    CHECK(staged.handoff->attempts == 0);
    CHECK(staged.handoff->createdAtUnix == kNow);

    CHECK(staged.forwarderPath == rig.config.forwarderNro);
    CHECK(staged.forwarderArgs.find("--handoff=" + rig.config.handoffPath()) != std::string::npos);
}

TEST_CASE("the download is refused before the first byte when space is short")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.freeSpaceShort();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::InsufficientSpace);
    CHECK(rig.http.downloads.empty());
    CHECK_FALSE(rig.files.exists(rig.config.handoffPath()));
}

TEST_CASE("unknown free space proceeds rather than refusing")
{
    // statvfs is not guaranteed on every devoptab. A missing statistic must not
    // become a stranded device.
    Rig rig;
    rig.withForwarderInRomfs();
    rig.files.freeSpace = std::nullopt;
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    CHECK(service.stage(*check.manifest).result == StageResult::Staged);
}

TEST_CASE("a digest mismatch is retried exactly once, then reported")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back("corrupted payload xxxxx");
    rig.http.downloadBodies.push_back("corrupted payload yyyyy");
    rig.http.downloadBodies.push_back(kAppBody);  // never reached

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::VerifyFailed);
    CHECK(staged.transport == infra::HttpError::DigestMismatch);
    CHECK(rig.http.downloads.size() == 2);
    CHECK_FALSE(rig.files.exists(rig.config.handoffPath()));
    CHECK(staged.detail.find("untouched") != std::string::npos);
}

TEST_CASE("a second attempt that verifies is accepted")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back("corrupted payload xxxxx");
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    CHECK(service.stage(*check.manifest).result == StageResult::Staged);
    CHECK(rig.http.downloads.size() == 2);
}

TEST_CASE("a transient failure is retried up to three times")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    // No bodies queued: the fake reports ConnectFailed every time.

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::DownloadFailed);
    CHECK(rig.http.downloads.size() == 3);
}

TEST_CASE("a rate limit is not retried at all")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);
    rig.rateLimitDownloads();

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::DownloadFailed);
    CHECK(rig.http.downloads.size() == 1);
}

TEST_CASE("a missing forwarder is restored from romfs before the handoff is written")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    REQUIRE_FALSE(rig.files.exists(rig.config.forwarderNro));

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    CHECK(service.stage(*check.manifest).result == StageResult::Staged);
    CHECK(rig.files.exists(rig.config.forwarderNro));
}

TEST_CASE("no forwarder anywhere aborts before anything is handed off")
{
    // A handoff with nothing able to act on it is a pending update that can
    // never install - and the staged binary would sit there forever.
    Rig rig;  // deliberately no forwarder in romfs
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::ForwarderUnavailable);
    CHECK_FALSE(rig.files.exists(rig.config.handoffPath()));
    CHECK_FALSE(rig.files.exists(rig.config.stagedNroPath()));
}

TEST_CASE("a handoff that cannot be written leaves nothing staged")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.files.writeFailures.insert(rig.config.handoffPath());
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::WriteFailed);
    CHECK_FALSE(rig.files.exists(rig.config.stagedNroPath()));
}

TEST_CASE("a configured path the forwarder would reject is caught here, not after the chainload")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.config.targetNro = "switch/nsx-manager/nsx-manager.nro";  // relative
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    const StageOutcome staged = service.stage(*check.manifest);

    CHECK(staged.result == StageResult::WriteFailed);
    CHECK(staged.detail.find("reject") != std::string::npos);
    CHECK_FALSE(rig.files.exists(rig.config.handoffPath()));
}

TEST_CASE("discardStaged removes every leftover")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    REQUIRE(service.stage(*check.manifest).result == StageResult::Staged);

    service.discardStaged();
    CHECK_FALSE(rig.files.exists(rig.config.stagedNroPath()));
    CHECK_FALSE(rig.files.exists(rig.config.handoffPath()));
}

TEST_CASE("every staging result has a description")
{
    for (const StageResult r :
         {StageResult::Staged, StageResult::NoAppAsset, StageResult::InsufficientSpace,
          StageResult::DownloadFailed, StageResult::VerifyFailed,
          StageResult::ForwarderUnavailable, StageResult::WriteFailed}) {
        CHECK_FALSE(describe(r).empty());
        CHECK(describe(r) != "unknown");
    }
}

// ---------------------------------------------------------------------------
// The loop closes: what this service writes is what the forwarder acts on
// ---------------------------------------------------------------------------

TEST_CASE("the handoff this service writes drives the forwarder to install it")
{
    Rig rig;
    rig.withForwarderInRomfs();
    rig.files.files[rig.config.targetNro] = "the currently installed binary";
    rig.http.fetchResults.push_back(ok(manifestFor("0.2.0", kAppBody)));
    rig.http.downloadBodies.push_back(kAppBody);

    UpdateService service = rig.service();
    const CheckOutcome check = service.check();
    REQUIRE(service.stage(*check.manifest).result == StageResult::Staged);

    // Everything below is what the forwarder does on the next boot, using the
    // bytes this service actually left on the card.
    const std::string document = rig.files.files[rig.config.handoffPath()];
    const core::Result<core::Handoff, core::HandoffError> parsed = core::parseHandoff(document);
    REQUIRE(parsed.hasValue());

    const core::Handoff& handoff = parsed.value();
    CHECK(handoff.toVersion == "0.2.0");
    CHECK(core::digestsEqual(handoff.stagedSha256, core::Sha256::of(kAppBody)));

    core::FilePresence present;
    present.handoff = rig.files.exists(rig.config.handoffPath());
    present.staged = rig.files.exists(handoff.stagedNro);
    present.target = rig.files.exists(handoff.targetNro);
    present.backup = rig.files.exists(handoff.backupNro);

    const core::RecoveryDecision recovery = core::decideRecovery(handoff, present);
    CHECK(recovery.action == core::RecoveryAction::ProceedWithSwap);
    CHECK(recovery.mutatesFilesystem());

    // And the digest the forwarder re-checks is the digest of what is there.
    const std::optional<core::Sha256::Digest> onCard = rig.files.digestOf(handoff.stagedNro);
    REQUIRE(onCard.has_value());
    CHECK(core::digestsEqual(*onCard, handoff.stagedSha256));
}
