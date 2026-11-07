/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollControl.h>
#include <gweni/controls/progressBar.h>
#include <gweni/anim.h>
#include <gweni/utility.h>

#include <cmath>

namespace gweni
{
namespace controls
{

class ProgressBarThink: public gweni::anim::Animation
{
public:

    ProgressBarThink()
        : m_fLastFrame(0.f)
    {}

    void think() override
    {
        float fDiff=platform::getTimeInSeconds()-m_fLastFrame;
        gweni_cast<ProgressBar>(m_control)->cycleThink(gweni::clamp(fDiff, 0.f, 0.3f));
        m_fLastFrame=platform::getTimeInSeconds();
    }

    float m_fLastFrame;
};


GWENI_CONTROL_CONSTRUCTOR(ProgressBar)
{
    setMouseInputEnabled(true);
    setBounds(gweni::Rect(0, 0, 128, 32));
    setTextPadding(Padding(3, 3, 3, 3));
    setHorizontal();
    setTextAlignment(Alignment::Center);
    m_progress=0.0f;
    m_autoLabel=true;
    m_labelPercent=-1;
    m_cycleSpeed=0.0f;
    m_indeterminate=false;
    m_indeterminateSpeed=0.75f;
    m_segmentLength=0.3f;
    m_phase=0.0f;
    m_segmentMarked=Rect(0, 0, -1, -1);
    anim::add(this, new ProgressBarThink());
}

void ProgressBar::setProgress(float val)
{
    val=clamp(val, 0.f, 1.f);

    // only the fill changes: the bounds stay as they are. An indeterminate bar
    // keeps the value for later and draws its segment meanwhile.
    if(val != m_progress)
    {
        m_progress=val;

        if(!m_indeterminate)
            addStateChange(StateChange_Background);
    }

    if(m_autoLabel && !m_indeterminate)
        updateAutoLabel();
}

void ProgressBar::updateAutoLabel()
{
    const int displayVal=m_progress*100;

    // a cycling bar calls this every step: the text is set on a new number only
    if(displayVal == m_labelPercent)
        return;

    m_labelPercent=displayVal;
    setText(toString(displayVal)+"%");
}

void ProgressBar::setIndeterminate(bool indeterminate)
{
    if(indeterminate == m_indeterminate)
        return;

    m_indeterminate=indeterminate;
    addStateChange(StateChange_Background);

    if(m_indeterminate)
    {
        // it starts wholly on the track at its start, so it shows at once
        m_phase=m_segmentLength/(1.0f+m_segmentLength);
        m_segmentMarked=indeterminateSegment(getRenderBounds());

        if(m_autoLabel)
        {
            m_labelPercent=-1;
            setText("");
        }
    }
    else if(m_autoLabel)
        updateAutoLabel();
}

void ProgressBar::setSegmentLength(float fraction)
{
    fraction=clamp(fraction, 0.05f, 1.0f);

    if(fraction == m_segmentLength)
        return;

    m_segmentLength=fraction;

    if(m_indeterminate)
        addStateChange(StateChange_Background);
}

Rect ProgressBar::indeterminateSegment(const Rect &track) const
{
    // the segment's start runs from -length (just before the track) to 1 (just
    // past it), so it slides in at the start and out at the end
    const float start=m_phase*(1.0f+m_segmentLength)-m_segmentLength;
    const int extent=m_horizontal ? track.w : track.h;
    const int a=clamp((int)std::floor(start*extent+0.5f), 0, extent);
    const int b=clamp((int)std::floor((start+m_segmentLength)*extent+0.5f), 0, extent);

    if(m_horizontal)
        return Rect(track.x+a, track.y, b-a, track.h);

    // a vertical bar runs from the bottom up
    return Rect(track.x, track.y+track.h-b, track.w, b-a);
}

void ProgressBar::advanceIndeterminate(float delta)
{
    if(m_indeterminateSpeed == 0.0f)
        return;

    // one cycle is the track and the segment's own length
    m_phase+=m_indeterminateSpeed*delta/(1.0f+m_segmentLength);
    m_phase-=std::floor(m_phase);

    // a step of less than a unit leaves the segment's rect, and its slot, as
    // they are
    const Rect segment=indeterminateSegment(getRenderBounds());

    if(segment == m_segmentMarked)
        return;

    m_segmentMarked=segment;
    addStateChange(StateChange_Background);
}

void ProgressBar::cycleThink(float delta)
{
    if(!visible())
        return;

    if(m_indeterminate)
    {
        advanceIndeterminate(delta);
        return;
    }

    if(m_cycleSpeed == 0.0f)
        return;

    float progress=m_progress+m_cycleSpeed*delta;

    if(progress < 0.0f)
        progress+=1.0f;

    if(progress > 1.0f)
        progress-=1.0f;

    setProgress(progress);
}

float ProgressBar::getCycleSpeed()
{
    return m_cycleSpeed;
}

void ProgressBar::setCycleSpeed(float f)
{
    m_cycleSpeed=f;
}

}//namespace controls
}//namespace gweni
