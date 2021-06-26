/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollBar.h>
#include <gweni/controls/scrollBarButton.h>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(ScrollBarButton)
{
    setDirectionUp();
}

void ScrollBarButton::setDirectionUp()
{
    m_direction=Position::Top;
}

void ScrollBarButton::setDirectionDown()
{
    m_direction=Position::Bottom;
}

void ScrollBarButton::setDirectionLeft()
{
    m_direction=Position::Left;
}

void ScrollBarButton::setDirectionRight()
{
    m_direction=Position::Right;
}

void ScrollBarButton::render(skin::Base *skin)
{
//    skin->drawScrollButton(this, skin::Generate, m_direction, m_depressed, isHovered(), isDisabled());
    skin->drawControl(this);
}

}//namespace internal
}//namespace controls
}//namespace gweni
