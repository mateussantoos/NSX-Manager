// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/firmware_tab.hpp"

#include <utility>

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

FirmwareTab::FirmwareTab(domain::CatalogService& catalog, domain::FirmwareInstallService& firmware,
                         std::function<void(const std::string&, const std::string&)> onChainload)
    : m_catalog(catalog), m_firmware(firmware), m_onChainload(std::move(onChainload))
{
    m_status = new brls::ListItem("nsx/tabs/firmware"_i18n);
    m_status->setValue("nsx/state/loading"_i18n);
    addView(m_status);

    m_detail = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    addView(m_detail);

    auto* poller = new CallbackTask(kPollPeriodMs, m_alive, [this]() { refreshProgress(); });
    poller->start();

    startFetch();
}

FirmwareTab::~FirmwareTab()
{
    m_alive.reset();
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    m_job.requestCancel();
    m_job.join();
}

void FirmwareTab::startFetch()
{
    m_outcome.reset();
    m_installing = false;
    m_status->setValue("nsx/state/loading"_i18n);
    m_detail->setText("");

    const bool started = m_job.start([this]() {
        m_job.publish({"catalog/fetching"_i18n, "", -1, 0});
        auto result = std::make_unique<domain::CatalogOutcome>(m_catalog.fetch());
        m_outcome = std::move(result);
    });

    if (!started) {
        m_detail->setText("update/busy"_i18n);
    }
}

void FirmwareTab::startInstall(const core::CatalogItem& item)
{
    if (m_job.running()) {
        return;
    }

    m_installing = true;
    m_firmwareOutcome.reset();

    m_status->setValue("catalog/installing"_i18n);
    m_detail->setText(item.name);

    m_progressDialog = new ProgressDialog(item.name, [this]() { m_job.requestCancel(); });
    m_progressDialog->setStage("catalog/stage_preflight"_i18n);
    m_progressDialog->open();

    const bool started = m_job.start([this, item]() {
        SpeedMeter meter;
        auto onProgress = [this, &meter](const domain::FirmwareProgress& p) -> bool {
            if (m_job.cancelRequested()) {
                return false;
            }

            std::string stageName;
            std::string detail;
            int current = -1;
            int total = 0;

            switch (p.stage) {
                case domain::FirmwareStage::Preflight:
                    stageName = "catalog/stage_preflight"_i18n;
                    detail = p.detail;
                    break;
                case domain::FirmwareStage::Downloading: {
                    stageName = "catalog/stage_downloading"_i18n;
                    meter.update(p.done);
                    const std::string speed = meter.format();
                    detail = formatBytes(p.done) + " / " + formatBytes(p.total);
                    if (!speed.empty()) {
                        detail += "  (" + speed + ")";
                    }
                    if (p.total > 0) {
                        current = static_cast<int>((p.done * 100ULL) / p.total);
                        total = 100;
                    }
                    break;
                }
                case domain::FirmwareStage::Clearing: {
                    meter.reset();
                    stageName = "catalog/stage_clearing"_i18n;
                    detail = p.detail;
                    break;
                }
                case domain::FirmwareStage::Extracting: {
                    meter.reset();
                    stageName = "catalog/stage_extracting"_i18n;
                    if (p.total > 0) {
                        detail = std::to_string(p.done) + " / " + std::to_string(p.total);
                        if (!p.detail.empty()) {
                            detail += " - " + p.detail;
                        }
                        current = static_cast<int>((p.done * 100ULL) / p.total);
                        total = 100;
                    }
                    else {
                        detail = p.detail;
                    }
                    break;
                }
            }

            m_job.publish({stageName, detail, current, total});
            return true;
        };

        auto outcome =
            std::make_unique<domain::FirmwareOutcome>(m_firmware.stage(item, onProgress));
        m_firmwareOutcome = std::move(outcome);
    });

    if (!started) {
        if (m_progressDialog) {
            m_progressDialog->close();
            m_progressDialog = nullptr;
        }
        m_installing = false;
        m_detail->setText("update/busy"_i18n);
    }
}

void FirmwareTab::refreshProgress()
{
    if (m_job.running()) {
        const BackgroundJob::Status status = m_job.poll();
        if (m_progressDialog) {
            m_progressDialog->setStage(status.headline);
            m_progressDialog->setProgress(status.current, status.total);
            m_progressDialog->setDetail(status.detail);
        }
        else {
            m_status->setValue(status.headline);
            if (!status.detail.empty()) {
                m_detail->setText(status.detail);
            }
        }
        return;
    }

    if (!m_job.takeFinished()) {
        return;
    }

    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }

    if (m_installing) {
        onInstallFinished();
    }
    else {
        onFetchFinished();
    }
}

