// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <borealis.hpp>

#include "nsx/domain/network/telemetry_service.hpp"
#include "nsx/domain/ports/ports.hpp"
#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/ui/widgets/background_job.hpp"

namespace nsx::ui {

/// @brief System overview snapshot for dashboard display.
struct SystemOverview
{
    std::string model{"Nintendo Switch"};
    std::string hosVersion{"Unknown"};
    std::string amsVersion{"Not detected"};
    std::string nandType{"SysNAND"};
    std::string fsType{"FAT32"};
    bool isExFAT{false};
};

using SystemOverviewQuery = std::function<SystemOverview()>;
using TabSelectCallback = std::function<void(int tabIndex)>;

/// @brief Rich custom dashboard view displaying top 4 cards and status cards.
class DashboardSummaryView : public brls::View
{
public:
    DashboardSummaryView(domain::FileStore& files, SystemOverviewQuery query);
    ~DashboardSummaryView() override;

    void updateTelemetry(const infra::TelemetryReport& report);
    void updateUpdateOutcome(const domain::CheckOutcome& outcome);
    void refresh();

    void draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
              brls::Style* viewStyle, brls::FrameContext* ctx) override;
    void willAppear(bool resetState = false) override;

private:
    void updateStorage();
    void drawImageSafe(NVGcontext* vg, int& textureId, const std::string& path, float imgX,
                       float imgY, float imgW, float imgH);
    void drawChipIcon(NVGcontext* vg, float cx, float cy, float size);

    domain::FileStore& m_files;
    SystemOverviewQuery m_queryOverview;

    SystemOverview m_overview;
    std::string m_storageFreeStr{"-- GB"};
    float m_storagePct{0.5f};

    int m_joyconsImg{0};
    int m_amsImg{0};
    int m_sdImg{0};

    std::mutex m_mutex;
    infra::TelemetryStatus m_telemetryStatus{infra::TelemetryStatus::Protected};
    std::string m_telemetryText{"Telemetria: Protegida (Seguro)"};
    bool m_telemetryChecked{false};

    std::string m_updateStatusStr{"NSX Manager: Atualizado"};
    bool m_hasUpdate{false};
};

/// @brief Interactive action button with neon red glow on focus.
class DashboardActionButton : public brls::Button
{
public:
    DashboardActionButton(std::string top, std::string bottom, int iconType,
                          std::function<void()> onClickCb);

    void layout(NVGcontext* vg, brls::Style* viewStyle, brls::FontStash* stash) override;
    void draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
              brls::Style* viewStyle, brls::FrameContext* ctx) override;

    bool isHighlightBackgroundEnabled() override { return false; }

    bool onClick() override;

private:
    std::string m_topText;
    std::string m_bottomText;
    int m_iconType{0};
    std::function<void()> m_callback;
    float m_focusAnim{0.0f};
    float m_pulse{0.0f};
};

/// @brief Modern modular dashboard tab.
class HomeTab : public brls::BoxLayout
{
public:
    HomeTab(domain::UpdateService& update, domain::TelemetryService& telemetry,
            domain::FileStore& files, SystemOverviewQuery queryOverview = {},
            TabSelectCallback onSelectTab = {});

    ~HomeTab() override;

    brls::View* getDefaultFocus() override;

private:
    void runTelemetryCheck();
    void runUpdateCheck();

    domain::UpdateService& m_update;
    domain::TelemetryService& m_telemetry;
    domain::FileStore& m_files;
    SystemOverviewQuery m_queryOverview;
    TabSelectCallback m_onSelectTab;

    DashboardSummaryView* m_summaryView{nullptr};
    brls::BoxLayout* m_buttonsRow{nullptr};

    BackgroundJob m_telemetryJob;
    BackgroundJob m_updateJob;
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
};

}  // namespace nsx::ui
