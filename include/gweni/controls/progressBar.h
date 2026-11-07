/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_progressbar_h_
#define _gweni_controls_progressbar_h_
#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace anim
{
class Animation;
}

namespace controls
{
class GWENI_EXPORT ProgressBar: public Label
{
public:

    GWENI_CONTROL(ProgressBar, Label);

public:
    //void render(skin::Base *skin) override;

    virtual void setVertical()
    {
        m_horizontal=false;
    }

    virtual void setHorizontal()
    {
        m_horizontal=true;
    }

    bool isHorizontal() const
    {
        return m_horizontal;
    }

    virtual void  setProgress(float val);
    virtual float getProgress() const
    {
        return m_progress;
    }

    //! While on, the label is the progress in whole percent, set only when that
    //! number changes.
    virtual void setAutoLabel(bool b)
    {
        m_autoLabel=b;
        m_labelPercent=-1;
    }

    virtual float getCycleSpeed();
    virtual void  setCycleSpeed(float f);

    //! An indeterminate bar shows work of unknown length: a segment of
    //! getSegmentLength() of the track moves along it, start to end (bottom to
    //! top when vertical), slides out past the end and in again at the start.
    //! It starts wholly on the track at its start. The auto label is empty
    //! meanwhile. setProgress() still stores its value, and
    //! setIndeterminate(false) shows it again, with its label.
    virtual void setIndeterminate(bool indeterminate);
    bool isIndeterminate() const
    {
        return m_indeterminate;
    }

    //! How fast the segment moves, in track lengths per second.
    void setIndeterminateSpeed(float speed);
    float getIndeterminateSpeed() const
    {
        return m_indeterminateSpeed;
    }

    //! The segment's length as a fraction of the track, 0.05..1.
    virtual void setSegmentLength(float fraction);
    float getSegmentLength() const
    {
        return m_segmentLength;
    }

    //! Where the segment is in its cycle, 0..1: at 0 it is just before the
    //! track's start, and it leaves past the end as the phase comes to 1.
    float getIndeterminatePhase() const
    {
        return m_phase;
    }

    //! The segment's rect inside \p track (the bar's render bounds), in whole
    //! units and cut to the track: empty while it is wholly outside. The skin
    //! draws this rect, and cycleThink() marks StateChange_Background only on a
    //! frame it changes.
    Rect indeterminateSegment(const Rect &track) const;

    virtual void cycleThink(float delta);

protected:

    void updateAutoLabel();
    void advanceIndeterminate(float delta);
    //! Registers the bar's animation when it starts moving (cycling, or
    //! indeterminate at a non-zero speed) and cancels it when it stops, so a
    //! bar standing still costs its canvas's update phase nothing.
    void updateAnimation();

    float m_progress;
    float m_cycleSpeed;

    bool m_horizontal;
    bool m_autoLabel;
    //! the percentage the auto label shows, -1 when it may show anything
    int m_labelPercent;

    bool m_indeterminate;
    float m_indeterminateSpeed;
    float m_segmentLength;
    float m_phase;
    //! the segment as last marked for drawing
    Rect m_segmentMarked;
    //! the registered animation while the bar moves, else null
    anim::Animation *m_animation=nullptr;
};


}
}
#endif//_gweni_controls_progressbar_h_
