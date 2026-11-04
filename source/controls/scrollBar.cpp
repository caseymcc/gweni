/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollBar.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/scrollBarBar.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(BaseScrollBar)
{
    for(int i=0; i < 2; i++)
    {
        m_scrollButton[i]=newChild<internal::ScrollBarButton>();
    }

    m_bar=newChild<internal::ScrollBarBar>();
    setBounds(0, 0, 15, 15);
    m_depressed=false;
    m_scrolledAmount=0;
    m_contentSize=0;
    m_viewableContentSize=0;
    setNudgeAmount(20);
}

void BaseScrollBar::onBarMoved(event::Info)
{
    onBarMovedCaller.call(this);
}

void BaseScrollBar::barMovedNotification()
{
    onBarMoved(this);
}

void BaseScrollBar::setContentSize(float size)
{
    if(m_contentSize != size)
        invalidate();

    m_contentSize=size;
}

void BaseScrollBar::setViewableContentSize(float size)
{
    if(m_viewableContentSize != size)
        invalidate();

    m_viewableContentSize=size;
}

bool BaseScrollBar::setScrolledAmount(float amount, bool forceUpdate)
{
    if(m_scrolledAmount == amount && !forceUpdate)
        return false;

    m_scrolledAmount=amount;
    invalidate();
    barMovedNotification();
    return true;
}

}//namespace controls
}//namespace gweni
