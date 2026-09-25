// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

#include "nsx/domain/catalog/catalog_service.hpp"
#include "nsx/domain/cfw/cfw_install_service.hpp"
#include "nsx/domain/firmware/firmware_install_service.hpp"
#include "nsx/domain/maintenance/cleanup_service.hpp"
#include "nsx/domain/network/telemetry_service.hpp"
#include "nsx/domain/ports/ports.hpp"
#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/domain/sysmodule/sysmodule_service.hpp"
#include "nsx/ui/tabs/home_tab.hpp"
#include "nsx/ui/tabs/tools_tab.hpp"

/// @brief Borealis views. The only layer allowed to know the UI framework exists.
namespace nsx::ui {

/// @brief Why the shell could not start.
/// @since 0.2.0
enum class ShellError
{
    None,          ///< It started.
    RomfsMissing,  ///< The fonts Borealis draws with are not in romfs.
    BorealisInit   ///< Borealis refused to initialise; see its log.
};

/// @brief A short English description of a startup failure.
/// @param error The error to describe.
/// @return A description suitable for a log line or a console fallback.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(ShellError error);

/// @brief The use-cases the shell drives.
///
/// @details References, not values. The concrete adapters behind them are built
///          in `app/main.cpp` and nowhere else - the composition root is the
///          only place allowed to know that an `HttpGateway` is really libcurl.
/// @since 0.3.0
struct ShellServices
{
    domain::UpdateService& update;              ///< The application's own updates.
    domain::CatalogService& catalog;            ///< The content catalogue.
    domain::CfwInstallService& cfw;             ///< CFW pack installs.
    domain::FirmwareInstallService& firmware;   ///< Official firmware.
    domain::CleanupService& cleanup;            ///< Maintenance cleanup service.
    domain::SysmoduleService& sysmodules;       ///< Sysmodule manager.
    domain::TelemetryService& telemetry;        ///< Telemetry protection checker.
    domain::FileStore& files;                   ///< FileStore for storage queries.
    SystemVersionsQuery querySystemVersions{};  ///< Horizon OS & Atmosphere version query.
    FixArchiveBitCallback fixArchiveBit{};      ///< Platform archive bit repair.
    RebootCallback rebootToPayload{};           ///< Platform reboot to payload.
};

/// @brief What the shell wants to happen after it closes.
/// @since 0.3.0
struct ShellOutcome
{
    ShellError error{ShellError::None};  ///< Why it did not start, if it did not.

    /// @brief Binary to chainload, or empty to return to hbmenu.
    std::string chainloadPath;

    /// @brief Complete argv for `envSetNextLoad`; empty unless chainloading.
    std::string chainloadArgs;

    /// @brief Whether the caller should chainload something.
    /// @return True when a path was set.
    [[nodiscard]] bool chainloads() const { return !chainloadPath.empty(); }
};

/// @brief Whether the resources Borealis draws with are actually present.
///
/// @details Borealis resolves its fonts through `BOREALIS_ASSET()`, which this
///          build defines as `romfs:/`, and it does **not** fail when one is
///          missing - `loadFont` returns -1, `Application::init` still
///          succeeds, and the console renders a black screen with no
///          indication why. Checking first turns that into a message.
///
/// @return True when every required resource is readable.
/// @since 0.2.0
[[nodiscard]] bool resourcesPresent();

/// @brief Start Borealis, show the shell, and run until the user exits.
///
/// @details Blocks for the lifetime of the UI.
///
///          Long work does **not** run here. Anything slow - a download, an
///          extraction, a merge - goes to a @ref BackgroundJob, because
///          Borealis runs every task on the UI thread and would otherwise
///          freeze for the duration. See `widgets/background_job.hpp`.
///
/// @param services The use-cases to drive.
/// @return What to do after the UI closes.
/// @since 0.2.0
[[nodiscard]] ShellOutcome runShell(const ShellServices& services);

}  // namespace nsx::ui
