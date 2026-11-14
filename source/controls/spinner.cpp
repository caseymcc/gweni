/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/spinner.h>
#include <gweni/anim.h>
#include <gweni/gweni.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/platform.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{

namespace
{

//! A spinner's turn: one step a frame while it spins and shows.
class SpinnerTurn: public anim::Animation
{
public:
    void think() override
    {
        gweni_cast<Spinner>(m_control)->turnStep();
    }
};

//! Eased in and out, 0..1 to 0..1 (a half cosine).
double ease(double x)
{
    return 0.5-0.5*std::cos(3.14159265358979323846*x);
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(Spinner)
{
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    setTabable(false);
    placeArc();
    updateAnimation();
}

// The base turns at speed; within a cycle of CycleTurns base turns the head
// runs MaxSweep-MinSweep ahead (the first half) and the tail catches it up
// (the second), so the arc grows and shrinks, and each cycle starts that far
// on from the last, where its tail ended.
Spinner::Arc Spinner::arcAt(double seconds, float speed)
{
    const double range=MaxSweep-MinSweep;
    const double turns=seconds*double(speed);
    const double cycles=turns/CycleTurns;
    const double cycle=std::floor(cycles);
    const double u=cycles-cycle;
    double head;
    double tail;

    if(u < 0.5)
    {
        head=range*ease(2.0*u);
        tail=0.0;
    }
    else
    {
        head=range;
        tail=range*ease(2.0*u-1.0);
    }

    // the whole turns of each part apart, so a long run keeps its precision
    double start=std::fmod(turns, 1.0)+std::fmod(range*cycle, 1.0)+tail;

    start-=std::floor(start);

    Arc arc;

    arc.start=start;
    arc.sweep=MinSweep+head-tail;
    return arc;
}

void Spinner::start()
{
    if(m_spinning)
        return;

    m_spinning=true;
    if(m_hiddenByStop)
    {
        m_hiddenByStop=false;
        ParentClass::setHidden(false);
    }
    placeArc();
    // the stopped ring is drawn as the arc
    addStateChange(StateChange_Background);
    updateAnimation();
}

void Spinner::stop()
{
    if(!m_spinning)
        return;

    m_spinning=false;
    updateAnimation();
    addStateChange(StateChange_Background);
    if(m_hideWhenStopped && !hidden())
    {
        ParentClass::setHidden(true);
        m_hiddenByStop=true;
    }
}

void Spinner::setHidden(bool hidden)
{
    // the app's own hide or show: start() leaves it as the app set it
    m_hiddenByStop=false;
    ParentClass::setHidden(hidden);
}

void Spinner::setSpeed(float turnsPerSecond)
{
    if(turnsPerSecond == m_speed)
        return;

    m_speed=turnsPerSecond;
    placeArc();
    updateAnimation();
}

void Spinner::setSpinnerSize(Size size)
{
    if((size == m_size) && (m_diameter == 0))
        return;

    m_size=size;
    m_diameter=0;
    invalidate();
    invalidateParent();
}

void Spinner::setDiameter(int units)
{
    units=std::max(units, 0);
    if(units == m_diameter)
        return;

    m_diameter=units;
    invalidate();
    invalidateParent();
}

int Spinner::getDiameter()
{
    if(m_diameter > 0)
        return m_diameter;

    switch(m_size)
    {
    case Size::Medium:
        return MediumDiameter;
    case Size::Large:
        return LargeDiameter;
    case Size::Small:
    default:
        break;
    }

    skin::Base *skin=getSkin();

    return skin?std::max(measureLineHeight(skin->getDefaultFont()), 1):MediumDiameter/2;
}

float Spinner::getThickness() const
{
    const Rect &bounds=getBounds();

    return std::max(MinThickness, ThicknessShare*float(std::min(bounds.w, bounds.h)));
}

void Spinner::setHideWhenStopped(bool hide)
{
    if(hide == m_hideWhenStopped)
        return;

    m_hideWhenStopped=hide;
    if(m_spinning)
        return;

    if(hide && !hidden())
    {
        ParentClass::setHidden(true);
        m_hiddenByStop=true;
    }
    else if(!hide && m_hiddenByStop)
    {
        m_hiddenByStop=false;
        ParentClass::setHidden(false);
    }
}

void Spinner::setShowTrack(bool show)
{
    if(show == m_showTrack)
        return;

    m_showTrack=show;
    addStateChange(StateChange_Background);
}

void Spinner::updateAnimation()
{
#if GWENI_ANIMATE
    const bool turning=m_spinning && (m_speed != 0.0f) && visible();

    if(turning && !m_turn)
    {
        m_turn=new SpinnerTurn();
        anim::add(this, m_turn);
    }
    else if(!turning && m_turn)
    {
        anim::cancel(this, m_turn);
        m_turn=nullptr;
    }
#endif
}

void Spinner::placeArc()
{
    const Arc arc=arcAt(platform::getTimeInSeconds(), m_speed);
    const uint16_t start=renderer::arcStartBits(float(arc.start));
    const uint16_t sweep=renderer::arcSweepBits(float(arc.sweep));

    if((start == m_startBits) && (sweep == m_sweepBits))
        return;

    m_startBits=start;
    m_sweepBits=sweep;
    // stopped, it draws the still ring whatever the angles
    if(m_spinning)
        addStateChange(StateChange_Background);
}

void Spinner::turnStep()
{
    placeArc();
}

void Spinner::onVisibleChanged()
{
    ParentClass::onVisibleChanged();
    if(visible() && m_spinning)
        placeArc();
    updateAnimation();
}

void Spinner::updateMouseInput()
{
    setMouseInputEnabled(hasToolTip());
}

void Spinner::setToolTipText(const gweni::String &text)
{
    ParentClass::setToolTipText(text);
    updateMouseInput();
}

void Spinner::setToolTip(Base *tooltip)
{
    ParentClass::setToolTip(tooltip);
    updateMouseInput();
}

void Spinner::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const int diameter=getDiameter();

    if(dim == Dim::X)
        m_preferredSize.width=diameter;
    else
        m_preferredSize.height=diameter;
}

}//namespace controls
}//namespace gweni
