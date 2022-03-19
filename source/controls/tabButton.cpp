/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/tabButton.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/highlight.h>
#include <gweni/dragAndDrop.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(TabButton)
{
    m_page=nullptr;
    m_control=nullptr;
    dragAndDrop_setPackage(true, "TabButtonMove");
    setAlignment(Alignment::Top|Alignment::Left);
    setTextPadding(Padding(2, 2, 2, 2));
}

void TabButton::layout(skin::Base *skin)
{
    DockPosition parentDock=m_control->getTabStrip()->getDock();

    if(parentDock == DockPosition::Bottom)
        setPadding(Padding(3, 1, 5, 4));
    else if(parentDock == DockPosition::Top)
        setPadding(Padding(3, 3, 5, 2));
    else
        setPadding(Padding(3, 2, 5, 2));

    ParentClass::layout(skin);
}


void TabButton::setTabControl(TabControl *ctrl)
{
    if(m_control == ctrl)
        return;

    if(m_control)
        m_control->onLoseTab(this);

    m_control=ctrl;
}

bool TabButton::dragAndDrop_shouldStartDrag()
{
    return m_control->doesAllowDrag();
}

bool TabButton::onKeyUp(bool down)
{
    onKeyLeft(down);
    return true;
}

bool TabButton::onKeyDown(bool down)
{
    onKeyRight(down);
    return true;
}

bool TabButton::onKeyLeft(bool down)
{
    if(down)
    {
        Base::List::reverse_iterator it=std::find(
            m_parent->getChildren().rbegin(), m_parent->getChildren().rend(), this);

        if(it != m_parent->getChildren().rend() && (++it != m_parent->getChildren().rend()))
        {
            Base *nextTab=*it;
            getTabControl()->onTabPressed(nextTab);
            gweni::KeyboardFocus=nextTab;
        }
    }

    return true;
}

bool TabButton::onKeyRight(bool down)
{
    if(down)
    {
        Base::List::iterator it=std::find(m_parent->getChildren().begin(),
            m_parent->getChildren().end(), this);

        if(it != m_parent->getChildren().end() && (++it != m_parent->getChildren().end()))
        {
            Base *nextTab=*it;
            getTabControl()->onTabPressed(nextTab);
            gweni::KeyboardFocus=nextTab;
        }
    }

    return true;
}

void TabButton::updateColors()
{
    if(!isActive())
    {
        setImageAlpha(0.5);

        if(isDisabled())
            return setTextColor(getSkin()->Colors.Tab.Inactive.Disabled);

        if(isDepressed())
            return setTextColor(getSkin()->Colors.Tab.Inactive.Down);

        if(isHovered())
            return setTextColor(getSkin()->Colors.Tab.Inactive.Hover);

        return setTextColor(getSkin()->Colors.Tab.Inactive.Normal);
    }

    setImageAlpha(1.0);

    if(isDisabled())
        return setTextColor(getSkin()->Colors.Tab.Active.Disabled);

    if(isDepressed())
        return setTextColor(getSkin()->Colors.Tab.Active.Down);

    if(isHovered())
        return setTextColor(getSkin()->Colors.Tab.Active.Hover);

    setTextColor(getSkin()->Colors.Tab.Active.Normal);
}

}//namespace controls
}//namespace gweni
