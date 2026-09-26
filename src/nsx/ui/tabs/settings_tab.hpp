// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <borealis.hpp>

#include "nsx/domain/catalog/catalog_service.hpp"
#include "nsx/domain/selfupdate/update_service.hpp"

namespace nsx::ui {

/// @brief Settings and About tab: application details, licenses, mirror preferences, and language.
/// @since 0.5.0
class SettingsTab : public brls::List
{
public:
    /// @brief Construct the settings and about tab.
    /// @param update Update service.
    /// @param catalog Catalog service.
    SettingsTab(domain::UpdateService& update, domain::CatalogService& catalog);

private:
    void setupAboutSection();
    void setupNetworkSection();
    void setupPreferencesSection();
    void setupLegalSection();

    void toggleLanguage();
    void showNoticesDialog();

    domain::UpdateService& m_update;
    domain::CatalogService& m_catalog;

    brls::ListItem* m_langItem{nullptr};
};

}  // namespace nsx::ui
