/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */
#include "demo/sequencer.h"
#include "gweni/anim.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/sequencer.h"
#include "gweni/platforms/platform.h"

#include <cmath>
#include <cstdio>

namespace gweni
{

//! The playback: one step a frame while it plays and the demo shows.
class SequencerPlay: public anim::Animation
{
public:
    void think() override
    {
        gweni_cast<SequencerDemo>(m_control)->playStep();
    }
};

namespace
{

const char *const TrackNames[SequencerDemo::Tracks]={
    "Camera (locked)", "Hero / position", "Hero / rotation", "Hero / scale", "Sidekick / position",
    "Light key", "Light fill", "Fog density", "Music", "Dialogue", "Effects", "Subtitles"};

}//namespace

// Each track's clips and keys from a formula of its index, whole frames at
// FrameRate, so every platform shows the same sequence: clips in the theme's
// SequencerClip or a chart series' colours, a key every second or so.
GWENI_CONTROL_CONSTRUCTOR(SequencerDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("Drag clips and keys (Alt: no snap), their edges to resize; scrub the ruler; Ctrl+wheel zooms");
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(10, 10, 10, 4));

    controls::Base *bar=newChild<controls::Base>();
    bar->setDock(DockPosition::Top);
    bar->setHeight(24);
    bar->setMargin(Margin(10, 0, 10, 6));

    m_play=bar->newChild<controls::Button>();
    m_play->setText("Play");
    m_play->setIsToggle(true);
    m_play->setDock(DockPosition::Left);
    m_play->setWidth(70);
    m_play->onToggleCaller.add(this, &SequencerDemo::playButtonToggled);

    m_readout=bar->newChild<controls::Label>();
    m_readout->setDock(DockPosition::Left);
    m_readout->setMargin(Margin(10, 4, 0, 0));
    m_readout->setWidth(200);

    m_sequencer=newChild<controls::Sequencer>();
    m_sequencer->setDock(DockPosition::Center);
    m_sequencer->setMargin(Margin(10, 0, 10, 10));
    m_sequencer->setFrameRate(FrameRate);
    m_sequencer->setTimeRange(0.0, Length);
    m_sequencer->onPlayheadChanged.add(this, &SequencerDemo::playheadChanged);
    m_sequencer->onPlayToggled.add(this, &SequencerDemo::playToggled);
    m_sequencer->onRemoveRequested.add(this, &SequencerDemo::removeRequested);

    static const char *const surfaces[]={"", "ChartSeries1", "ChartSeries2", "ChartSeries3", "ChartSeries4"};
    const double frame=1.0/FrameRate;
    uint64_t id=1;

    for(int t=0; t < Tracks; ++t)
    {
        m_sequencer->addTrack(TrackNames[t], uint64_t(t));

        // clips: a frame count from the track and the clip's index
        int at=(t*7)%30;
        const int clips=1+(t*5)%3;

        for(int c=0; c < clips; ++c)
        {
            const int length=36+((t*13+c*29)%60);

            if(at+length > int(Length*FrameRate))
                break;
            m_sequencer->addClip(size_t(t), at*frame, (at+length)*frame, surfaces[(t+c)%5], id++);
            at+=length+12+((t*3+c*7)%24);
        }

        // keys: about one a second, from the track's own phase
        for(int k=(t*11)%20; k < int(Length*FrameRate); k+=18+((t*5+k)%14))
            m_sequencer->addKey(size_t(t), k*frame, id++);
    }
    showReadout();
}

SequencerDemo::~SequencerDemo()
{
    // the registry cancels a deleted control's animations
}

void SequencerDemo::showReadout()
{
    char text[64];
    const double t=m_sequencer->getPlayhead();

    std::snprintf(text, sizeof(text), "Frame %lld  (%.2f s)", m_sequencer->frameAt(t), t);
    m_readout->setText(text);
}

void SequencerDemo::playheadChanged(event::Info)
{
    if(m_readoutFollows)
        showReadout();
}

void SequencerDemo::playToggled(event::Info)
{
    setPlaying(!m_playing);
}

void SequencerDemo::playButtonToggled(event::Info)
{
    setPlaying(m_play->getToggleState());
}

// The first track is locked: its items stay.
void SequencerDemo::removeRequested(event::Info)
{
    if(m_sequencer->getRemoveRequest().track == 0)
        m_sequencer->vetoRemove();
}

void SequencerDemo::setPlaying(bool playing)
{
    if(playing == m_playing)
        return;

    m_playing=playing;
    if(m_play->getToggleState() != playing)
        m_play->setToggleState(playing);
    m_play->setText(playing ? "Pause" : "Play");
    m_lastTime=platform::getTimeInSeconds();
    updateAnimation();
}

void SequencerDemo::onVisibleChanged()
{
    updateAnimation();
}

void SequencerDemo::updateAnimation()
{
#if GWENI_ANIMATE
    const bool running=m_playing && visible();

    if(running && !m_step)
    {
        m_lastTime=platform::getTimeInSeconds();
        m_step=new SequencerPlay();
        anim::add(this, m_step);
    }
    else if(!running && m_step)
    {
        anim::cancel(this, m_step);
        m_step=nullptr;
    }
#endif
}

// The playhead goes on by the clock's time since the last step, from the
// range's start again past its end.
void SequencerDemo::playStep()
{
    const double now=platform::getTimeInSeconds();
    double t=m_sequencer->getPlayhead()+(now-m_lastTime);

    m_lastTime=now;
    if(t >= m_sequencer->getRangeEnd())
        t=m_sequencer->getRangeStart()+std::fmod(t-m_sequencer->getRangeStart(), Length);
    m_sequencer->setPlayhead(t);
}

}//namespace gweni
