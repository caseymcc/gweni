/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/resizer.h>

namespace gweni
{
namespace controls
{
namespace internal
{


GWENI_CONTROL_CONSTRUCTOR(Resizer)
{
    setResizeDirection(Alignment::Left);
    setMouseInputEnabled(true);
    setSize(6, 6);
}

void Resizer::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(m_disabled)
        return;

    if(!m_target)
        return;

    if(!m_depressed)
        return;

    gweni::Rect bounds=m_target->getBounds();
    Size pntMin=m_target->getMinimumSize();
    gweni::Point cursorPos=m_target->canvasPosToLocal(gweni::Point(x, y));
    gweni::Point delta=m_target->localPosToCanvas(m_holdPos);
    delta.x-=x;
    delta.y-=y;

    if(m_resizeDirection  &Alignment::Left)
    {
        bounds.x-=delta.x;
        bounds.w+=delta.x;

        // Conform to minimum size here so we don't
        // go all weird when we snap it in the base conrt
        if(bounds.w < pntMin.width)
        {
            const int diff=pntMin.width-bounds.w;
            bounds.w+=diff;
            bounds.x-=diff;
        }
    }

    if(m_resizeDirection  &Alignment::Top)
    {
        bounds.y-=delta.y;
        bounds.h+=delta.y;

        // Conform to minimum size here so we don't
        // go all weird when we snap it in the base conrt
        if(bounds.h < pntMin.height)
        {
            const int diff=pntMin.height-bounds.h;
            bounds.h+=diff;
            bounds.y-=diff;
        }
    }

    if(m_resizeDirection  &Alignment::Right)
    {
        // This is complicated.
        // Basically we want to use the HoldPos, so it doesn't snap to the edge
        // of the control
        // But we need to move the HoldPos with the window movement. Yikes.
        // I actually think this might be a big hack around the way this control
        // works with regards to the holdpos being on the parent panel.
        const int woff=bounds.w - m_holdPos.x;
        int diff=bounds.w;
        bounds.w=cursorPos.x + woff;

        if(bounds.w < pntMin.width)
            bounds.w=pntMin.width;

        diff-=bounds.w;
        m_holdPos.x-=diff;
    }

    if(m_resizeDirection  &Alignment::Bottom)
    {
        const int hoff=bounds.h - m_holdPos.y;
        int diff=bounds.h;
        bounds.h=cursorPos.y + hoff;

        if(bounds.h < pntMin.height)
            bounds.h=pntMin.height;

        diff-=bounds.h;
        m_holdPos.y-=diff;
    }

    m_target->setBounds(bounds);
    onResize.call(this);
}

void Resizer::setResizeDirection(Alignment dir)
{
    m_resizeDirection=dir;

    if((dir  &Alignment::Left)||(dir  &Alignment::Right))
    {
        m_sizeFlags.horizontal=SizeFlag::Fixed;
        m_sizeFlags.vertical=SizeFlag::Expand;
    }

    if((dir  &Alignment::Top)||(dir  &Alignment::Bottom))
    {
        m_sizeFlags.horizontal=SizeFlag::Expand;
        m_sizeFlags.vertical=SizeFlag::Fixed;
    }

    if(((dir  &Alignment::Left) && (dir  &Alignment::Top))
        || ((dir  &Alignment::Right) && (dir  &Alignment::Bottom)))
        return setCursor(gweni::cursor::SizeNWSE);

    if(((dir  &Alignment::Right) && (dir  &Alignment::Top))
        || ((dir  &Alignment::Left) && (dir  &Alignment::Bottom)))
        return setCursor(gweni::cursor::SizeNESW);

    if((dir  &Alignment::Right) || (dir  &Alignment::Left))
        return setCursor(gweni::cursor::SizeWE);

    if((dir  &Alignment::Top) || (dir  &Alignment::Bottom))
        return setCursor(gweni::cursor::SizeNS);
}

}//namespace internal
}//namespace controls
}//namespace gweni
