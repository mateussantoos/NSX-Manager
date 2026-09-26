// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <borealis.hpp>

namespace nsx::ui {

/// @brief Custom dark minimalist theme with deep black background and crimson red accents.
/// @details Overrides standard Borealis colors, replacing Nintendo blue with crimson red
/// (0xE60012).
class DarkMinimalistTheme : public brls::Theme
{
public:
    DarkMinimalistTheme();
};

}  // namespace nsx::ui
