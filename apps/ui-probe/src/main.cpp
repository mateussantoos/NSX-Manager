// SPDX-License-Identifier: GPL-3.0-only
//
// nsx-ui-probe.nro - does Borealis actually work on this console?
//
// A throwaway binary with one job: boot the shell and nothing else. It exists
// because three things can fail silently between "the build succeeded" and "the
// interface appears" - the portlib link line, the romfs resource staging, and
// the font loading - and none of them is visible from a green build.
//
// Kept SEPARATE from nsx-manager.nro on purpose. The application is the binary
// the self-update path is verified against on hardware; replacing its entry
// point to try out a UI would take that away for as long as the UI is unproven.
// When the shell is real this binary goes away and main.cpp calls runShell().
//
// Not published: the release workflow uploads by explicit name
// (dist/nsx-manager-*.nro, dist/nsx-forwarder-*.nro), which this never matches.

#include <cstdio>

#include <switch.h>

#include "nsx/ui/app_shell/shell.hpp"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    if (R_FAILED(romfsInit())) {
        // No console is initialised at this point, so there is nowhere to print
        // that would be seen. Returning non-zero is the whole report.
        return 1;
    }

    const nsx::ui::ShellError result = nsx::ui::runShell();

    romfsExit();

    if (result == nsx::ui::ShellError::None) {
        return 0;
    }

    // Borealis failed before it could draw anything, so fall back to the
    // console to say why rather than returning to hbmenu in silence.
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    const std::string_view why = nsx::ui::describe(result);
    std::printf("\x1b[2J\x1b[H");
    std::printf("\x1b[31mNSX UI probe failed\x1b[0m\n\n  %.*s\n\n",
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
    return 1;
}
