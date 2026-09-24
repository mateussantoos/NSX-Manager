// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/selfupdate/update_service.hpp"

#include <utility>

namespace nsx::domain {

namespace {

using core::BackoffState;
using core::CacheMetadata;
using core::FetchPlan;
using core::Handoff;
using core::Sha256;
using core::UpdateManifest;
using infra::HttpError;

/// A manifest is a few kilobytes. Anything claiming to be one and running to a
/// megabyte is not a manifest, and reading it into a console's heap to find out
/// is not necessary.
constexpr std::uint64_t kMaxDocumentBytes = 1u << 20u;

/// Small JSON side-files: backoff counters, cache metadata.
constexpr std::uint64_t kMaxSidecarBytes = 64u * 1024u;

/// A transfer that failed for a reason another attempt might survive.
bool isTransient(HttpError error)
{
    switch (error) {
        case HttpError::Timeout:
        case HttpError::ConnectFailed:
        case HttpError::DnsFailure:
        case HttpError::SizeMismatch:
            return true;
        default:
            return false;
    }
}

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

std::string_view describe(StageResult result)
{
    switch (result) {
        case StageResult::Staged:
            return "update staged";
        case StageResult::NoAppAsset:
            return "the release contains no application binary";
        case StageResult::InsufficientSpace:
            return "not enough free space";
        case StageResult::DownloadFailed:
            return "the download failed";
        case StageResult::VerifyFailed:
            return "the download did not verify";
        case StageResult::ForwarderUnavailable:
            return "the repair binary is missing";
        case StageResult::WriteFailed:
            return "the update could not be recorded";
    }
    return "unknown";
}

UpdateService::UpdateService(HttpGateway& http, FileStore& files, const Clock& clock,
                             SelfUpdateConfig config)
    : m_http(http), m_files(files), m_clock(clock), m_config(std::move(config))
{
}

// ---------------------------------------------------------------------------
// Cache and backoff persistence
// ---------------------------------------------------------------------------

std::optional<UpdateService::CachedDocument> UpdateService::loadCache() const
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
        m_files.readText(m_config.cachedManifestPath(), kMaxDocumentBytes);
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

BackoffState UpdateService::loadBackoff() const
{
    const std::optional<std::string> raw =
        m_files.readText(m_config.backoffPath(), kMaxSidecarBytes);
    if (!raw.has_value()) {
        return BackoffState{};
    }
    return core::parseBackoff(*raw).value_or(BackoffState{});
}

void UpdateService::saveBackoff(const BackoffState& state)
{
    // Best effort. Failing to persist a throttling hint must not fail an update
    // check that otherwise succeeded.
    (void)m_files.makeDirectories(m_config.cacheDir);
    (void)m_files.writeAtomic(m_config.backoffPath(), core::serializeBackoff(state));
}

bool UpdateService::storeCache(const std::string& document, const std::string& etag,
                               const std::string& sourceUrl, std::int64_t nowUnix)
{
    if (!m_files.makeDirectories(m_config.cacheDir)) {
        return false;
    }

    CacheMetadata meta;
    meta.fetchedAtUnix = nowUnix;
    meta.etag = etag;
    meta.documentSha256 = Sha256::hexOf(document);
    meta.documentSize = document.size();
    meta.sourceUrl = sourceUrl;

    // Document first, metadata second. Interrupted between the two, the old
    // metadata no longer describes the file on disk, cacheMatchesDocument says
    // so, and the cache is ignored rather than half-trusted.
    if (!m_files.writeAtomic(m_config.cachedManifestPath(), document)) {
        return false;
    }
    return m_files.writeAtomic(m_config.cacheMetadataPath(), serializeCacheMetadata(meta));
}

// ---------------------------------------------------------------------------
// check()
// ---------------------------------------------------------------------------

CheckOutcome UpdateService::fromDocument(const std::string& document, bool servedFromCache,
                                         std::int64_t ageSeconds, FetchPlan plan,
                                         std::string detail) const
{
    CheckOutcome out;
    out.plan = plan;
    out.servedFromCache = servedFromCache;
    out.manifestAgeSeconds = ageSeconds;
    out.detail = std::move(detail);

    const core::Result<UpdateManifest, core::ManifestError> parsed = core::parseManifest(document);
    if (!parsed.hasValue()) {
        const std::string why(core::describe(parsed.error()));
        out.decision = core::unreadableManifest(why);
        out.detail = why;
        return out;
    }

    out.manifest = parsed.value();
    out.decision = core::decideUpdate(m_config.installed, parsed.value(), m_config.channel);
    return out;
}

CheckOutcome UpdateService::check()
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
            CheckOutcome out;
            out.plan = plan.plan;
            out.decision = core::unreadableManifest(plan.reason);
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
        m_http.fetch(m_config.manifestUrl, plan.ifNoneMatch);

    if (!response.hasValue() && hostUnreachable(response.error()) && !m_config.mirrorUrl.empty()) {
        // The mirror is a different document with a different ETag history, so
        // the conditional header is dropped rather than sent somewhere it
        // cannot mean anything.
        usedMirror = true;
        response = m_http.fetch(m_config.mirrorUrl, {});
    }

