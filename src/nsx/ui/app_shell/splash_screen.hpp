// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <borealis.hpp>

#include "nsx/domain/selfupdate/update_service.hpp"
#include "nsx/ui/app_shell/shell.hpp"

namespace nsx::ui {

/// @brief Preload splash screen with logo animation, shimmer sweep, and progress indicator.
/// @since 0.2.1
class SplashScreen : public brls::View
{
public:
    using CompletionCallback = std::function<void(const std::optional<domain::CheckOutcome>&)>;

    SplashScreen(const ShellServices& services, CompletionCallback onFinished);
    ~SplashScreen() override;

    void draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
              brls::Style* style, brls::FrameContext* ctx) override;

    bool isTranslucent() override { return false; }

private:
    void runSubsystemInit();

    const ShellServices& m_services;
    CompletionCallback m_onFinished;

    std::thread m_worker;
    std::mutex m_mutex;
    std::string m_stageText;
    std::atomic<float> m_targetProgress{0.05f};
    float m_currentProgress{0.0f};
    std::atomic<bool> m_backgroundDone{false};
    bool m_transitionStarted{false};

    std::optional<domain::CheckOutcome> m_updateOutcome;

    std::chrono::steady_clock::time_point m_startTime;
    int m_logoTexture{-1};
};

}  // namespace nsx::ui
