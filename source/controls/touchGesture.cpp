/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */

#include "touchGesture.h"

#include <gweni/anim.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{
namespace internal
{

#if GWENI_ANIMATE

namespace
{

// A control panned on after the finger lifted: its speed per axis in canvas
// units a second, falling by e every Canvas::FlingTimeConstant on the canvas
// clock, the distance it covers given to touchPan() frame by frame, what that
// did not move (a ScrollControl's fractions) kept. An axis stops under
// Canvas::StopFlingSpeed or once a whole unit asked for moves nothing (the
// end of the range); the animation finishes when both have.
class Fling;

std::vector<Fling *> &liveFlings()
{
    static std::vector<Fling *> flings;
    return flings;
}

class Fling: public anim::Animation
{
public:
    Fling(const float velocity[2], const float scale[2], const float pending[2])
    {
        for(int axis=0; axis < 2; ++axis)
        {
            m_velocity[axis]=velocity[axis];
            m_scale[axis]=scale[axis];
            m_pending[axis]=pending[axis];
            m_moving[axis]=std::fabs(velocity[axis]) >= Canvas::StopFlingSpeed;
        }
        m_last=platform::getTimeInSeconds();
        liveFlings().push_back(this);
    }

    ~Fling() override
    {
        std::vector<Fling *> &flings=liveFlings();

        flings.erase(std::remove(flings.begin(), flings.end(), this), flings.end());
    }

    void think() override
    {
        if(!m_control->visible())
        {
            m_moving[0]=m_moving[1]=false;
            return;
        }

        const double now=platform::getTimeInSeconds();
        const double elapsed=now-m_last;

        if(elapsed <= 0.0)
            return;
        m_last=now;

        const double decay=std::exp(-elapsed/Canvas::FlingTimeConstant);
        PointF delta(0.0f, 0.0f);

        for(int axis=0; axis < 2; ++axis)
        {
            if(!m_moving[axis])
                continue;

            m_pending[axis]+=static_cast<float>(m_velocity[axis]*Canvas::FlingTimeConstant*(1.0-decay));
            m_velocity[axis]=static_cast<float>(m_velocity[axis]*decay);
            (axis == 0?delta.x:delta.y)=m_pending[axis]*m_scale[axis];
        }

        const PointF moved=m_control->touchPan(delta);

        for(int axis=0; axis < 2; ++axis)
        {
            if(!m_moving[axis])
                continue;

            const float asked=(axis == 0)?delta.x:delta.y;
            const float along=(axis == 0)?moved.x:moved.y;

            m_pending[axis]-=along/m_scale[axis];
            if(((along == 0.0f) && (std::fabs(asked) >= 1.0f))
                || (std::fabs(m_velocity[axis]) < Canvas::StopFlingSpeed))
                m_moving[axis]=false;
        }
    }