    const std::string sourceUrl = usedMirror ? m_config.mirrorUrl : m_config.manifestUrl;

    if (!response.hasValue()) {
        const HttpError error = response.error();
        saveBackoff(
            core::afterFailure(backoff, now, m_config.backoff, static_cast<std::uint64_t>(now)));

        if (cached.has_value()) {
            CheckOutcome out = fromDocument(cached->document, true, plan.cacheAgeSeconds, plan.plan,
                                            "update check failed (" + describeTransport(error) +
                                                "); showing the last known release");
            out.transport = error;
            out.usedMirror = usedMirror;
            return out;
        }

        CheckOutcome out;
        out.plan = plan.plan;
        out.transport = error;
        out.usedMirror = usedMirror;
        out.detail = describeTransport(error);
        out.decision = core::unreadableManifest(out.detail);
        return out;
    }

    // 304: the cache is still current. Re-stamp it so the TTL restarts rather
    // than asking again on the next launch.
    if (response.value().status == 304 && cached.has_value()) {
        (void)storeCache(cached->document,
                         cached->meta.etag.empty() ? plan.ifNoneMatch : cached->meta.etag,
                         cached->meta.sourceUrl.empty() ? sourceUrl : cached->meta.sourceUrl, now);
        saveBackoff(core::afterSuccess(backoff, now));

        CheckOutcome out =
            fromDocument(cached->document, true, 0, plan.plan, "not modified since last check");
        out.usedMirror = usedMirror;
        return out;
    }

    CheckOutcome out =
        fromDocument(response.value().body, false, 0, plan.plan, "fetched a fresh manifest");
    out.usedMirror = usedMirror;

    if (out.decision.action == core::UpdateAction::ManifestUnreadable) {
        // Never cache a document this build cannot read: the next launch would
        // serve it straight back from disk and fail identically for six hours.
        // A server that keeps answering with something unparseable is also worth
        // backing off from, for the same reason a dead one is.
        saveBackoff(
            core::afterFailure(backoff, now, m_config.backoff, static_cast<std::uint64_t>(now)));

        if (cached.has_value()) {
            CheckOutcome fallback =
                fromDocument(cached->document, true, plan.cacheAgeSeconds, plan.plan,
                             "the published manifest could not be read (" + out.detail +
                                 "); showing the last known release");
            fallback.usedMirror = usedMirror;
            return fallback;
        }
        return out;
    }

    (void)storeCache(response.value().body, response.value().etag, sourceUrl, now);
    saveBackoff(core::afterSuccess(backoff, now));
    return out;
}

// ---------------------------------------------------------------------------
// stage()
// ---------------------------------------------------------------------------

void UpdateService::discardStaged()
{
    (void)m_files.remove(m_config.handoffPath());
    (void)m_files.remove(m_config.stagedNroPath() + ".part");
    (void)m_files.remove(m_config.stagedNroPath());
    (void)m_files.remove(m_config.backupNroPath());
}

void UpdateService::discardStalePartials()
{
    (void)m_files.remove(m_config.stagedNroPath() + ".part");
}

bool UpdateService::installForwarder()
{
    const bool chainloadable = ensureForwarder();

    // The hbmenu entry is a recovery convenience, not part of the update path,
    // so its failure is not this function's answer. Copied from romfs rather
    // than from the chainload copy so a damaged one cannot propagate.
    if (!m_files.exists(m_config.repairEntryNro)) {
        const std::string::size_type slash = m_config.repairEntryNro.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
            (void)m_files.makeDirectories(m_config.repairEntryNro.substr(0, slash));
        }
        (void)m_files.copyFile(m_config.forwarderSource, m_config.repairEntryNro);
    }

    return chainloadable;
}

bool UpdateService::ensureForwarder()
{
    if (m_files.exists(m_config.forwarderNro)) {
        return true;
    }

    // Missing entirely - restore it from the copy shipped inside this NRO's
    // romfs. If that cannot be done there is nothing able to perform the swap,
    // and the caller must stop before recording a handoff.
    const std::string::size_type slash = m_config.forwarderNro.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        (void)m_files.makeDirectories(m_config.forwarderNro.substr(0, slash));
    }
    if (!m_files.copyFile(m_config.forwarderSource, m_config.forwarderNro)) {
        return false;
    }
    return m_files.exists(m_config.forwarderNro);
}

