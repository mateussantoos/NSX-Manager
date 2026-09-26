// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/tools_tab.hpp"

#include <utility>
#include <vector>

#include "nsx/ui/widgets/linear_list_item.hpp"

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

ToolsTab::ToolsTab(domain::CleanupService& cleanup, domain::SysmoduleService& sysmodules,
                   domain::TelemetryService& telemetry, FixArchiveBitCallback fixArchiveBit,
                   RebootCallback rebootToPayload, ListPayloadsCallback listPayloads,
                   RebootSpecificPayloadCallback rebootSpecificPayload)
    : m_cleanup(cleanup),
      m_sysmodules(sysmodules),
      m_telemetry(telemetry),
      m_fixArchiveBit(std::move(fixArchiveBit)),
      m_rebootToPayload(std::move(rebootToPayload)),
      m_listPayloads(std::move(listPayloads)),
      m_rebootSpecificPayload(std::move(rebootSpecificPayload))
{
    setupTelemetrySection();
    setupMaintenanceSection();
    setupSysmodulesSection();

    auto* poller = new CallbackTask(kPollPeriodMs, m_alive, [this]() { refresh(); });
    poller->start();

    runTelemetryCheck();
}

ToolsTab::~ToolsTab()
{
    m_alive.reset();
    m_telemetryJob.requestCancel();
    m_archiveJob.requestCancel();
    m_telemetryJob.join();
    m_archiveJob.join();
}

void ToolsTab::setupTelemetrySection()
{
    addView(new brls::Header("tools/telemetry/header"_i18n));

    m_telemetryItem =
        new LinearListItem("tools/telemetry/label"_i18n, "tools/telemetry/checking"_i18n);

    m_telemetryItem->getClickEvent()->subscribe([this](brls::View*) {
        if (!m_telemetryJob.running()) {
            runTelemetryCheck();
        }
    });
    addView(m_telemetryItem);

    m_telemetryDetail = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    addView(m_telemetryDetail);
}

void ToolsTab::runTelemetryCheck()
{
    m_telemetryItem->setValue("tools/telemetry/checking"_i18n);
    m_telemetryDetail->setText("");

    (void)m_telemetryJob.start([this]() {
        const auto report = m_telemetry.checkProtection();
        std::lock_guard lock(m_resultMutex);
        m_pendingTelemetry = report;
    });
}

void ToolsTab::refresh()
{
    if (m_telemetryJob.takeFinished()) {
        std::lock_guard lock(m_resultMutex);
        if (m_pendingTelemetry.has_value()) {
            const auto report = *m_pendingTelemetry;
            m_pendingTelemetry.reset();

            if (report.overall == infra::TelemetryStatus::Protected) {
                m_telemetryItem->setValue("tools/telemetry/protected"_i18n);
                m_telemetryDetail->setText("tools/telemetry/protected_detail"_i18n);
            }
            else {
                m_telemetryItem->setValue("tools/telemetry/unshielded"_i18n);
                m_telemetryDetail->setText("tools/telemetry/unshielded_detail"_i18n);
            }
        }
    }

    if (m_archiveJob.takeFinished()) {
        std::lock_guard lock(m_resultMutex);
        if (m_pendingArchiveBit.has_value()) {
            const auto res = *m_pendingArchiveBit;
            m_pendingArchiveBit.reset();

            const std::string msg =
                brls::i18n::getStr("tools/maintenance/archive_bit_done", res.first, res.second);
            auto* doneDialog = new brls::Dialog(msg);
            doneDialog->addButton("nsx/actions/ok"_i18n,
                                  [doneDialog](brls::View*) { doneDialog->close(); });
            doneDialog->setCancelable(true);
            doneDialog->open();
        }
    }
}

