/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollControl.h>
#include <gweni/controls/scrollBar.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/horizontalScrollBar.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(ScrollControl)
{
    setMouseInputEnabled(false);
    m_verticalScrollBar=newChild<VerticalScrollBar>();
    m_verticalScrollBar->setDock(DockPosition::Right);
    m_verticalScrollBar->onBarMovedCaller.add(this, &ScrollControl::onVeritcalBarMoved);
    m_verticalScrollBar->setNudgeAmount(30);
    m_canScrollVertical=true;

    m_horizontalScrollBar=newChild<HorizontalScrollBar>();
    m_horizontalScrollBar->setDock(DockPosition::Bottom);
    m_horizontalScrollBar->onBarMovedCaller.add(this, &ScrollControl::onHorizontalBarMoved);
    m_canScrollHorizontal=true;
    m_horizontalScrollBar->setNudgeAmount(30);

    m_innerPanel=newChild<Base>();
    m_innerPanel->setAlignment(Alignment::Absolute);
    m_innerPanel->setPos(0, 0);
    m_innerPanel->setMargin(Margin(5, 5, 5, 5));
    m_innerPanel->sendToBack();
    m_innerPanel->setMouseInputEnabled(true);
    m_autoHideBars=true;
}

void ScrollControl::setScroll(bool h, bool v)
{
    m_canScrollVertical=v;
    m_canScrollHorizontal=h;
    m_verticalScrollBar->setHidden(!m_canScrollVertical);
    m_horizontalScrollBar->setHidden(!m_canScrollHorizontal);
}

void ScrollControl::setInnerSize(int w, int h)
{
    m_innerPanel->setSize(w, h);
}

void ScrollControl::onVeritcalBarMoved(event::Info)
{
    invalidate();
}

void ScrollControl::onHorizontalBarMoved(event::Info)
{
    invalidate();
}

void ScrollControl::onChildBoundsChanged(gweni::Rect /*oldChildBounds*/, Base * /*child*/)
{
    updateScrollBars();
    invalidate();
}

void ScrollControl::layout(skin::Base *skin)
{
    updateScrollBars();
    ParentClass::layout(skin);
}

bool ScrollControl::onMouseWheeled(int iDelta)
{
    if(canScrollVertical() && m_verticalScrollBar->visible())
    {
        if(m_verticalScrollBar->setScrolledAmount(
            m_verticalScrollBar->getScrolledAmount()
            - m_verticalScrollBar->getNudgeAmount()  *iDelta / 60.0f, true))
            return true;
    }

    if(canScrollHorizontal() && m_horizontalScrollBar->visible())
    {
        if(m_horizontalScrollBar->setScrolledAmount(
            m_horizontalScrollBar->getScrolledAmount()
            - m_horizontalScrollBar->getNudgeAmount()  *iDelta / 60.0f, true))
            return true;
    }

    return false;
}

void ScrollControl::render(skin::Base *skin)
{
#if 0
    // Debug render - this shouldn't render ANYTHING REALLY - it should be up to
    // the parent!
    gweni::Rect rect=getRenderBounds();
    gweni::renderer::Base *render=skin->getRenderer();
    render->setDrawColor(gweni::Color(255, 255, 0, 100));
    render->drawFilledRect(rect);
    render->setDrawColor(gweni::Color(255, 0, 0, 100));
    render->drawFilledRect(m_innerPanel->getBounds());
    render->renderText(skin->getDefaultFont(),
        gweni::Point(0, 0),
        format("Offset: %i %i", m_innerPanel->getX(), m_innerPanel->getY()));
#else // 0
    (void)skin;
#endif // 0
}

bool ScrollControl::contentsAreDocked()
{
    if(!m_innerPanel)
        return false;

    for(auto &&child : m_innerPanel->getChildren())
    {
        if(child->getDock() == DockPosition::None)
            return false;
    }

    return true;
}