StageOutcome UpdateService::stage(const UpdateManifest& manifest,
                                  const infra::ProgressCallback& onProgress)
{
    StageOutcome out;

    const core::ManifestAsset* asset = manifest.findAsset(core::AssetKind::AppNro);
    if (asset == nullptr) {
        out.result = StageResult::NoAppAsset;
        out.detail = "the manifest describes no app-nro asset";
        return out;
    }

    if (!m_files.makeDirectories(m_config.stagingDir)) {
        out.result = StageResult::WriteFailed;
        out.detail = "could not create " + m_config.stagingDir;
        return out;
    }

    // Pre-flight, before the first byte. The size comes from the manifest, so
    // this costs no extra request.
    const std::optional<std::uint64_t> free = m_files.freeSpaceBytes(m_config.stagingDir);
    const std::uint64_t headroom =
        asset->size * static_cast<std::uint64_t>(m_config.freeSpaceHeadroomPercent) / 100u;
    const std::uint64_t needed = asset->size + headroom;
    if (free.has_value() && *free < needed) {
        out.result = StageResult::InsufficientSpace;
        out.detail = "need " + std::to_string(needed / 1024u / 1024u) + " MB free, have " +
                     std::to_string(*free / 1024u / 1024u) + " MB";
        return out;
    }

    // A leftover .part is unverified by definition and its bytes are worthless
    // to us - there is no resume yet, so it can only take up space.
    (void)m_files.remove(m_config.stagedNroPath() + ".part");

    infra::ExpectedArtifact expected;
    expected.size = asset->size;
    expected.sha256 = asset->sha256;

    // The failure matrix in docs/architecture/update-pipeline.md: a transient
    // transport failure is worth three attempts, a digest mismatch exactly two,
    // and everything else none. A mismatch that repeats is not a flaky network,
    // it is a file that does not match what was published, and retrying it
    // forever only delays telling the user so.
    constexpr int kTransientAttempts = 3;
    constexpr int kDigestAttempts = 2;

    int transientTries = 0;
    int digestTries = 0;
    HttpError lastError = HttpError::None;

    while (true) {
        const core::Result<std::monostate, HttpError> downloaded =
            m_http.download(asset->url, m_config.stagedNroPath(), expected, onProgress);
        if (downloaded.hasValue()) {
            lastError = HttpError::None;
            break;
        }

        lastError = downloaded.error();

        if (lastError == HttpError::DigestMismatch && ++digestTries < kDigestAttempts) {
            continue;
        }
        if (isTransient(lastError) && ++transientTries < kTransientAttempts) {
            continue;
        }
        break;
    }

    if (lastError != HttpError::None) {
        out.transport = lastError;
        out.result =
            (lastError == HttpError::DigestMismatch || lastError == HttpError::SizeMismatch)
                ? StageResult::VerifyFailed
                : StageResult::DownloadFailed;
        out.detail = describeTransport(lastError);
        if (out.result == StageResult::VerifyFailed) {
            out.detail += " - the update was discarded and your current version is untouched";
        }
        return out;
    }

    // The gateway verified the bytes it wrote. Read the file back from the
    // filesystem we will actually hand to the forwarder: this is what catches a
    // card that accepted the write and returned something else.
    const std::optional<Sha256::Digest> onDisk = m_files.digestOf(m_config.stagedNroPath());
    if (!onDisk.has_value() || !core::digestsEqual(*onDisk, asset->sha256)) {
        (void)m_files.remove(m_config.stagedNroPath());
        out.result = StageResult::VerifyFailed;
        out.detail = "the staged binary did not verify after it was written";
        return out;
    }

    // Before the handoff, never after. A handoff pointing at a chainload that
    // cannot happen is a pending update nothing can install.
    if (!ensureForwarder()) {
        (void)m_files.remove(m_config.stagedNroPath());
        out.result = StageResult::ForwarderUnavailable;
        out.detail =
            "could not restore " + m_config.forwarderNro + " from " + m_config.forwarderSource;
        return out;
    }

    Handoff handoff;
    handoff.createdAtUnix = m_clock.nowUnix();
    handoff.fromVersion = m_config.installed.toString();
    handoff.toVersion = manifest.version.toString();
    handoff.stagedNro = m_config.stagedNroPath();
    handoff.stagedSha256 = asset->sha256;
    handoff.targetNro = m_config.targetNro;
    handoff.backupNro = m_config.backupNroPath();
    handoff.forwarderNro = m_config.forwarderNro;
    handoff.attempts = 0;
    handoff.maxAttempts = m_config.maxSwapAttempts;

    const std::string document = core::serializeHandoff(handoff);

    // Read back what we are about to write, with the same parser the forwarder
    // uses. A configured path that the parser rejects - relative, or carrying a
    // `..` - would otherwise produce a handoff the forwarder refuses, and it
    // would refuse it after the chainload rather than here where it can be
    // reported.
    const core::Result<Handoff, core::HandoffError> selfCheck = core::parseHandoff(document);
    if (!selfCheck.hasValue()) {
        (void)m_files.remove(m_config.stagedNroPath());
        out.result = StageResult::WriteFailed;
        out.detail = "refusing to write a handoff this build would reject: " +
                     std::string(core::describe(selfCheck.error()));
        return out;
    }

    if (!m_files.writeAtomic(m_config.handoffPath(), document)) {
        (void)m_files.remove(m_config.stagedNroPath());
        out.result = StageResult::WriteFailed;
        out.detail = "could not write " + m_config.handoffPath();
        return out;
    }

    out.result = StageResult::Staged;
    out.handoff = handoff;
    out.forwarderPath = m_config.forwarderNro;
    out.forwarderArgs = "\"" + m_config.forwarderNro + "\" --handoff=" + m_config.handoffPath();
    out.detail = "staged " + handoff.toVersion + "; restart to install";
    return out;
}

}  // namespace nsx::domain
