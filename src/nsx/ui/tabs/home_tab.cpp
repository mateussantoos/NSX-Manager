// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/tabs/home_tab.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include "nsx/core/version/version.hpp"

namespace nsx::ui {

using namespace brls::i18n::literals;

// ============================================================================
// DashboardSummaryView
// ============================================================================

DashboardSummaryView::DashboardSummaryView(domain::FileStore& files, SystemOverviewQuery query)
    : m_files(files), m_queryOverview(std::move(query))
{
    this->setHeight(320);
    refresh();
}

DashboardSummaryView::~DashboardSummaryView()
{
    NVGcontext* vg = brls::Application::getNVGContext();
    if (vg) {
        if (m_joyconsImg > 0) {
            nvgDeleteImage(vg, m_joyconsImg);
        }
        if (m_amsImg > 0) {
            nvgDeleteImage(vg, m_amsImg);
        }
        if (m_sdImg > 0) {
            nvgDeleteImage(vg, m_sdImg);
        }
    }
}

void DashboardSummaryView::refresh()
{
    if (m_queryOverview) {
        m_overview = m_queryOverview();
    }
    updateStorage();
}

void DashboardSummaryView::willAppear(bool resetState)
{
    refresh();
    brls::View::willAppear(resetState);
}

void DashboardSummaryView::updateStorage()
{
    const auto freeOpt = m_files.freeSpaceBytes("/");
    const auto totalOpt = m_files.totalSpaceBytes("/");

    if (freeOpt.has_value()) {
        const double freeGB = static_cast<double>(*freeOpt) / (1024.0 * 1024.0 * 1024.0);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f GB", freeGB);
        m_storageFreeStr = buf;

        if (totalOpt.has_value() && *totalOpt > 0) {
            const double totalGB = static_cast<double>(*totalOpt) / (1024.0 * 1024.0 * 1024.0);
            m_storagePct = static_cast<float>(std::clamp(freeGB / totalGB, 0.02, 1.0));
        }
        else {
            m_storagePct = 0.5f;
        }
    }
    else {
        m_storageFreeStr = "-- GB";
        m_storagePct = 0.5f;
    }
}

void DashboardSummaryView::updateTelemetry(const infra::TelemetryReport& report)
{
    std::lock_guard lock(m_mutex);
    m_telemetryChecked = true;
    m_telemetryStatus = report.overall;

    if (report.overall == infra::TelemetryStatus::Protected) {
        m_telemetryText = "Telemetria: Bloqueada (Seguro)";
    }
    else if (report.overall == infra::TelemetryStatus::Unshielded) {
        m_telemetryText = "Telemetria: Desprotegida (Alerta!)";
    }
    else {
        m_telemetryText = "Rede: Desconectada (Offline)";
    }
}

void DashboardSummaryView::updateUpdateOutcome(const domain::CheckOutcome& outcome)
{
    std::lock_guard lock(m_mutex);
    if (outcome.offersUpdate()) {
        m_hasUpdate = true;
        m_updateStatusStr = "NSX Manager: Nova versao disponivel";
    }
    else {
        m_hasUpdate = false;
        m_updateStatusStr = "NSX Manager: Atualizado";
    }
}

void DashboardSummaryView::updateMotd(const std::optional<domain::motd::MessageOfTheDay>& bulletin)
{
    std::lock_guard lock(m_mutex);
    m_motd = bulletin;
    if (m_motd.has_value()) {
        this->setHeight(376);
    }
    else {
        this->setHeight(320);
    }
}

void DashboardSummaryView::drawImageSafe(NVGcontext* vg, int& textureId, const std::string& path,
                                         float imgX, float imgY, float imgW, float imgH)
{
    if (textureId <= 0) {
        textureId = nvgCreateImage(vg, path.c_str(), 0);
    }
    if (textureId > 0) {
        const NVGpaint imgPaint =
            nvgImagePattern(vg, imgX, imgY, imgW, imgH, 0.0f, textureId, 1.0f);
        nvgBeginPath(vg);
        nvgRect(vg, imgX, imgY, imgW, imgH);
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);
    }
}

