// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <borealis.hpp>

#include "nsx/domain/maintenance/cleanup_service.hpp"
#include "nsx/domain/network/telemetry_service.hpp"
#include "nsx/domain/sysmodule/sysmodule_service.hpp"
#include "nsx/ui/widgets/background_job.hpp"

namespace nsx::ui {

using FixArchiveBitCallback = std::function<std::pair<std::size_t, std::size_t>(
    const std::function<bool(std::string_view)>&)>;
using RebootCallback = std::function<bool()>;

/// @brief Maintenance, power controls, sysmodule manager, and telemetry status.
/// @since 0.4.0
class ToolsTab : public brls::List
{
public:
    /// @brief Construct the tools and maintenance tab.
    /// @param cleanup Storage cleanup service.
    /// @param sysmodules Sysmodules management service.
    /// @param telemetry Telemetry diagnostics service.
    /// @param fixArchiveBit Archive-bit recursive repair callback.
    /// @param rebootToPayload Reboot to payload callback.
    ToolsTab(domain::CleanupService& cleanup, domain::SysmoduleService& sysmodules,
             domain::TelemetryService& telemetry, FixArchiveBitCallback fixArchiveBit,
             RebootCallback rebootToPayload);
    ~ToolsTab() override;

private:
    void setupTelemetrySection();
    void setupMaintenanceSection();
    void setupSysmodulesSection();

    void runTelemetryCheck();
    void runFixArchiveBit();
    void runPurgeStaging();
    void runRebootToPayload();
    void refresh();

    domain::CleanupService& m_cleanup;
    domain::SysmoduleService& m_sysmodules;
    domain::TelemetryService& m_telemetry;
    FixArchiveBitCallback m_fixArchiveBit;
    RebootCallback m_rebootToPayload;

    brls::ListItem* m_telemetryItem{nullptr};
    brls::Label* m_telemetryDetail{nullptr};

    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
    BackgroundJob m_telemetryJob;
    BackgroundJob m_archiveJob;

    std::optional<infra::TelemetryReport> m_pendingTelemetry;
    std::optional<std::pair<std::size_t, std::size_t>> m_pendingArchiveBit;
    std::mutex m_resultMutex;
};

}  // namespace nsx::ui
