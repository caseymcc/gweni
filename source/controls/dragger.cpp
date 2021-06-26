/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/dragger.h>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(Dragger)
{
    m_target=nullptr;
    setMouseInputEnabled(true);
    m_depressed=false;
    m_doMove=true;
}

void Dragger::onMouseClickLeft(int x, int y, bool down)
{
    if(m_disabled)
        return;

    if(down)
    {
        m_depressed=true;

        if(m_target)
            m_holdPos=m_target->canvasPosToLocal(gweni::Point(x, y));

        gweni::MouseFocus=this;
        onDragStart.call(this);
    }
    else
    {
        m_depressed=false;
        gweni::MouseFocus=nullptr;
    }
}

void Dragger::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    if(m_disabled)
        return;

    if(!m_depressed)
        return;

    if(m_doMove && m_target)
    {
        gweni::Point p=gweni::Point(x-m_holdPos.x, y-m_holdPos.y);

        // Translate to parent
        if(m_target->getParent())
            p=m_target->getParent()->canvasPosToLocal(p);

        m_target->moveTo(p.x, p.y);
    }

    gweni::event::Info info(this);
    info.point=Point(deltaX, deltaY);
    onDragged.call(this, info);
}

void Dragger::render(skin::Base *skin)
{
    // skin->drawButton(this,false,false, false);
}

void Dragger::onMouseDoubleClickLeft(int x, int y)
{
    if(m_disabled)
        return;

    onDoubleClickLeft.call(this);
}

}//namespace internal
}//namespace controls
}//namespace gweni
