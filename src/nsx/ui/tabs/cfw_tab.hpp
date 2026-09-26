// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <string>

#include <borealis.hpp>

#include "nsx/domain/catalog/catalog_service.hpp"
#include "nsx/domain/cfw/cfw_install_service.hpp"
#include "nsx/ui/widgets/background_job.hpp"
#include "nsx/ui/widgets/progress_dialog.hpp"

namespace nsx::ui {

/// @brief The CFW packs tab: list available packs, install, stage and merge.
///
/// @details The catalogue is fetched on a @ref BackgroundJob so the UI never
///          blocks on the network. Once a catalogue is available the tab
///          rebuilds itself with one brls::ListItem per
///          @ref nsx::core::ContentKind::CfwPack entry.
///
/// @since 0.3.0
class CfwTab : public brls::List
{
public:
    /// @brief Build the tab.
    /// @param catalog The catalogue source.
    /// @param cfw The install use-case.
    CfwTab(domain::CatalogService& catalog, domain::CfwInstallService& cfw);

    ~CfwTab() override;

private:
    void startFetch();
    void onFetchFinished();
    void startInstall(const core::CatalogItem& item);
    void onInstallFinished();
    void refreshProgress();
    void rebuildList();

    domain::CatalogService& m_catalog;
    domain::CfwInstallService& m_cfw;

    BackgroundJob m_job;

    /// Alive-flag for the polling task. See UpdateTab::m_alive for the
    /// rationale: the TaskManager owns the RepeatingTask, not us.
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

    brls::ListItem* m_status{};
    brls::Label* m_detail{};
    ProgressDialog* m_progressDialog{nullptr};

    std::unique_ptr<domain::CatalogOutcome> m_outcome;
    std::unique_ptr<domain::InstallOutcome> m_cfwOutcome;

    bool m_installing{false};
};

}  // namespace nsx::ui
