// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

#include <borealis.hpp>

namespace nsx::ui {

/// @brief Measures and smooths download/transfer speed over time.
/// @since 0.3.0
class SpeedMeter
{
public:
    void update(std::uint64_t bytes)
    {
        const auto now = std::chrono::steady_clock::now();
        if (!m_started) {
            m_lastTime = now;
            m_lastBytes = bytes;
            m_started = true;
            return;
        }

        const auto elapsed = std::chrono::duration<double>(now - m_lastTime).count();
        if (elapsed >= 0.25) {
            if (bytes >= m_lastBytes) {
                const double diff = static_cast<double>(bytes - m_lastBytes);
                m_currentSpeed = diff / elapsed;
            }
            m_lastTime = now;
            m_lastBytes = bytes;
        }
    }

    [[nodiscard]] std::string format() const
    {
        if (m_currentSpeed <= 0.0) {
            return "";
        }
        char buf[64];
        if (m_currentSpeed >= 1024.0 * 1024.0) {
            std::snprintf(buf, sizeof(buf), "%.1f MB/s", m_currentSpeed / (1024.0 * 1024.0));
        }
        else if (m_currentSpeed >= 1024.0) {
            std::snprintf(buf, sizeof(buf), "%.0f KB/s", m_currentSpeed / 1024.0);
        }
        else {
            std::snprintf(buf, sizeof(buf), "%.0f B/s", m_currentSpeed);
        }
        return std::string(buf);
    }

    void reset()
    {
        m_started = false;
        m_currentSpeed = 0.0;
    }

private:
    bool m_started{false};
    std::chrono::steady_clock::time_point m_lastTime;
    std::uint64_t m_lastBytes{};
    double m_currentSpeed{0.0};
};

/// @brief Format byte quantities human-readably (e.g. "45.2 MB").
/// @since 0.3.0
inline std::string formatBytes(std::uint64_t bytes)
{
    char buf[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.2f GB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    else if (bytes >= 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    else if (bytes >= 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.0f KB", static_cast<double>(bytes) / 1024.0);
    }
    else {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return std::string(buf);
}

/// @brief A modal dialog showing real-time install progress.
///
/// @details Displays current stage, progress bar with percentage, and
///          a detail line (e.g. download speed, bytes/files transferred).
///          Provides cooperative atomic cancellation when the Cancel button
///          is clicked or controller button B is pressed.
/// @since 0.3.0
class ProgressDialog : public brls::Dialog
{
public:
    /// @brief Construct the progress dialog.
    /// @param title Header title (usually package or firmware name).
    /// @param onCancelCallback Called once when user presses B or clicks Cancel.
    explicit ProgressDialog(const std::string& title,
                            std::function<void()> onCancelCallback = nullptr);

    /// @brief Update the stage text (e.g., "Downloading...", "Extracting...").
    void setStage(const std::string& stage);

    /// @brief Update progress bar and percentage.
    /// @param current Current units (bytes or files); -1 for indeterminate.
    /// @param total Total units expected; 0 when unknown.
    void setProgress(int current, int total);

    /// @brief Update secondary detail (e.g., speed, file path).
    void setDetail(const std::string& detail);

    /// @brief Mark the dialog as cancelling.
    void setCancelling();

    /// @brief Intercept B button cancellation.
    bool onCancel() override;

private:
    brls::Label* m_titleLabel{};
    brls::Label* m_stageLabel{};
    brls::ProgressDisplay* m_progressDisplay{};
    brls::Label* m_detailLabel{};
    std::function<void()> m_onCancelCallback;
    bool m_cancelling{false};
};

}  // namespace nsx::ui
