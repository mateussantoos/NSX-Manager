/*
    Borealis, a Nintendo Switch UI Library
    Copyright (C) 2019  natinusala
    Copyright (C) 2019  WerWolv
    Copyright (C) 2019  p-sam

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <borealis/application.hpp>
#include <borealis/i18n.hpp>
#include <borealis/sidebar.hpp>
#include <cmath>

using namespace brls::i18n::literals;

namespace brls
{

Sidebar::Sidebar()
    : BoxLayout(BoxLayoutOrientation::VERTICAL)
{
    Style* style = Application::getStyle();

    this->setWidth(style->Sidebar.width);
    this->setSpacing(style->Sidebar.spacing);
    this->setMargins(style->Sidebar.marginTop, style->Sidebar.marginRight, style->Sidebar.marginBottom, style->Sidebar.marginLeft);
    this->setBackground(ViewBackground::SIDEBAR);
}

View* Sidebar::getDefaultFocus()
{
    // Sanity check
    if (this->lastFocus >= this->children.size())
        this->lastFocus = 0;

    View* toFocus { nullptr };
    // Try to focus last focused one
    if (this->children.size() != 0)
        toFocus = this->children[this->lastFocus]->view->getDefaultFocus();

    if (toFocus)
        return toFocus;

    // Otherwise just get the first available item
    return BoxLayout::getDefaultFocus();
}

void Sidebar::onChildFocusGained(View* child)
{
    size_t position = *((size_t*)child->getParentUserData());

    this->lastFocus = position;

    BoxLayout::onChildFocusGained(child);
}

static void drawSidebarIcon(NVGcontext* vg, SidebarIcon icon, float iconX, float centerY, NVGcolor iconColor, FrameContext* ctx)
{
    switch (icon)
    {
        case SidebarIcon::Home:
        {
            // Lucide home: crisp geometric roof + body + door opening
            // Roof
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX - 7.5f, centerY - 0.5f);
            nvgLineTo(vg, iconX, centerY - 7.0f);
            nvgLineTo(vg, iconX + 7.5f, centerY - 0.5f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);

            // House body & base
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX - 5.5f, centerY - 0.5f);
            nvgLineTo(vg, iconX - 5.5f, centerY + 6.5f);
            nvgLineTo(vg, iconX + 5.5f, centerY + 6.5f);
            nvgLineTo(vg, iconX + 5.5f, centerY - 0.5f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);

            // Door opening
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX - 2.0f, centerY + 6.5f);
            nvgLineTo(vg, iconX - 2.0f, centerY + 2.5f);
            nvgLineTo(vg, iconX + 2.0f, centerY + 2.5f);
            nvgLineTo(vg, iconX + 2.0f, centerY + 6.5f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.8f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
            break;
        }
        case SidebarIcon::Update:
        {
            // Lucide arrow-down-circle: clean circular badge with centered download arrow
            nvgBeginPath(vg);
            nvgCircle(vg, iconX, centerY, 7.2f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);

            // Arrow stem
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX, centerY - 3.5f);
            nvgLineTo(vg, iconX, centerY + 3.2f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgLineCap(vg, NVG_ROUND);
            nvgStroke(vg);

            // Arrow head
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX - 2.8f, centerY + 0.6f);
            nvgLineTo(vg, iconX, centerY + 3.5f);
            nvgLineTo(vg, iconX + 2.8f, centerY + 0.6f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
            break;
        }
        case SidebarIcon::Atmosphere:
        {
            // Lucide package / 3D box isometric outline
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX, centerY - 7.5f);
            nvgLineTo(vg, iconX + 6.8f, centerY - 3.5f);
            nvgLineTo(vg, iconX + 6.8f, centerY + 4.0f);
            nvgLineTo(vg, iconX, centerY + 7.8f);
            nvgLineTo(vg, iconX - 6.8f, centerY + 4.0f);
            nvgLineTo(vg, iconX - 6.8f, centerY - 3.5f);
            nvgClosePath(vg);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.8f);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);

            // Inner Y edges meeting at center
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX, centerY);
            nvgLineTo(vg, iconX, centerY + 7.8f);
            nvgMoveTo(vg, iconX, centerY);
            nvgLineTo(vg, iconX - 6.8f, centerY - 3.5f);
            nvgMoveTo(vg, iconX, centerY);
            nvgLineTo(vg, iconX + 6.8f, centerY - 3.5f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.6f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
            break;
        }
        case SidebarIcon::Firmware:
        {
            // Lucide cpu: microchip body with pins
            nvgBeginPath(vg);
            nvgRoundedRect(vg, iconX - 5.5f, centerY - 5.5f, 11.0f, 11.0f, 2.0f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);

            // Inner core square
            nvgBeginPath(vg);
            nvgRoundedRect(vg, iconX - 2.2f, centerY - 2.2f, 4.4f, 4.4f, 1.0f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.4f);
            nvgStroke(vg);

            // 8 external pins
            nvgBeginPath(vg);
            // Top pins
            nvgMoveTo(vg, iconX - 2.5f, centerY - 5.5f); nvgLineTo(vg, iconX - 2.5f, centerY - 8.0f);
            nvgMoveTo(vg, iconX + 2.5f, centerY - 5.5f); nvgLineTo(vg, iconX + 2.5f, centerY - 8.0f);
            // Bottom pins
            nvgMoveTo(vg, iconX - 2.5f, centerY + 5.5f); nvgLineTo(vg, iconX - 2.5f, centerY + 8.0f);
            nvgMoveTo(vg, iconX + 2.5f, centerY + 5.5f); nvgLineTo(vg, iconX + 2.5f, centerY + 8.0f);
            // Left pins
            nvgMoveTo(vg, iconX - 5.5f, centerY - 2.5f); nvgLineTo(vg, iconX - 8.0f, centerY - 2.5f);
            nvgMoveTo(vg, iconX - 5.5f, centerY + 2.5f); nvgLineTo(vg, iconX - 8.0f, centerY + 2.5f);
            // Right pins
            nvgMoveTo(vg, iconX + 5.5f, centerY - 2.5f); nvgLineTo(vg, iconX + 8.0f, centerY - 2.5f);
            nvgMoveTo(vg, iconX + 5.5f, centerY + 2.5f); nvgLineTo(vg, iconX + 8.0f, centerY + 2.5f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.8f);
            nvgLineCap(vg, NVG_ROUND);
            nvgStroke(vg);
            break;
        }
        case SidebarIcon::Tools:
        {
            // Lucide wrench: 45-degree angle open-end spanner (geometric line-art with 2.0px stroke)
            // Handle: angled shaft extending to bottom-left
            nvgBeginPath(vg);
            nvgMoveTo(vg, iconX - 6.5f, centerY + 6.5f);
            nvgLineTo(vg, iconX + 0.8f, centerY - 0.8f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.4f);
            nvgLineCap(vg, NVG_ROUND);
            nvgStroke(vg);

            // Open-end wrench jaws pointing top-right at 45 degrees
            nvgBeginPath(vg);
            // Outer left jaw
            nvgMoveTo(vg, iconX - 0.5f, centerY - 1.8f);
            nvgLineTo(vg, iconX + 1.2f, centerY - 6.8f);
            // Left jaw inner face
            nvgLineTo(vg, iconX + 3.0f, centerY - 5.0f);
            // Jaw throat
            nvgLineTo(vg, iconX + 5.0f, centerY - 3.0f);
            // Right jaw inner face
            nvgLineTo(vg, iconX + 6.8f, centerY - 1.2f);
            // Outer right jaw
            nvgLineTo(vg, iconX + 1.8f, centerY + 0.5f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 2.0f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
            break;
        }
        case SidebarIcon::Settings:
        {
            // Lucide settings: gear outline
            nvgBeginPath(vg);
            nvgCircle(vg, iconX, centerY, 2.8f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.8f);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgCircle(vg, iconX, centerY, 5.0f);
            nvgStrokeColor(vg, iconColor);
            nvgStrokeWidth(vg, 1.6f);
            nvgStroke(vg);

            for (int t = 0; t < 6; ++t)
            {
                const float angle = static_cast<float>(t) * 3.14159265f / 3.0f;
                const float cosA  = std::cos(angle);
                const float sinA  = std::sin(angle);
                nvgBeginPath(vg);
                nvgMoveTo(vg, iconX + 4.8f * cosA, centerY + 4.8f * sinA);
                nvgLineTo(vg, iconX + 7.5f * cosA, centerY + 7.5f * sinA);
                nvgStrokeColor(vg, iconColor);
                nvgStrokeWidth(vg, 2.0f);
                nvgLineCap(vg, NVG_ROUND);
                nvgStroke(vg);
            }
            break;
        }
        case SidebarIcon::None:
        default:
            break;
    }
}

SidebarItem* Sidebar::addItem(std::string label, View* view, SidebarIcon icon)
{
    SidebarItem* item = new SidebarItem(label, this, icon);
    item->setAssociatedView(view);

    if (this->isEmpty())
        setActive(item);

    this->addView(item);

    return item;
}

void Sidebar::addSeparator()
{
    SidebarSeparator* separator = new SidebarSeparator();
    this->addView(separator);
}

void Sidebar::setActive(SidebarItem* active)
{
    if (currentActive)
        currentActive->setActive(false);

    currentActive = active;
    active->setActive(true);
}

SidebarItem::SidebarItem(std::string label, Sidebar* sidebar, SidebarIcon icon)
    : label(label)
    , icon(icon)
    , sidebar(sidebar)
{
    Style* style = Application::getStyle();
    this->setHeight(style->Sidebar.Item.height);

    if (this->icon == SidebarIcon::None)
    {
        if (label.find("In") != std::string::npos || label.find("Home") != std::string::npos)
            this->icon = SidebarIcon::Home;
        else if (label.find("Atuali") != std::string::npos || label.find("Update") != std::string::npos)
            this->icon = SidebarIcon::Update;
        else if (label.find("Atmos") != std::string::npos || label.find("AMS") != std::string::npos || label.find("CFW") != std::string::npos)
            this->icon = SidebarIcon::Atmosphere;
        else if (label.find("Firmware") != std::string::npos)
            this->icon = SidebarIcon::Firmware;
        else if (label.find("Ferramenta") != std::string::npos || label.find("Tools") != std::string::npos)
            this->icon = SidebarIcon::Tools;
        else if (label.find("Configura") != std::string::npos || label.find("Settings") != std::string::npos)
            this->icon = SidebarIcon::Settings;
    }

    this->registerAction("brls/hints/ok"_i18n, Key::A, [this] { return this->onClick(); });
}

void SidebarItem::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, Style* style, FrameContext* ctx)
{
    const bool isFoc = this->isFocused();
    const bool isAct = this->active;

    // 1. Subtle selection pill background on focus or active
    if (isFoc)
    {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + 4.0f, y + 2.0f, width - 8.0f, height - 4.0f, 6.0f);
        nvgFillColor(vg, nvgRGBA(230, 0, 18, 28));
        nvgFill(vg);
    }
    else if (isAct)
    {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + 4.0f, y + 2.0f, width - 8.0f, height - 4.0f, 6.0f);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 8));
        nvgFill(vg);
    }

    // 2. Vertical crimson indicator bar (|) on the left edge
    if (isAct || isFoc)
    {
        const float barX = static_cast<float>(x) + 4.0f;
        const float barY = static_cast<float>(y) + 8.0f;
        const float barW = 3.5f;
        const float barH = static_cast<float>(height) - 16.0f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, barX, barY, barW, barH, 1.75f);
        nvgFillColor(vg, isFoc ? nvgRGB(230, 0, 18) : nvgRGBA(230, 0, 18, 220));
        nvgFill(vg);
    }

    // 3. Colors for Icon and Label
    NVGcolor iconColor;
    NVGcolor textColor;
    if (isFoc)
    {
        iconColor = nvgRGB(230, 0, 18);
        textColor = nvgRGB(255, 255, 255);
    }
    else if (isAct)
    {
        iconColor = nvgRGB(230, 0, 18);
        textColor = nvgRGB(255, 255, 255);
    }
    else
    {
        iconColor = nvgRGB(130, 136, 145);
        textColor = nvgRGB(155, 160, 168);
    }

    const float centerY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float iconX   = static_cast<float>(x) + 26.0f;

    // 4. Draw Sidebar Icon
    drawSidebarIcon(vg, this->icon, iconX, centerY, iconColor, ctx);

    // 5. Draw Label
    const float textX = (this->icon != SidebarIcon::None) ? (iconX + 20.0f) : (static_cast<float>(x) + style->Sidebar.Item.textOffsetX);
    nvgFillColor(vg, a(textColor));
    nvgFontSize(vg, style->Sidebar.Item.textSize);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgBeginPath(vg);
    nvgText(vg, textX, centerY, this->label.c_str(), nullptr);
}

bool SidebarItem::onClick()
{
    Application::onGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    return true;
}

void SidebarItem::setActive(bool active)
{
    this->active = active;
}

SidebarSeparator::SidebarSeparator()
{
    Style* style = Application::getStyle();
    this->setHeight(style->Sidebar.Separator.height);
}

void SidebarSeparator::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, Style* style, FrameContext* ctx)
{
    nvgFillColor(vg, a(ctx->theme->sidebarSeparatorColor));
    nvgBeginPath(vg);
    nvgRect(vg, x, y + height / 2, width, 1);
    nvgFill(vg);
}

void SidebarItem::setAssociatedView(View* view)
{
    this->associatedView = view;
}

bool SidebarItem::isActive()
{
    return this->active;
}

void SidebarItem::onFocusGained()
{
    this->sidebar->setActive(this);
    View::onFocusGained();
}

View* SidebarItem::getAssociatedView()
{
    return this->associatedView;
}


ViewType SidebarItem::getViewType()
{
    return this->viewType;
}

SidebarItem::~SidebarItem()
{
    if (this->associatedView)
        delete this->associatedView;
}

} // namespace brls
