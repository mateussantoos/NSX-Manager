// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/ui/theme/dark_theme.hpp"

namespace nsx::ui {

DarkMinimalistTheme::DarkMinimalistTheme()
{
    // Deep black minimalist background (#000000)
    this->backgroundColor[0] = 0.0f;
    this->backgroundColor[1] = 0.0f;
    this->backgroundColor[2] = 0.0f;
    this->backgroundColorRGB = nvgRGB(0, 0, 0);

    this->textColor = nvgRGB(255, 255, 255);
    this->descriptionColor = nvgRGB(160, 160, 160);

    this->notificationTextColor = nvgRGB(255, 255, 255);
    this->backdropColor = nvgRGBA(0, 0, 0, 190);

    this->separatorColor = nvgRGB(40, 40, 40);

    // Sidebar: deep sleek dark
    this->sidebarColor = nvgRGB(12, 12, 12);
    // Active tab underline and accent: Crimson Red (#E60012)
    this->activeTabColor = nvgRGB(230, 0, 18);
    this->sidebarSeparatorColor = nvgRGB(30, 30, 30);

    // Focus & selection highlight: Crimson Red (#E60012)
    this->highlightBackgroundColor = nvgRGB(26, 8, 10);
    this->highlightColor1 = nvgRGB(230, 0, 18);
    this->highlightColor2 = nvgRGB(255, 60, 75);

    this->listItemSeparatorColor = nvgRGB(32, 32, 32);
    this->listItemValueColor = nvgRGB(255, 70, 80);
    this->listItemFaintValueColor = nvgRGB(100, 100, 100);

    this->tableEvenBackgroundColor = nvgRGB(18, 18, 18);
    this->tableBodyTextColor = nvgRGB(180, 180, 180);

    this->dropdownBackgroundColor = nvgRGBA(14, 14, 14, 230);

    this->nextStageBulletColor = nvgRGB(230, 0, 18);

    this->spinnerBarColor = nvgRGBA(230, 0, 18, 150);

    this->scrollBarColor = nvgRGB(70, 70, 70);
    this->scrollBarAlphaNormal = 0.25f;
    this->scrollBarAlphaFull = 0.6f;

    this->clickAnimationAlpha = 0.35f;

    // Section title rectangle accent: Crimson Red
    this->headerRectangleColor = nvgRGB(230, 0, 18);

    // Primary and interactive buttons
    this->buttonPrimaryEnabledBackgroundColor = nvgRGB(230, 0, 18);
    this->buttonPrimaryDisabledBackgroundColor = nvgRGB(50, 50, 50);
    this->buttonPrimaryEnabledTextColor = nvgRGB(255, 255, 255);
    this->buttonPrimaryDisabledTextColor = nvgRGB(110, 110, 110);
    this->buttonBorderedBorderColor = nvgRGB(230, 0, 18);
    this->buttonBorderedTextColor = nvgRGB(255, 255, 255);
    this->buttonRegularBackgroundColor = nvgRGB(28, 28, 28);
    this->buttonRegularTextColor = nvgRGB(255, 255, 255);
    this->buttonRegularBorderColor = nvgRGB(45, 45, 45);

    // Dialogs: minimalist dark with red accents
    this->dialogColor = nvgRGB(18, 18, 18);
    this->dialogBackdrop = nvgRGBA(0, 0, 0, 180);
    this->dialogButtonColor = nvgRGB(230, 0, 18);
    this->dialogButtonSeparatorColor = nvgRGB(45, 45, 45);
}

}  // namespace nsx::ui
