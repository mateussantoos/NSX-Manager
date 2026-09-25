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
#include "nsx/ui/tabs/cfw_tab.hpp"
#include "nsx/ui/tabs/firmware_tab.hpp"
#include "nsx/ui/tabs/home_tab.hpp"
#include "nsx/ui/tabs/settings_tab.hpp"
#include "nsx/ui/tabs/tools_tab.hpp"
#include "nsx/ui/tabs/update_tab.hpp"
#include "nsx/ui/theme/dark_theme.hpp"

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

    auto* themeWrapper = new brls::LibraryViewsThemeVariantsWrapper(new DarkMinimalistTheme(),
                                                                    new DarkMinimalistTheme());
    if (!brls::Application::init("nsx/name"_i18n, nullptr, themeWrapper)) {
        outcome.error = ShellError::BorealisInit;
        return outcome;
    }

    auto* style = brls::Application::getStyle();
    style->Sidebar.width = 280;
    style->Sidebar.marginLeft = 16;
    style->Sidebar.marginRight = 16;
    style->Sidebar.marginTop = 30;
    style->Sidebar.marginBottom = 30;
    style->Sidebar.Item.height = 50;
    style->Sidebar.Item.textSize = 17;
    style->Sidebar.Item.textOffsetX = 44;

    auto* root = new brls::TabFrame();
    root->setTitle("nsx/name"_i18n);
    root->setIcon(BOREALIS_ASSET("images/logo.png"));
    root->setFooterText(std::string(core::version::kString));
    if (root->sidebar) {
        root->sidebar->setWidth(280);
        root->sidebar->setMargins(30, 16, 30, 16);
    }

    // Quit first, then chainload. Application::quit() ends mainLoop, and doing
    // the envSetNextLoad after it returns means the UI is fully torn down - and
    // the background worker joined - before the next binary starts.
    auto* updates = new UpdateTab(services.update,
                                  [&outcome](const std::string& path, const std::string& args) {
                                      outcome.chainloadPath = path;
                                      outcome.chainloadArgs = args;
                                      brls::Application::quit();
                                  });

    auto onSelectTab = [root](int tabIndex) {
        if (!root || !root->sidebar) {
            return;
        }
        int currentItemIndex = 0;
        for (size_t i = 0; i < root->sidebar->getViewsCount(); ++i) {
            brls::View* child = root->sidebar->getChild(i);
            auto* item = dynamic_cast<brls::SidebarItem*>(child);
            if (item) {
                if (currentItemIndex == tabIndex) {
                    brls::Application::giveFocus(item);
                    item->onClick();
                    break;
                }
                currentItemIndex++;
            }
        }
    };

    root->addTab("nsx/tabs/home"_i18n,
                 new HomeTab(services.update, services.telemetry, services.files,
                             services.querySystemOverview, onSelectTab),
                 brls::SidebarIcon::Home);
    root->addSeparator();
    root->addTab("update/title"_i18n, updates, brls::SidebarIcon::Update);
    root->addTab("nsx/tabs/cfw"_i18n, new CfwTab(services.catalog, services.cfw),
                 brls::SidebarIcon::Atmosphere);
    root->addTab("nsx/tabs/firmware"_i18n,
                 new FirmwareTab(services.catalog, services.firmware,
                                 [&outcome](const std::string& path, const std::string& args) {
                                     outcome.chainloadPath = path;
                                     outcome.chainloadArgs = args;
                                     brls::Application::quit();
                                 }),
                 brls::SidebarIcon::Firmware);
    root->addTab("nsx/tabs/tools"_i18n,
                 new ToolsTab(services.cleanup, services.sysmodules, services.telemetry,
                              services.fixArchiveBit, services.rebootToPayload),
                 brls::SidebarIcon::Tools);
    root->addSeparator();
    root->addTab("nsx/tabs/settings"_i18n, new SettingsTab(services.update, services.catalog),
                 brls::SidebarIcon::Settings);

    // Borealis owns these from here; they are freed when the application shuts
    // down or the view is popped.
    brls::Application::pushView(root);

    while (brls::Application::mainLoop()) {
        // Borealis drives everything from inside mainLoop.
    }

    return outcome;
}

}  // namespace nsx::ui
