// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "nsx/core/update/backoff.hpp"
#include "nsx/core/update/handoff.hpp"
#include "nsx/core/update/manifest.hpp"
#include "nsx/core/update/manifest_cache.hpp"
#include "nsx/core/update/update_policy.hpp"
#include "nsx/core/version/semver.hpp"
#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief Everything the update flow needs to know about this installation.
///
/// @details Paths are configuration rather than constants so the whole flow can
///          run against a fake filesystem in a unit test. The defaults are the
///          real ones and are documented in `docs/reference/sd-layout.md`.
/// @since 0.2.0
struct SelfUpdateConfig
{
    /// @brief Stable-named release asset. A web redirect, **not** `api.github.com`.
    /// @details Deliberately not the REST API: unauthenticated REST is 60
    ///          requests per hour per IP and a `304` still spends one, which is
    ///          the 403 the predecessor lived with.
    std::string manifestUrl{
        "https://github.com/mateussantoos/nsx-manager/releases/latest/download/update.json"};

    /// @brief Orphan-branch mirror, tried only when the primary is unreachable.
    std::string mirrorUrl{
        "https://raw.githubusercontent.com/mateussantoos/nsx-manager/"
        "release-metadata/update.json"};

    std::string cacheDir{"/config/nsx-manager/cache"};      ///< Manifest cache and backoff state.
    std::string stagingDir{"/config/nsx-manager/staging"};  ///< Downloads and the handoff.
    std::string targetNro{"/switch/nsx-manager/nsx-manager.nro"};  ///< The application itself.

    /// @brief The forwarder copy that is actually chainloaded.
    std::string forwarderNro{"/config/nsx-manager/forwarder/nsx-forwarder.nro"};

    /// @brief Where a missing or damaged forwarder is restored from.
    std::string forwarderSource{"romfs:/nsx-forwarder.nro"};

    /// @brief The copy hbmenu lists as "NSX Manager (Repair)".
    /// @details Beside the application rather than in `/config`, because hbmenu
    ///          only lists `/switch`. It exists so there is always a launchable
    ///          entry point even if the application binary is lost (ADR-0007).
    std::string repairEntryNro{"/switch/nsx-manager/nsx-forwarder.nro"};

    core::SemVer installed;                        ///< The running version.
    core::Channel channel{core::Channel::Stable};  ///< Which channel to accept.

    std::int64_t cacheTtlSeconds{core::kManifestCacheTtlSeconds};  ///< Manifest cache lifetime.
    core::BackoffPolicy backoff;                                   ///< Failure throttling.

    /// @brief Extra free space required beyond the download, as a percentage.
    /// @details Twenty percent. The download is written once and renamed, so the
    ///          headroom covers filesystem overhead and the copy the forwarder
    ///          makes beside the target, not a second full copy here.
    int freeSpaceHeadroomPercent{20};

    int maxSwapAttempts{3};  ///< Recorded in the handoff; the forwarder enforces it.

    /// @brief Path of the cached manifest document.
    /// @return `<cacheDir>/update.json`.
    [[nodiscard]] std::string cachedManifestPath() const { return cacheDir + "/update.json"; }

    /// @brief Path of the cache metadata beside the document.
    /// @return `<cacheDir>/update.meta.json`.
    [[nodiscard]] std::string cacheMetadataPath() const { return cacheDir + "/update.meta.json"; }

    /// @brief Path of the persisted backoff state.
    /// @return `<cacheDir>/backoff.json`.
    [[nodiscard]] std::string backoffPath() const { return cacheDir + "/backoff.json"; }

    /// @brief Path the verified new binary is staged at.
    /// @return `<stagingDir>/nsx-manager.nro`.
    [[nodiscard]] std::string stagedNroPath() const { return stagingDir + "/nsx-manager.nro"; }

    /// @brief Path the outgoing binary is kept at during the swap.
    /// @return `<stagingDir>/nsx-manager.nro.bak`.
    [[nodiscard]] std::string backupNroPath() const { return stagingDir + "/nsx-manager.nro.bak"; }

    /// @brief Path of the instruction the forwarder reads.
    /// @return `<stagingDir>/handoff.json`.
    [[nodiscard]] std::string handoffPath() const { return stagingDir + "/handoff.json"; }
};

/// @brief The result of an update check.
/// @since 0.2.0
struct CheckOutcome
{
    core::UpdateDecision decision;                 ///< What the UI should do.
    std::optional<core::UpdateManifest> manifest;  ///< Present whenever one was readable.

    core::FetchPlan plan{core::FetchPlan::Fetch};        ///< What the check actually did.
    bool servedFromCache{};                              ///< No fresh document was obtained.
    bool usedMirror{};                                   ///< The primary host was unreachable.
    std::int64_t manifestAgeSeconds{};                   ///< Age of the manifest being reported on.
    infra::HttpError transport{infra::HttpError::None};  ///< Set only when the network failed.
    std::string detail;                                  ///< One line for the log and the UI.

    /// @brief Whether the user should be offered an update.
    /// @return True for an optional or mandatory update.
    [[nodiscard]] bool offersUpdate() const { return decision.offersUpdate(); }
};

/// @brief Why staging an update stopped.
/// @since 0.2.0
enum class StageResult
{
    Staged,                ///< Verified, handed off, ready to chainload.
    NoAppAsset,            ///< The manifest describes no application binary.
    InsufficientSpace,     ///< Refused before the first byte.
    DownloadFailed,        ///< Transport, status or size failure.
    VerifyFailed,          ///< The bytes arrived but did not match the digest.
    ForwarderUnavailable,  ///< No forwarder to chainload; nothing was handed off.
    WriteFailed            ///< Could not record the handoff.
};

