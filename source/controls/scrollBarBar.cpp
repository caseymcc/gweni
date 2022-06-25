/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollBar.h>
#include <gweni/controls/scrollBarBar.h>

namespace gweni
{
namespace controls
{
namespace internal
{

// Actual bar representing height of parent

GWENI_CONTROL_CONSTRUCTOR(ScrollBarBar)
{
    restrictToParent(true);
    setTarget(this);
}

//void ScrollBarBar::render(skin::Base *skin)
//{
//
////    skin->drawScrollBarBar(this, skin::Generate, m_depressed, isHovered(), m_horizontal);
//    if(getStateChange() != StateChange_Nothing)
//    {
//        if(m_skinControl)
//            m_skinControl->update(skin->getRenderer(), this);
//    }
//
//    ParentClass::render(skin);
//}

void ScrollBarBar::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);

    if(!m_depressed)
        return;

    invalidateParent();
}

void ScrollBarBar::onMouseClickLeft(int x, int y, bool down)
{
    ParentClass::onMouseClickLeft(x, y, down);
    invalidateParent();
}

void ScrollBarBar::layout(skin::Base * /*skin*/)
{
    if(!getParent())
        return;

    // Move to our current position to force clamping - is this a hack?
    moveTo(getX(), getY());
}

void ScrollBarBar::moveTo(int x, int y)
{
    ParentClass::moveTo(x, y);
}

}//namespace internal
}//namespace controls
}//namespace gweni
