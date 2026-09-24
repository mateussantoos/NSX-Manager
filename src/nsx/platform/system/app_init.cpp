// SPDX-License-Identifier: GPL-3.0-only
//
// The services this application requires, brought up before main().
//
// WHY THIS FILE EXISTS, AND WHY IT IS NOT IN A LIBRARY
// ----------------------------------------------------
// libnx calls `userAppInit()` from its startup code, before main. It provides a
// weak, empty default, and a program overrides it by defining its own.
//
// Borealis ships such an override in `library/lib/switch_wrapper.c`, and its
// Application::init() DEPENDS on it: init calls plGetSharedFontByType() and
// setsysGetColorSetId() directly, and neither `pl` nor `setsys` is initialised
// anywhere inside Borealis itself.
//
// That works when Borealis is built the way upstream builds it - a devkitPro
// Makefile linking every object directly. It does NOT work when the same
// sources are compiled into a static library, which is what this project does
// (ADR-0002). A static library only contributes an object file when the link
// needs a symbol from it, and nothing in our code references `userAppInit`. So
// `switch_wrapper.o` sat unused inside libnsx_borealis.a, libnx's empty default
// was used instead, and the first call to plGetSharedFontByType() hit an
// uninitialised service and took the process down with a system error dialog -
// before any frame, log line or fallback message could appear.
//
// So the override lives HERE, in first-party code, and
// `nsx_add_switch_services()` adds it directly to each NRO target's own
// sources. Never through a library, or it would be dropped exactly the same
// way. `tools/lint/check_app_init.sh` fails the build if a linked binary loses
// it again.
//
// Defining it ourselves is also the honest arrangement: which services this
// application needs is our decision, not a side effect of a vendored file we do
// not control.

#include <switch.h>
#include <unistd.h>

namespace {

/// The nxlink socket, so stdout reaches a host running `nxlink -s`. -1 when
/// there is no network, which is normal and not an error.
int g_nxlinkSocket = -1;

}  // namespace

extern "C" void userAppInit()
{
    // romfs: fonts, translations, the forwarder and the RCM payload all live
    // inside the NRO. Nothing that reads romfs:/ works before this.
    romfsInit();

    // setsys: Borealis reads the console's light/dark theme through
    // setsysGetColorSetId() inside Application::init.
    setsysInitialize();

    // pl: the shared system fonts. Borealis calls plGetSharedFontByType() for
    // the standard face and every CJK fallback. THIS is the one whose absence
    // crashed the UI probe.
    plInitialize(PlServiceType_User);

    // set: the console's configured language, which decides the locale Borealis
    // loads from romfs:/i18n/.
    setInitialize();

    // nifm: whether there is an internet connection. The update check reports
    // "no network" rather than attempting a request and timing out.
    nifmInitialize(NifmServiceType_User);

    // Sockets, and stdout over the network. Both are best effort: a console
    // with no network must still start, so neither result is checked.
    socketInitializeDefault();
    g_nxlinkSocket = nxlinkStdio();
}

extern "C" void userAppExit()
{
    if (g_nxlinkSocket != -1) {
        close(g_nxlinkSocket);
        g_nxlinkSocket = -1;
    }

    // Reverse order of userAppInit.
    socketExit();
    nifmExit();
    setExit();
    plExit();
    setsysExit();
    romfsExit();
}
