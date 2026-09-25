/*
    Borealis, a Nintendo Switch UI Library
    Copyright (C) 2019  natinusala
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

#pragma once

#include <borealis/box_layout.hpp>
#include <string>
#include <vector>

namespace brls
{

enum class SidebarIcon
{
    None,
    Home,
    Update,
    Atmosphere,
    Firmware,
    Tools,
    Settings
};

// A sidebar with multiple tabs
class SidebarSeparator : public View
{
  public:
    SidebarSeparator();

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, Style* style, FrameContext* ctx) override;
};

class Sidebar;

// TODO: Use a Label view with integrated ticker for label and sublabel, have the label always tick when active
class SidebarItem : public View
{
  private:
    std::string label;
    bool active = false;
    SidebarIcon icon = SidebarIcon::None;

    Sidebar* sidebar     = nullptr;
    View* associatedView = nullptr;

    ViewType viewType = ViewType::SIDEBARITEM;

  public:
    SidebarItem(std::string label, Sidebar* sidebar, SidebarIcon icon = SidebarIcon::None);

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, Style* style, FrameContext* ctx) override;

    void drawHighlight(NVGcontext* vg, Theme* theme, float alpha, Style* style, bool background) override
    {
        // Suppress default hollow rectangular bounding box
    }

    bool isHighlightBackgroundEnabled() override
    {
        return false;
    }

    View* getDefaultFocus() override
    {
        return this;
    }

    virtual bool onClick();

    void setActive(bool active);
    bool isActive();

    void setIcon(SidebarIcon icon) { this->icon = icon; }
    SidebarIcon getIcon() const { return this->icon; }

    void onFocusGained() override;

    void setAssociatedView(View* view);
    View* getAssociatedView();
    ViewType getViewType() override;

    ~SidebarItem();
};

class Sidebar : public BoxLayout
{
  private:
    SidebarItem* currentActive = nullptr;

  public:
    Sidebar();

    SidebarItem* addItem(std::string label, View* view, SidebarIcon icon = SidebarIcon::None);
    void addSeparator();

    void setActive(SidebarItem* item);

    View* getDefaultFocus() override;
    void onChildFocusGained(View* child) override;

    size_t lastFocus = 0;
};

} // namespace brls