void DashboardSummaryView::drawChipIcon(NVGcontext* vg, float cx, float cy, float size)
{
    const float half = size * 0.5f;
    const float rx = cx - half;
    const float ry = cy - half;

    // Outer chip body (modern dark purple)
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rx + 4.0f, ry + 4.0f, size - 8.0f, size - 8.0f, 4.0f);
    nvgFillColor(vg, nvgRGB(88, 52, 140));
    nvgFill(vg);

    // Inner core
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rx + 8.0f, ry + 8.0f, size - 16.0f, size - 16.0f, 2.0f);
    nvgFillColor(vg, nvgRGB(58, 30, 96));
    nvgFill(vg);

    // Pins on top and bottom
    nvgFillColor(vg, nvgRGB(180, 160, 220));
    for (int i = 0; i < 3; i++) {
        const float px = rx + 8.0f + static_cast<float>(i) * 5.5f;
        nvgBeginPath(vg);
        nvgRect(vg, px, ry + 1.0f, 2.0f, 3.0f);
        nvgRect(vg, px, ry + size - 4.0f, 2.0f, 3.0f);
        nvgFill(vg);
    }
    // Pins on left and right
    for (int i = 0; i < 3; i++) {
        const float py = ry + 8.0f + static_cast<float>(i) * 5.5f;
        nvgBeginPath(vg);
        nvgRect(vg, rx + 1.0f, py, 3.0f, 2.0f);
        nvgRect(vg, rx + size - 4.0f, py, 3.0f, 2.0f);
        nvgFill(vg);
    }
}

void DashboardSummaryView::drawWifiSignal(NVGcontext* vg, float cx, float cy, int bars)
{
    constexpr float barW = 3.5f;
    constexpr float barSpacing = 2.5f;
    constexpr float heights[3] = {6.0f, 10.0f, 15.0f};
    const float startX = cx - (3.0f * barW + 2.0f * barSpacing) * 0.5f;
    const float baseY = cy + 7.5f;

    for (int i = 0; i < 3; ++i) {
        const float bx = startX + static_cast<float>(i) * (barW + barSpacing);
        const float bh = heights[i];
        const float by = baseY - bh;

        nvgBeginPath(vg);
        nvgRoundedRect(vg, bx, by, barW, bh, 1.0f);
        if (i < bars) {
            nvgFillColor(vg, nvgRGB(46, 204, 113));
        }
        else {
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 40));
        }
        nvgFill(vg);
    }
}

