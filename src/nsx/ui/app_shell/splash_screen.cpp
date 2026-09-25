// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/app_shell/splash_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace nsx::ui {

using namespace brls::i18n::literals;

namespace {

bool fileReadable(const char* path)
{
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return true;
}

}  // namespace

SplashScreen::SplashScreen(const ShellServices& services, CompletionCallback onFinished)
    : m_services(services),
      m_onFinished(std::move(onFinished)),
      m_startTime(std::chrono::steady_clock::now())
{
    m_stageText = "nsx/splash/init"_i18n;
    m_worker = std::thread([this]() { runSubsystemInit(); });
}

SplashScreen::~SplashScreen()
{
    if (m_worker.joinable()) {
        m_worker.join();
    }
    if (m_logoTexture > 0) {
        nvgDeleteImage(brls::Application::getNVGContext(), m_logoTexture);
        m_logoTexture = -1;
    }
}

void SplashScreen::runSubsystemInit()
{
    // Stage 1: Hardware and storage probe
    {
        std::lock_guard lock(m_mutex);
        m_stageText = "nsx/splash/system"_i18n;
    }
    m_targetProgress = 0.30f;
    if (m_services.querySystemOverview) {
        (void)m_services.querySystemOverview();
    }
    (void)m_services.files.freeSpaceBytes("/");

    // Stage 2: Telemetry protection / network connectivity
    {
        std::lock_guard lock(m_mutex);
        m_stageText = "nsx/splash/network"_i18n;
    }
    m_targetProgress = 0.65f;
    (void)m_services.telemetry.checkProtection();

    // Stage 3: Update and manifest cache probe
    {
        std::lock_guard lock(m_mutex);
        m_stageText = "nsx/splash/manifest"_i18n;
    }
    m_targetProgress = 0.90f;
    m_updateOutcome = m_services.update.check();

    // Stage 4: Initialization complete
    {
        std::lock_guard lock(m_mutex);
        m_stageText = "nsx/splash/ready"_i18n;
    }
    m_targetProgress = 1.0f;
    m_backgroundDone = true;
}

