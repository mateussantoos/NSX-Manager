// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "nsx/core/catalog/catalog.hpp"
#include "nsx/core/cfw/merge_marker.hpp"
#include "nsx/core/paths/preserve_rules.hpp"
#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief Where a CFW pack install stages, backs up and lands.
/// @details Paths are configuration so the whole flow can run against a fake
///          filesystem. The defaults are the real ones.
/// @since 0.3.0
struct CfwInstallConfig
{
    std::string stagingRoot{"/config/nsx-manager/staging/cfw"};  ///< Downloads and extracted trees.
    std::string sdRoot{"/"};                                     ///< The card itself.
    std::string atmosphereContents{"/atmosphere/contents"};      ///< Title content.
    std::string bootloader{"/bootloader"};                       ///< Hekate and its payloads.
    std::string firmwareStaging{"/firmware"};                    ///< Where Daybreak looks.
    std::string switchApps{"/switch"};                           ///< Homebrew.
    std::string themes{"/themes"};                               ///< Home menu themes.

    /// @brief Extra free space required beyond the download, as a percentage.
    /// @details A pack is downloaded, then expanded beside itself, so the peak
    ///          requirement is roughly the archive plus its expanded size. The
    ///          expanded size is not known until the archive is open, so this
    ///          multiplies the download instead - deliberately generous.
    int freeSpaceHeadroomPercent{250};

    /// @brief Ceiling on an archive's declared expanded size.
    /// @details Refused before a byte is decompressed. A pack is large; a
    ///          decompression bomb is larger.
    std::uint64_t maxUncompressedBytes{6ULL * 1024 * 1024 * 1024};

    /// @brief Resolve a catalogue destination to a directory.
    /// @param target The destination kind from the catalogue.
    /// @return The absolute directory.
    /// @details The catalogue names a kind and never a path, so this function is
    ///          the complete list of places an install can write. See ADR-0018.
    [[nodiscard]] std::string resolve(core::InstallTarget target) const;

    /// @brief The directory an item is downloaded and expanded in.
    /// @param itemId The catalogue item id.
    /// @return `<stagingRoot>/<itemId>`.
    [[nodiscard]] std::string stagingFor(const std::string& itemId) const
    {
        return stagingRoot + "/" + itemId;
    }

    /// @brief The archive path for an item.
    /// @param itemId The catalogue item id.
    /// @return `<stagingRoot>/<itemId>.zip`.
    [[nodiscard]] std::string archiveFor(const std::string& itemId) const
    {
        return stagingRoot + "/" + itemId + ".zip";
    }

    /// @brief Where an item's displaced originals are kept.
    /// @param itemId The catalogue item id.
    /// @return `<stagingRoot>/<itemId>.backup`.
    [[nodiscard]] std::string backupFor(const std::string& itemId) const
    {
        return stagingRoot + "/" + itemId + ".backup";
    }

    /// @brief The marker written while a merge is in progress.
    /// @return `<stagingRoot>/merge.json`.
    /// @details One at a time, by design: two concurrent merges into the same
    ///          card could not be rolled back independently.
    [[nodiscard]] std::string markerPath() const { return stagingRoot + "/merge.json"; }

    /// @brief Every path the merge intends to write, one per line.
    /// @return `<stagingRoot>/merge.files`.
    /// @details Written once before the merge begins. The merge MOVES files out
    ///          of the staging tree, so by the time a rollback runs the tree no
    ///          longer says what was installed - and a file the pack created
    ///          fresh has no backup to identify it by either. Without this list
    ///          those files could not be removed, and a failed install would
    ///          leave pieces of a pack behind.
    [[nodiscard]] std::string mergeFileListPath() const { return stagingRoot + "/merge.files"; }
};

/// @brief How far an install got.
/// @since 0.3.0
enum class InstallResult
{
    Installed,          ///< Merged and committed.
    InsufficientSpace,  ///< Refused before the first byte.
    DownloadFailed,     ///< Transport, status or size failure.
    VerifyFailed,       ///< The bytes arrived and did not match the digest.
    UnsafeArchive,      ///< An entry would have written outside the destination.
    ExtractFailed,      ///< The archive could not be expanded.
    MergeFailed,        ///< A file could not be moved into place.
    RolledBack,         ///< A merge failed and the card was put back as it was.
    Cancelled,          ///< The user stopped it.
    Busy                ///< Another merge is already in flight.
};

/// @brief Which stage an install is in, for a progress display.
/// @since 0.3.0
enum class InstallStage
{
    Preflight,    ///< Checking space and clearing leftovers.
    Downloading,  ///< Fetching the package.
    Extracting,   ///< Expanding it into staging.
    Merging       ///< Moving it into place.
};

/// @brief Progress through an install.
/// @since 0.3.0
struct InstallProgress
{
    InstallStage stage{InstallStage::Preflight};  ///< What is happening.
    std::uint64_t done{};                         ///< Units completed in this stage.
    std::uint64_t total{};                        ///< Units expected; zero when unknown.
    std::string detail;                           ///< The current file, when there is one.
};

