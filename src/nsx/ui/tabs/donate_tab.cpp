// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/donate_tab.hpp"

#include <string>

namespace nsx::ui {

using namespace brls::i18n::literals;

DonateTab::DonateTab() = default;

DonateTab::~DonateTab()
{
    if (m_qrTexture > 0) {
        NVGcontext* vg = brls::Application::getNVGContext();
        if (vg) {
            nvgDeleteImage(vg, m_qrTexture);
        }
        m_qrTexture = -1;
    }
}

void DonateTab::draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
                     brls::Style* /*style*/, brls::FrameContext* ctx)
{
    const auto curX = static_cast<float>(viewX);
    const auto curY = static_cast<float>(viewY);
    const auto totalW = static_cast<float>(viewW);
    const auto totalH = static_cast<float>(viewH);

    // 1. OLED True Black Background (#000000)
    nvgBeginPath(vg);
    nvgRect(vg, curX, curY, totalW, totalH);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);

    // 2. Centered Card Container
    constexpr float cardW = 580.0f;
    constexpr float cardH = 550.0f;
    const float cardX = curX + (totalW - cardW) * 0.5f;
    const float cardY = curY + (totalH - cardH) * 0.5f;

    // Outer card body
    nvgBeginPath(vg);
    nvgRoundedRect(vg, cardX, cardY, cardW, cardH, 16.0f);
    nvgFillColor(vg, nvgRGB(14, 14, 14));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(38, 38, 38));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);

    // Top Crimson Accent line
    nvgBeginPath(vg);
    nvgRoundedRect(vg, cardX + 32.0f, cardY, cardW - 64.0f, 3.0f, 1.5f);
    nvgFillColor(vg, nvgRGB(230, 0, 18));
    nvgFill(vg);

    // Title: "Apoiar o Projeto" / "Support the Project"
    const std::string titleText = "donate/title"_i18n;
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 24.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgText(vg, cardX + cardW * 0.5f, cardY + 28.0f, titleText.c_str(), nullptr);

    // 3. High-Resolution QR Code Frame with White Plate Quiet Zone
    constexpr float plateSize = 250.0f;
    const float plateX = cardX + (cardW - plateSize) * 0.5f;
    const float plateY = cardY + 76.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, plateX, plateY, plateSize, plateSize, 12.0f);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(230, 0, 18, 60));
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);

    // Load QR texture lazily
    if (m_qrTexture <= 0) {
        m_qrTexture = nvgCreateImage(vg, BOREALIS_ASSET("images/qr.png"), 0);
        if (m_qrTexture <= 0) {
            m_qrTexture = nvgCreateImage(vg, BOREALIS_ASSET("qr.png"), 0);
        }
    }

    constexpr float qrSize = 230.0f;
    const float qrX = plateX + (plateSize - qrSize) * 0.5f;
    const float qrY = plateY + (plateSize - qrSize) * 0.5f;

    if (m_qrTexture > 0) {
        const NVGpaint imgPaint =
            nvgImagePattern(vg, qrX, qrY, qrSize, qrSize, 0.0f, m_qrTexture, 1.0f);
        nvgBeginPath(vg);
        nvgRect(vg, qrX, qrY, qrSize, qrSize);
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);
    }

    // 4. Subtitle / Description Text
    const std::string subtitleText = "donate/subtitle"_i18n;
    nvgFontSize(vg, 15.0f);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(180, 180, 180));
    nvgTextBox(vg, cardX + 36.0f, cardY + 348.0f, cardW - 72.0f, subtitleText.c_str(), nullptr);

    // 5. Pill Badge: "PIX / Doações" / "PIX / Donations"
    const std::string badgeText = "donate/badge"_i18n;
    constexpr float badgeW = 200.0f;
    constexpr float badgeH = 32.0f;
    const float badgeX = cardX + (cardW - badgeW) * 0.5f;
    const float badgeY = cardY + 438.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, badgeX, badgeY, badgeW, badgeH, 16.0f);
    nvgFillColor(vg, nvgRGBA(230, 0, 18, 30));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(230, 0, 18, 120));
    nvgStrokeWidth(vg, 1.2f);
    nvgStroke(vg);

    nvgFontSize(vg, 14.0f);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(255, 110, 110));
    nvgText(vg, badgeX + badgeW * 0.5f, badgeY + badgeH * 0.5f, badgeText.c_str(), nullptr);

    // Footer note
    const std::string hintText = "donate/hint"_i18n;
    nvgFontSize(vg, 12.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(110, 110, 110));
    nvgText(vg, cardX + cardW * 0.5f, cardY + 484.0f, hintText.c_str(), nullptr);
}

}  // namespace nsx::ui
