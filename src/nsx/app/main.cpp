// SPDX-License-Identifier: GPL-3.0-only
//
// nsx-manager.nro - the composition root.
//
// The only place that constructs a concrete adapter. Everything above it takes
// ports: the UI knows it has an UpdateService, not that the bytes arrive over
// libcurl or that the card is reached through stdio.
//
// Services (romfs, pl, setsys, set, nifm) are brought up before main by
// userAppInit in platform/system/app_init.cpp - Borealis calls into pl and
// setsys from inside its own initialisation and cannot wait for us.
//
// See docs/architecture/overview.md.

#include <cstdio>
#include <string>

#include <switch.h>

#include "nsx/core/version/version.hpp"
#include "nsx/domain/catalog/catalog_service.hpp"
#include "nsx/domain/cfw/cfw_install_service.hpp"
#include "nsx/domain/cfw/zip_archive_gateway.hpp"
#include "nsx/domain/firmware/firmware_install_service.hpp"
#include "nsx/domain/maintenance/cleanup_service.hpp"
#include "nsx/domain/network/telemetry_service.hpp"
#include "nsx/domain/selfupdate/curl_gateway.hpp"
#include "nsx/domain/selfupdate/sd_file_store.hpp"
#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/domain/sysmodule/sysmodule_service.hpp"
#include "nsx/platform/fs/archive_bit.hpp"
#include "nsx/platform/power/reboot.hpp"
#include "nsx/platform/system/system_info.hpp"
#include "nsx/ui/app_shell/shell.hpp"

namespace {

/// The running version, from the generated header. Never typed anywhere.
nsx::core::SemVer installedVersion()
{
    const nsx::core::Result<nsx::core::SemVer, nsx::core::SemVerError> parsed =
        nsx::core::parseSemVer(nsx::core::version::kString);
    return parsed.hasValue() ? parsed.value() : nsx::core::SemVer{};
}

/// Report a failure that happened before the UI could draw anything.
///
/// Falls back to the console because there is nothing else left: if Borealis
/// could not start, it cannot show why it could not start.
void reportStartupFailure(std::string_view why)
{
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    std::printf("\x1b[2J\x1b[H");
    std::printf("\x1b[31mNSX Manager could not start\x1b[0m\n\n  %.*s\n\n",
                static_cast<int>(why.size()), why.data());
    std::printf("  Press + to exit.\n");

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
}

}  // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // The adapters. Constructed here and nowhere else.
    nsx::domain::SdFileStore files;
    nsx::domain::CurlGateway http;
    nsx::domain::ZipArchiveGateway archives;
    const nsx::domain::SystemClock clock;

    nsx::domain::SelfUpdateConfig updateConfig;
    updateConfig.installed = installedVersion();

    nsx::domain::UpdateService update(http, files, clock, updateConfig);
    nsx::domain::CatalogService catalog(http, files, clock, {});
    nsx::domain::CfwInstallService cfw(http, archives, files, clock, {});
    nsx::domain::FirmwareInstallService firmware(http, archives, files, {});
    nsx::domain::CleanupService cleanup(files, {});
    nsx::domain::SysmoduleService sysmodules(files);
    nsx::domain::TelemetryService telemetry;

    // Platform callbacks for hardware actions
    auto fixArchiveBit = [](const std::function<bool(std::string_view)>& onProgress) {
        const auto rep = nsx::platform::fixAllCommonDirectories(onProgress);
        return std::make_pair(rep.directoriesScanned, rep.directoriesFixed);
    };

    auto rebootToPayload = []() -> bool { return nsx::platform::rebootToPayload().hasValue(); };

    auto querySystemVersions = []() -> std::pair<std::string, std::string> {
        const auto versions = nsx::platform::querySystemVersions();
        return {versions.hosVersion, versions.amsVersion};
    };

    // First run has no forwarder on the card, and staging an update refuses
    // without one. Done before the UI can offer an update, rather than
    // discovered after a download.
    const bool forwarderReady = update.installForwarder();
    update.discardStalePartials();

    // A merge interrupted by a power cut leaves the card part one pack and part
    // another. Undo it before anything else touches the card - including the
    // user, through the UI.
    const nsx::domain::InstallOutcome recovered = cfw.recoverInterruptedMerge();
    if (recovered.result == nsx::domain::InstallResult::MergeFailed) {
        // Reported, not fatal. The application still runs and the message says
        // what to reinstall.
        std::printf("%s\n", recovered.detail.c_str());
    }

    const nsx::ui::ShellServices services{update,        catalog,        cfw,
                                          firmware,      cleanup,        sysmodules,
                                          telemetry,     files,          querySystemVersions,
                                          fixArchiveBit, rebootToPayload};
    const nsx::ui::ShellOutcome outcome = nsx::ui::runShell(services);

    if (outcome.error != nsx::ui::ShellError::None) {
        reportStartupFailure(nsx::ui::describe(outcome.error));
        return 1;
    }

    // The handoff, after the UI is fully torn down and its background worker
    // joined. Only ever to a path that exists: chainloading a missing one drops
    // the user back to hbmenu with no explanation.
    if (outcome.chainloads() && (forwarderReady || files.exists(outcome.chainloadPath))) {
        if (R_FAILED(
                envSetNextLoad(outcome.chainloadPath.c_str(), outcome.chainloadArgs.c_str()))) {
            reportStartupFailure("Could not hand off execution to target. Run from hbmenu.");
            return 1;
        }
    }

    return 0;
}
