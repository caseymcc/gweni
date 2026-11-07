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

    const bool wasDepressed=m_depressed;

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

    // A handle that has a down look (a slider thumb, a scroll bar thumb, a
    // splitter bar) shows it on the next frame.
    if(m_depressed != wasDepressed)
        redraw();
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

        // A part its owner places (a splitter bar, a slider or scrollbar
        // thumb) is moved as computed bounds; the owner reads the new
        // position as its value. Anything else, a window dragged by its title
        // bar, keeps where the user put it: a request.
        if(m_target->getPlacement() == Placement::Owner)
        {
            p=m_target->restrictedPos(p.x, p.y);
            placeChildPos(m_target, p.x, p.y);
        }
        else
            m_target->moveTo(p.x, p.y);
    }

    gweni::event::Info info(this);
    info.point=Point(deltaX, deltaY);
    onDragged.call(this, info);
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
