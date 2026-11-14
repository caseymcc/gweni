/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/navigationRail.h"
#include "gweni/controls/badge.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/navigationRail.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(NavigationRailDemo)
{
    setDock(DockPosition::Center);
    m_rail=newChild<controls::NavigationRail>("rail");
    m_rail->setExpandedWidth(ExpandedWidth);

    m_menu=newChild<controls::Button>("menu");
    m_menu->setText("Menu");
    m_menu->setHeight(44);
    m_menu->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    m_menu->onPressCaller.add(this, &NavigationRailDemo::toggleCollapsed);
    m_rail->setHeader(m_menu);

    const char *labels[]={"Home", "Inbox", "Activity", "Projects", "Reports", "Archive"};
    for(size_t i=0; i < 6; ++i)
        m_items.push_back(m_rail->addItem(labels[i], i%2?"test16.png":"gwen.png", labels[i]));
    m_items[1]->setBadgeCount(9);
    m_items[2]->setBadgeDot(true);
    m_items.back()->setDisabled(true);
    m_footer=m_rail->getFooter()->newChild<controls::NavigationItem>("settings");
    m_footer->setText("Settings");
    m_footer->setImage("gwen.png");

    // Fixed content beside the rail makes the rail's own width/slot costs
    // visible without reflowing an unrelated page on a manual collapse.
    controls::Base *content=newChild<controls::Base>("content");
    content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    content->setBounds(ExpandedWidth+28, 28, 310, 260);
    m_page=content->newChild<controls::Label>("page");
    m_page->setFont("OpenSans.ttf", 24, false);
    m_page->setText("Home");
    m_page->setPos(0, 0);

    auto *help=content->newChild<controls::Label>("help");
    help->setText("Choose a page, or use Up/Down then Return.\n"
        "Menu toggles the labels.\n"
        "Below 560 units, this app collapses the rail.");
    help->setPos(0, 48);
    m_count=content->newChild<controls::Button>("new-message");
    m_count->setText("New inbox message");
    m_count->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_count->setBounds(0, 128, 150, 30);
    m_count->onPressCaller.add(this, &NavigationRailDemo::newMessage);

    m_rail->onSelectionChange.add(this, &NavigationRailDemo::pageChanged);
    m_rail->setSelected(m_items.front());
}

void NavigationRailDemo::pageChanged(event::Info)
{
    if(auto *item=m_rail->getSelected())
        m_page->setText(item->getText());
}

void NavigationRailDemo::toggleCollapsed(event::Info)
{
    m_rail->setCollapsed(!m_rail->isCollapsed());
}

void NavigationRailDemo::newMessage(event::Info)
{
    m_items[1]->setBadgeCount(m_items[1]->getBadge()->getCount()+1);
}

void NavigationRailDemo::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);
    // This page fills its viewport; its preferred extent is independent of
    // the rail's responsive width, which is chosen after X allocation.
    Size size=getPreferredSize();
    if(dim == Dim::X) size.width=0;
    else size.height=0;
    setPreferredSize(size);
}

void NavigationRailDemo::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X && m_layoutWidth != getWidth())
    {
        m_layoutWidth=getWidth();
        const bool collapsed=m_layoutWidth < CollapseBelow;
        if(m_rail->isCollapsed() != collapsed)
        {
            // Visibility is an input to children arranged later in this
            // pass, just as their bounds are; it is not a second app edit.
            PlacementScope placement;
            m_rail->setCollapsed(collapsed);
            // The app now knows its allocated width. Re-measure the changed
            // rail before this same X arrange docks it; no second pass.
            m_rail->doCalculateSize(skin, dim);
        }
    }
    ParentClass::arrange(skin, dim);
}

}//namespace gweni
