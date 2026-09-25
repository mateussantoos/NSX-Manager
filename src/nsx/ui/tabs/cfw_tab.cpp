// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/cfw_tab.hpp"

#include <utility>

#include "nsx/core/paths/preserve_rules.hpp"

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

CfwTab::CfwTab(domain::CatalogService& catalog, domain::CfwInstallService& cfw)
    : m_catalog(catalog), m_cfw(cfw)
{
    m_status = new brls::ListItem("nsx/tabs/cfw"_i18n);
    m_status->setValue("nsx/state/loading"_i18n);
    addView(m_status);

    m_detail = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    addView(m_detail);

    auto* poller = new CallbackTask(kPollPeriodMs, m_alive, [this]() { refreshProgress(); });
    poller->start();

    startFetch();
}

CfwTab::~CfwTab()
{
    m_alive.reset();
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    m_job.requestCancel();
    m_job.join();
}

void CfwTab::startFetch()
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

void CfwTab::startInstall(const core::CatalogItem& item)
{
    if (m_job.running()) {
        return;
    }

    m_installing = true;
    m_cfwOutcome.reset();

    m_status->setValue("catalog/installing"_i18n);
    m_detail->setText(item.name);

    m_progressDialog = new ProgressDialog(item.name, [this]() { m_job.requestCancel(); });
    m_progressDialog->setStage("catalog/stage_preflight"_i18n);
    m_progressDialog->open();

    const bool started = m_job.start([this, item]() {
        SpeedMeter meter;
        auto onProgress = [this, &meter](const domain::InstallProgress& p) -> bool {
            if (m_job.cancelRequested()) {
                return false;
            }

            std::string stageName;
            std::string detail;
            int current = -1;
            int total = 0;

            switch (p.stage) {
                case domain::InstallStage::Preflight:
                    stageName = "catalog/stage_preflight"_i18n;
                    detail = p.detail;
                    break;
                case domain::InstallStage::Downloading: {
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
                case domain::InstallStage::Extracting: {
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
                case domain::InstallStage::Merging: {
                    meter.reset();
                    stageName = "catalog/stage_merging"_i18n;
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

        const core::PreserveRules preserve = core::PreserveRules::defaults();
        auto outcome =
            std::make_unique<domain::InstallOutcome>(m_cfw.install(item, preserve, onProgress));
        m_cfwOutcome = std::move(outcome);
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

void CfwTab::refreshProgress()
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

void CfwTab::onFetchFinished()
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

void CfwTab::onInstallFinished()
{
    m_installing = false;

    if (!m_cfwOutcome) {
        m_status->setValue("catalog/unavailable"_i18n);
        m_detail->setText("catalog/fetch_failed"_i18n);
        return;
    }

    if (m_cfwOutcome->installed()) {
        m_status->setValue("catalog/loaded"_i18n);
        m_detail->setText("catalog/install_success"_i18n);

        auto* dialog = new brls::Dialog("catalog/install_success"_i18n);
        dialog->addButton("nsx/actions/ok"_i18n, [dialog](brls::View*) { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
        return;
    }

    if (m_cfwOutcome->result == domain::InstallResult::Cancelled) {
        m_status->setValue("catalog/loaded"_i18n);
        m_detail->setText("catalog/install_cancelled"_i18n);
        return;
    }

    m_status->setValue("catalog/unavailable"_i18n);
    const std::string desc = std::string(domain::describe(m_cfwOutcome->result));
    const std::string reason =
        m_cfwOutcome->detail.empty() ? desc : desc + " (" + m_cfwOutcome->detail + ")";
    m_detail->setText(reason);

    auto* dialog = new brls::Dialog(reason);
    dialog->addButton("nsx/actions/ok"_i18n, [dialog](brls::View*) { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void CfwTab::rebuildList()
{
    if (!m_outcome || !m_outcome->catalog.has_value()) {
        return;
    }

    const std::vector<const core::CatalogItem*> items =
        m_outcome->catalog->ofKind(core::ContentKind::CfwPack);

    if (items.empty()) {
        m_status->setValue("catalog/empty"_i18n);
        m_detail->setText("catalog/no_cfw"_i18n);
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

            auto* dialog = new brls::Dialog("catalog/confirm_install"_i18n);
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