void FirmwareTab::onFetchFinished()
{
    if (!m_outcome || !m_outcome->available()) {
        const std::string reason = m_outcome ? m_outcome->detail : "";
        m_status->setValue("catalog/unavailable"_i18n);
        m_detail->setText(reason.empty() ? "catalog/fetch_failed"_i18n : reason);
        return;
    }

    m_detail->setText(m_outcome->detail);
    rebuildList();
}

void FirmwareTab::onInstallFinished()
{
    m_installing = false;

    if (!m_firmwareOutcome) {
        m_status->setValue("catalog/unavailable"_i18n);
        m_detail->setText("catalog/fetch_failed"_i18n);
        return;
    }

    if (m_firmwareOutcome->readyForDaybreak()) {
        m_status->setValue("catalog/loaded"_i18n);
        m_detail->setText(m_firmwareOutcome->detail);

        auto* dialog = new brls::Dialog("catalog/confirm_daybreak"_i18n);
        const std::string daybreakNro = m_firmware.config().daybreakNro;
        const std::string daybreakArgs = m_firmwareOutcome->daybreakArgs;

        dialog->addButton("catalog/launch_daybreak"_i18n,
                          [this, dialog, daybreakNro, daybreakArgs](brls::View*) {
                              dialog->close([this, daybreakNro, daybreakArgs]() {
                                  if (m_onChainload) {
                                      m_onChainload(daybreakNro, daybreakArgs);
                                  }
                              });
                          });
        dialog->addButton("nsx/actions/cancel"_i18n, [dialog](brls::View*) { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
        return;
    }

    if (m_firmwareOutcome->result == domain::FirmwareResult::Cancelled) {
        m_status->setValue("catalog/loaded"_i18n);
        m_detail->setText("catalog/install_cancelled"_i18n);
        return;
    }

    m_status->setValue("catalog/unavailable"_i18n);
    const std::string desc = std::string(domain::describe(m_firmwareOutcome->result));
    std::string reason =
        m_firmwareOutcome->detail.empty() ? desc : desc + " (" + m_firmwareOutcome->detail + ")";
    if (m_firmwareOutcome->result == domain::FirmwareResult::DirectoryNotOurs &&
        !m_firmwareOutcome->unexpectedFiles.empty()) {
        reason += ": " + m_firmwareOutcome->unexpectedFiles.front();
    }
    m_detail->setText(reason);

    auto* dialog = new brls::Dialog(reason);
    dialog->addButton("nsx/actions/ok"_i18n, [dialog](brls::View*) { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void FirmwareTab::rebuildList()
{
    if (!m_outcome || !m_outcome->catalog.has_value()) {
        return;
    }

    const std::vector<const core::CatalogItem*> items =
        m_outcome->catalog->ofKind(core::ContentKind::Firmware);

    if (items.empty()) {
        m_status->setValue("catalog/empty"_i18n);
        m_detail->setText("catalog/no_firmware"_i18n);
        return;
    }

    m_status->setValue("catalog/loaded"_i18n);

    for (const core::CatalogItem* item : items) {
        auto* row = new brls::ListItem(item->name);

        std::string value = item->versionText;
        if (!item->summary.empty()) {
            value += " - " + item->summary;
        }
        row->setValue(value);

        const std::string itemId = item->id;
        row->getClickEvent()->subscribe([this, itemId](brls::View*) {
            if (m_job.running()) {
                return;
            }

            if (!m_outcome || !m_outcome->catalog.has_value()) {
                return;
            }

            const core::CatalogItem* target = m_outcome->catalog->find(itemId);
            if (target == nullptr) {
                return;
            }

            auto* dialog = new brls::Dialog("catalog/confirm_firmware"_i18n);
            const core::CatalogItem captured = *target;

            dialog->addButton("nsx/actions/ok"_i18n, [this, captured, dialog](brls::View*) {
                dialog->close([this, captured]() { startInstall(captured); });
            });
            dialog->addButton("nsx/actions/cancel"_i18n,
                              [dialog](brls::View*) { dialog->close(); });
            dialog->setCancelable(true);
            dialog->open();
        });

        addView(row);
    }
}

}  // namespace nsx::ui
