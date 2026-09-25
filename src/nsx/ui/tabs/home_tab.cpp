// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/home_tab.hpp"

#include <utility>

#include "nsx/core/version/version.hpp"
#include "nsx/ui/widgets/progress_dialog.hpp"

namespace nsx::ui {

using namespace brls::i18n::literals;

namespace {

constexpr retro_time_t kPollPeriodMs = 100;

class CallbackTask : public brls::RepeatingTask
{
public:
    CallbackTask(retro_time_t period, std::weak_ptr<bool> alive, std::function<void()> callback)
        : brls::RepeatingTask(period), m_alive(std::move(alive)), m_callback(std::move(callback))
    {
    }

    void run(retro_time_t currentTime) override
    {
        brls::RepeatingTask::run(currentTime);
        if (m_alive.expired()) {
            return;
        }
        m_callback();
    }

private:
    std::weak_ptr<bool> m_alive;
    std::function<void()> m_callback;
};

}  // namespace

HomeTab::HomeTab(domain::UpdateService& update, domain::TelemetryService& telemetry,
                 domain::FileStore& files, SystemVersionsQuery queryVersions)
    : m_update(update),
      m_telemetry(telemetry),
      m_files(files),
      m_queryVersions(std::move(queryVersions))
{
    setupSystemSection();
    setupStorageSection();
    setupTelemetrySection();
    setupUpdateSection();

    auto* poller = new CallbackTask(kPollPeriodMs, m_alive, [this]() { refresh(); });
    poller->start();

    refreshStorageInfo();
    runTelemetryCheck();
    runUpdateCheck();
}

HomeTab::~HomeTab()
{
    m_alive.reset();
    m_telemetryJob.requestCancel();
    m_updateJob.requestCancel();
    m_telemetryJob.join();
    m_updateJob.join();
}

void HomeTab::setupSystemSection()
{
    addView(new brls::Header("home/system/header"_i18n));

    std::string hos = "Unknown";
    std::string ams = "Not detected";
    if (m_queryVersions) {
        const auto versions = m_queryVersions();
        hos = versions.first;
        ams = versions.second;
    }

    m_hosItem = new brls::ListItem("home/system/hos_version"_i18n);
    m_hosItem->setValue(hos);
    addView(m_hosItem);

    m_amsItem = new brls::ListItem("home/system/ams_version"_i18n);
    m_amsItem->setValue(ams);
    addView(m_amsItem);
}

void HomeTab::setupStorageSection()
{
    addView(new brls::Header("home/storage/header"_i18n));

    m_storageItem = new brls::ListItem("home/storage/capacity"_i18n);
    m_storageItem->setValue("nsx/state/loading"_i18n);
    m_storageItem->getClickEvent()->subscribe([this](brls::View*) { refreshStorageInfo(); });
    addView(m_storageItem);

    m_storageGauge = new brls::ProgressDisplay(brls::ProgressDisplayFlags::PERCENTAGE);
    addView(m_storageGauge);
}

void HomeTab::setupTelemetrySection()
{
    addView(new brls::Header("home/telemetry/header"_i18n));

    m_telemetryItem = new brls::ListItem("home/telemetry/status"_i18n);
    m_telemetryItem->setValue("tools/telemetry/checking"_i18n);
    m_telemetryItem->getClickEvent()->subscribe([this](brls::View*) {
        if (!m_telemetryJob.running()) {
            runTelemetryCheck();
        }
    });
    addView(m_telemetryItem);

    m_telemetryDetail = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    addView(m_telemetryDetail);
}

void HomeTab::setupUpdateSection()
{
    addView(new brls::Header("home/update/header"_i18n));

    m_updateItem = new brls::ListItem("home/update/label"_i18n);
    m_updateItem->setValue("nsx/state/loading"_i18n);
    m_updateItem->getClickEvent()->subscribe([this](brls::View*) {
        if (!m_updateJob.running()) {
            runUpdateCheck();
        }
    });
    addView(m_updateItem);

    m_updateDetail = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    addView(m_updateDetail);
}

void HomeTab::refreshStorageInfo()
{
    const auto freeOpt = m_files.freeSpaceBytes("/");
    const auto totalOpt = m_files.totalSpaceBytes("/");

    if (freeOpt.has_value() && totalOpt.has_value() && *totalOpt > 0) {
        const std::uint64_t freeBytes = *freeOpt;
        const std::uint64_t totalBytes = *totalOpt;
        const std::string text = formatBytes(freeBytes) + " " + "home/storage/free_of"_i18n + " " +
                                 formatBytes(totalBytes);
        m_storageItem->setValue(text);

        const std::uint64_t usedBytes = totalBytes > freeBytes ? (totalBytes - freeBytes) : 0;
        const int usedPct = static_cast<int>((usedBytes * 100ULL) / totalBytes);
        if (m_storageGauge) {
            m_storageGauge->setProgress(usedPct, 100);
        }
    }
    else if (freeOpt.has_value()) {
        m_storageItem->setValue(formatBytes(*freeOpt));
        if (m_storageGauge) {
            m_storageGauge->setProgress(0, 100);
        }
    }
    else {
        m_storageItem->setValue("home/storage/unavailable"_i18n);
        if (m_storageGauge) {
            m_storageGauge->setProgress(0, 100);
        }
    }
}

void HomeTab::runTelemetryCheck()
{
    m_telemetryItem->setValue("tools/telemetry/checking"_i18n);
    m_telemetryDetail->setText("");

    (void)m_telemetryJob.start([this]() {
        const auto report = m_telemetry.checkProtection();
        std::lock_guard lock(m_mutex);
        m_pendingTelemetry = report;
    });
}

void HomeTab::runUpdateCheck()
{
    m_updateItem->setValue("nsx/state/loading"_i18n);
    m_updateDetail->setText("");

    (void)m_updateJob.start([this]() {
        const auto outcome = m_update.check();
        std::lock_guard lock(m_mutex);
        m_pendingUpdate = outcome;
    });
}

void HomeTab::refresh()
{
    if (m_telemetryJob.takeFinished()) {
        std::lock_guard lock(m_mutex);
        if (m_pendingTelemetry.has_value()) {
            const auto report = *m_pendingTelemetry;
            m_pendingTelemetry.reset();

            if (report.overall == infra::TelemetryStatus::Protected) {
                m_telemetryItem->setValue("tools/telemetry/protected"_i18n);
                m_telemetryDetail->setText("tools/telemetry/protected_detail"_i18n);
            }
            else if (report.overall == infra::TelemetryStatus::Offline) {
                m_telemetryItem->setValue("home/telemetry/offline"_i18n);
                m_telemetryDetail->setText("home/telemetry/offline_detail"_i18n);
            }
            else {
                m_telemetryItem->setValue("tools/telemetry/unshielded"_i18n);
                m_telemetryDetail->setText("tools/telemetry/unshielded_detail"_i18n);
            }
        }
    }

    if (m_updateJob.takeFinished()) {
        std::lock_guard lock(m_mutex);
        if (m_pendingUpdate.has_value()) {
            const auto outcome = *m_pendingUpdate;
            m_pendingUpdate.reset();

            if (outcome.offersUpdate()) {
                m_updateItem->setValue("home/update/available"_i18n);
                const std::string ver =
                    outcome.manifest ? outcome.manifest->version.toString() : "";
                m_updateDetail->setText(ver.empty() ? outcome.detail
                                                    : (ver + " - " + outcome.detail));
            }
            else if (outcome.decision.action == core::UpdateAction::UpToDate) {
                m_updateItem->setValue("home/update/up_to_date"_i18n);
                m_updateDetail->setText(std::string(core::version::kString));
            }
            else {
                m_updateItem->setValue("nsx/state/unavailable"_i18n);
                m_updateDetail->setText(outcome.detail);
            }
        }
    }
}

}  // namespace nsx::ui
