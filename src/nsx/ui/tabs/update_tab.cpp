// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/update_tab.hpp"

#include <utility>

#include "nsx/core/version/version.hpp"

namespace nsx::ui {

using namespace brls::i18n::literals;

namespace {

/// How often the UI reads what the worker published. 100 ms is well under a
/// frame budget and far more often than a user can perceive a number changing.
constexpr retro_time_t kPollPeriodMs = 100;

std::string bytesToMegabytes(std::uint64_t bytes)
{
    return std::to_string(bytes / 1024u / 1024u) + " MB";
}

/// A RepeatingTask that calls a function, and stops calling it once its owner
/// is gone.
///
/// Borealis offers no callback form - the class is meant to be subclassed. The
/// weak_ptr is the lifetime guard described in UpdateTab::m_alive.
class CallbackTask : public brls::RepeatingTask
{
public:
    CallbackTask(retro_time_t period, std::weak_ptr<bool> alive, std::function<void()> callback)
        : brls::RepeatingTask(period), m_alive(std::move(alive)), m_callback(std::move(callback))
    {
    }

    void run(retro_time_t currentTime) override
    {
        // Required by the base class: it is what reschedules the next run.
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

UpdateTab::UpdateTab(domain::UpdateService& service,
                     std::function<void(const std::string&, const std::string&)> onChainload)
    : m_service(service), m_onChainload(std::move(onChainload))
{
    m_status = new brls::ListItem("update/title"_i18n);
    m_status->setValue(std::string(core::version::kString));
    addView(m_status);

    m_action = new brls::ListItem("update/check_now"_i18n);
    m_action->getClickEvent()->subscribe([this](brls::View*) {
        if (m_job.running()) {
            return;
        }
        if (m_check && m_check->offersUpdate()) {
            startInstall();
        }
        else {
            startCheck();
        }
    });
    addView(m_action);

    m_detail = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);
    addView(m_detail);

    // One task drives every refresh, so there is exactly one place that reads
    // worker state. The TaskManager owns it from construction; see m_alive.
    auto* poller = new CallbackTask(kPollPeriodMs, m_alive, [this]() { refreshProgress(); });
    poller->start();

    setIdle("update/idle"_i18n);
}

UpdateTab::~UpdateTab()
{
    // Tells the polling task to stop touching this view. It is owned by the
    // TaskManager, not by us, so it is not deleted here.
    m_alive.reset();

    // The worker may still be writing to the SD card. Letting the process tear
    // down around it is how a card ends up with a half-written file.
    m_job.requestCancel();
    m_job.join();
}

void UpdateTab::setIdle(const std::string& status)
{
    m_status->setValue(status);
    m_action->setLabel(m_check && m_check->offersUpdate() ? "update/install_now"_i18n
                                                          : "update/check_now"_i18n);
}

void UpdateTab::startCheck()
{
    m_check.reset();
    m_stage.reset();
    m_installing = false;

    m_status->setValue("update/checking"_i18n);
    m_detail->setText("");

    const bool started = m_job.start([this]() {
        m_job.publish({"update/checking"_i18n, "", -1, 0});
        auto outcome = std::make_unique<domain::CheckOutcome>(m_service.check());
        m_check = std::move(outcome);
    });

    if (!started) {
        m_detail->setText("update/busy"_i18n);
    }
}

void UpdateTab::startInstall()
{
    if (!m_check || !m_check->manifest.has_value()) {
        return;
    }

    m_installing = true;
    m_status->setValue("update/downloading"_i18n);
    m_action->setLabel("nsx/actions/cancel"_i18n);

    const core::UpdateManifest manifest = *m_check->manifest;

    const bool started = m_job.start([this, manifest]() {
        // Runs on the worker thread. Nothing here touches a view: progress goes
        // through publish() and the result through a value the UI reads only
        // after takeFinished().
        auto progress = [this](const infra::Progress& p) {
            if (m_job.cancelRequested()) {
                return false;
            }
            const int percent = p.total > 0 ? static_cast<int>((p.received * 100U) / p.total) : -1;
            m_job.publish({"update/downloading"_i18n, bytesToMegabytes(p.received), percent, 100});
            return true;
        };

        auto outcome = std::make_unique<domain::StageOutcome>(m_service.stage(manifest, progress));
        m_stage = std::move(outcome);
    });

    if (!started) {
        m_detail->setText("update/busy"_i18n);
        m_installing = false;
    }
}

void UpdateTab::refreshProgress()
{
    if (m_job.running()) {
        const BackgroundJob::Status status = m_job.poll();
        m_status->setValue(status.current >= 0
                               ? status.headline + "  " + std::to_string(status.current) + "%"
                               : status.headline);
        if (!status.detail.empty()) {
            m_detail->setText(status.detail);
        }
        return;
    }

    if (!m_job.takeFinished()) {
        return;
    }

    if (m_installing) {
        onInstallFinished();
    }
    else {
        onCheckFinished();
    }
}

void UpdateTab::onCheckFinished()
{
    if (!m_check) {
        setIdle("update/decision/unreadable"_i18n);
        return;
    }

    const std::string_view action = core::describe(m_check->decision.action);
    m_status->setValue(std::string(action));
    m_detail->setText(m_check->detail);
    m_action->setLabel(m_check->offersUpdate() ? "update/install_now"_i18n
                                               : "update/check_now"_i18n);
}

void UpdateTab::onInstallFinished()
{
    m_installing = false;
    m_action->setLabel("update/check_now"_i18n);

    if (!m_stage) {
        setIdle("update/decision/unreadable"_i18n);
        return;
    }

    if (!m_stage->readyToChainload()) {
        m_status->setValue(std::string(domain::describe(m_stage->result)));
        m_detail->setText(m_stage->detail.empty() ? "update/failure/untouched"_i18n
                                                  : m_stage->detail);
        return;
    }

    // Staged and verified. Confirm before taking the application away from the
    // user - a restart they did not ask for reads as a crash.
    auto* dialog = new brls::Dialog("update/progress/staged"_i18n);
    const std::string path = m_stage->forwarderPath;
    const std::string args = m_stage->forwarderArgs;

    dialog->addButton("nsx/actions/ok"_i18n, [this, dialog, path, args](brls::View*) {
        dialog->close([this, path, args]() {
            if (m_onChainload) {
                m_onChainload(path, args);
            }
        });
    });
    dialog->setCancelable(false);
    dialog->open();
}

}  // namespace nsx::ui
