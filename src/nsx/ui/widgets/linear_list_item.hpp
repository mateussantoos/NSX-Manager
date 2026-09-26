// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

#include <borealis.hpp>

namespace nsx::ui {

/// @brief Clean, compact linear list item for utility tabs (tools, settings).
/// @details Replaces wide rounded Nintendo rows with compact, left-aligned
///          items, minimal vertical spacing, and right-aligned status badge.
class LinearListItem : public brls::ListItem
{
public:
    /// @brief Construct a linear list row item.
    /// @param label Primary label text.
    /// @param value Secondary value text.
    /// @param icon Optional leading icon glyph.
    LinearListItem(std::string label, std::string value = "", std::string icon = "");

    /// @brief Set leading icon glyph.
    /// @param icon Icon glyph string.
    void setIcon(std::string icon);

    /// @brief Set trailing status badge text.
    /// @param badge Badge text.
    /// @param isAccent True if badge should use accent red color.
    void setBadge(std::string badge, bool isAccent = false);

    /// @brief Set secondary value string.
    /// @param value Value string.
    void setValue(std::string value);

    /// @brief Render list item.
    void draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
              brls::Style* viewStyle, brls::FrameContext* ctx) override;

    /// @brief Whether background highlight is enabled.
    bool isHighlightBackgroundEnabled() override { return false; }

protected:
    /// @brief Leading icon glyph.
    std::string m_icon;
    /// @brief Trailing badge string.
    std::string m_badge;
    /// @brief Whether badge is highlighted with accent color.
    bool m_isAccentBadge{false};
    /// @brief Focus animation progress.
    float m_focusAnim{0.0f};
};

/// @brief Linear list item with a compact toggle indicator.
class LinearToggleItem : public LinearListItem
{
public:
    /// @brief Construct a linear toggle row item.
    /// @param label Primary label text.
    /// @param initial Initial toggle state.
    /// @param icon Optional leading icon glyph.
    LinearToggleItem(std::string label, bool initial = false, std::string icon = "");

    /// @brief Get current toggle state.
    [[nodiscard]] bool getToggleState() const { return m_state; }

    /// @brief Set toggle state.
    /// @param state New boolean state.
    void setToggleState(bool state);

    /// @brief Click activation handler.
    bool onClick() override;

private:
    bool m_state{false};
    void updateBadge();
};

}  // namespace nsx::ui