void ToolsTab::setupMaintenanceSection()
{
    addView(new brls::Header("tools/maintenance/header"_i18n));

    // 1. Fix Archive Bit
    auto* fixItem =
        new LinearListItem("tools/maintenance/archive_bit_label"_i18n, "Executar / Run");
    fixItem->getClickEvent()->subscribe([this](brls::View*) {
        if (m_archiveJob.running()) {
            return;
        }

        auto* dialog = new brls::Dialog("tools/maintenance/archive_bit_confirm"_i18n);
        dialog->addButton("nsx/actions/ok"_i18n, [this, dialog](brls::View*) {
            dialog->close([this]() { runFixArchiveBit(); });
        });
        dialog->addButton("nsx/actions/cancel"_i18n, [dialog](brls::View*) { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
    });
    addView(fixItem);
    addView(new brls::Label(brls::LabelStyle::DESCRIPTION,
                            "tools/maintenance/archive_bit_desc"_i18n, true));

    // 2. Reboot to Payload / RCM
    auto* rebootItem =
        new LinearListItem("tools/maintenance/reboot_rcm_label"_i18n, "Reiniciar / Reboot");
    rebootItem->getClickEvent()->subscribe([this](brls::View*) {
        auto* dialog = new brls::Dialog("tools/maintenance/reboot_rcm_confirm"_i18n);
        dialog->addButton("nsx/actions/ok"_i18n, [this, dialog](brls::View*) {
            dialog->close([this]() { runRebootToPayload(); });
        });
        dialog->addButton("nsx/actions/cancel"_i18n, [dialog](brls::View*) { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
    });
    addView(rebootItem);
    addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "tools/maintenance/reboot_rcm_desc"_i18n,
                            true));

    // 3. Purge Staging Cache
    auto* purgeItem = new LinearListItem("tools/maintenance/cleanup_label"_i18n, "Limpar / Clean");
    purgeItem->getClickEvent()->subscribe([this](brls::View*) {
        if (m_archiveJob.running()) {
            return;
        }

        auto* dialog = new brls::Dialog("tools/maintenance/cleanup_confirm"_i18n);
        dialog->addButton("nsx/actions/ok"_i18n, [this, dialog](brls::View*) {
            dialog->close([this]() { runPurgeStaging(); });
        });
        dialog->addButton("nsx/actions/cancel"_i18n, [dialog](brls::View*) { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
    });
    addView(purgeItem);
    addView(new brls::Label(brls::LabelStyle::DESCRIPTION, "tools/maintenance/cleanup_desc"_i18n,
                            true));
}

void ToolsTab::runFixArchiveBit()
{
    if (!m_fixArchiveBit) {
        return;
    }

    (void)m_archiveJob.start([this]() {
        auto onProgress = [this](std::string_view) -> bool {
            return !m_archiveJob.cancelRequested();
        };

        const auto rep = m_fixArchiveBit(onProgress);
        std::lock_guard lock(m_resultMutex);
        m_pendingArchiveBit = rep;
    });
}

void ToolsTab::runRebootToPayload()
{
    if (m_listPayloads && m_rebootSpecificPayload) {
        const auto payloads = m_listPayloads();
        if (payloads.size() > 1) {
            auto* dialog = new brls::Dialog("tools/maintenance/reboot_rcm_confirm"_i18n);
            for (const auto& [name, path] : payloads) {
                const std::string payloadPath = path;
                dialog->addButton(name, [this, dialog, payloadPath](brls::View*) {
                    dialog->close([this, payloadPath]() {
                        if (!m_rebootSpecificPayload(payloadPath)) {
                            auto* failDialog =
                                new brls::Dialog("tools/maintenance/reboot_failed"_i18n);
                            failDialog->addButton("nsx/actions/ok"_i18n, [failDialog](brls::View*) {
                                failDialog->close();
                            });
                            failDialog->setCancelable(true);
                            failDialog->open();
                        }
                    });
                });
            }
            dialog->addButton("nsx/actions/cancel"_i18n,
                              [dialog](brls::View*) { dialog->close(); });
            dialog->setCancelable(true);
            dialog->open();
            return;
        }
        if (payloads.size() == 1) {
            if (m_rebootSpecificPayload(payloads[0].second)) {
                return;
            }
        }
    }

    if (m_rebootToPayload) {
        const bool ok = m_rebootToPayload();
        if (!ok) {
            auto* failDialog = new brls::Dialog("tools/maintenance/reboot_failed"_i18n);
            failDialog->addButton("nsx/actions/ok"_i18n,
                                  [failDialog](brls::View*) { failDialog->close(); });
            failDialog->setCancelable(true);
            failDialog->open();
        }
    }
}

void ToolsTab::runPurgeStaging()
{
    const auto report = m_cleanup.cleanStaging();
    std::string msg;
    if (report.success) {
        msg = brls::i18n::getStr("tools/maintenance/cleanup_done", report.filesRemoved);
    }
    else {
        msg = brls::i18n::getStr("tools/maintenance/cleanup_failed", report.detail);
    }

    auto* dialog = new brls::Dialog(msg);
    dialog->addButton("nsx/actions/ok"_i18n, [dialog](brls::View*) { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void ToolsTab::setupSysmodulesSection()
{
    addView(new brls::Header("tools/sysmodules/header"_i18n));

    const auto modules = m_sysmodules.listSysmodules();
    if (modules.empty()) {
        addView(new brls::Label(brls::LabelStyle::DESCRIPTION,
                                "tools/sysmodules/no_sysmodules"_i18n, true));
        return;
    }

    for (const auto& mod : modules) {
        auto* toggle = new LinearToggleItem(mod.name, mod.enabled);

        const std::string tid = mod.titleId;
        toggle->getClickEvent()->subscribe([this, tid, toggle](brls::View*) {
            const bool state = toggle->getToggleState();
            (void)m_sysmodules.setEnabled(tid, state);
        });

        addView(toggle);
    }
}

}  // namespace nsx::ui
