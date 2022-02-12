/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/tabStrip.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/highlight.h>
#include <gweni/dragAndDrop.h>
#include <gweni/platforms/commonPlatform.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(TabStrip)
{
    m_tabDragControl=nullptr;
    m_allowReorder=false;
}

bool TabStrip::dragAndDrop_handleDrop(gweni::draganddrop::Package * /*package*/, int x, int y)
{
    gweni::Point LocalPos=canvasPosToLocal(gweni::Point(x, y));
    TabButton *button=gweni_cast<TabButton>(draganddrop::SourceControl);
    TabControl *tabControl=gweni_cast<TabControl>(getParent());

    if(tabControl && button)
    {
        if(button->getTabControl() != tabControl)
        {
            // We've moved tab controls!
            tabControl->addPage(button);
        }
    }

    Base *DroppedOn=getControlAt(LocalPos.x, LocalPos.y);

    if(DroppedOn)
    {
        gweni::Point DropPos=DroppedOn->canvasPosToLocal(gweni::Point(x, y));
        draganddrop::SourceControl->bringNextToControl(DroppedOn, DropPos.x > DroppedOn->getWidth()/2);
    }
    else
    {
        draganddrop::SourceControl->bringToFront();
    }

    return true;
}

bool TabStrip::dragAndDrop_canAcceptPackage(gweni::draganddrop::Package *package)
{
    if(!m_allowReorder)
        return false;

    if(package->name == "TabButtonMove")
        return true;

    return false;
}

void TabStrip::layout(skin::Base *skin)
{
    gweni::Point largestTab(5, 5);
    int num=0;

    for(auto &&control : getChildren())
    {
        TabButton *button=gweni_cast<TabButton>(control);

        if(!button)
            continue;

        button->sizeToContents();
        Margin m;
        int iNotFirst=num > 0?-1:0;

        {
            m.left=iNotFirst;
            button->dock(Position::Left);
        }

        if(m_dock == Position::Left)
        {
            m.top=iNotFirst;
            button->dock(Position::Top);
        }

        if(m_dock == Position::Right)
        {
            m.top=iNotFirst;
            button->dock(Position::Top);
        }

        if(m_dock == Position::Bottom)
        {
            m.left=iNotFirst;
            button->dock(Position::Left);
        }

        largestTab.x=std::max(largestTab.x, button->getWidth());
        largestTab.y=std::max(largestTab.y, button->getHeight());
        button->setMargin(m);
        num++;
    }

    if(m_dock == Position::Top || m_dock == Position::Bottom)
        setSize(getWidth(), largestTab.y);

    if(m_dock == Position::Left || m_dock == Position::Right)
        setSize(largestTab.x, getHeight());

    ParentClass::layout(skin);
}

void TabStrip::dragAndDrop_hoverEnter(gweni::draganddrop::Package * /*package*/, int /*x*/,
    int /*y*/)
{
    GWENI_ASSERT_MSG(!m_tabDragControl, "TabStrip::DragAndDrop_HoverEnter");

    m_tabDragControl=newChild<internal::Highlight>();
    m_tabDragControl->setMouseInputEnabled(false);
    m_tabDragControl->setSize(3, getHeight());
}

void TabStrip::dragAndDrop_hoverLeave(gweni::draganddrop::Package * /*package*/)
{
    delete m_tabDragControl;
    m_tabDragControl=nullptr;
}

void TabStrip::dragAndDrop_hover(gweni::draganddrop::Package * /*package*/, int x, int y)
{
    gweni::Point LocalPos=canvasPosToLocal(gweni::Point(x, y));
    Base *DroppedOn=getControlAt(LocalPos.x, LocalPos.y);

    if(DroppedOn && DroppedOn != this)
    {
        gweni::Point DropPos=DroppedOn->canvasPosToLocal(gweni::Point(x, y));
        m_tabDragControl->setBounds(gweni::Rect(0, 0, 3, getHeight()));
        m_tabDragControl->bringToFront();
        m_tabDragControl->setPos(DroppedOn->getX()-1, 0);

        if(DropPos.x > DroppedOn->getWidth()/2)
            m_tabDragControl->moveBy(DroppedOn->getWidth()-1, 0);

        m_tabDragControl->dock(Position::None);
    }
    else
    {
        m_tabDragControl->dock(Position::Left);
        m_tabDragControl->bringToFront();
    }
}

void TabStrip::setTabPosition(Position pos)
{
    dock(pos);

    if(m_dock == Position::Top)
        setPadding(Padding(5, 0, 0, 0));

    if(m_dock == Position::Left)
        setPadding(Padding(0, 5, 0, 0));

    if(m_dock == Position::Right)
        setPadding(Padding(0, 5, 0, 0));

    if(m_dock == Position::Bottom)
        setPadding(Padding(5, 0, 0, 0));

    invalidateChildren(true);
}

}//namespace controls
}//namespace gweni
