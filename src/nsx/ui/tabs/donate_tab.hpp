// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <borealis.hpp>

namespace nsx::ui {

/// @brief Dedicated donation view with high-resolution QR code and OLED dark styling.
/// @since 0.3.2
class DonateTab : public brls::View
{
public:
    DonateTab();
    ~DonateTab() override;

    void draw(NVGcontext* vg, int viewX, int viewY, unsigned viewW, unsigned viewH,
              brls::Style* style, brls::FrameContext* ctx) override;

private:
    int m_qrTexture{-1};
};

}  // namespace nsx::ui
