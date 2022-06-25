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
    m_scrollButton[SCROLL_BUTTON_LEFT]->setWidth(getHeight());
    m_scrollButton[SCROLL_BUTTON_LEFT]->setDock(DockPosition::Left);
    m_scrollButton[SCROLL_BUTTON_RIGHT]->setWidth(getHeight());
    m_scrollButton[SCROLL_BUTTON_RIGHT]->setDock(DockPosition::Right);
    m_bar->setHeight(getButtonSize());
    m_bar->setPadding(Padding(getButtonSize(), 0, getButtonSize(), 0));
    float barWidth=(m_viewableContentSize/m_contentSize)*(getWidth()-getButtonSize());

    if(barWidth < getButtonSize()/2)
        barWidth=getButtonSize()/2;

    m_bar->setWidth(barWidth);
    m_bar->setHidden(getWidth()-(getButtonSize()*2) <= barWidth);

    // Based on our last scroll amount, produce a position for the bar
    if(!m_bar->isDepressed())
        setScrolledAmount(getScrolledAmount(), true);
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
    {
        int newX=getButtonSize()+(amount*((getWidth()-m_bar->getWidth())-(getButtonSize()*2)));
        m_bar->moveTo(newX, m_bar->getY());
    }

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


