// SPDX-License-Identifier: GPL-3.0-only
//
// nsx-forwarder.nro - the second binary, and the reason a failed update is
// never fatal.
//
// A running .nro cannot overwrite its own file, so the application stages the
// new build, chainloads this, and this performs the swap. It also appears in
// hbmenu as "NSX Manager (Repair)" so there is always something launchable.
//
// The decision of what to do comes from nsx::core::decideRecovery, which is
// pure and host-tested against every row of the recovery table. This binary
// only carries it out - see swap.cpp.
//
// See docs/architecture/self-update-forwarder.md and ADR-0007.

#include <cstdio>
#include <string>
#include <string_view>

#include "swap.hpp"
#include <switch.h>

#include "nsx/core/version/version.hpp"

namespace {

#ifndef NSX_HANDOFF_PATH
#define NSX_HANDOFF_PATH "/config/nsx-manager/staging/handoff.json"
#endif

constexpr std::string_view kHandoffFlag = "--handoff=";
constexpr const char* kTargetNro = "/switch/nsx-manager/nsx-manager.nro";

/// Resolve the handoff path from argv, falling back to the compile-time
/// absolute path.
///
/// **Never relative.** The predecessor opened `"forwarder.conf"` relative to
/// the working directory (app-forwarder/source/main.cpp:28). That worked only
/// because hbmenu happens to chdir into the NRO's directory, and when the open
/// failed there was no error path at all - every key silently became an empty
/// string, which was then handed to rename() and remove().
std::string resolveHandoffPath(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] ? argv[i] : "";
        if (arg.size() > kHandoffFlag.size() &&
            arg.compare(0, kHandoffFlag.size(), kHandoffFlag) == 0) {
            const std::string_view value = arg.substr(kHandoffFlag.size());
            if (!value.empty() && value.front() == '/') {
                return std::string(value);
            }
        }
    }
    return NSX_HANDOFF_PATH;
}

bool fileExists(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return true;
}

void printOutcome(const nsx::forwarder::SwapOutcome& outcome)
{
    const std::string_view text = nsx::forwarder::describe(outcome.result);
    const bool bad = outcome.result == nsx::forwarder::SwapResult::IoFailed ||
                     outcome.result == nsx::forwarder::SwapResult::DigestMismatch ||
                     outcome.result == nsx::forwarder::SwapResult::Unrecoverable;

    std::printf("  result  : %s%.*s\x1b[0m\n", bad ? "\x1b[31m" : "\x1b[32m",
                static_cast<int>(text.size()), text.data());
    if (!outcome.detail.empty()) {
        std::printf("            %s\n", outcome.detail.c_str());
    }
}

}  // namespace

int main(int argc, char** argv)
{
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    const std::string handoffPath = resolveHandoffPath(argc, argv);

    using nsx::core::version::kString;

    std::printf("\x1b[2J\x1b[H");
    std::printf("\x1b[36mNSX Manager (Repair)\x1b[0m %.*s\n\n", static_cast<int>(kString.size()),
                kString.data());
    std::printf("  handoff : %s\n",
                fileExists(handoffPath.c_str()) ? "present" : "none - nothing staged");

    const nsx::forwarder::SwapOutcome outcome = nsx::forwarder::runSwap(handoffPath);
    printOutcome(outcome);

    const bool haveTarget = fileExists(kTargetNro);
    std::printf("  target  : %s\n", haveTarget ? "present" : "\x1b[31mMISSING\x1b[0m");

    if (!outcome.applicationUsable() || !haveTarget) {
        std::printf("\n  \x1b[31mNSX Manager could not be repaired automatically.\x1b[0m\n");
        std::printf("  Reinstall from the release zip - your settings are not affected.\n");
    }

    std::printf("\n  Press A to launch NSX Manager, + to exit.\n");

    bool launch = false;
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_A) {
            launch = true;
            break;
        }
        if (down & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(nullptr);
    }

    consoleExit(nullptr);

    // Hand control back to the application. Only ever to a file that exists:
    // chainloading a missing path would drop the user back to hbmenu with no
    // explanation.
    if (launch && haveTarget) {
        const std::string quoted = std::string("\"") + kTargetNro + "\"";
        envSetNextLoad(kTargetNro, quoted.c_str());
    }
    return 0;
}