void DashboardSummaryView::draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW,
                                unsigned viewH, brls::Style*, brls::FrameContext* ctx)
{
    (void)viewH;
    const auto totalW = static_cast<float>(viewW);
    const auto curX = static_cast<float>(viewX);
    const auto curY = static_cast<float>(viewY);

    // ==========================================
    // 0. MOTD BANNER (IF ACTIVE)
    // ==========================================
    float motdOffset = 0.0f;
    std::optional<domain::motd::MessageOfTheDay> activeMotd;
    {
        std::lock_guard lock(m_mutex);
        activeMotd = m_motd;
    }

    if (activeMotd.has_value()) {
        const float bannerY = curY;
        constexpr float bannerH = 44.0f;
        motdOffset = bannerH + 12.0f;

        NVGcolor borderColor;
        NVGcolor bgColor;
        if (activeMotd->severity == "error") {
            borderColor = nvgRGB(231, 76, 60);
            bgColor = nvgRGBA(231, 76, 60, 30);
        }
        else if (activeMotd->severity == "warning") {
            borderColor = nvgRGB(243, 156, 18);
            bgColor = nvgRGBA(243, 156, 18, 30);
        }
        else {
            borderColor = nvgRGB(52, 152, 219);
            bgColor = nvgRGBA(52, 152, 219, 30);
        }

        nvgBeginPath(vg);
        nvgRoundedRect(vg, curX, bannerY, totalW, bannerH, 8.0f);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);
        nvgStrokeColor(vg, borderColor);
        nvgStrokeWidth(vg, 1.2f);
        nvgStroke(vg);

        // Alert Tag Badge
        nvgBeginPath(vg);
        nvgRoundedRect(vg, curX + 12.0f, bannerY + 11.0f, 56.0f, 22.0f, 4.0f);
        nvgFillColor(vg, borderColor);
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgFontSize(vg, 11.0f);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, curX + 40.0f, bannerY + 22.0f, "AVISO", nullptr);

        // Title and Message
        nvgBeginPath(vg);
        nvgFontSize(vg, 13.5f);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        std::string bannerText = activeMotd->title;
        if (!activeMotd->message.empty()) {
            bannerText += " - " + activeMotd->message;
        }
        nvgText(vg, curX + 78.0f, bannerY + 22.0f, bannerText.c_str(), nullptr);
    }

    // ==========================================
    // 1. SECTION 1 HEADER: Resumo do sistema
    // ==========================================
    const float sec1Y = curY + motdOffset;
    nvgBeginPath(vg);
    nvgFillColor(vg, nvgRGB(230, 0, 18));
    nvgRect(vg, curX, sec1Y + 2.0f, 4.0f, 18.0f);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 18.0f);
    nvgFillColor(vg, nvgRGB(240, 240, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, curX + 12.0f, sec1Y + 11.0f, "Resumo do Sistema", nullptr);

    // ==========================================
    // 2. BLOCK 1: TOP 4 CARDS
    // ==========================================
    constexpr float gap = 12.0f;
    const float cardW = (totalW - gap * 3.0f) / 4.0f;
    constexpr float cardH = 68.0f;
    const float cardsY = sec1Y + 26.0f;

    for (int i = 0; i < 4; ++i) {
        const float cx = curX + static_cast<float>(i) * (cardW + gap);

        // Subtle dark card background (#141414 / #1C1C1C) and refined border
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cx, cardsY, cardW, cardH, 8.0f);
        nvgFillColor(vg, nvgRGB(20, 23, 27));
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGB(36, 40, 48));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        if (i == 0) {
            // Hardware Card (Switch model + SoC stepping)
            drawImageSafe(vg, m_joyconsImg, BOREALIS_ASSET("images/joycons.png"), cx + 10.0f,
                          cardsY + 16.0f, 38.0f, 36.0f);

            nvgBeginPath(vg);
            nvgFontFaceId(vg, ctx->fontStash->regular);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGB(142, 146, 152));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, cx + 54.0f, cardsY + 18.0f, "Hardware", nullptr);

            nvgBeginPath(vg);
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgText(vg, cx + 54.0f, cardsY + 36.0f, m_overview.model.c_str(), nullptr);

            nvgBeginPath(vg);
            nvgFontSize(vg, 11.5f);
            nvgFillColor(vg, nvgRGB(52, 152, 219));
            nvgText(vg, cx + 54.0f, cardsY + 52.0f, m_overview.socStepping.c_str(), nullptr);
        }
        else if (i == 1) {
            // System Version Card (HOS version + Atmosphere version + NAND mode badge)
            drawChipIcon(vg, cx + 24.0f, cardsY + 34.0f, 32.0f);

            nvgBeginPath(vg);
            nvgFontFaceId(vg, ctx->fontStash->regular);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGB(142, 146, 152));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, cx + 46.0f, cardsY + 18.0f, "Sistema / AMS", nullptr);

            // NAND Mode badge
            const float nandBadgeW = 54.0f;
            const float nandBadgeX = cx + cardW - nandBadgeW - 8.0f;
            const float nandBadgeY = cardsY + 10.0f;
            const bool isEmu = (m_overview.nandType.find("Emu") != std::string::npos);

            nvgBeginPath(vg);
            nvgRoundedRect(vg, nandBadgeX, nandBadgeY, nandBadgeW, 15.0f, 3.5f);
            nvgFillColor(vg, isEmu ? nvgRGBA(155, 89, 182, 40) : nvgRGBA(52, 152, 219, 40));
            nvgFill(vg);
            nvgStrokeColor(vg, isEmu ? nvgRGB(155, 89, 182) : nvgRGB(52, 152, 219));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgFontSize(vg, 9.5f);
            nvgFillColor(vg, isEmu ? nvgRGB(180, 130, 220) : nvgRGB(90, 180, 240));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, nandBadgeX + nandBadgeW * 0.5f, nandBadgeY + 7.5f,
                    m_overview.nandType.c_str(), nullptr);

            // Horizon OS
            nvgBeginPath(vg);
            nvgFontSize(vg, 14.0f);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            const std::string hosText = "HOS: " + m_overview.hosVersion;
            nvgText(vg, cx + 46.0f, cardsY + 36.0f, hosText.c_str(), nullptr);

            // Atmosphere
            nvgBeginPath(vg);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGB(46, 204, 113));
            const std::string amsText = "AMS: " + m_overview.amsVersion;
            nvgText(vg, cx + 46.0f, cardsY + 52.0f, amsText.c_str(), nullptr);
        }
        else if (i == 2) {
            // Live Network Card
            if (m_overview.networkMedium == "Wi-Fi") {
                drawWifiSignal(vg, cx + 22.0f, cardsY + 34.0f, m_overview.wifiSignalBars);
            }
            else {
                nvgBeginPath(vg);
                nvgCircle(vg, cx + 22.0f, cardsY + 34.0f, 10.0f);
                nvgFillColor(vg,
                             m_overview.isConnected ? nvgRGB(46, 204, 113) : nvgRGB(142, 146, 152));
                nvgFill(vg);
            }

            nvgBeginPath(vg);
            nvgFontFaceId(vg, ctx->fontStash->regular);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGB(142, 146, 152));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, cx + 44.0f, cardsY + 18.0f, m_overview.networkMedium.c_str(), nullptr);

            // 90DNS Shield badge
            const float shieldBadgeW = 50.0f;
            const float shieldBadgeX = cx + cardW - shieldBadgeW - 8.0f;
            const float shieldBadgeY = cardsY + 10.0f;
            const bool isShielded = (m_telemetryStatus == infra::TelemetryStatus::Protected);

            nvgBeginPath(vg);
            nvgRoundedRect(vg, shieldBadgeX, shieldBadgeY, shieldBadgeW, 15.0f, 3.5f);
            nvgFillColor(vg, isShielded ? nvgRGBA(46, 204, 113, 40) : nvgRGBA(231, 76, 60, 40));
            nvgFill(vg);
            nvgStrokeColor(vg, isShielded ? nvgRGB(46, 204, 113) : nvgRGB(231, 76, 60));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgFontSize(vg, 9.5f);
            nvgFillColor(vg, isShielded ? nvgRGB(46, 204, 113) : nvgRGB(231, 76, 60));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, shieldBadgeX + shieldBadgeW * 0.5f, shieldBadgeY + 7.5f,
                    isShielded ? "90DNS" : "ALERTA", nullptr);

            // Middle: SSID or Connection status
            nvgBeginPath(vg);
            nvgFontSize(vg, 14.0f);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            const std::string netTitle =
                m_overview.isConnected ? (!m_overview.ssid.empty() ? m_overview.ssid : "Conectado")
                                       : "Desconectado";
            nvgText(vg, cx + 44.0f, cardsY + 36.0f, netTitle.c_str(), nullptr);

            // Bottom: IP address
            nvgBeginPath(vg);
            nvgFontSize(vg, 11.5f);
            nvgFillColor(vg, nvgRGB(142, 146, 152));
            const std::string ipStr = m_overview.ipAddress.empty() ? "--" : m_overview.ipAddress;
            nvgText(vg, cx + 44.0f, cardsY + 52.0f, ipStr.c_str(), nullptr);
        }
        else if (i == 3) {
            // SD Storage & Gauge Card
            drawImageSafe(vg, m_sdImg, BOREALIS_ASSET("images/sdcard.png"), cx + 14.0f,
                          cardsY + 16.0f, 26.0f, 36.0f);

            nvgBeginPath(vg);
            nvgFontFaceId(vg, ctx->fontStash->regular);
            nvgFontSize(vg, 12.0f);
            nvgFillColor(vg, nvgRGB(142, 146, 152));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, cx + 48.0f, cardsY + 18.0f, "MicroSD", nullptr);

            // Filesystem Badge (FAT32 green / exFAT orange)
            const float badgeW = m_overview.isExFAT ? 58.0f : 50.0f;
            const float badgeX = cx + cardW - badgeW - 8.0f;
            const float badgeY = cardsY + 10.0f;

            nvgBeginPath(vg);
            nvgRoundedRect(vg, badgeX, badgeY, badgeW, 15.0f, 3.5f);
            if (m_overview.isExFAT) {
                nvgFillColor(vg, nvgRGBA(230, 126, 34, 40));
                nvgFill(vg);
                nvgStrokeColor(vg, nvgRGB(230, 126, 34));
            }
            else {
                nvgFillColor(vg, nvgRGBA(46, 204, 113, 40));
                nvgFill(vg);
                nvgStrokeColor(vg, nvgRGB(46, 204, 113));
            }
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgFontSize(vg, 9.5f);
            nvgFillColor(vg, m_overview.isExFAT ? nvgRGB(230, 126, 34) : nvgRGB(46, 204, 113));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, badgeX + badgeW * 0.5f, badgeY + 7.5f, m_overview.fsType.c_str(), nullptr);

            // Free space number
            nvgBeginPath(vg);
            nvgFontSize(vg, 16.0f);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, cx + 48.0f, cardsY + 36.0f, m_storageFreeStr.c_str(), nullptr);

            // Capacity bar gauge
            const float barW = cardW - 58.0f;
            const float barX = cx + 48.0f;
            const float barY = cardsY + 48.0f;

            nvgBeginPath(vg);
            nvgRoundedRect(vg, barX, barY, barW, 4.0f, 2.0f);
            nvgFillColor(vg, nvgRGB(36, 40, 48));
            nvgFill(vg);

            nvgBeginPath(vg);
            nvgRoundedRect(vg, barX, barY, barW * m_storagePct, 4.0f, 2.0f);
            nvgFillColor(vg, nvgRGB(46, 204, 113));
            nvgFill(vg);
        }
    }

    // ==========================================
    // 3. BLOCK 2: MIDDLE SECTION (2 SIDE-BY-SIDE CARDS)
    // ==========================================
    const float midY = cardsY + cardH + 16.0f;
    constexpr float midGap = 14.0f;
    const float midW = (totalW - midGap) / 2.0f;
    constexpr float midH = 176.0f;

    // --- LEFT HEADER: Status & Protection ---
    nvgBeginPath(vg);
    nvgFillColor(vg, nvgRGB(230, 0, 18));
    nvgRect(vg, curX, midY + 2.0f, 4.0f, 18.0f);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 18.0f);
    nvgFillColor(vg, nvgRGB(240, 240, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, curX + 12.0f, midY + 11.0f, "Status do Sistema & Seguranca", nullptr);

    // --- RIGHT HEADER: Updates & Platform ---
    const float rightX = curX + midW + midGap;

    nvgBeginPath(vg);
    nvgFillColor(vg, nvgRGB(243, 156, 18));
    nvgRect(vg, rightX, midY + 2.0f, 4.0f, 18.0f);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 18.0f);
    nvgFillColor(vg, nvgRGB(240, 240, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, rightX + 12.0f, midY + 11.0f, "NSX Manager & Plataforma", nullptr);

    // --- LEFT CARD BODY ---
    const float cardBodyY = midY + 26.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, curX, cardBodyY, midW, midH, 8.0f);
    nvgFillColor(vg, nvgRGB(20, 23, 27));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(36, 40, 48));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Diagnostics bullets
    struct Bullet
    {
        NVGcolor color;
        std::string text;
    };

    std::vector<Bullet> bullets;
    bullets.push_back({nvgRGB(46, 204, 113), "Atmosphere (AMS) ativo: " + m_overview.amsVersion});

    {
        std::lock_guard lock(m_mutex);
        if (!m_telemetryChecked) {
            bullets.push_back({nvgRGB(160, 160, 160), "Verificando escudo de telemetria..."});
        }
        else if (m_telemetryStatus == infra::TelemetryStatus::Protected) {
            bullets.push_back({nvgRGB(46, 204, 113), m_telemetryText});
        }
        else if (m_telemetryStatus == infra::TelemetryStatus::Unshielded) {
            bullets.push_back({nvgRGB(231, 76, 60), m_telemetryText});
        }
        else {
            bullets.push_back({nvgRGB(243, 156, 18), m_telemetryText});
        }
    }

    bullets.push_back({nvgRGB(52, 152, 219), "Modo NAND: " + m_overview.nandType});

    std::string fsDesc = "Sistema de Arquivos: " + m_overview.fsType;
    fsDesc += m_overview.isExFAT ? " (risco de corrupcao)" : " (ideal)";
    bullets.push_back({m_overview.isExFAT ? nvgRGB(230, 126, 34) : nvgRGB(46, 204, 113), fsDesc});

    bullets.push_back(
        {nvgRGB(52, 152, 219), "Firmware Horizon instalado: " + m_overview.hosVersion});

    const float bulletStartY = cardBodyY + 20.0f;
    for (size_t b = 0; b < bullets.size(); ++b) {
        const float by = bulletStartY + static_cast<float>(b) * 22.0f;

        // Dot
        nvgBeginPath(vg);
        nvgCircle(vg, curX + 18.0f, by, 3.5f);
        nvgFillColor(vg, bullets[b].color);
        nvgFill(vg);

        // Text
        nvgBeginPath(vg);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgFontSize(vg, 14.0f);
        nvgFillColor(vg, nvgRGB(226, 232, 240));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, curX + 30.0f, by, bullets[b].text.c_str(), nullptr);
    }

    // Bottom Badge in Left Card
    const float badgeY = cardBodyY + midH - 34.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, curX + 16.0f, badgeY, midW - 32.0f, 24.0f, 4.0f);

    const bool isShielded = (m_telemetryStatus == infra::TelemetryStatus::Protected);
    nvgFillColor(vg, isShielded ? nvgRGBA(46, 204, 113, 25) : nvgRGBA(231, 76, 60, 25));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 13.5f);
    nvgFillColor(vg, isShielded ? nvgRGB(46, 204, 113) : nvgRGB(231, 76, 60));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, curX + 26.0f, badgeY + 12.0f,
            isShielded ? "Sistema Protegido contra Telemetria." : "Atencao: Conexao desprotegida.",
            nullptr);

    // --- RIGHT CARD BODY ---
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rightX, cardBodyY, midW, midH, 8.0f);
    nvgFillColor(vg, nvgRGB(20, 23, 27));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(36, 40, 48));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Description text
    const std::string desc =
        "O NSX Manager e uma solucao moderna e segura em C++20 para gerenciamento de pacotes "
        "Atmosphere (AMS), firmwares e manutencao avancada do Nintendo Switch.";
    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 13.5f);
    nvgFillColor(vg, nvgRGB(0, 220, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(vg, rightX + 16.0f, cardBodyY + 16.0f, midW - 32.0f, desc.c_str(), nullptr);

    // App Version badge
    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 14.0f);
    nvgFillColor(vg, nvgRGB(240, 240, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    std::string appStr = "Versao Instalada: ";
    appStr += core::version::kString;
    nvgText(vg, rightX + 16.0f, cardBodyY + midH - 42.0f, appStr.c_str(), nullptr);

    // Server Status Indicator
    nvgBeginPath(vg);
    nvgCircle(vg, rightX + 22.0f, cardBodyY + midH - 20.0f, 4.0f);
    nvgFillColor(vg, nvgRGB(46, 204, 113));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgFontSize(vg, 13.0f);
    nvgFillColor(vg, nvgRGB(46, 204, 113));
    nvgText(vg, rightX + 32.0f, cardBodyY + midH - 20.0f, "Canal Oficial (GitHub Releases)",
            nullptr);
}

// ============================================================================
// DashboardActionButton
// ============================================================================

DashboardActionButton::DashboardActionButton(std::string top, std::string bottom, int iconType,
                                             std::function<void()> onClickCb)
    : brls::Button(brls::ButtonStyle::BORDERLESS),
      m_topText(std::move(top)),
      m_bottomText(std::move(bottom)),
      m_iconType(iconType),
      m_callback(std::move(onClickCb))
{
    this->setWidth(220);
    this->setHeight(68);
}

void DashboardActionButton::layout(NVGcontext*, brls::Style*, brls::FontStash*)
{
    if (this->getParent() && this->getParent()->getWidth() > 100) {
        const unsigned parentW = this->getParent()->getWidth();
        const unsigned btnW = (parentW - 3 * 12) / 4;
        this->setWidth(btnW);
    }
    this->setHeight(68);
}

bool DashboardActionButton::onClick()
{
    if (m_callback) {
        m_callback();
        return true;
    }
    return false;
}

void DashboardActionButton::draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW,
                                 unsigned viewH, brls::Style*, brls::FrameContext* ctx)
{
    const bool isFoc = this->isFocused();
    m_focusAnim += ((isFoc ? 1.0f : 0.0f) - m_focusAnim) * 0.22f;
    m_pulse += 0.06f;

    const float liftY = 3.5f * m_focusAnim;
    const float drawY = static_cast<float>(viewY) - liftY;
    const auto fx = static_cast<float>(viewX);
    const auto fw = static_cast<float>(viewW);
    const auto fh = static_cast<float>(viewH);

    // 1. Card Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, fx, drawY, fw, fh, 9.0f);
    const auto bgR = static_cast<unsigned char>(20 + 16 * m_focusAnim);
    const auto bgG = static_cast<unsigned char>(23 - 8 * m_focusAnim);
    const auto bgB = static_cast<unsigned char>(27 - 8 * m_focusAnim);
    nvgFillColor(vg, nvgRGB(bgR, bgG, bgB));
    nvgFill(vg);

    // 2. Single Crisp Border (Handles stroke cleanly without double focus border)
    nvgBeginPath(vg);
    nvgRoundedRect(vg, fx, drawY, fw, fh, 9.0f);
    if (m_focusAnim > 0.01f) {
        nvgStrokeColor(vg, nvgRGB(230, 0, 18));
        nvgStrokeWidth(vg, 2.0f);
    }
    else {
        nvgStrokeColor(vg, nvgRGB(36, 40, 48));
        nvgStrokeWidth(vg, 1.0f);
    }
    nvgStroke(vg);

    // 4. Circular Icon Badge on Left
    const float badgeX = fx + 27.0f;
    const float badgeY = drawY + fh * 0.5f;
    constexpr float badgeR = 18.0f;

    nvgBeginPath(vg);
    nvgCircle(vg, badgeX, badgeY, badgeR);
    const auto badgeBgAlpha = static_cast<unsigned char>(16 + 28 * m_focusAnim);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, badgeBgAlpha));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, badgeX, badgeY, badgeR);
    const auto bRingR = static_cast<unsigned char>(38 + (230 - 38) * m_focusAnim);
    const auto bRingG = static_cast<unsigned char>(44 - 44 * m_focusAnim);
    const auto bRingB = static_cast<unsigned char>(52 - 34 * m_focusAnim);
    const auto bRingA = static_cast<unsigned char>(160 + 95 * m_focusAnim);
    nvgStrokeColor(vg, nvgRGBA(bRingR, bRingG, bRingB, bRingA));
    nvgStrokeWidth(vg, 1.2f + 0.6f * m_focusAnim);
    nvgStroke(vg);

    // Vector Icon
    const NVGcolor iconColor = isFoc ? nvgRGB(230, 0, 18) : nvgRGBA(235, 238, 245, 240);

    if (m_iconType == 0) {
        // Atmosphere: Lucide package / 3D box isometric outline
        nvgBeginPath(vg);
        nvgMoveTo(vg, badgeX, badgeY - 7.5f);
        nvgLineTo(vg, badgeX + 6.8f, badgeY - 3.5f);
        nvgLineTo(vg, badgeX + 6.8f, badgeY + 4.0f);
        nvgLineTo(vg, badgeX, badgeY + 7.8f);
        nvgLineTo(vg, badgeX - 6.8f, badgeY + 4.0f);
        nvgLineTo(vg, badgeX - 6.8f, badgeY - 3.5f);
        nvgClosePath(vg);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 1.8f);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStroke(vg);

        // Inner Y edges meeting at center
        nvgBeginPath(vg);
        nvgMoveTo(vg, badgeX, badgeY);
        nvgLineTo(vg, badgeX, badgeY + 7.8f);
        nvgMoveTo(vg, badgeX, badgeY);
        nvgLineTo(vg, badgeX - 6.8f, badgeY - 3.5f);
        nvgMoveTo(vg, badgeX, badgeY);
        nvgLineTo(vg, badgeX + 6.8f, badgeY - 3.5f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 1.6f);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStroke(vg);
    }
    else if (m_iconType == 1) {
        // Firmware: Lucide cpu / microchip outline with crisp pins
        nvgBeginPath(vg);
        nvgRoundedRect(vg, badgeX - 5.5f, badgeY - 5.5f, 11.0f, 11.0f, 2.0f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);

        // Inner core square
        nvgBeginPath(vg);
        nvgRoundedRect(vg, badgeX - 2.2f, badgeY - 2.2f, 4.4f, 4.4f, 1.0f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 1.4f);
        nvgStroke(vg);

        // 8 external pins
        nvgBeginPath(vg);
        // Top pins
        nvgMoveTo(vg, badgeX - 2.5f, badgeY - 5.5f);
        nvgLineTo(vg, badgeX - 2.5f, badgeY - 8.0f);
        nvgMoveTo(vg, badgeX + 2.5f, badgeY - 5.5f);
        nvgLineTo(vg, badgeX + 2.5f, badgeY - 8.0f);
        // Bottom pins
        nvgMoveTo(vg, badgeX - 2.5f, badgeY + 5.5f);
        nvgLineTo(vg, badgeX - 2.5f, badgeY + 8.0f);
        nvgMoveTo(vg, badgeX + 2.5f, badgeY + 5.5f);
        nvgLineTo(vg, badgeX + 2.5f, badgeY + 8.0f);
        // Left pins
        nvgMoveTo(vg, badgeX - 5.5f, badgeY - 2.5f);
        nvgLineTo(vg, badgeX - 8.0f, badgeY - 2.5f);
        nvgMoveTo(vg, badgeX - 5.5f, badgeY + 2.5f);
        nvgLineTo(vg, badgeX - 8.0f, badgeY + 2.5f);
        // Right pins
        nvgMoveTo(vg, badgeX + 5.5f, badgeY - 2.5f);
        nvgLineTo(vg, badgeX + 8.0f, badgeY - 2.5f);
        nvgMoveTo(vg, badgeX + 5.5f, badgeY + 2.5f);
        nvgLineTo(vg, badgeX + 8.0f, badgeY + 2.5f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 1.8f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
    }
    else if (m_iconType == 2) {
        // Tools: Lucide wrench / 45-degree angle open-end spanner
        // Handle: angled shaft extending to bottom-left
        nvgBeginPath(vg);
        nvgMoveTo(vg, badgeX - 6.5f, badgeY + 6.5f);
        nvgLineTo(vg, badgeX + 0.8f, badgeY - 0.8f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 2.4f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);

        // Open-end wrench jaws pointing top-right at 45 degrees
        nvgBeginPath(vg);
        nvgMoveTo(vg, badgeX - 0.5f, badgeY - 1.8f);
        nvgLineTo(vg, badgeX + 1.2f, badgeY - 6.8f);
        nvgLineTo(vg, badgeX + 3.0f, badgeY - 5.0f);
        nvgLineTo(vg, badgeX + 5.0f, badgeY - 3.0f);
        nvgLineTo(vg, badgeX + 6.8f, badgeY - 1.2f);
        nvgLineTo(vg, badgeX + 1.8f, badgeY + 0.5f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 2.0f);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStroke(vg);
    }
    else if (m_iconType == 3) {
        // Settings: Lucide settings gear
        nvgBeginPath(vg);
        nvgCircle(vg, badgeX, badgeY, 2.8f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 1.8f);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgCircle(vg, badgeX, badgeY, 5.0f);
        nvgStrokeColor(vg, iconColor);
        nvgStrokeWidth(vg, 1.6f);
        nvgStroke(vg);

        for (int t = 0; t < 6; ++t) {
            const float angle = static_cast<float>(t) * 3.14159265f / 3.0f;
            const float cosA = std::cos(angle);
            const float sinA = std::sin(angle);
            nvgBeginPath(vg);
            nvgMoveTo(vg, badgeX + 4.8f * cosA, badgeY + 4.8f * sinA);
            nvgLineTo(vg, badgeX + 7.5f * cosA, badgeY + 7.5f * sinA);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgLineCap(vg, NVG_ROUND);
            nvgStroke(vg);
        }
    }

    // 5. Left-aligned Text beside the Badge
    const float textStartX = fx + 54.0f;

    // Top text (Category)
    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 12.0f);
    const auto catR = static_cast<unsigned char>(142 + (255 - 142) * m_focusAnim);
    const auto catG = static_cast<unsigned char>(146 + (190 - 146) * m_focusAnim);
    const auto catB = static_cast<unsigned char>(152 + (190 - 152) * m_focusAnim);
    nvgFillColor(vg, nvgRGB(catR, catG, catB));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, textStartX, drawY + 23.0f, m_topText.c_str(), nullptr);

    // Bottom text (Action)
    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 15.0f);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, textStartX, drawY + 45.0f, m_bottomText.c_str(), nullptr);
}

