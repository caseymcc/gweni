/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollBar.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(BaseScrollBar)
{
    // The arrow buttons are TxT at the two ends, placed by layout(): the bar's
    // thickness is its skin's, never its buttons' (an empty caption's).
    for(int i=0; i < 2; i++)
    {
        m_scrollButton[i]=newChild<internal::ScrollBarButton>();
        m_scrollButton[i]->setPlacement(Placement::Owner);
    }

    m_bar=newChild<internal::ScrollBarBar>();
    // Sized from the content and placed at the scroll amount by layout(), so
    // it does not count in the size this bar asks for.
    m_bar->setPlacement(Placement::Owner);
    m_depressed=false;
    m_scrolledAmount=0;
    m_contentSize=0;
    m_viewableContentSize=0;
    setNudgeAmount(20);
}

int BaseScrollBar::getButtonSize()
{
    if(skin::Base *skin=getSkin())
        return skin->Metrics.ScrollBarThickness;

    return decltype(skin::Base::Metrics){}.ScrollBarThickness;
}

void BaseScrollBar::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const int thickness=getButtonSize();
    const bool across=(dim == Dim::X) != isHorizontal();

    if(dim == Dim::X)
        m_preferredSize.width=across?thickness:preferredFromContent(thickness*2, Dim::X);
    else
        m_preferredSize.height=across?thickness:preferredFromContent(thickness*2, Dim::Y);
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
