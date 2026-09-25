// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <borealis.hpp>

#include "nsx/domain/catalog/catalog_service.hpp"
#include "nsx/domain/firmware/firmware_install_service.hpp"
#include "nsx/ui/widgets/background_job.hpp"
#include "nsx/ui/widgets/progress_dialog.hpp"

namespace nsx::ui {

/// @brief The Firmware tab: list official firmware sets, download and hand off
///        to Daybreak for installation.
///
/// @details Structurally identical to @ref CfwTab but filters the catalogue for
///          @ref nsx::core::ContentKind::Firmware entries. The tab fetches
///          independently so navigating to it does not duplicate the network
///          request if the cached catalogue is fresh.
///
/// @since 0.3.0
class FirmwareTab : public brls::List
{
public:
    /// @brief Build the tab.
    /// @param catalog The catalogue source.
    /// @param firmware The firmware install use-case.
    /// @param onChainload Callback to chainload a target nro on exit.
    FirmwareTab(domain::CatalogService& catalog, domain::FirmwareInstallService& firmware,
                std::function<void(const std::string&, const std::string&)> onChainload = {});

    ~FirmwareTab() override;

private:
    void startFetch();
    void onFetchFinished();
    void startInstall(const core::CatalogItem& item);
    void onInstallFinished();
    void refreshProgress();
    void rebuildList();

    domain::CatalogService& m_catalog;
    domain::FirmwareInstallService& m_firmware;
    std::function<void(const std::string&, const std::string&)> m_onChainload;

    BackgroundJob m_job;

    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

    brls::ListItem* m_status{};
    brls::Label* m_detail{};
    ProgressDialog* m_progressDialog{nullptr};

    std::unique_ptr<domain::CatalogOutcome> m_outcome;
    std::unique_ptr<domain::FirmwareOutcome> m_firmwareOutcome;

    bool m_installing{false};
};

}  // namespace nsx::ui
