/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/dockBase.h>
#include <gweni/controls/dockedTabControl.h>
#include <gweni/controls/highlight.h>
#include <gweni/dragAndDrop.h>
#include <gweni/controls/resizer.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(DockBase)
{
    enlargePrimitiveIds(this, m_primitiveIds, 6);

    setPadding(Padding(1, 1, 1, 1));
    setSize(200, 200);
    m_dockedTabControl=nullptr;
    m_left=nullptr;
    m_right=nullptr;
    m_top=nullptr;
    m_bottom=nullptr;
    m_drawHover=false;
}

TabControl *DockBase::getTabControl()
{
    return m_dockedTabControl;
}

void DockBase::setupChildDock(Position pos)
{
    if(!m_dockedTabControl)
    {
        m_dockedTabControl=new DockedTabControl(this);
        m_dockedTabControl->onLoseTabCaller.add(this, &DockBase::onTabRemoved);
        m_dockedTabControl->setTabStripPosition(Position::Bottom);
        m_dockedTabControl->setShowTitleBar(true);
    }

    dock(pos);
    Position sizeDirection=Position::Left;

    if(pos == Position::Left)
        sizeDirection=Position::Right;

    if(pos == Position::Top)
        sizeDirection=Position::Bottom;

    if(pos == Position::Bottom)
        sizeDirection=Position::Top;

    internal::Resizer *sizer=new internal::Resizer(this);
    sizer->dock(sizeDirection);
    sizer->setResizeDirection(sizeDirection);
    sizer->setSize(2, 2);
    sizer->setTarget(this);
}

void DockBase::render(skin::Base * /*skin*/)
{
    // gweni::Render->setDrawColor( colors::Black );
    // gweni::Render->drawLinedRect( getRenderBounds() );
}

DockBase **DockBase::getChildDockPtr(Position pos)
{
    if(pos == Position::Left)
        return &m_left;

    if(pos == Position::Right)
        return &m_right;

    if(pos == Position::Top)
        return &m_top;

    if(pos == Position::Bottom)
        return &m_bottom;

    return nullptr;
}

DockBase *DockBase::getChildDock(Position pos)
{
    DockBase **dock=getChildDockPtr(pos);

    if(!(*dock))
    {
        (*dock)=new DockBase(this);
        (*dock)->setupChildDock(pos);
    }
    else
    {
        (*dock)->setHidden(false);
    }

    return *dock;
}

Position DockBase::getDroppedTabDirection(int x, int y)
{
    const int w=getWidth();
    const int h=getHeight();
    const float top=static_cast<float>(y) / h;
    const float left=static_cast<float>(x) / w;
    const float right=static_cast<float>(w-x) / w;
    const float bottom=static_cast<float>(h-y) / h;
    const float minimum=std::min(std::min(std::min(top, left), right), bottom);
    m_dropFar=(minimum < 0.2f);

    if(minimum > 0.3f)
        return Position::Fill;

    if(top == minimum && (!m_top || m_top->hidden()))
        return Position::Top;

    if(left == minimum && (!m_left || m_left->hidden()))
        return Position::Left;

    if(right == minimum && (!m_right || m_right->hidden()))
        return Position::Right;

    if(bottom == minimum && (!m_bottom || m_bottom->hidden()))
        return Position::Bottom;

    return Position::Fill;
}

bool DockBase::dragAndDrop_canAcceptPackage(gweni::draganddrop::Package *package)
{
    // A TAB button dropped
    if(package->name == "TabButtonMove")
        return true;

    // a TAB window dropped
    if(package->name == "TabWindowMove")
        return true;

    return false;
}

bool DockBase::dragAndDrop_handleDrop(gweni::draganddrop::Package *package, int x, int y)
{
    gweni::Point pos=canvasPosToLocal(gweni::Point(x, y));
    Position dir=getDroppedTabDirection(pos.x, pos.y);
    DockedTabControl *addTo=m_dockedTabControl;

    if(dir == Position::Fill && addTo == nullptr)
        return false;

    if(dir != Position::Fill)
    {
        DockBase *dock=getChildDock(dir);
        addTo=dock->m_dockedTabControl;

        if(!m_dropFar)
            dock->bringToFront();
        else
            dock->sendToBack();
    }

    if(package->name == "TabButtonMove")
    {
        TabButton *tabButton=gweni_cast<TabButton>(draganddrop::SourceControl);

        if(!tabButton)
            return false;

        addTo->addPage(tabButton);
    }

    if(package->name == "TabWindowMove")
    {
        DockedTabControl *tabControl=gweni_cast<DockedTabControl>(draganddrop::SourceControl);

        if(!tabControl)
            return false;

        if(tabControl == addTo)
            return false;

        tabControl->moveTabsTo(addTo);
    }

    invalidate();
    return true;
}

