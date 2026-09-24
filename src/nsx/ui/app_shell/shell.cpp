// SPDX-License-Identifier: GPL-3.0-only
//
// The Borealis shell.
//
// What this file is for right now: proving that Borealis links against the
// devkitPro portlibs, finds its resources in romfs, and draws a frame on
// hardware. Those are three separate ways for a UI to fail silently, and none
// of them is visible from a successful build.
//
// See ADR-0010 for why this fork, and docs/architecture/overview.md for the
// shell this grows into.

#include "nsx/ui/app_shell/shell.hpp"

#include <cstdio>

#include <borealis.hpp>

#include "nsx/core/version/version.hpp"

namespace nsx::ui {

// The `_i18n` literal, and only that. A blanket `using namespace brls` in a
// translation unit that also sees <switch.h> would be asking for the same
// ambiguity libnx's global `Result` already causes elsewhere.
using namespace brls::i18n::literals;

namespace {

/// Loaded by hard-coded path inside Borealis (library/lib/application.cpp:366,
/// :377). Listed again here because a missing one is not an error there.
constexpr const char* kRequiredResources[] = {
    BOREALIS_ASSET("inter/Inter-Switch.ttf"),
    BOREALIS_ASSET("material/MaterialIcons-Regular.ttf"),
    BOREALIS_ASSET("i18n/en-US/brls.json"),
};

bool readable(const char* path)
{
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return true;
}

/// The placeholder content view.
///
/// A label rather than nothing, because "the screen is blank" and "the screen
/// drew what it was asked to" are the two outcomes this whole exercise is
/// trying to tell apart. If this text appears on a console, the link line, the
/// romfs staging, the font loading and the i18n lookup all worked.
brls::View* buildPlaceholder()
{
    auto* list = new brls::List();

    auto* status = new brls::ListItem("nsx/name"_i18n);
    status->setValue(std::string(core::version::kString));
    list->addView(status);

    auto* built = new brls::ListItem("Build");
    built->setValue(std::string(core::version::kGitSha));
    list->addView(built);

    auto* note =
        new brls::Label(brls::LabelStyle::DESCRIPTION,
                        "Borealis is running. The shell, the tabs and the update view are not "
                        "built yet - this frame exists to prove the framework boots on hardware.",
                        true);
    list->addView(note);

    return list;
}

}  // namespace

std::string_view describe(ShellError error)
{
    switch (error) {
        case ShellError::None:
            return "the shell ran";
        case ShellError::RomfsMissing:
            return "the interface resources are missing from this build";
        case ShellError::BorealisInit:
            return "the interface could not be initialised";
    }
    return "unknown";
}

bool resourcesPresent()
{
    for (const char* path : kRequiredResources) {
        if (!readable(path)) {
            return false;
        }
    }
    return true;
}

ShellError runShell()
{
    // Checked before init, not after. Borealis treats a missing font as a
    // loadFont() of -1 and carries on, so by the time anything looks wrong the
    // only symptom is a black screen.
    if (!resourcesPresent()) {
        return ShellError::RomfsMissing;
    }

    brls::Logger::setLogLevel(brls::LogLevel::INFO);

    // Before init: Application::init takes an already-translated title.
    brls::i18n::loadTranslations();

    if (!brls::Application::init("nsx/name"_i18n)) {
        return ShellError::BorealisInit;
    }

    auto* frame = new brls::AppletFrame(true, true);
    frame->setTitle("nsx/name"_i18n);
    frame->setFooterText(std::string(core::version::kString));
    frame->setContentView(buildPlaceholder());

    // Borealis owns this pointer from here; it is freed when the view is
    // popped or the application shuts down.
    brls::Application::pushView(frame);

    while (brls::Application::mainLoop()) {
        // Borealis drives everything from inside mainLoop.
    }

    return ShellError::None;
}

}  // namespace nsx::ui
