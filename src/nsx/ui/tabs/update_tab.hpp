// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <string>

#include <borealis.hpp>

#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/ui/widgets/background_job.hpp"

namespace nsx::ui {

/// @brief The Updates tab: check, offer, install, hand off.
///
/// @details The only tab wired to a complete use-case, because the self-update
///          flow is the only one with no missing piece behind it.
///
///          Every slow step runs on a @ref BackgroundJob. The view reads
///          progress from a `RepeatingTask` on the UI thread and never touches
///          the worker's memory directly - see `widgets/background_job.hpp` for
///          why that boundary exists.
///
/// @since 0.3.0
class UpdateTab : public brls::List
{
public:
    /// @brief Build the tab.
    /// @param service The update use-case.
    /// @param onChainload Called with the forwarder argv when an update is
    ///        staged and the application should hand over.
    UpdateTab(domain::UpdateService& service,
              std::function<void(const std::string&, const std::string&)> onChainload);

    ~UpdateTab() override;

private:
    void startCheck();
    void startInstall();
    void onCheckFinished();
    void onInstallFinished();
    void refreshProgress();
    void setIdle(const std::string& status);

    domain::UpdateService& m_service;
    std::function<void(const std::string&, const std::string&)> m_onChainload;

    BackgroundJob m_job;

    /// Alive-flag for the polling task.
    ///
    /// brls::RepeatingTask registers itself with the TaskManager in its
    /// CONSTRUCTOR, so the manager owns it, and stop() deletes it. A view that
    /// called stop() from its destructor would race the manager's own cleanup
    /// for the same pointer. So the task outlives this view by design and
    /// checks this flag instead - once it expires, it does nothing.
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};

    brls::ListItem* m_status{};
    brls::ListItem* m_action{};
    brls::Label* m_detail{};

    // Written by the worker thread once, read by the UI thread after
    // takeFinished() reports completion. The atomic ordering inside
    // BackgroundJob is what makes that safe without another lock.
    std::unique_ptr<domain::CheckOutcome> m_check;
    std::unique_ptr<domain::StageOutcome> m_stage;

    bool m_installing{};
};

}  // namespace nsx::ui
