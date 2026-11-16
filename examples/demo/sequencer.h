/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */
#ifndef _gweni_sequencer_example_h_
#define _gweni_sequencer_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
class Label;
class Sequencer;
}

class SequencerPlay;

//! An animation's tracks in a Sequencer: Tracks tracks of clips and keys, a
//! Play toggle (and Space) advancing the playhead on the canvas clock while it
//! plays and the demo shows, a readout of the playhead's frame, and a first
//! track whose items refuse to be deleted (its caption says it is locked).
class SequencerDemo: public controls::Base
{
public:
    GWENI_CONTROL(SequencerDemo, controls::Base);

public:
    ~SequencerDemo() override;

    static constexpr int Tracks=12;
    static constexpr double FrameRate=24.0;
    static constexpr double Length=10.0;

    controls::Sequencer *sequencer() const { return m_sequencer; }
    controls::Button *playButton() const { return m_play; }
    controls::Label *readout() const { return m_readout; }

    //! Starts or stops the playback (the Play toggle follows).
    void setPlaying(bool playing);
    bool isPlaying() const { return m_playing; }
    //! Whether the playback's animation is registered: only while playing
    //! and shown.
    bool isAnimating() const { return m_step != nullptr; }
    //! Whether the readout follows the playhead (a test turns it off to see
    //! a scrub's own writes).
    void setReadoutFollows(bool follows) { m_readoutFollows=follows; }

    void onVisibleChanged() override;

private:
    friend class SequencerPlay;

    void playheadChanged(event::Info info);
    void playToggled(event::Info info);
    void playButtonToggled(event::Info info);
    void removeRequested(event::Info info);
    void showReadout();
    void updateAnimation();
    //! One frame of the playback.
    void playStep();

    controls::Sequencer *m_sequencer=nullptr;
    controls::Button *m_play=nullptr;
    controls::Label *m_readout=nullptr;
    bool m_playing=false;
    bool m_readoutFollows=true;
    double m_lastTime=0.0;
    SequencerPlay *m_step=nullptr;
};

}//namespace gweni

#endif//_gweni_sequencer_example_h_
