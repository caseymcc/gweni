/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_spinner_h_
#define _gweni_controls_spinner_h_

#include <gweni/controls/base.h>

#include <cstdint>

namespace gweni
{
namespace anim
{
class Animation;
}

namespace controls
{

//! A busy indicator (Material's CircularProgressIndicator, WinUI's
//! ProgressRing, GTK's Spinner): an arc that turns round a ring while
//! something runs, drawn by the textured skin as one arc slot
//! (renderer::Base::drawArc), with an optional track ring behind it
//! (setShowTrack).
//!
//! The arc moves as Material's does: it turns at setSpeed() turns a second,
//! and its sweep grows from MinSweep to MaxSweep, its head running ahead,
//! then shrinks back, its tail catching up, once every CycleTurns turns, each
//! cycle starting where the last one's tail ended. Its angles are a function
//! of the canvas clock alone (platform::getTimeInSeconds()), arcAt(), so two
//! spinners spinning at one speed are drawn alike, a hidden one shown again
//! is where the clock says, and a stepped clock gives the same frames in any
//! run.
//!
//! Its animation is registered only while isSpinning(), the speed is not 0
//! and it is visible(): start(), stop(), setSpeed() and onVisibleChanged()
//! register or cancel it, so a stopped or hidden spinner (on a page switched
//! away from) runs nothing. A step keeps the angles as the arc's slot keeps
//! them (renderer::arcStartBits, arcSweepBits) and marks
//! StateChange_Background only when they change, so a step that would draw
//! the same arc writes nothing.
//!
//! Stopped, it hides itself (setHideWhenStopped(true), the default) and
//! start() shows it again; with setHideWhenStopped(false) it shows a full ring
//! in the SpinnerStopped surface's colour, dimmed, standing still.
//!
//! Its size is its diameter: setSpinnerSize(Small), the default, is the skin
//! font's line height, so it fits in a Button's row beside its text; Medium and
//! Large suit a card; setDiameter() sets one of its own. The stroke is a tenth
//! of the diameter, at least MinThickness. It takes no keyboard input and no
//! focus, and no mouse input but for the hover a tooltip needs: with one set
//! (setToolTipText, setToolTip), it is hovered and shows it, and the hover and
//! a press over it are its own, not its parent's. So a spinner inside a Button
//! gets no tooltip: give the Button one, or the button has a dead spot.
class GWENI_EXPORT Spinner: public Base
{
public:

    GWENI_CONTROL(Spinner, Base);

public:

    enum class Size
    {
        Small,      //!< the skin font's line height
        Medium,     //!< MediumDiameter
        Large       //!< LargeDiameter
    };

    //! Diameters in units.
    static constexpr int MediumDiameter=32;
    static constexpr int LargeDiameter=48;
    //! The stroke is diameter*ThicknessShare, at least MinThickness units.
    static constexpr float ThicknessShare=0.1f;
    static constexpr float MinThickness=1.5f;
    //! The sweep's range, in turns, and the base turns one grow and shrink
    //! cycle takes.
    static constexpr double MinSweep=0.08;
    static constexpr double MaxSweep=0.72;
    static constexpr double CycleTurns=1.5;
    //! The default speed, turns a second.
    static constexpr float DefaultSpeed=1.0f;

    //! The arc at one time: where it starts, turns from the top clockwise
    //! (0..1), and how far it runs on (MinSweep..MaxSweep).
    struct Arc
    {
        double start=0.0;
        double sweep=0.0;
    };

    //! The arc at \p seconds on the clock, turning at \p speed turns a second.
    static Arc arcAt(double seconds, float speed);

    //! Spinning (the default) or not: start() shows it again if stop() hid it
    //! (setHideWhenStopped); stop() leaves the arc where it is, hidden or
    //! drawn as the stopped ring.
    void start();
    void stop();
    bool isSpinning() const { return m_spinning; }

    //! Turns a second (DefaultSpeed). 0 stands it still, with no animation.
    void setSpeed(float turnsPerSecond);
    float getSpeed() const { return m_speed; }

    //! A size of the three; clears setDiameter()'s.
    void setSpinnerSize(Size size);
    Size getSpinnerSize() const { return m_size; }
    //! A diameter of its own in units, or 0 for setSpinnerSize()'s.
    void setDiameter(int units);
    //! The diameter it asks for, in units: setDiameter()'s, or its size's.
    int getDiameter();
    //! The stroke it is drawn with, in units: a tenth of the shorter side of
    //! its bounds, at least MinThickness.
    float getThickness() const;

    //! Whether stop() hides it (the default) or leaves the stopped ring.
    void setHideWhenStopped(bool hide);
    bool hidesWhenStopped() const { return m_hideWhenStopped; }

    //! A track ring behind the arc (SpinnerTrack), off by default.
    void setShowTrack(bool show);
    bool showsTrack() const { return m_showTrack; }

    //! The arc as last marked for drawing, as its slot keeps it.
    uint16_t getStartBits() const { return m_startBits; }
    uint16_t getSweepBits() const { return m_sweepBits; }
    //! Those in turns, as the renderer is given them.
    float getArcStart() const { return float(m_startBits)/65536.0f; }
    float getArcSweep() const { return m_sweepBits == 0xffff?1.0f:float(m_sweepBits)/65535.0f; }

    //! Whether its animation is registered.
    bool isTurning() const { return m_turn != nullptr; }

    //! The animation's step: the arc at the clock's time, marked when its
    //! bits changed.
    void turnStep();

    //! Hidden or shown: registers or cancels the animation, and shown, puts
    //! the arc where the clock says.
    void onVisibleChanged() override;

    //! The app's hide or show: start() leaves it as the app set it.
    void setHidden(bool hidden) override;

    //! With a tooltip, it takes the mouse (the hover shows it); without, none.
    void setToolTipText(const gweni::String &text) override;
    void setToolTip(Base *tooltip) override;

    void calculateSize(skin::Base *skin, Dim dim) override;

private:
    //! Registers or cancels the animation for isSpinning(), the speed and
    //! visible().
    void updateAnimation();
    //! The arc at the clock's time, the spinner marked if its bits changed.
    void placeArc();
    void updateMouseInput();

    bool m_spinning=true;
    bool m_hideWhenStopped=true;
    //! stop() hid it, so start() shows it again
    bool m_hiddenByStop=false;
    bool m_showTrack=false;
    float m_speed=DefaultSpeed;
    Size m_size=Size::Small;
    int m_diameter=0;
    uint16_t m_startBits=0;
    uint16_t m_sweepBits=0;
    anim::Animation *m_turn=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_spinner_h_