// ============================================================================
// HomeTab
// ============================================================================

HomeTab::HomeTab(domain::UpdateService& update, domain::TelemetryService& telemetry,
                 domain::FileStore& files, domain::motd::MotdService* motd,
                 SystemOverviewQuery queryOverview, TabSelectCallback onSelectTab)
    : brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL),
      m_update(update),
      m_telemetry(telemetry),
      m_files(files),
      m_motd(motd),
      m_queryOverview(std::move(queryOverview)),
      m_onSelectTab(std::move(onSelectTab))
{
    this->setSpacing(14);
    this->setMargins(12, 20, 12, 20);

    // 1. Modular Dashboard Grid (Top 4 Cards & Middle Panels)
    m_summaryView = new DashboardSummaryView(m_files, m_queryOverview);
    this->addView(m_summaryView);

    if (m_motd) {
        const auto cached = m_motd->currentBulletin();
        if (cached.has_value()) {
            m_summaryView->updateMotd(cached);
        }
    }

    // 2. Quick Actions Toolbar (Interactive horizontal tiles)
    m_buttonsRow = new brls::BoxLayout(brls::BoxLayoutOrientation::HORIZONTAL);
    m_buttonsRow->setSpacing(12);
    m_buttonsRow->setHeight(68);

    auto cb = m_onSelectTab;

    // Tile 1: Atmosphere (Index 2 in TabFrame)
    auto* btnAms = new DashboardActionButton("Atmosphere", "Gerenciar AMS", 0, [cb]() {
        if (cb) {
            cb(2);
        }
    });
    m_buttonsRow->addView(btnAms, true, false);

    // Tile 2: Firmware (Index 3 in TabFrame)
    auto* btnFw = new DashboardActionButton("Firmware", "Atualizar FW", 1, [cb]() {
        if (cb) {
            cb(3);
        }
    });
    m_buttonsRow->addView(btnFw, true, false);

    // Tile 3: Tools (Index 4 in TabFrame)
    auto* btnTools = new DashboardActionButton("Ferramentas", "Manutencao", 2, [cb]() {
        if (cb) {
            cb(4);
        }
    });
    m_buttonsRow->addView(btnTools, true, false);

    // Tile 4: Settings (Index 6 in TabFrame)
    auto* btnSettings = new DashboardActionButton("Configuracoes", "Ajustes", 3, [cb]() {
        if (cb) {
            cb(6);
        }
    });
    m_buttonsRow->addView(btnSettings, true, false);

    this->addView(m_buttonsRow);

    // Dispatch background diagnostic checks
    runTelemetryCheck();
    runUpdateCheck();
}

