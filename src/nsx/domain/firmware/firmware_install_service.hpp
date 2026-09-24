// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "nsx/core/catalog/catalog.hpp"
#include "nsx/core/firmware/firmware_set.hpp"
#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief Where firmware is staged and who installs it.
/// @since 0.3.0
struct FirmwareConfig
{
    /// @brief The directory Daybreak reads a firmware set from.
    /// @details A well-known path this application does not own. Other tools
    ///          and every guide use it, which is exactly why clearing it is
    ///          done carefully rather than with a recursive delete.
    std::string firmwareDir{"/firmware"};

    std::string stagingRoot{"/config/nsx-manager/staging/firmware"};  ///< The download lives here.
    std::string daybreakNro{"/switch/daybreak.nro"};                  ///< Atmosphere's installer.

    /// @brief Extra free space required beyond the download, as a percentage.
    /// @details A firmware set is downloaded and then expanded, so the peak is
    ///          roughly twice the archive plus the existing set until it is
    ///          cleared.
    int freeSpaceHeadroomPercent{220};

    /// @brief Ceiling on the declared expanded size.
    std::uint64_t maxUncompressedBytes{4ULL * 1024 * 1024 * 1024};

    /// @brief The archive path for a firmware item.
    /// @param itemId The catalogue item id.
    /// @return `<stagingRoot>/<itemId>.zip`.
    [[nodiscard]] std::string archiveFor(const std::string& itemId) const
    {
        return stagingRoot + "/" + itemId + ".zip";
    }
};

/// @brief How far a firmware install got.
/// @since 0.3.0
enum class FirmwareResult
{
    Ready,               ///< Extracted and verified; Daybreak can install it.
    NotFirmwareItem,     ///< The catalogue item is not of kind `firmware`.
    InsufficientSpace,   ///< Refused before the first byte.
    DownloadFailed,      ///< Transport or status failure.
    VerifyFailed,        ///< The bytes arrived and did not match the digest.
    UnsafeArchive,       ///< An entry would have written outside the destination.
    ExtractFailed,       ///< The archive could not be expanded.
    DirectoryNotOurs,    ///< `/firmware/` holds files this application will not delete.
    ClearFailed,         ///< The previous set could not be removed.
    NotFirmwareContent,  ///< The archive expanded to something that is not a firmware set.
    DaybreakMissing,     ///< Extracted, but there is no installer to hand off to.
    Cancelled            ///< The user stopped it.
};

/// @brief Which stage a firmware install is in.
/// @since 0.3.0
enum class FirmwareStage
{
    Preflight,    ///< Checking space and inspecting the existing set.
    Downloading,  ///< Fetching the archive.
    Clearing,     ///< Removing the previous set.
    Extracting    ///< Expanding into the firmware directory.
};

/// @brief Progress through a firmware install.
/// @since 0.3.0
struct FirmwareProgress
{
    FirmwareStage stage{FirmwareStage::Preflight};  ///< What is happening.
    std::uint64_t done{};                           ///< Units completed.
    std::uint64_t total{};                          ///< Units expected; zero when unknown.
    std::string detail;                             ///< The current file, when there is one.
};

/// @brief Called as a firmware install proceeds.
/// @return False to cancel.
using FirmwareCallback = std::function<bool(const FirmwareProgress&)>;

/// @brief The result of staging a firmware set.
/// @since 0.3.0
struct FirmwareOutcome
{
    FirmwareResult result{FirmwareResult::DownloadFailed};  ///< How far it got.
    std::size_t filesStaged{};                              ///< NCAs written.
    std::size_t previousFilesRemoved{};                     ///< Files of an old set cleared.

    /// @brief Files in `/firmware/` this application refused to delete.
    /// @details Populated only for @ref FirmwareResult::DirectoryNotOurs, so the
    ///          message can name them instead of saying "something is in the way".
    std::vector<std::string> unexpectedFiles;

    infra::HttpError transport{infra::HttpError::None};  ///< Set only on a network failure.
    std::string detail;                                  ///< One line for the log and the UI.

    /// @brief The path to hand to Daybreak; empty unless ready.
    std::string firmwareDir;

    /// @brief Complete argv for `envSetNextLoad`; empty unless ready.
    std::string daybreakArgs;

    /// @brief Whether the caller may chainload Daybreak now.
    /// @return True only when a verified set is in place.
    [[nodiscard]] bool readyForDaybreak() const { return result == FirmwareResult::Ready; }
};

/// @brief A short English description of a firmware result.
/// @param result The result to describe.
/// @return A description suitable for a log line.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(FirmwareResult result);

/// @brief Download and stage official firmware for Daybreak to install.
///
/// @details **This application never installs firmware.** It downloads a set,
///          verifies it, puts it where Daybreak looks, and hands over. Writing
///          to NAND is Daybreak's job and it does it properly; a second
///          implementation of that would be a second way to brick a console.
///
///          The sequence differs from a CFW pack in one important way: there is
///          no merge and no backup. A firmware set is **replaced whole**,
///          because mixing NCAs from two versions is how Daybreak installs a
///          system that will not boot. So the previous set is cleared rather
///          than displaced.
///
///          That makes clearing the dangerous step, and it is guarded:
///
///          1. `/firmware/` is scanned first. Every entry must be something this
///             application recognises as firmware content.
///          2. Anything else stops the install and is named back to the caller.
///             `/firmware/` is a well-known path this application does not own,
///             and a recursive delete of a directory a user may have put things
///             in is not a risk worth taking to save them one manual step.
///          3. Clearing happens **after** the download verifies. A failed
///             download must never cost the user the firmware they already had.
///
/// @see docs/architecture/firmware-install.md
/// @since 0.3.0
class FirmwareInstallService
{
public:
    /// @brief Construct the service over its ports.
    /// @param http Network access.
    /// @param archives Archive extraction.
    /// @param files Filesystem access.
    /// @param config Paths and limits.
    FirmwareInstallService(HttpGateway& http, ArchiveGateway& archives, FileStore& files,
                           FirmwareConfig config);

    /// @brief Download, verify and stage a firmware set.
    /// @param item The catalogue item; must be of kind `firmware`.
    /// @param onProgress Optional; returning false cancels.
    /// @return How far it got, and what to chainload if it worked.
    [[nodiscard]] FirmwareOutcome stage(const core::CatalogItem& item,
                                        const FirmwareCallback& onProgress = {});

    /// @brief Report what is in the firmware directory without changing it.
    /// @return What was found.
    /// @details Lets the UI warn about an unexpected file before the user starts
    ///          a download that would only stop at the same place.
    [[nodiscard]] core::FirmwareDirectoryScan inspect() const;

    /// @brief Delete a leftover download.
    /// @param itemId The catalogue item id.
    void discardStaging(const std::string& itemId);

    /// @brief The configuration this service was built with.
    /// @return A reference to the configuration.
    [[nodiscard]] const FirmwareConfig& config() const { return m_config; }

private:
    HttpGateway& m_http;
    ArchiveGateway& m_archives;
    FileStore& m_files;
    FirmwareConfig m_config;
};

}  // namespace nsx::domain
