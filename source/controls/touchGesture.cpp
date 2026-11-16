/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */

#include "touchGesture.h"

#include <gweni/inputHandler.h>

#include <cmath>

namespace gweni
{
namespace controls
{
namespace internal
{

void TouchGesture::start(Base *target, const PointF &at)
{
    m_target=(target && (target != m_canvas))?target->getControlId():0;
    m_down=at;
    m_last=at;
    m_panning=false;
    m_axes[0]=Axis();
    m_axes[1]=Axis();
}

void TouchGesture::abandon()
{
    if(active())
        m_canvas->setInputCapture(nullptr);
    m_target=0;
    m_panning=false;
}

void TouchGesture::captureTouch(int /*id*/, TouchPhase phase, float x, float y)
{
    switch(phase)
    {
    case TouchPhase::Down:
        break;
    case TouchPhase::Move:
        move(PointF(x, y));
        break;
    case TouchPhase::Up:
        move(PointF(x, y));
        lift(false);
        break;
    case TouchPhase::Cancel:
        lift(true);
        break;
    }
}

void TouchGesture::move(const PointF &at)
{
    float stepX=at.x-m_last.x;
    float stepY=at.y-m_last.y;

    m_last=at;
    if(!m_panning)
    {
        const float fromX=at.x-m_down.x;
        const float fromY=at.y-m_down.y;

        if(std::sqrt(fromX*fromX+fromY*fromY) < Canvas::TouchSlop)
            return;

        // the pan catches up with the finger: what it moved under the slop
        // counts too
        m_panning=true;
        stepX=fromX;
        stepY=fromY;
    }

    m_axes[0].pending+=stepX;
    m_axes[1].pending+=stepY;

    for(int axis=0; axis < 2; ++axis)
    {
        if(!m_axes[axis].control)
            latch(axis);
    }
    panLatched();
}

float TouchGesture::unitsPerCanvasUnit(Base *control, int axis) const
{
    const PointF from=control->canvasPosToLocalF(m_down);
    const PointF to=control->canvasPosToLocalF(PointF(m_down.x+(axis == 0?1.0f:0.0f),
        m_down.y+(axis == 1?1.0f:0.0f)));
    const float units=(axis == 0)?(to.x-from.x):(to.y-from.y);

    return (units > 0.0f)?units:1.0f;
}

// Only once a whole unit has gathered: a ZoomPanel takes any fraction, so a
// fraction alone would hand the axis to it past a ScrollControl under it that
// moves in whole units. Nothing under the finger moving along the axis drops
// what gathered, so it does not pile up for a control that can move later.
void TouchGesture::latch(int axis)
{
    Axis &state=m_axes[axis];

    if(std::fabs(state.pending) < 1.0f)
        return;

    for(Base *control=Base::getControlById(m_target); control && (control != m_canvas);
        control=control->getActualParent())
    {
        const float scale=unitsPerCanvasUnit(control, axis);
        const float local=state.pending*scale;
        const PointF moved=control->touchPan(axis == 0?PointF(local, 0.0f):PointF(0.0f, local));
        const float along=(axis == 0)?moved.x:moved.y;

        if(along != 0.0f)
        {
            state.control=control->getControlId();
            state.scale=scale;
            state.pending-=along/scale;
            return;
        }
    }
    state.pending=0.0f;
}

// One call per control: a ZoomPanel holding both axes is panned once.
void TouchGesture::panLatched()
{
    Axis &x=m_axes[0];
    Axis &y=m_axes[1];
    const bool same=x.control && (x.control == y.control);

    for(int axis=0; axis < 2; ++axis)
    {
        Axis &state=m_axes[axis];

        if(!state.control || ((axis == 1) && same))
            continue;

        Base *control=Base::getControlById(state.control);

        if(!control || !control->visible())
        {
            state.control=0;
            state.pending=0.0f;
            continue;
        }

        PointF delta(0.0f, 0.0f);

        if(same)
            delta=PointF(x.pending*x.scale, y.pending*y.scale);
        else if(axis == 0)
            delta.x=x.pending*x.scale;
        else
            delta.y=y.pending*y.scale;

        if((std::fabs(delta.x) < 1e-6f) && (std::fabs(delta.y) < 1e-6f))
            continue;

        const PointF moved=control->touchPan(delta);

        if(same || (axis == 0))
            x.pending-=moved.x/x.scale;
        if(same || (axis == 1))
            y.pending-=moved.y/y.scale;
    }
}

void TouchGesture::lift(bool cancelled)
{
    Canvas *canvas=m_canvas;
    const bool tap=!m_panning && !cancelled;
    const PointF down=m_down;

    abandon();
    if(!canvas)
        return;

    if(tap)
    {
        const Point point(static_cast<int>(std::lround(down.x)), static_cast<int>(std::lround(down.y)));
        const Point mouse=input::getMousePosition();

        canvas->inputMouseMoved(point.x, point.y, point.x-mouse.x, point.y-mouse.y);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
    }
    canvas->inputMouseLeave();
}

}//namespace internal
}//namespace controls
}//namespace gweni
