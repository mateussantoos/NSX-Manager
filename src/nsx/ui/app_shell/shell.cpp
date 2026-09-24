// SPDX-License-Identifier: GPL-3.0-only
//
// The Borealis shell.
//
// See ADR-0010 for why this fork, and docs/architecture/overview.md for the
// startup sequence.
//
// The services this needs - pl, setsys, set, romfs - are brought up before main
// by userAppInit in platform/system/app_init.cpp. Borealis calls into them from
// inside Application::init and cannot wait for us to do it afterwards.

#include "nsx/ui/app_shell/shell.hpp"

#include <cstdio>

#include <borealis.hpp>

#include "nsx/core/version/version.hpp"
#include "nsx/ui/tabs/update_tab.hpp"

namespace nsx::ui {

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

/// A tab for a use-case whose data source does not exist yet.
///
/// The install services behind these are complete and tested; what is missing
/// is the catalogue FETCH. `core/catalog` parses a catalogue and nothing
/// downloads one. An empty list would say "there is nothing to install", which
/// is a different claim and a false one.
brls::View* pendingTab(const std::string& title, const std::string& because)
{
    auto* list = new brls::List();

    auto* header = new brls::ListItem(title);
    header->setValue("nsx/state/unavailable"_i18n);
    list->addView(header);

    list->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, because, true));
    return list;
}

brls::View* buildSystemTab()
{
    auto* list = new brls::List();

    auto* version = new brls::ListItem("nsx/about/version_label"_i18n);
    version->setValue(std::string(core::version::kString));
    list->addView(version);

    auto* build = new brls::ListItem("nsx/about/build_label"_i18n);
    build->setValue(std::string(core::version::kGitSha));
    list->addView(build);

    auto* built = new brls::ListItem("nsx/about/date_label"_i18n);
    built->setValue(std::string(core::version::kBuildDate));
    list->addView(built);

    list->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "nsx/about/licence"_i18n, true));
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

ShellOutcome runShell(const ShellServices& services)
{
    ShellOutcome outcome;

    // Checked before init, not after. Borealis treats a missing font as a
    // loadFont() of -1 and carries on, so by the time anything looks wrong the
    // only symptom is a black screen.
    if (!resourcesPresent()) {
        outcome.error = ShellError::RomfsMissing;
        return outcome;
    }

    brls::Logger::setLogLevel(brls::LogLevel::INFO);
    brls::i18n::loadTranslations();

    if (!brls::Application::init("nsx/name"_i18n)) {
        outcome.error = ShellError::BorealisInit;
        return outcome;
    }

    auto* root = new brls::TabFrame();
    root->setTitle("nsx/name"_i18n);
    root->setIcon(BOREALIS_ASSET("images/logo.png"));
    root->setFooterText(std::string(core::version::kString));

    // Quit first, then chainload. Application::quit() ends mainLoop, and doing
    // the envSetNextLoad after it returns means the UI is fully torn down - and
    // the background worker joined - before the next binary starts.
    auto* updates = new UpdateTab(services.update,
                                  [&outcome](const std::string& path, const std::string& args) {
                                      outcome.chainloadPath = path;
                                      outcome.chainloadArgs = args;
                                      brls::Application::quit();
                                  });

    root->addTab("update/title"_i18n, updates);
    root->addSeparator();
    root->addTab("nsx/tabs/cfw"_i18n, pendingTab("nsx/tabs/cfw"_i18n, "nsx/pending/catalog"_i18n));
    root->addTab("nsx/tabs/firmware"_i18n,
                 pendingTab("nsx/tabs/firmware"_i18n, "nsx/pending/catalog"_i18n));
    root->addSeparator();
    root->addTab("nsx/tabs/system"_i18n, buildSystemTab());

    // Borealis owns these from here; they are freed when the application shuts
    // down or the view is popped.
    brls::Application::pushView(root);

    while (brls::Application::mainLoop()) {
        // Borealis drives everything from inside mainLoop.
    }

    // Held for the tabs that are not wired yet. Naming them here keeps the
    // composition root's shape final, so landing the catalogue fetch touches
    // this file and nothing else.
    (void)services.cfw;
    (void)services.firmware;

    return outcome;
}

}  // namespace nsx::ui
