// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <borealis.hpp>

#include "nsx/domain/network/telemetry_service.hpp"
#include "nsx/domain/ports/ports.hpp"
#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/ui/widgets/background_job.hpp"

namespace nsx::ui {

/// @brief Callback returning Horizon OS version and Atmosphere version strings.
using SystemVersionsQuery = std::function<std::pair<std::string, std::string>()>;

/// @brief The Home dashboard tab providing summary cards and system status.
class HomeTab : public brls::List
{
public:
    HomeTab(domain::UpdateService& update, domain::TelemetryService& telemetry,
            domain::FileStore& files, SystemVersionsQuery queryVersions = {});

    ~HomeTab() override;

private:
    void setupSystemSection();
    void setupStorageSection();
    void setupTelemetrySection();
    void setupUpdateSection();

    void refresh();
    void runTelemetryCheck();
    void runUpdateCheck();
    void refreshStorageInfo();

    domain::UpdateService& m_update;
    domain::TelemetryService& m_telemetry;
    domain::FileStore& m_files;
    SystemVersionsQuery m_queryVersions;

    brls::ListItem* m_hosItem{nullptr};
    brls::ListItem* m_amsItem{nullptr};
    brls::ListItem* m_storageItem{nullptr};
    brls::ProgressDisplay* m_storageGauge{nullptr};
    brls::ListItem* m_telemetryItem{nullptr};
    brls::Label* m_telemetryDetail{nullptr};
    brls::ListItem* m_updateItem{nullptr};
    brls::Label* m_updateDetail{nullptr};

    BackgroundJob m_telemetryJob;
    BackgroundJob m_updateJob;
    std::mutex m_mutex;
    std::optional<infra::TelemetryReport> m_pendingTelemetry;
    std::optional<domain::CheckOutcome> m_pendingUpdate;

    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
};

}  // namespace nsx::ui
