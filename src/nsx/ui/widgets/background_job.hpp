// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace nsx::ui {

/// @brief Runs one long operation off the UI thread and reports it back safely.
///
/// @details Borealis has no background-work facility. `TaskManager` holds
///          `RepeatingTask`s and runs them from `frame()`, on the UI thread, so
///          anything slow started from a click freezes the interface for its
///          whole duration. A firmware set is several hundred megabytes; a CFW
///          pack merge is thousands of files. Neither can run there.
///
///          So the work runs on a `std::thread` and this is the boundary
///          between it and the views:
///
///          - the worker **never touches a Borealis view**. It only calls
///            @ref publish, which takes a lock and stores plain values.
///          - the UI thread calls @ref poll once a frame and is the only side
///            that reads those values into widgets.
///          - @ref cancelRequested is an atomic the worker checks from inside
///            its progress callbacks, which is how B stops a download.
///
///          One job at a time by design. Two concurrent installs writing to the
///          same card is not a situation worth supporting, and @ref running
///          makes it easy to refuse.
///
/// @since 0.3.0
class BackgroundJob
{
public:
    /// @brief Progress as the worker last published it.
    /// @since 0.3.0
    struct Status
    {
        std::string headline;  ///< What is happening, already translated.
        std::string detail;    ///< The current file or version, when there is one.
        int current{};         ///< Progress numerator; -1 when indeterminate.
        int total{};           ///< Progress denominator; zero when unknown.
    };

    BackgroundJob() = default;
    ~BackgroundJob();

    BackgroundJob(const BackgroundJob&) = delete;
    BackgroundJob& operator=(const BackgroundJob&) = delete;

    /// @brief Start the work on its own thread.
    /// @param work What to run. Called once, on the worker thread.
    /// @return False when a job is already running.
    /// @note @p work must not touch Borealis. Use @ref publish.
    [[nodiscard]] bool start(std::function<void()> work);

    /// @brief Store progress from the worker thread.
    /// @param status What to show.
    /// @details Safe to call from the worker, and only from it.
    void publish(Status status);

    /// @brief Read the latest progress from the UI thread.
    /// @return A copy of what the worker last published.
    [[nodiscard]] Status poll() const;

    /// @brief Whether the worker is still going.
    /// @return True until the work returns.
    [[nodiscard]] bool running() const { return m_running.load(); }

    /// @brief Whether the work finished since the last call.
    /// @return True exactly once per completed job.
    /// @details Lets a `RepeatingTask` notice completion without the worker
    ///          having to reach into the view hierarchy to say so.
    [[nodiscard]] bool takeFinished();

    /// @brief Ask the work to stop.
    /// @details Sets a flag; the work decides when to honour it. Nothing is
    ///          killed, because a download interrupted mid-rename is exactly
    ///          what the install services are written to avoid.
    void requestCancel() { m_cancel.store(true); }

    /// @brief Whether a cancel was asked for.
    /// @return True once @ref requestCancel has been called.
    /// @details Read by the worker from inside its progress callbacks.
    [[nodiscard]] bool cancelRequested() const { return m_cancel.load(); }

    /// @brief Wait for the worker and release it.
    /// @details Called from the destructor. Also safe to call directly before
    ///          shutting the UI down, which matters: a thread still writing to
    ///          the SD card while the process exits is how a card gets a
    ///          half-written file.
    void join();

private:
    std::thread m_thread;
    mutable std::mutex m_mutex;
    Status m_status;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_finished{false};
    std::atomic<bool> m_cancel{false};
};

}  // namespace nsx::ui
