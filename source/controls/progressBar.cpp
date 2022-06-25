/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/scrollControl.h>
#include <gweni/controls/progressBar.h>
#include <gweni/anim.h>
#include <gweni/utility.h>

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
    setAlignment(Alignment::Center);
    m_progress=0.0f;
    m_autoLabel=true;
    m_cycleSpeed=0.0f;
    anim::add(this, new ProgressBarThink());
}

void ProgressBar::setProgress(float val)
{
    m_progress=clamp(val, 0.f, 1.f);

    if(m_autoLabel)
    {
        const int displayVal=m_progress*100;
        setText(toString(displayVal)+"%");
    }
}

void ProgressBar::cycleThink(float delta)
{
    if(!visible())
        return;

    if(m_cycleSpeed == 0.0f)
        return;

    m_progress+=m_cycleSpeed*delta;

    if(m_progress < 0.0f)
        m_progress+=1.0f;

    if(m_progress > 1.0f)
        m_progress-=1.0f;

    redraw();
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
