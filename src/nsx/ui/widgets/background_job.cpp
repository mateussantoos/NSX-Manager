// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/widgets/background_job.hpp"

#include <utility>

namespace nsx::ui {

BackgroundJob::~BackgroundJob()
{
    join();
}

bool BackgroundJob::start(std::function<void()> work)
{
    if (m_running.load()) {
        return false;
    }

    // A previous thread may still be joinable even though its work returned.
    join();

    m_cancel.store(false);
    m_finished.store(false);
    m_running.store(true);

    m_thread = std::thread([this, job = std::move(work)]() {
        job();
        // Order matters: `finished` is what the UI polls for, so it must not be
        // visible before the work has actually returned.
        m_running.store(false);
        m_finished.store(true);
    });

    return true;
}

void BackgroundJob::publish(Status status)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_status = std::move(status);
}

BackgroundJob::Status BackgroundJob::poll() const
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    return m_status;
}

bool BackgroundJob::takeFinished()
{
    return m_finished.exchange(false);
}

void BackgroundJob::join()
{
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

}  // namespace nsx::ui