/// @brief The result of staging an update.
/// @since 0.2.0
struct StageOutcome
{
    StageResult result{StageResult::DownloadFailed};  ///< What happened.
    std::optional<core::Handoff> handoff;             ///< Present only when staged.
    std::string forwarderPath;                        ///< The binary to chainload.
    std::string forwarderArgs;  ///< Complete argv string for `envSetNextLoad`.
    infra::HttpError transport{infra::HttpError::None};  ///< Set only when the network failed.
    std::string detail;                                  ///< One line for the log and the UI.

    /// @brief Whether the caller may chainload the forwarder now.
    /// @return True only when everything verified and the handoff was recorded.
    [[nodiscard]] bool readyToChainload() const { return result == StageResult::Staged; }
};

/// @brief A short English description of a staging result.
/// @param result The result to describe.
/// @return A description suitable for a log line.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(StageResult result);

/// @brief The self-update use-case: check, then stage.
///
/// @details Holds no state of its own. Everything that persists lives on the SD
///          card through @ref FileStore, which is what lets the backoff survive
///          the application quitting - the thing that actually stops a failing
///          endpoint being re-hammered on every launch.
///
///          The rules it applies all live in `core` and are host-tested in
///          isolation: @ref nsx::core::planManifestFetch decides whether to use
///          the network, @ref nsx::core::parseManifest decides whether a
///          document is trustworthy, @ref nsx::core::decideUpdate decides what
///          to offer, @ref nsx::core::afterFailure decides when to try again.
///          This class sequences them and owns no policy itself.
///
/// @see docs/architecture/update-pipeline.md
/// @since 0.2.0
class UpdateService
{
public:
    /// @brief Construct the service over its ports.
    /// @param http Network access.
    /// @param files Filesystem access.
    /// @param clock Time source.
    /// @param config Paths, channel and policy.
    UpdateService(HttpGateway& http, FileStore& files, const Clock& clock, SelfUpdateConfig config);

    /// @brief Find out whether an update is available.
    ///
    /// @details Never blocks on the network when the cache is fresh, never
    ///          reports "up to date" when it simply could not find out, and
    ///          never writes a document to the cache that it could not parse.
    ///
    /// @return The decision, the manifest behind it, and how it was obtained.
    [[nodiscard]] CheckOutcome check();

    /// @brief Download, verify and hand off an update.
    ///
    /// @details The order is the safety property. Space is checked before the
    ///          first byte; the digest is verified before the file gets a name
    ///          anything would act on; the forwarder is confirmed present
    ///          **before** the handoff is written, so there is no state in which
    ///          an update is pending with nothing able to install it.
    ///
    /// @param manifest The manifest to install from, as returned by @ref check.
    /// @param onProgress Optional; returning false aborts the download.
    /// @return What happened, and what to chainload if it worked.
    [[nodiscard]] StageOutcome stage(const core::UpdateManifest& manifest,
                                     const infra::ProgressCallback& onProgress = {});

    /// @brief Put the forwarder where it can be chainloaded and launched.
    ///
    /// @details Two copies, from the one in this binary's romfs:
    ///
    ///          - @ref SelfUpdateConfig::forwarderNro, which @ref stage
    ///            chainloads. **Load-bearing**: without it there is nothing able
    ///            to perform a swap, and this returning false is the reason
    ///            @ref stage refuses to write a handoff.
    ///          - @ref SelfUpdateConfig::repairEntryNro, which hbmenu shows as
    ///            "NSX Manager (Repair)". Best effort - it is how a user
    ///            recovers by hand, not something the update path needs.
    ///
    ///          Call it at startup. An installation that has only ever been
    ///          unzipped has neither copy until something puts them there.
    ///
    /// @return True when the chainload copy exists.
    /// @see ADR-0007
    bool installForwarder();

    /// @brief Delete an interrupted download.
    ///
    /// @details Removes **only** the `.part`, which is unverified by definition
    ///          and which nothing will ever resume - there is no `Range:` resume
    ///          yet. Deliberately leaves the staged binary and the handoff
    ///          alone: those describe an update still in flight, and discarding
    ///          them here would silently abandon it.
    ///
    ///          Safe at startup. @ref discardStaged is not.
    void discardStalePartials();

    /// @brief Delete a previous attempt's leftovers.
    /// @details Removes the staged binary, its `.part`, the backup **and the
    ///          handoff**. Only call this when abandoning an update on purpose -
    ///          at startup it would discard one that is still in flight. Use
    ///          @ref discardStalePartials there instead.
    void discardStaged();

    /// @brief The configuration this service was built with.
    /// @return A reference to the configuration.
    [[nodiscard]] const SelfUpdateConfig& config() const { return m_config; }

    /// @brief Change update channel between Stable and Preview.
    void setChannel(core::Channel channel) { m_config.channel = channel; }

    /// @brief Whether to swap primary and fallback mirror URLs.
    void setPreferMirror(bool prefer)
    {
        if (prefer && !m_config.mirrorUrl.empty() && m_config.manifestUrl != m_config.mirrorUrl) {
            std::swap(m_config.manifestUrl, m_config.mirrorUrl);
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

    [[nodiscard]] CheckOutcome fromDocument(const std::string& document, bool servedFromCache,
                                            std::int64_t ageSeconds, core::FetchPlan plan,
                                            std::string detail) const;

    [[nodiscard]] bool ensureForwarder();

    HttpGateway& m_http;
    FileStore& m_files;
    const Clock& m_clock;
    SelfUpdateConfig m_config;
};

}  // namespace nsx::domain
