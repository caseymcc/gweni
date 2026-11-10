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

// Each button's dock and margin follow the side this strip is docked on and
// its place in the row. They are set before the buttons are measured, so the
// sizes taken from them are this pass's. The strip's own size along the row's
// cross axis is its preferred size, the largest button, which its dock gives
// it.
void TabStrip::calculateSize(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
        placeButtons();

    ParentClass::calculateSize(skin, dim);
}

void TabStrip::placeButtons()
{
    PlacementScope placement;
    int num=0;

    for(auto &&control : getChildren())
    {
        TabButton *button=gweni_cast<TabButton>(control);

        if(!button)
            continue;

        // Each tab after the first overlaps the one before it by 1 px along the
        // row, so neighbours share their border. Only along the row: across it
        // a tab fills the strip.
        Margin m;
        int iNotFirst=num > 0?-1:0;

        if(m_dock == DockPosition::Top)
        {
            m.left=iNotFirst;
            button->setDock(DockPosition::Left);
        }

        if(m_dock == DockPosition::Left)
        {
            m.top=iNotFirst;
            button->setDock(DockPosition::Top);
        }

        if(m_dock == DockPosition::Right)
        {
            m.top=iNotFirst;
            button->setDock(DockPosition::Top);
        }

        if(m_dock == DockPosition::Bottom)
        {
            m.left=iNotFirst;
            button->setDock(DockPosition::Left);
        }

        button->setMargin(m);
        num++;
    }
}

void TabStrip::setScrollOffset(int offset)
{
    if(m_scrollOffset == offset)
        return;

    m_scrollOffset=offset;
    // the tabs are placed again; nothing is measured again
    invalidate();
}

// The offset moves the row's origin, the left padding, for this arrange only:
// calculateSize() measures the strip with its real padding, so its preferred
// size, and its TabControl's, are the whole row's whatever is scrolled.
void TabStrip::arrangeHorizontal(skin::Base *skin)
{
    const int left=m_padding.left;

    m_padding.left=left-m_scrollOffset;
    ParentClass::arrangeHorizontal(skin);
    m_padding.left=left;
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
        // Over a tab: this strip places the marker at the edge of the tab
        // the drop would go before or after.
        gweni::Point DropPos=DroppedOn->canvasPosToLocal(gweni::Point(x, y));
        int markerX=DroppedOn->getX()-1;

        if(DropPos.x > DroppedOn->getWidth()/2)
            markerX+=DroppedOn->getWidth()-1;

        m_tabDragControl->bringToFront();
        m_tabDragControl->setDock(DockPosition::None);
        m_tabDragControl->setPlacement(Placement::Owner);
        placeChild(m_tabDragControl, markerX, 0, 3, getHeight());
    }
    else
    {
        m_tabDragControl->setPlacement(Placement::Parent);
        m_tabDragControl->setDock(DockPosition::Left);
        m_tabDragControl->bringToFront();
    }
}

void TabStrip::setTabPosition(DockPosition pos)
{
    const bool moved=m_dock != pos;

    setDock(pos);

    // a tab's look follows the side (its square corners face the page), and its
    // bounds may not change with it (Top and Bottom place it alike)
    if(moved)
    {
        for(auto &&control : getChildren())
        {
            if(gweni_cast<TabButton>(control))
                control->addStateChange(StateChange_Background);
        }
    }

    // the row starts LeadingPadding in along the strip, and the side facing the
    // page reaches PageOverlap into it
    if(m_dock == DockPosition::Top)
    {
        setPadding(Padding(LeadingPadding, 0, 0, 0));
        setMargin(Margin(0, 0, 0, -PageOverlap));
    }

    if(m_dock == DockPosition::Left)
    {
        setPadding(Padding(0, LeadingPadding, 0, 0));
        setMargin(Margin(0, 0, -PageOverlap, 0));
    }

    if(m_dock == DockPosition::Right)
    {
        setPadding(Padding(0, LeadingPadding, 0, 0));
        setMargin(Margin(-PageOverlap, 0, 0, 0));
    }

    if(m_dock == DockPosition::Bottom)
    {
        setPadding(Padding(LeadingPadding, 0, 0, 0));
        setMargin(Margin(0, -PageOverlap, 0, 0));
    }

    invalidateChildren(true);
}

}//namespace controls
}//namespace gweni