void SplashScreen::draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
                        brls::Style* /*style*/, brls::FrameContext* ctx)
{
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - m_startTime).count();

    // 1. Pure black background (#000000)
    nvgBeginPath(vg);
    nvgRect(vg, static_cast<float>(viewX), static_cast<float>(viewY), static_cast<float>(viewW),
            static_cast<float>(viewH));
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);

    // 2. Centered splash logo with smooth entrance fade-in
    if (m_logoTexture <= 0) {
        const char* splashPath = fileReadable(BOREALIS_ASSET("images/splash.png"))
                                     ? BOREALIS_ASSET("images/splash.png")
                                     : BOREALIS_ASSET("splash.png");
        m_logoTexture = nvgCreateImage(vg, splashPath, 0);
    }

    const float logoW = 412.0f;
    const float logoH = 210.0f;
    const float logoX = static_cast<float>(viewX) + (static_cast<float>(viewW) - logoW) * 0.5f;
    const float logoY =
        static_cast<float>(viewY) + (static_cast<float>(viewH) - logoH) * 0.5f - 24.0f;

    const float logoAlpha = std::clamp(elapsed / 0.7f, 0.0f, 1.0f);

    if (m_logoTexture > 0 && logoAlpha > 0.001f) {
        const NVGpaint imgPaint =
            nvgImagePattern(vg, logoX, logoY, logoW, logoH, 0.0f, m_logoTexture, logoAlpha);
        nvgBeginPath(vg);
        nvgRect(vg, logoX, logoY, logoW, logoH);
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);

        // 3. Shimmer / skeleton sweep effect across the logo
        if (logoAlpha > 0.1f) {
            constexpr float kCycleDuration = 1.6f;
            const float cycleFrac = std::fmod(elapsed, kCycleDuration) / kCycleDuration;
            const float sweepSpan = logoW + 200.0f;
            const float streakX = (logoX - 100.0f) + cycleFrac * sweepSpan;

            nvgSave(vg);
            nvgScissor(vg, logoX, logoY, logoW, logoH);

            // Diagonal light band with soft linear gradient edges
            const float streakW = 90.0f;
            const NVGpaint shimmerGrad = nvgLinearGradient(
                vg, streakX - streakW * 0.5f, logoY, streakX + streakW * 0.5f, logoY,
                nvgRGBA(255, 255, 255, 0),
                nvgRGBA(255, 255, 255, static_cast<unsigned char>(40.0f * logoAlpha)));

            nvgBeginPath(vg);
            nvgMoveTo(vg, streakX - 35.0f, logoY);
            nvgLineTo(vg, streakX + 35.0f, logoY);
            nvgLineTo(vg, streakX + 15.0f, logoY + logoH);
            nvgLineTo(vg, streakX - 55.0f, logoY + logoH);
            nvgClosePath(vg);
            nvgFillPaint(vg, shimmerGrad);
            nvgFill(vg);

            // Intense inner highlight beam
            const NVGpaint coreGrad = nvgLinearGradient(
                vg, streakX - 15.0f, logoY, streakX + 15.0f, logoY, nvgRGBA(255, 255, 255, 0),
                nvgRGBA(255, 255, 255, static_cast<unsigned char>(70.0f * logoAlpha)));

            nvgBeginPath(vg);
            nvgMoveTo(vg, streakX - 10.0f, logoY);
            nvgLineTo(vg, streakX + 10.0f, logoY);
            nvgLineTo(vg, streakX - 10.0f, logoY + logoH);
            nvgLineTo(vg, streakX - 30.0f, logoY + logoH);
            nvgClosePath(vg);
            nvgFillPaint(vg, coreGrad);
            nvgFill(vg);

            nvgResetScissor(vg);
            nvgRestore(vg);
        }
    }

    // 4. Modern, slim horizontal loading progress bar beneath the logo
    // Track: #1A1A1A, Fill: #E60012
    const float barW = 320.0f;
    const float barH = 3.5f;
    const float barX = static_cast<float>(viewX) + (static_cast<float>(viewW) - barW) * 0.5f;
    const float barY = logoY + logoH + 36.0f;

    // Smooth progress interpolation
    m_currentProgress += (m_targetProgress.load() - m_currentProgress) * 0.10f;

    // Track (#1A1A1A)
    nvgBeginPath(vg);
    nvgRoundedRect(vg, barX, barY, barW, barH, 2.0f);
    nvgFillColor(vg, nvgRGB(26, 26, 26));
    nvgFill(vg);

    // Fill (#E60012)
    const float fillW = std::clamp(m_currentProgress, 0.0f, 1.0f) * barW;
    if (fillW > 2.0f) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, barX, barY, fillW, barH, 2.0f);
        nvgFillColor(vg, nvgRGB(230, 0, 18));
        nvgFill(vg);
    }

    // Stage text indicator
    std::string stage;
    {
        std::lock_guard lock(m_mutex);
        stage = m_stageText;
    }
    if (!stage.empty() && logoAlpha > 0.05f) {
        nvgFontSize(vg, 13.0f);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(115, 115, 115, static_cast<unsigned char>(255 * logoAlpha)));
        nvgText(vg, static_cast<float>(viewX) + static_cast<float>(viewW) * 0.5f, barY + 14.0f,
                stage.c_str(), nullptr);
    }

    // 5. Check if ready to transition to AppShell
    constexpr float kMinSplashTime = 1.8f;
    if (m_backgroundDone.load() && elapsed >= kMinSplashTime && m_currentProgress >= 0.95f) {
        if (!m_transitionStarted) {
            m_transitionStarted = true;
            if (m_onFinished) {
                m_onFinished(m_updateOutcome);
            }
        }
    }
}

}  // namespace nsx::ui