void ScrollControl::updateScrollBars()
{
    if(!m_innerPanel)
        return;

    // Not sure what this is here to fix/change. Adding it breaks auto-scrollbars for
    // menus, controls, etc. -- BQ
    //
    //    if (contentsAreDocked())
    //    {
    //        m_verticalScrollBar->setHidden(false);
    //        m_horizontalScrollBar->setHidden(false);
    //        m_innerPanel->setSize(getSize());
    //        m_innerPanel->setPos(0, 0);
    //        return;
    //    }

    int childrenWidth=0;
    int childrenHeight=0;

    // Get the max size of all our children together
    for(auto &&child : m_innerPanel->getChildren())
    {
        childrenWidth=std::max(childrenWidth, child->right());
        childrenHeight=std::max(childrenHeight, child->bottom());
    }

    if(m_canScrollHorizontal)
    {
        m_innerPanel->setSize(std::max(getWidth(), childrenWidth),
            std::max(getHeight(), childrenHeight));
    }
    else
    {
        m_innerPanel->setSize(getWidth() - (m_verticalScrollBar->hidden()
            ?0:m_verticalScrollBar->getWidth()-1),
            std::max(getHeight(), childrenHeight));
    }

    const float wPercent=
        static_cast<float>(getWidth()) /
        (childrenWidth + (m_verticalScrollBar->hidden()?0:m_verticalScrollBar->getWidth()));

    const float hPercent=
        static_cast<float>(getHeight()) /
        (childrenHeight
            + (m_horizontalScrollBar->hidden()?0:m_horizontalScrollBar->getHeight()));

    if(m_canScrollVertical)
        setVerticalScrollRequired(hPercent >= 1);
    else
        m_verticalScrollBar->setHidden(true);

    if(m_canScrollHorizontal)
        setHorizontalScrollRequired(wPercent >= 1);
    else
        m_horizontalScrollBar->setHidden(true);

    m_verticalScrollBar->setContentSize(m_innerPanel->getHeight());
    m_verticalScrollBar->setViewableContentSize(getHeight() - (m_horizontalScrollBar->hidden()
        ?0:m_horizontalScrollBar->getHeight()));
    m_horizontalScrollBar->setContentSize(m_innerPanel->getWidth());
    m_horizontalScrollBar->setViewableContentSize(getWidth() - (m_verticalScrollBar->hidden()
        ?0:m_verticalScrollBar->getWidth()));
    int newInnerPanelPosX=0;
    int newInnerPanelPosY=0;

    if(canScrollVertical() && !m_verticalScrollBar->hidden())
    {
        newInnerPanelPosY=
            -(m_innerPanel->getHeight() - getHeight() +
                (m_horizontalScrollBar->hidden()?0:m_horizontalScrollBar->getHeight()))
            *m_verticalScrollBar->getScrolledAmount();
    }

    if(canScrollHorizontal() && !m_horizontalScrollBar->hidden())
    {
        newInnerPanelPosX=
            -(m_innerPanel->getWidth() - getWidth() +
                (m_verticalScrollBar->hidden()?0:m_verticalScrollBar->getWidth()))
            *m_horizontalScrollBar->getScrolledAmount();
    }

    m_innerPanel->setPos(newInnerPanelPosX, newInnerPanelPosY);
}

void ScrollControl::setVerticalScrollRequired(bool req)
{
    if(req)
    {
        m_verticalScrollBar->setScrolledAmount(0, true);
        m_verticalScrollBar->setDisabled(true);

        if(m_autoHideBars)
            m_verticalScrollBar->setHidden(true);
    }
    else
    {
        m_verticalScrollBar->setHidden(false);
        m_verticalScrollBar->setDisabled(false);
    }
}

void ScrollControl::setHorizontalScrollRequired(bool req)
{
    if(req)
    {
        m_horizontalScrollBar->setScrolledAmount(0, true);
        m_horizontalScrollBar->setDisabled(true);

        if(m_autoHideBars)
            m_horizontalScrollBar->setHidden(true);
    }
    else
    {
        m_horizontalScrollBar->setHidden(false);
        m_horizontalScrollBar->setDisabled(true);
    }
}

void ScrollControl::scrollToBottom()
{
    if(!canScrollVertical())
        return;

    updateScrollBars();
    m_verticalScrollBar->scrollToBottom();
}

void ScrollControl::scrollToTop()
{
    if(canScrollVertical())
    {
        updateScrollBars();
        m_verticalScrollBar->scrollToTop();
    }
}

void ScrollControl::scrollToLeft()
{
    if(canScrollHorizontal())
    {
        updateScrollBars();
        m_horizontalScrollBar->scrollToLeft();
    }
}

void ScrollControl::scrollToRight()
{
    if(canScrollHorizontal())
    {
        updateScrollBars();
        m_horizontalScrollBar->scrollToRight();
    }
}

void ScrollControl::clear()
{
    m_innerPanel->removeAllChildren();
}

}//namespace controls
}//namespace gweni
