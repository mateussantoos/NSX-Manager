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

#include <switch.h>

#include "nsx/core/hash/sha256.hpp"
#include "nsx/core/update/update_policy.hpp"
#include "nsx/core/version/version.hpp"
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

    std::printf("\n  The update pipeline is not wired to the UI yet.\n");
    std::printf("  Press + to exit.\n");

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(nullptr);
    }

    if (services) {
        shutdownServices();
    }
    consoleExit(nullptr);
    return healthy ? 0 : 1;
}
