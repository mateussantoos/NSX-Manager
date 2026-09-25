// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/settings_tab.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "nsx/core/version/version.hpp"

namespace nsx::ui {

using namespace brls::i18n::literals;

SettingsTab::SettingsTab(domain::UpdateService& update, domain::CatalogService& catalog)
    : m_update(update), m_catalog(catalog)
{
    setupAboutSection();
    setupNetworkSection();
    setupPreferencesSection();
    setupLegalSection();
}

void SettingsTab::setupAboutSection()
{
    addView(new brls::Header("settings/about/header"_i18n));

    auto* version = new brls::ListItem("settings/about/version"_i18n);
    version->setValue(std::string(core::version::kString));
    addView(version);

    auto* build = new brls::ListItem("settings/about/commit"_i18n);
    build->setValue(std::string(core::version::kGitSha));
    addView(build);

    auto* built = new brls::ListItem("settings/about/date"_i18n);
    built->setValue(std::string(core::version::kBuildDate));
    addView(built);

    auto* toolchain = new brls::ListItem("settings/about/toolchain"_i18n);
#if defined(__SWITCH__)
    toolchain->setValue("devkitA64 (GCC " __VERSION__ ") / libnx");
#else
    toolchain->setValue("Host Toolchain (Clang/GCC)");
#endif
    addView(toolchain);

    addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "settings/about/licence"_i18n, true));
}

void SettingsTab::setupNetworkSection()
{
    addView(new brls::Header("settings/network/header"_i18n));

    // Update Channel Toggle (Stable vs Preview/Beta)
    const bool isPreview = (m_update.config().channel == core::Channel::Beta);
    auto* channelToggle = new brls::ToggleListItem("settings/network/preview_channel"_i18n,
                                                   isPreview, "settings/network/preview_desc"_i18n);

    channelToggle->getClickEvent()->subscribe([this, channelToggle](brls::View*) {
        const bool enabled = channelToggle->getToggleState();
        m_update.setChannel(enabled ? core::Channel::Beta : core::Channel::Stable);
    });
    addView(channelToggle);

    // Alternative Mirror Toggle
    auto* mirrorToggle = new brls::ToggleListItem("settings/network/mirror_raw"_i18n, false,
                                                  "settings/network/mirror_raw_desc"_i18n);

    mirrorToggle->getClickEvent()->subscribe([this, mirrorToggle](brls::View*) {
        const bool prefer = mirrorToggle->getToggleState();
        m_update.setPreferMirror(prefer);
        m_catalog.setPreferMirror(prefer);
    });
    addView(mirrorToggle);
}

void SettingsTab::setupPreferencesSection()
{
    addView(new brls::Header("settings/preferences/header"_i18n));

    m_langItem = new brls::ListItem("settings/preferences/language"_i18n);
    m_langItem->setValue("settings/preferences/language_current"_i18n);

    m_langItem->getClickEvent()->subscribe([this](brls::View*) { toggleLanguage(); });
    addView(m_langItem);
    addView(new brls::Label(brls::LabelStyle::DESCRIPTION,
                            "settings/preferences/language_desc"_i18n, true));
}

void SettingsTab::toggleLanguage()
{
    const std::string current = brls::i18n::getCurrentLocale();
    const bool isPtBr = (current == "pt-BR");

    const std::string confirmMsg = isPtBr ? "Switch interface language to English (en-US)?"
                                          : "settings/preferences/language_switch_confirm"_i18n;

    auto* dialog = new brls::Dialog(confirmMsg);
    dialog->addButton("nsx/actions/ok"_i18n, [this, isPtBr, dialog](brls::View*) {
        dialog->close([this, isPtBr]() {
            if (isPtBr) {
                brls::i18n::loadTranslations("en-US");
                m_langItem->setValue("English (en-US)");
            }
            else {
                brls::i18n::loadTranslations("pt-BR");
                m_langItem->setValue("Português (pt-BR)");
            }

            auto* done = new brls::Dialog("settings/preferences/language_changed"_i18n);
            done->addButton("nsx/actions/ok"_i18n, [done](brls::View*) { done->close(); });
            done->setCancelable(true);
            done->open();
        });
    });

    dialog->addButton("nsx/actions/cancel"_i18n, [dialog](brls::View*) { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void SettingsTab::setupLegalSection()
{
    addView(new brls::Header("settings/legal/header"_i18n));

    auto* noticesItem = new brls::ListItem("settings/legal/view_notices"_i18n);
    noticesItem->getClickEvent()->subscribe([this](brls::View*) { showNoticesDialog(); });
    addView(noticesItem);
    addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "settings/legal/view_notices_desc"_i18n,
                            true));
}

void SettingsTab::showNoticesDialog()
{
    auto* dialog = new brls::Dialog("settings/legal/notices_summary"_i18n);
    dialog->addButton("nsx/actions/ok"_i18n, [dialog](brls::View*) { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

}  // namespace nsx::ui
