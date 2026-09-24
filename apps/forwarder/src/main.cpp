// SPDX-License-Identifier: GPL-3.0-only
//
// nsx-forwarder.nro - the second binary, and the reason a failed update is
// never fatal.
//
// A running .nro cannot overwrite its own file, so the application stages the
// new build, chainloads this, and this performs the swap. It also appears in
// hbmenu as "NSX Manager (Repair)" so there is always something launchable.
//
// This is the baseline: it resolves and reports the handoff. The swap state
// machine lands with nsx::core::Handoff, which both binaries will share - the
// predecessor had two divergent ad-hoc parsers for the same file
// (utils.cpp:555-575 and app-forwarder/source/main.cpp:26-46).
//
// See docs/architecture/self-update-forwarder.md and ADR-0007.

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

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

/// Hand control to the application. Called whether or not a swap happened: if
/// there is nothing to do, the forwarder's job is to get out of the way.
void chainload(const char* path)
{
    if (fileExists(path)) {
        const std::string quoted = std::string("\"") + path + "\"";
        envSetNextLoad(path, quoted.c_str());
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
    const bool havePending = fileExists(handoffPath.c_str());
    const bool haveTarget = fileExists(kTargetNro);

    using nsx::core::version::kString;

    std::printf("\x1b[2J\x1b[H");
    std::printf("\x1b[36mNSX Manager (Repair)\x1b[0m %.*s\n\n", static_cast<int>(kString.size()),
                kString.data());
    std::printf("  handoff : %s\n", handoffPath.c_str());
    std::printf("            %s\n", havePending ? "present" : "none - nothing staged");
    std::printf("  target  : %s\n", haveTarget ? "present" : "\x1b[31mMISSING\x1b[0m");

    if (havePending) {
        // The swap sequence is deliberately not implemented yet: doing it
        // without the shared Handoff parser and its rollback state machine
        // would risk the exact failure this binary exists to prevent.
        std::printf("\n  A staged update was found.\n");
        std::printf("  The swap is not implemented in this build.\n");
    }
    else if (!haveTarget) {
        std::printf("\n  \x1b[31mThe application is missing and nothing is staged.\x1b[0m\n");
        std::printf("  Reinstall from the release zip.\n");
    }
    else {
        std::printf("\n  Nothing to repair.\n");
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

    if (launch) {
        chainload(kTargetNro);
    }
    return 0;
}