    bool finished() override { return !m_moving[0] && !m_moving[1]; }

private:
    float m_velocity[2];
    float m_scale[2];
    float m_pending[2];
    bool m_moving[2];
    double m_last;
};

}//namespace

#endif//GWENI_ANIMATE

size_t TouchGesture::stopFlings(Canvas *canvas)
{
    size_t stopped=0;

#if GWENI_ANIMATE
    // cancelling deletes, and a deleted fling leaves the list
    const std::vector<Fling *> flings=liveFlings();

    for(Fling *fling : flings)
    {
        if(fling->m_removed || !fling->m_control || (fling->m_control->getCanvas() != canvas))
            continue;

        ++stopped;
        anim::cancel(fling->m_control, fling);
    }
#else
    (void)canvas;
#endif
    return stopped;
}

void TouchGesture::start(Base *target, const PointF &at, bool stoppedFling)
{
    m_target=(target && (target != m_canvas))?target->getControlId():NoControl;
    m_down=at;
    m_last=at;
    m_panning=false;
    m_noTap=stoppedFling;
    m_longPressed=false;
    m_axes[0]=Axis();
    m_axes[1]=Axis();
    m_samples.clear();
    m_samples.push_back({platform::getTimeInSeconds(), at});
    m_direction[0]=m_direction[1]=0;
    m_extreme[0]=at.x;
    m_extreme[1]=at.y;
    m_extremeTime[0]=m_extremeTime[1]=m_samples.back().time;

    // a finger that stops a fling is no long press either
    if(m_canvas && (m_target != NoControl) && !stoppedFling)
        m_longPressTimer=m_canvas->after(Canvas::LongPressSeconds, [this]() { longPress(); });
}

void TouchGesture::abandon()
{
    if(m_canvas && m_longPressTimer)
        m_canvas->cancel(m_longPressTimer);
    m_longPressTimer=TimerHandle();
    if(active())
        m_canvas->setInputCapture(nullptr);
    m_target=NoControl;
    m_panning=false;
}

// Only while the finger is still down and within the slop: a pan or a lift
// cancels the timer.
void TouchGesture::longPress()
{
    m_longPressTimer=TimerHandle();
    if(!active())
        return;

    Base *target=Base::getControlById(m_target);
    Base *owner=target?m_canvas->contextMenuOwnerFor(target):nullptr;

    if(!owner)
        return;

    m_longPressed=true;
    m_noTap=true;
    m_canvas->closeMenus();
    m_canvas->showContextMenu(owner, Point(static_cast<int>(std::lround(m_down.x)),
        static_cast<int>(std::lround(m_down.y))));
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
    // after a long press the finger only lifts
    if(m_longPressed)
        return;

    float stepX=at.x-m_last.x;
    float stepY=at.y-m_last.y;
    const double now=platform::getTimeInSeconds();

    m_last=at;

    // A turn back along an axis, the slop or more from the furthest the
    // finger went the other way: the speed is taken from that furthest
    // point on. A finger rolling back a little as it lifts, or wobbling
    // sideways, turns nothing.
    double turnedAt=-1.0;

    for(int axis=0; axis < 2; ++axis)
    {
        const float along=(axis == 0)?at.x:at.y;
        const float from=along-m_extreme[axis];

        if(m_direction[axis] == 0)
        {
            if(std::fabs(from) >= 1.0f)
            {
                m_direction[axis]=(from > 0.0f)?1:-1;
                m_extreme[axis]=along;
                m_extremeTime[axis]=now;
            }
        }
        else if(from*m_direction[axis] > 0.0f)
        {
            m_extreme[axis]=along;
            m_extremeTime[axis]=now;
        }
        else if(-from*m_direction[axis] >= Canvas::TouchSlop)
        {
            turnedAt=std::max(turnedAt, m_extremeTime[axis]);
            m_direction[axis]=-m_direction[axis];
            m_extreme[axis]=along;
            m_extremeTime[axis]=now;
        }
    }
    if(turnedAt >= 0.0)
    {
        while((m_samples.size() > 1) && (m_samples[1].time <= turnedAt))
            m_samples.erase(m_samples.begin());
    }

    // the samples the fling's speed is taken over, and the one before them
    if(!m_samples.empty() && (m_samples.back().time == now))
        m_samples.back().at=at;
    else
        m_samples.push_back({now, at});
    while((m_samples.size() > 2) && (m_samples[1].time < now-Canvas::FlingWindowSeconds))
        m_samples.erase(m_samples.begin());

    if(!m_panning)
    {
        const float fromX=at.x-m_down.x;
        const float fromY=at.y-m_down.y;

        if(std::sqrt(fromX*fromX+fromY*fromY) < Canvas::TouchSlop)
            return;

        // the pan catches up with the finger: what it moved under the slop
        // counts too
        m_panning=true;
        if(m_longPressTimer)
        {
            m_canvas->cancel(m_longPressTimer);
            m_longPressTimer=TimerHandle();
        }
        stepX=fromX;
        stepY=fromY;
    }

    m_axes[0].pending+=stepX;
    m_axes[1].pending+=stepY;

    for(int axis=0; axis < 2; ++axis)
    {
        if(m_axes[axis].control == NoControl)
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

// A control is asked once a whole unit of its own has gathered: a ZoomPanel
// takes any fraction, so a fraction alone would hand the axis to it past a
// ScrollControl under it that moves in whole units (of its own, smaller than
// the canvas's in a panel zoomed in). Nothing under the finger moving along
// the axis drops what gathered, so it does not pile up for a control that can
// move later.
void TouchGesture::latch(int axis)
{
    Axis &state=m_axes[axis];

    for(Base *control=Base::getControlById(m_target); control && (control != m_canvas);
        control=control->getActualParent())
    {
        const float scale=unitsPerCanvasUnit(control, axis);
        const float local=state.pending*scale;

        if(std::fabs(local) < 1.0f)
            return;

        const PointF moved=control->touchPan(axis == 0?PointF(local, 0.0f):PointF(0.0f, local));
        const float along=(axis == 0)?moved.x:moved.y;

        if(along != 0.0f)
        {
            state.control=control->getControlId();
            state.scale=scale;
            state.pending-=along/scale;
            dropRefused(state);
            return;
        }
    }
    state.pending=0.0f;
}

// What a latched control left of a pan: a whole unit of its own or more is
// what it refused at the end of its range, dropped, so the finger turning
// back moves it at once; the fraction under a unit is kept.
void TouchGesture::dropRefused(Axis &state)
{
    const float local=state.pending*state.scale;

    if(std::fabs(local) >= 1.0f)
        state.pending=std::fmod(local, 1.0f)/state.scale;
}

// One call per control: a ZoomPanel holding both axes is panned once.
void TouchGesture::panLatched()
{
    Axis &x=m_axes[0];
    Axis &y=m_axes[1];
    const bool same=(x.control != NoControl) && (x.control == y.control);

    for(int axis=0; axis < 2; ++axis)
    {
        Axis &state=m_axes[axis];

        if((state.control == NoControl) || ((axis == 1) && same))
            continue;

        Base *control=Base::getControlById(state.control);

        if(!control || !control->visible())
        {
            state.control=NoControl;
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
        {
            x.pending-=moved.x/x.scale;
            dropRefused(x);
        }
        if(same || (axis == 1))
        {
            y.pending-=moved.y/y.scale;
            dropRefused(y);
        }
    }
}

// The finger's speed over the window before the lift, each latched axis's
// control flung at it: one fling per control, both axes in one when they went
// to the same one.
void TouchGesture::fling()
{
#if GWENI_ANIMATE
    if(m_samples.size() < 2)
        return;

    const Sample &last=m_samples.back();
    const Sample *first=&m_samples.front();

    // the oldest inside the window, or the one just before it when none
    // is (moves a frame apart, the clock stepped once)
    for(const Sample &sample : m_samples)
    {
        if(sample.time >= last.time-Canvas::FlingWindowSeconds)
        {
            first=&sample;
            break;
        }
    }
    if(first == &last)
        first=&m_samples[m_samples.size()-2];

    const double elapsed=last.time-first->time;

    if((elapsed <= 0.0) || (elapsed > 2.0*Canvas::FlingWindowSeconds))
        return;

    float velocity[2]={
        static_cast<float>((last.at.x-first->at.x)/elapsed),
        static_cast<float>((last.at.y-first->at.y)/elapsed)};

    if(std::sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]) < Canvas::MinFlingSpeed)
        return;

    for(float &speed : velocity)
        speed=std::min(std::max(speed, -Canvas::MaxFlingSpeed), Canvas::MaxFlingSpeed);

    for(int axis=0; axis < 2; ++axis)
    {
        const Axis &state=m_axes[axis];

        if((state.control == NoControl) || ((axis == 1) && (state.control == m_axes[0].control)))
            continue;

        Base *control=Base::getControlById(state.control);

        if(!control || !control->visible())
            continue;

        const bool both=(axis == 0) && (m_axes[1].control == state.control);
        const float speeds[2]={(axis == 0)?velocity[0]:0.0f, (axis == 1 || both)?velocity[1]:0.0f};
        const float scales[2]={m_axes[0].scale, m_axes[1].scale};
        const float pending[2]={(axis == 0)?m_axes[0].pending:0.0f, (axis == 1 || both)?m_axes[1].pending:0.0f};

        anim::add(control, new Fling(speeds, scales, pending));
    }
#endif
}

void TouchGesture::lift(bool cancelled)
{
    Canvas *canvas=m_canvas;
    const bool tap=!m_panning && !cancelled && !m_noTap;
    const PointF down=m_down;

    if(m_panning && !cancelled && !m_longPressed)
        fling();

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
