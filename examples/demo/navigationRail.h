/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_navigationrail_example_h_
#define _gweni_navigationrail_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Button;
class Label;
class NavigationItem;
class NavigationRail;
}

//! App-controlled collapse, Material keyboard selection, icon badges,
//! a header menu, and a settings footer sharing the main selection.
class NavigationRailDemo: public controls::Base
{
public:
    GWENI_CONTROL(NavigationRailDemo, controls::Base);

public:
    static constexpr int CollapseBelow=560;
    static constexpr int ExpandedWidth=180;

    controls::NavigationRail *rail() const { return m_rail; }
    const std::vector<controls::NavigationItem *> &items() const { return m_items; }
    controls::NavigationItem *footerItem() const { return m_footer; }
    controls::Button *menuButton() const { return m_menu; }
    controls::Button *countButton() const { return m_count; }
    controls::Label *pageLabel() const { return m_page; }

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

private:
    void pageChanged(event::Info info);
    void toggleCollapsed(event::Info info);
    void newMessage(event::Info info);

    controls::NavigationRail *m_rail=nullptr;
    std::vector<controls::NavigationItem *> m_items;
    controls::NavigationItem *m_footer=nullptr;
    controls::Button *m_menu=nullptr;
    controls::Button *m_count=nullptr;
    controls::Label *m_page=nullptr;
    int m_layoutWidth=-1;
};

}//namespace gweni
#endif//_gweni_navigationrail_example_h_
