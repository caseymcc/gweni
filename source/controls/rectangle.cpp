/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/rectangle.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(Rectangle)
{
    enlargePrimitiveIds(this, m_primitiveIds, 1);
    m_color=gweni::Color(255, 255, 255, 255);
}

void Rectangle::render(skin::Base *skin)
{
    skin->getRender()->setDrawColor(m_color);
    skin->getRender()->drawFilledRect(m_primitiveIds[0], getRenderBounds());
}

}//namespace controls
}//namespace gweni
