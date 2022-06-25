/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollBar.h>
#include <gweni/controls/verticalScrollBar.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(VerticalScrollBar)
{
    m_bar->setVertical();
    m_scrollButton[SCROLL_BUTTON_UP]->setDirectionUp();
    m_scrollButton[SCROLL_BUTTON_UP]->onPressCaller.add(this, &VerticalScrollBar::nudgeUp);
    m_scrollButton[SCROLL_BUTTON_DOWN]->setDirectionDown();
    m_scrollButton[SCROLL_BUTTON_DOWN]->onPressCaller.add(this, &VerticalScrollBar::nudgeDown);
    m_bar->onDragged.add(this, &VerticalScrollBar::onBarMoved);
}

void VerticalScrollBar::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    m_scrollButton[SCROLL_BUTTON_UP]->setDock(DockPosition::Top);
    m_scrollButton[SCROLL_BUTTON_UP]->setHeight(getWidth());
    m_scrollButton[SCROLL_BUTTON_DOWN]->setDock(DockPosition::Bottom);
    m_scrollButton[SCROLL_BUTTON_DOWN]->setHeight(getWidth());
    m_bar->setWidth(getButtonSize());
    // Add padding
    m_bar->setPadding(Padding(0, getButtonSize(), 0, getButtonSize()));
    // Calculate bar sizes
    float barHeight=(m_viewableContentSize/m_contentSize)*(getHeight()-getButtonSize());

    if(barHeight < getButtonSize()/2)
        barHeight=getButtonSize()/2;

    m_bar->setHeight(barHeight);
    m_bar->setHidden(getHeight()-(getButtonSize()*2) <= barHeight);

    if(hidden())
        setScrolledAmount(0, true);

    // Based on our last scroll amount, produce a position for the bar
    if(!m_bar->isDepressed())
        setScrolledAmount(getScrolledAmount(), true);
}

void VerticalScrollBar::scrollToTop()
{
    setScrolledAmount(0, true);
}

void VerticalScrollBar::scrollToBottom()
{
    setScrolledAmount(1, true);
}

void VerticalScrollBar::nudgeUp(event::Info)
{
    if(!isDisabled())
        setScrolledAmount(getScrolledAmount()-getNudgeAmount(), true);
}

void VerticalScrollBar::nudgeDown(event::Info)
{
    if(!isDisabled())
        setScrolledAmount(getScrolledAmount()+getNudgeAmount(), true);
}

float VerticalScrollBar::getNudgeAmount()
{
    if(m_depressed)
        return m_viewableContentSize/m_contentSize;
    else
        return ParentClass::getNudgeAmount();
}

void VerticalScrollBar::onMouseClickLeft(int x, int y, bool down)
{
    if(down)
    {
        m_depressed=true;
        gweni::MouseFocus=this;
    }
    else
    {
        gweni::Point clickPos=canvasPosToLocal(gweni::Point(x, y));

        if(clickPos.y < m_bar->getY())
            nudgeUp(this);
        else if(clickPos.y > m_bar->getY()+m_bar->getHeight())
            nudgeDown(this);

        m_depressed=false;
        gweni::MouseFocus=nullptr;
    }
}

float VerticalScrollBar::calculateScrolledAmount()
{
    return static_cast<float>(m_bar->getY() - getButtonSize())
        / (getHeight() - m_bar->getHeight() - getButtonSize()*2);
}

bool VerticalScrollBar::setScrolledAmount(float amount, bool forceUpdate)
{
    amount=gweni::clamp(amount, 0.f, 1.f);

    if(!ParentClass::setScrolledAmount(amount, forceUpdate))
        return false;

    if(forceUpdate)
    {
        const int newY=getButtonSize()
            + amount  *(getHeight() - m_bar->getHeight() - getButtonSize()*2);
        m_bar->moveTo(m_bar->getX(), newY);
    }

    return true;
}

void VerticalScrollBar::onBarMoved(event::Info info)
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