HomeTab::~HomeTab()
{
    m_alive.reset();
    m_telemetryJob.requestCancel();
    m_updateJob.requestCancel();
    m_telemetryJob.join();
    m_updateJob.join();
}

brls::View* HomeTab::getDefaultFocus()
{
    if (m_buttonsRow) {
        return m_buttonsRow->getDefaultFocus();
    }
    return nullptr;
}

void HomeTab::runTelemetryCheck()
{
    (void)m_telemetryJob.start([this, alive = std::weak_ptr<bool>(m_alive)]() {
        const auto report = m_telemetry.checkProtection();
        if (alive.expired()) {
            return;
        }
        if (m_summaryView) {
            m_summaryView->updateTelemetry(report);
        }
    });
}

void HomeTab::runUpdateCheck()
{
    (void)m_updateJob.start([this, alive = std::weak_ptr<bool>(m_alive)]() {
        const auto outcome = m_update.check();
        if (alive.expired()) {
            return;
        }
        if (m_summaryView) {
            m_summaryView->updateUpdateOutcome(outcome);
        }

        if (m_motd) {
            auto manifestText =
                m_files.readText("/config/nsx-manager/cache/manifest.json", 64 * 1024);
            if (!manifestText.has_value()) {
                manifestText =
                    m_files.readText("/switch/nsx-manager/staging/update.json", 64 * 1024);
            }
            if (manifestText.has_value()) {
                if (m_motd->parseAndCache(*manifestText) && m_summaryView) {
                    m_summaryView->updateMotd(m_motd->currentBulletin());
                }
            }
            else {
                const auto cached = m_motd->currentBulletin();
                if (cached.has_value() && m_summaryView) {
                    m_summaryView->updateMotd(cached);
                }
            }
        }
    });
}

}  // namespace nsx::ui
