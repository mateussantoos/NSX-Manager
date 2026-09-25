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
    LinearListItem(std::string label, std::string value = "", std::string icon = "");

    void setIcon(std::string icon);
    void setBadge(std::string badge, bool isAccent = false);
    void setValue(std::string value);

    void draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
              brls::Style* viewStyle, brls::FrameContext* ctx) override;

    bool isHighlightBackgroundEnabled() override { return false; }

protected:
    std::string m_icon;
    std::string m_badge;
    bool m_isAccentBadge{false};
    float m_focusAnim{0.0f};
};

/// @brief Linear list item with a compact toggle indicator.
class LinearToggleItem : public LinearListItem
{
public:
    LinearToggleItem(std::string label, bool initial = false, std::string icon = "");

    [[nodiscard]] bool getToggleState() const { return m_state; }

    void setToggleState(bool state);

    bool onClick() override;

private:
    bool m_state{false};
    void updateBadge();
};

}  // namespace nsx::ui
