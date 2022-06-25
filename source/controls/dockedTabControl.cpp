/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/dockedTabControl.h>
#include <gweni/controls/highlight.h>
#include <gweni/dragAndDrop.h>
#include <gweni/controls/windowControl.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(DockedTabControl)
{
    m_windowControl=nullptr;
    setDock(DockPosition::Center);
    m_titleBar=newChild<TabTitleBar>();
    m_titleBar->setDock(DockPosition::Top);
    m_titleBar->setHidden(true);
    setAllowReorder(true);
}

void DockedTabControl::layout(skin::Base *skin)
{
    getTabStrip()->setHidden(tabCount() <= 1);
    updateTitleBar();
    ParentClass::layout(skin);
}

void DockedTabControl::updateTitleBar()
{
    if(!getCurrentButton())
        return;

    m_titleBar->updateFromTab(getCurrentButton());
}

void DockedTabControl::dragAndDrop_startDragging(gweni::draganddrop::Package *package, int x, int y)
{
    ParentClass::dragAndDrop_startDragging(package, x, y);
    setHidden(true);
    // This hiding our parent thing is kind of lousy.
    getParent()->setHidden(true);
}

void DockedTabControl::dragAndDrop_endDragging(bool bSuccess, int /*x*/, int /*y*/)
{
    setHidden(false);

    if(!bSuccess)
        getParent()->setHidden(false);

    /*
      * if ( !bSuccess )
      * {
      *     // Create our window control
      *     if ( !m_windowControl )
      *     {
      *         m_windowControl = new WindowControl( getCanvas() );
      *         m_windowControl->setBounds( x, y, getWidth(), getHeight() );
      *     }
     *
      *     m_windowControl->setPosition( x, y );
      *     setParent( m_windowControl );
      *     setPosition( 0, 0 );
      *     setDock( DockPosition::Center );
      * }
     */
}

void DockedTabControl::moveTabsTo(DockedTabControl *target)
{
    Base::List children=getTabStrip()->getChildren();

    for(auto &&child : children)
    {
        TabButton *button=gweni_cast<TabButton>(child);

        if(!button)
            continue;

        target->addPage(button);
    }

    invalidate();
}

}//namespace controls
}//namespace gweni
