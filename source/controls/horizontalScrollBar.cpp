/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollBar.h>
#include <gweni/controls/horizontalScrollBar.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(HorizontalScrollBar)
{
    m_bar->setHorizontal();
    m_scrollButton[SCROLL_BUTTON_LEFT]->setDirectionLeft();
    m_scrollButton[SCROLL_BUTTON_LEFT]->onPressCaller.add(this, &HorizontalScrollBar::nudgeLeft);
    m_scrollButton[SCROLL_BUTTON_RIGHT]->setDirectionRight();
    m_scrollButton[SCROLL_BUTTON_RIGHT]->onPressCaller.add(this, &HorizontalScrollBar::nudgeRight);
    m_bar->onDragged.add(this, &HorizontalScrollBar::onBarMoved);
}

void HorizontalScrollBar::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    // Everything below sets up children this pass lays out next.
    PlacementScope placement;

    const int thickness=getButtonSize();
    // The track between the two buttons, which the thumb runs along.
    const int track=getWidth()-thickness*2;

    placeChild(m_scrollButton[SCROLL_BUTTON_LEFT], 0, 0, thickness, thickness);
    placeChild(m_scrollButton[SCROLL_BUTTON_RIGHT], getWidth()-thickness, 0, thickness, thickness);

    // Its padding keeps a dragged thumb on the track.
    m_bar->setPadding(Padding(thickness, 0, thickness, 0));

    float barWidth=(m_viewableContentSize/m_contentSize)*track;

    if(barWidth < thickness/2)
        barWidth=thickness/2;

    placeChildSize(m_bar, barWidth, thickness);
    m_bar->setHidden(track <= barWidth);

    // Based on our last scroll amount, produce a position for the bar. Only
    // the thumb moves: setScrolledAmount() would invalidate this bar and its
    // ScrollControl, which is laying it out right now.
    if(!m_bar->isDepressed())
        placeBar();
}

void HorizontalScrollBar::placeBar()
{
    const int newX=getButtonSize()
        + (getScrolledAmount()*((getWidth()-m_bar->getWidth())-(getButtonSize()*2)));
    const Point pos=m_bar->restrictedPos(newX, m_bar->getY());

    placeChildPos(m_bar, pos.x, pos.y);
}

void HorizontalScrollBar::nudgeLeft(event::Info)
{
    if(!isDisabled())
        setScrolledAmount(getScrolledAmount()-getNudgeAmount(), true);
}

void HorizontalScrollBar::nudgeRight(event::Info)
{
    if(!isDisabled())
        setScrolledAmount(getScrolledAmount()+getNudgeAmount(), true);
}

void HorizontalScrollBar::scrollToLeft()
{
    setScrolledAmount(0, true);
}

void HorizontalScrollBar::scrollToRight()
{
    setScrolledAmount(1, true);
}

float HorizontalScrollBar::getNudgeAmount()
{
    if(m_depressed)
        return m_viewableContentSize/m_contentSize;
    else
        return ParentClass::getNudgeAmount();
}

void HorizontalScrollBar::onMouseClickLeft(int x, int y, bool down)
{
    if(down)
    {
        m_depressed=true;
        MouseFocus=this;
    }
    else
    {
        Point clickPos=canvasPosToLocal(Point(x, y));

        if(clickPos.x < m_bar->getX())
            nudgeLeft(this);
        else if(clickPos.x > m_bar->getX()+m_bar->getWidth())
            nudgeRight(this);

        m_depressed=false;
        MouseFocus=nullptr;
    }
}

float HorizontalScrollBar::calculateScrolledAmount()
{
    return static_cast<float>(m_bar->getX() - getButtonSize())
        / (getWidth() - m_bar->getWidth() - getButtonSize()*2);
}

bool HorizontalScrollBar::setScrolledAmount(float amount, bool forceUpdate)
{
    amount=clamp(amount, 0.f, 1.f);

    if(!ParentClass::setScrolledAmount(amount, forceUpdate))
        return false;

    if(forceUpdate)
        placeBar();

    return true;
}

void HorizontalScrollBar::onBarMoved(event::Info info)
{
    if(m_bar->isDepressed())
    {
        setScrolledAmount(calculateScrolledAmount(), false);
        ParentClass::onBarMoved(info);
    }
    else
    {
        invalidateParent();
    }
}

}//namespace controls
}//namespace gweni


