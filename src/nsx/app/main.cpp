// SPDX-License-Identifier: GPL-3.0-only
//
// Baseline entry point for nsx-manager.nro.
//
// Deliberately console-only for now. The Borealis shell arrives with the UI
// layer; what this proves today is the whole chain underneath it - devkitA64,
// libnx, romfs, the generated version header, the embedded CA bundle, and the
// core library - by booting on hardware and reporting what it was built from.
//
// See docs/architecture/overview.md for the startup sequence this grows into.

#include <cstdio>
#include <memory>
#include <string>

#include <switch.h>

#include "nsx/core/hash/sha256.hpp"
#include "nsx/core/update/update_policy.hpp"
#include "nsx/core/version/version.hpp"
#include "nsx/domain/selfupdate/curl_gateway.hpp"
#include "nsx/domain/selfupdate/sd_file_store.hpp"
#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/infra/http/ca_bundle.hpp"

namespace {

/// Services this application needs. Ordering matters: romfs cannot mount before
/// the filesystem is up.
bool initialiseServices()
{
    if (R_FAILED(romfsInit())) {
        return false;
    }
    if (R_FAILED(setsysInitialize())) {
        return false;
    }
    if (R_FAILED(nifmInitialize(NifmServiceType_User))) {
        return false;
    }
    return true;
}

void shutdownServices()
{
    nifmExit();
    setsysExit();
    romfsExit();
}

/// A self-check that the build is internally consistent. Cheap, and it fails
/// loudly on a device rather than silently producing wrong results later.
bool selfTest()
{
    // NOT `using namespace nsx::core` here. libnx declares a global `Result`
    // (the HOS status code) in <switch.h>, so the unqualified name is ambiguous
    // in any translation unit that includes both. Every file in platform/,
    // infra/ and app/ will hit this - qualify, do not import.
    using nsx::core::parseSemVer;
    using nsx::core::Sha256;

    // The hash the whole update path depends on.
    if (Sha256::hexOf("abc") !=
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
        return false;
    }

    // The version string must parse as the SemVer it claims to be.
    const nsx::core::Result<nsx::core::SemVer, nsx::core::SemVerError> self =
        parseSemVer(nsx::core::version::kString);
    if (!self) {
        return false;
    }

    // The trust anchor must actually be compiled in.
    if (nsx::infra::kCaBundleCertCount <= 0 || nsx::infra::kCaBundlePem.empty()) {
        return false;
    }

    return true;
}

/// Build the update service the way the composition root should: concrete
/// adapters constructed here and nowhere else, the running version taken from
/// the generated header rather than typed anywhere.
nsx::domain::SelfUpdateConfig updateConfig()
{
    nsx::domain::SelfUpdateConfig config;
    const nsx::core::Result<nsx::core::SemVer, nsx::core::SemVerError> self =
        nsx::core::parseSemVer(nsx::core::version::kString);
    if (self) {
        config.installed = self.value();
    }
    return config;
}

/// Block until the user answers, redrawing while we wait.
///
/// Returns false if the applet is closing, which must read as "no" - a system
/// that is tearing the process down is not consent to start a download.
bool confirm(PadState& pad)
{
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_A) {
            return true;
        }
        if (down & HidNpadButton_B) {
            return false;
        }
        consoleUpdate(nullptr);
    }
    return false;
}

/// Draw download progress on one line, and let the user out of it.
///
/// Returning false aborts the transfer. The console is redrawn from in here
/// because the transfer owns the thread for its whole duration - without this
/// the screen would freeze on the last frame drawn before the download began.
nsx::infra::ProgressCallback progressPrinter(PadState& pad)
{
    auto lastShown = std::make_shared<int>(-1);

    return [&pad, lastShown](const nsx::infra::Progress& p) {
        if (!appletMainLoop()) {
            return false;
        }

        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_B) {
            std::printf("\n  cancelled\n");
            return false;
        }

        // Only redraw when the whole-percent figure changes: curl calls this
        // far more often than the screen can usefully change.
        const int percent = p.total > 0 ? static_cast<int>((p.received * 100U) / p.total) : -1;
        if (percent != *lastShown) {
            *lastShown = percent;
            if (percent >= 0) {
                std::printf("\r  downloading: %3d%%  (%llu KB)   ", percent,
                            static_cast<unsigned long long>(p.received / 1024U));
            }
            else {
                std::printf("\r  downloading: %llu KB   ",
                            static_cast<unsigned long long>(p.received / 1024U));
            }
            consoleUpdate(nullptr);
        }
        return true;
    };
}

