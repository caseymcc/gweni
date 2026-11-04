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
    m_direction=Alignment::Top;
}

void ScrollBarButton::setDirectionDown()
{
    m_direction=Alignment::Bottom;
}

void ScrollBarButton::setDirectionLeft()
{
    m_direction=Alignment::Left;
}

void ScrollBarButton::setDirectionRight()
{
    m_direction=Alignment::Right;
}

}//namespace internal
}//namespace controls
}//namespace gweni