bool DockBase::isEmpty()
{
    if(m_dockedTabControl && m_dockedTabControl->tabCount() > 0)
        return false;

    if(m_left && !m_left->isEmpty())
        return false;

    if(m_right && !m_right->isEmpty())
        return false;

    if(m_top && !m_top->isEmpty())
        return false;

    if(m_bottom && !m_bottom->isEmpty())
        return false;

    return true;
}

void DockBase::onTabRemoved(event::Info)
{
    doRedundancyCheck();
    doConsolidateCheck();
}

void DockBase::doRedundancyCheck()
{
    if(!isEmpty())
        return;

    DockBase *dockParent=gweni_cast<DockBase>(getParent());

    if(!dockParent)
        return;

    dockParent->onRedundantChildDock(this);
}

void DockBase::doConsolidateCheck()
{
    if(isEmpty())
        return;

    if(!m_dockedTabControl)
        return;

    if(m_dockedTabControl->tabCount() > 0)
        return;

    if(m_bottom && !m_bottom->isEmpty())
    {
        m_bottom->m_dockedTabControl->moveTabsTo(m_dockedTabControl);
        return;
    }

    if(m_top && !m_top->isEmpty())
    {
        m_top->m_dockedTabControl->moveTabsTo(m_dockedTabControl);
        return;
    }

    if(m_left && !m_left->isEmpty())
    {
        m_left->m_dockedTabControl->moveTabsTo(m_dockedTabControl);
        return;
    }

    if(m_right && !m_right->isEmpty())
    {
        m_right->m_dockedTabControl->moveTabsTo(m_dockedTabControl);
        return;
    }
}

void DockBase::onRedundantChildDock(DockBase *dockBase)
{
    dockBase->setHidden(true);
    doRedundancyCheck();
    doConsolidateCheck();
}

void DockBase::dragAndDrop_hoverEnter(gweni::draganddrop::Package * /*package*/, int /*x*/,
    int /*y*/)
{
    m_drawHover=true;
}

void DockBase::dragAndDrop_hoverLeave(gweni::draganddrop::Package * /*package*/)
{
    m_drawHover=false;
}

void DockBase::dragAndDrop_hover(gweni::draganddrop::Package * /*package*/, int x, int y)
{
    gweni::Point pos=canvasPosToLocal(gweni::Point(x, y));
    Position dir=getDroppedTabDirection(pos.x, pos.y);

    if(dir == Position::Fill)
    {
        if(!m_dockedTabControl)
        {
            m_hoverRect=gweni::Rect(0, 0, 0, 0);
            return;
        }

        m_hoverRect=getInnerBounds();
        return;
    }

    m_hoverRect=getRenderBounds();
    int HelpBarWidth=0;

    if(dir == Position::Left)
    {
        HelpBarWidth=m_hoverRect.w/4;
        m_hoverRect.w=HelpBarWidth;
    }

    if(dir == Position::Right)
    {
        HelpBarWidth=m_hoverRect.w/4;
        m_hoverRect.x=m_hoverRect.w-HelpBarWidth;
        m_hoverRect.w=HelpBarWidth;
    }

    if(dir == Position::Top)
    {
        HelpBarWidth=m_hoverRect.h/4;
        m_hoverRect.h=HelpBarWidth;
    }

    if(dir == Position::Bottom)
    {
        HelpBarWidth=m_hoverRect.h/4;
        m_hoverRect.y=m_hoverRect.h-HelpBarWidth;
        m_hoverRect.h=HelpBarWidth;
    }

    if((dir == Position::Top || dir == Position::Bottom) && !m_dropFar)
    {
        if(m_left && m_left->visible())
        {
            m_hoverRect.x+=m_left->getWidth();
            m_hoverRect.w-=m_left->getWidth();
        }

        if(m_right && m_right->visible())
            m_hoverRect.w-=m_right->getWidth();
    }

    if((dir == Position::Left || dir == Position::Right) && !m_dropFar)
    {
        if(m_top && m_top->visible())
        {
            m_hoverRect.y+=m_top->getHeight();
            m_hoverRect.h-=m_top->getHeight();
        }

        if(m_bottom && m_bottom->visible())
            m_hoverRect.h-=m_bottom->getHeight();
    }
}

void DockBase::renderOver(skin::Base *skin)
{
    if(!m_drawHover)
        return;

    gweni::renderer::Base *render=skin->getRender();
    render->setDrawColor(gweni::Color(255, 100, 255, 20));
    render->drawFilledRect(m_primitiveIds[0], getRenderBounds());

    if(m_hoverRect.w == 0)
        return;

    render->setDrawColor(gweni::Color(255, 100, 255, 100));
    render->drawFilledRect(m_primitiveIds[1], m_hoverRect);
    render->setDrawColor(gweni::Color(255, 100, 255, 200));
    render->drawLinedRect(&m_primitiveIds[2], m_hoverRect);
}

}//namespace controls
}//namespace gweni