/// @brief Called as an install proceeds.
/// @return False to cancel.
using InstallCallback = std::function<bool(const InstallProgress&)>;

/// @brief The result of an install.
/// @since 0.3.0
struct InstallOutcome
{
    InstallResult result{InstallResult::DownloadFailed};  ///< How far it got.
    std::size_t filesMerged{};                            ///< Files moved into place.
    std::size_t filesPreserved{};                         ///< Left alone per `preserve.txt`.
    std::size_t filesReplaced{};                          ///< Existing files displaced to backup.
    infra::HttpError transport{infra::HttpError::None};   ///< Set only on a network failure.
    std::string detail;                                   ///< One line for the log and the UI.

    /// @brief Whether the card now holds the new pack.
    /// @return True only for a completed install.
    [[nodiscard]] bool installed() const { return result == InstallResult::Installed; }

    /// @brief Whether the card is in the state it started in.
    /// @return True when nothing was changed, or everything was undone.
    [[nodiscard]] bool cardUnchanged() const
    {
        return result != InstallResult::Installed && result != InstallResult::MergeFailed;
    }
};

/// @brief A short English description of an install result.
/// @param result The result to describe.
/// @return A description suitable for a log line.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(InstallResult result);

/// @brief Download, verify, extract and merge a CFW pack.
///
/// @details The sequence, and why it is this order:
///
///          1. **Pre-flight** - refuse for space before the first byte, using
///             the size the catalogue already published.
///          2. **Download** - to staging, digest verified by the gateway before
///             the file gets a name anything acts on.
///          3. **Extract** - into a staging tree, every entry through
///             @ref nsx::core::ExtractionPolicy. Nothing has touched the card
///             outside `staging/` up to this point, so a hostile archive is
///             discovered while the installation is still untouched.
///          4. **Merge** - move the staged tree into place, displacing what it
///             replaces into a backup directory rather than overwriting it.
///          5. **Commit** - delete the backup and the staging tree, then the
///             marker.
///
///          The merge is the only step that changes the card, and it is the
///          only one that can be interrupted destructively. A marker is written
///          before it starts and removed after it commits;
///          @ref recoverInterruptedMerge undoes whatever it finds.
///
///          **Not atomic, and cannot be.** A pack is thousands of files and a
///          FAT volume has no transaction. What it has instead is: nothing
///          outside `staging/` changes until step 4, and everything step 4
///          displaces is kept until step 5.
///
/// @see docs/architecture/cfw-install.md
/// @since 0.3.0
class CfwInstallService
{
public:
    /// @brief Construct the service over its ports.
    /// @param http Network access.
    /// @param archives Archive extraction.
    /// @param files Filesystem access.
    /// @param clock Time source.
    /// @param config Paths and limits.
    CfwInstallService(HttpGateway& http, ArchiveGateway& archives, FileStore& files,
                      const Clock& clock, CfwInstallConfig config);

    /// @brief Install a catalogue item.
    /// @param item The item to install, as parsed from the catalogue.
    /// @param preserve What the user asked to keep; ignored when the item does
    ///        not honour preserve rules.
    /// @param onProgress Optional; returning false cancels.
    /// @return How far it got, and what it did.
    [[nodiscard]] InstallOutcome install(const core::CatalogItem& item,
                                         const core::PreserveRules& preserve,
                                         const InstallCallback& onProgress = {});

    /// @brief Undo a merge that was interrupted.
    ///
    /// @details Call at startup, before anything else touches the card. A
    ///          console powered off during a merge comes back with the card
    ///          half one pack and half another, and this is what puts it back.
    ///
    /// @return What was done, and why.
    [[nodiscard]] InstallOutcome recoverInterruptedMerge();

    /// @brief Delete an item's staging area and archive.
    /// @param itemId The catalogue item id.
    /// @details Does not touch the backup or the marker: those belong to a
    ///          merge, and removing them is @ref recoverInterruptedMerge's
    ///          decision to make.
    void discardStaging(const std::string& itemId);

    /// @brief The configuration this service was built with.
    /// @return A reference to the configuration.
    [[nodiscard]] const CfwInstallConfig& config() const { return m_config; }

private:
    [[nodiscard]] InstallOutcome mergeStagedTree(const core::CatalogItem& item,
                                                 const core::PreserveRules& preserve,
                                                 const std::string& stagingDir,
                                                 const std::string& destination,
                                                 const InstallCallback& onProgress);
    [[nodiscard]] bool rollBack(const core::MergeMarker& marker,
                                const std::vector<std::string>& planned);
    [[nodiscard]] std::vector<std::string> readPlannedFiles() const;
    void commit(const core::MergeMarker& marker);

    HttpGateway& m_http;
    ArchiveGateway& m_archives;
    FileStore& m_files;
    const Clock& m_clock;
    CfwInstallConfig m_config;
};

}  // namespace nsx::domain
