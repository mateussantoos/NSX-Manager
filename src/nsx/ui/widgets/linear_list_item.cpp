// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/widgets/linear_list_item.hpp"

namespace nsx::ui {

LinearListItem::LinearListItem(std::string label, std::string value, std::string icon)
    : brls::ListItem(label, "", ""), m_icon(std::move(icon)), m_badge(std::move(value))
{
    this->setHeight(50);
}

void LinearListItem::setIcon(std::string icon)
{
    m_icon = std::move(icon);
}

void LinearListItem::setBadge(std::string badge, bool isAccent)
{
    m_badge = std::move(badge);
    m_isAccentBadge = isAccent;
}

void LinearListItem::setValue(std::string value)
{
    m_badge = value;
    brls::ListItem::setValue(std::move(value));
}

void LinearListItem::draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
                          brls::Style*, brls::FrameContext* ctx)
{
    const bool isFoc = this->isFocused();
    m_focusAnim += ((isFoc ? 1.0f : 0.0f) - m_focusAnim) * 0.25f;

    const auto fx = static_cast<float>(viewX);
    const auto fy = static_cast<float>(viewY);
    const auto fw = static_cast<float>(viewW);
    const auto fh = static_cast<float>(viewH);

    // 1. Focused Background & Left Accent
    if (m_focusAnim > 0.01f) {
        nvgBeginPath(vg);
        nvgRect(vg, fx, fy, fw, fh);
        const auto bgAlpha = static_cast<unsigned char>(35.0f * m_focusAnim);
        nvgFillColor(vg, nvgRGBA(230, 0, 18, bgAlpha));
        nvgFill(vg);

        // Crimson Red Left Vertical Bar Indicator (#E60012)
        nvgBeginPath(vg);
        nvgRect(vg, fx, fy + 4.0f, 3.5f, fh - 8.0f);
        nvgFillColor(vg, nvgRGB(230, 0, 18));
        nvgFill(vg);
    }

    // 2. Hairline Bottom Separator
    nvgBeginPath(vg);
    nvgRect(vg, fx + 16.0f, fy + fh - 1.0f, fw - 32.0f, 1.0f);
    nvgFillColor(vg, nvgRGB(32, 34, 40));
    nvgFill(vg);

    // 3. Left Icon Badge (if provided)
    float textStartX = fx + 16.0f;
    if (!m_icon.empty()) {
        nvgBeginPath(vg);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgFontSize(vg, 12.5f);
        nvgFillColor(vg, isFoc ? nvgRGB(230, 0, 18) : nvgRGB(140, 146, 156));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, fx + 14.0f, fy + fh * 0.5f, m_icon.c_str(), nullptr);
        textStartX = fx + 68.0f;
    }

    // 4. Main Item Label
    nvgBeginPath(vg);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 15.0f);
    nvgFillColor(vg, isFoc ? nvgRGB(255, 255, 255) : nvgRGB(226, 232, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, textStartX, fy + fh * 0.5f,
            this->labelView ? this->labelView->getText().c_str() : "", nullptr);

    // 5. Right Status / Value / Badge
    const std::string& displayVal = !m_badge.empty() ? m_badge : this->getValue();
    if (!displayVal.empty()) {
        nvgBeginPath(vg);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgFontSize(vg, 13.5f);

        if (m_isAccentBadge) {
            nvgFillColor(vg, nvgRGB(255, 75, 85));
        }
        else if (displayVal == "[ ON ]" || displayVal == "Safe" || displayVal == "Protected" ||
                 displayVal == "Ativo") {
            nvgFillColor(vg, nvgRGB(46, 204, 113));
        }
        else {
            nvgFillColor(vg, nvgRGB(160, 166, 178));
        }

        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, fx + fw - 20.0f, fy + fh * 0.5f, displayVal.c_str(), nullptr);
    }
}

// ============================================================================
// LinearToggleItem
// ============================================================================

LinearToggleItem::LinearToggleItem(std::string label, bool initial, std::string icon)
    : LinearListItem(std::move(label), "", std::move(icon)), m_state(initial)
{
    updateBadge();
}

void LinearToggleItem::setToggleState(bool state)
{
    m_state = state;
    updateBadge();
}

bool LinearToggleItem::onClick()
{
    m_state = !m_state;
    updateBadge();
    return LinearListItem::onClick();
}

void LinearToggleItem::updateBadge()
{
    this->setBadge(m_state ? "[ ON ]" : "[ OFF ]", false);
}

}  // namespace nsx::ui