/// Check, offer, stage, chainload.
///
/// @return The forwarder argv to hand to envSetNextLoad, or empty when there is
///         nothing to launch. Returning it rather than calling envSetNextLoad
///         here keeps the chainload in main(), after the services this function
///         used have been shut down.
std::string runUpdateFlow(PadState& pad)
{
    nsx::domain::SdFileStore files;
    nsx::domain::CurlGateway http;
    const nsx::domain::SystemClock clock;

    nsx::domain::UpdateService service(http, files, clock, updateConfig());

    std::printf("\n  Checking for updates...\n");
    consoleUpdate(nullptr);

    const nsx::domain::CheckOutcome outcome = service.check();

    const std::string_view action = nsx::core::describe(outcome.decision.action);
    std::printf("  %.*s\n", static_cast<int>(action.size()), action.data());
    if (!outcome.detail.empty()) {
        std::printf("  %s\n", outcome.detail.c_str());
    }

    if (!outcome.offersUpdate() || !outcome.manifest.has_value()) {
        return {};
    }

    std::printf("\n  Install %s now? (A = yes, B = no)\n",
                outcome.manifest->version.toString().c_str());
    consoleUpdate(nullptr);

    if (!confirm(pad)) {
        std::printf("  Not installed.\n");
        return {};
    }

    const nsx::domain::StageOutcome staged = service.stage(*outcome.manifest, progressPrinter(pad));

    std::printf("\n");
    if (!staged.readyToChainload()) {
        const std::string_view why = nsx::domain::describe(staged.result);
        std::printf("  \x1b[31m%.*s\x1b[0m\n", static_cast<int>(why.size()), why.data());
        if (!staged.detail.empty()) {
            std::printf("  %s\n", staged.detail.c_str());
        }
        std::printf("  Your current version is untouched.\n");
        return {};
    }

    std::printf("  \x1b[32m%s\x1b[0m\n", staged.detail.c_str());
    std::printf("  Launching the updater...\n");
    consoleUpdate(nullptr);
    return staged.forwarderArgs;
}

}  // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    const bool services = initialiseServices();
    const bool healthy = selfTest();

    using nsx::core::version::kBuildDate;
    using nsx::core::version::kGitSha;
    using nsx::core::version::kString;

    std::printf("\x1b[2J\x1b[H");
    std::printf("\x1b[36mNSX Manager\x1b[0m %.*s\n", static_cast<int>(kString.size()),
                kString.data());
    std::printf("  built %.*s from %.*s\n\n", static_cast<int>(kBuildDate.size()),
                kBuildDate.data(), static_cast<int>(kGitSha.size()), kGitSha.data());

    std::printf("  services   : %s\n", services ? "\x1b[32mok\x1b[0m" : "\x1b[31mFAILED\x1b[0m");
    std::printf("  self test  : %s\n", healthy ? "\x1b[32mok\x1b[0m" : "\x1b[31mFAILED\x1b[0m");
    std::printf("  CA bundle  : %d roots\n", nsx::infra::kCaBundleCertCount);

    NifmInternetConnectionStatus status{};
    if (services && R_SUCCEEDED(nifmGetInternetConnectionStatus(nullptr, nullptr, &status))) {
        std::printf("  network    : %s\n", status == NifmInternetConnectionStatus_Connected
                                               ? "connected"
                                               : "not connected");
    }

    // First run has neither forwarder copy - an unzipped installation contains
    // only the application. Do this before offering an update, because staging
    // one refuses to write a handoff without something able to act on it.
    bool forwarderReady = false;
    {
        nsx::domain::SdFileStore files;
        nsx::domain::CurlGateway http;
        const nsx::domain::SystemClock clock;
        nsx::domain::UpdateService service(http, files, clock, updateConfig());

        forwarderReady = service.installForwarder();

        // An interrupted download is unverified and nothing resumes it. The
        // staged binary and the handoff are deliberately left alone: they
        // describe an update still in flight.
        service.discardStalePartials();
    }
    std::printf("  forwarder  : %s\n",
                forwarderReady ? "\x1b[32mok\x1b[0m" : "\x1b[31mMISSING\x1b[0m");

    std::printf("\n  Press Y to check for updates, + to exit.\n");

    std::string chainload;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus) {
            break;
        }
        if (down & HidNpadButton_Y) {
            chainload = runUpdateFlow(pad);
            if (!chainload.empty()) {
                break;
            }
            std::printf("\n  Press Y to check again, + to exit.\n");
        }
        consoleUpdate(nullptr);
    }

    if (services) {
        shutdownServices();
    }
    consoleExit(nullptr);

    // The handoff is written and the staged binary is verified. Hand over to
    // the forwarder, which performs the swap this process cannot perform on
    // its own file. Only ever to a path that exists - chainloading a missing
    // one drops the user back to hbmenu with no explanation.
    if (!chainload.empty()) {
        const nsx::domain::SelfUpdateConfig config = updateConfig();
        if (R_FAILED(envSetNextLoad(config.forwarderNro.c_str(), chainload.c_str()))) {
            // Almost always "not launched from hbmenu", where there is no next
            // load to set. The update is staged and will be applied the next
            // time the repair entry runs, so say that rather than failing.
            std::printf("Could not launch the updater automatically.\n");
            std::printf("Run \"NSX Manager (Repair)\" from hbmenu to finish.\n");
            return 1;
        }
    }

    return healthy ? 0 : 1;
}
