/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_switch_h_
#define _gweni_controls_switch_h_

#include <gweni/controls/button.h>

namespace gweni
{
namespace anim
{
class Animation;
}

namespace controls
{

//! An on/off toggle (Material's Switch, WinUI's ToggleSwitch, GTK's Switch): a
//! pill track with a round thumb, the thumb at the left while off and at the
//! right while on, and an optional label beside it (setText(), on the right
//! unless setLabelOnLeft(true)).
//!
//! It is Button's toggle (setIsToggle(true)): its on state is
//! getToggleState(), and a click, Space or Return press it, which toggles it.
//! Button's look does not come into it: the Switch's own renderer draws the
//! track and the thumb, and its label keeps its colour as the state, hover
//! and press change, so none of them writes a glyph slot.
//!
//! The label is the switch's own Text, not a control beside it (a
//! CheckBoxWithLabel's): the track sits in the switch's padding on the
//! label's other side, so a hover or a click anywhere on the control, label
//! included, is the switch's, and a hover writes the switch's track slot
//! alone.
//!
//! It takes the keyboard focus (and Tab), which draws the track's border in
//! the SwitchFocus surface's; Space and Return toggle it on their release (a
//! key held repeats its press). While disabled it takes no mouse or keys and
//! draws its disabled look, on or off.
//!
//! Its size is the track's (TrackWidth x TrackHeight) and the label's beside
//! it, Gap apart, so a toggle never lays anything out.
//!
//! A toggle slides the thumb to its new end over SlideSeconds, eased, on the
//! canvas clock (platform::getTimeInSeconds()), by an animation registered
//! only while it moves (anim::add on the toggle, cancelled on arrival); a
//! toggle mid-slide turns it round from where the thumb is. A step marks the
//! switch only when the thumb's rect in whole units changes, so the track's
//! slot keeps its bytes and the thumb's alone is written. The thumb jumps to
//! its end (no animation) when the switch is not visible(), has not been drawn
//! yet (its first frame shows the thumb where it ends) or GWENI_ANIMATE is
//! off, and a step that finds the switch not visible() puts it there and
//! cancels itself.
class GWENI_EXPORT Switch: public Button
{
public:

    GWENI_CONTROL(Switch, Button);

public:

    //! The track, in units.
    static constexpr int TrackWidth=34;
    static constexpr int TrackHeight=18;
    //! The thumb's distance from the track's edges, in units: its diameter is
    //! TrackHeight-2*ThumbInset.
    static constexpr int ThumbInset=3;
    //! Units between the track and the label.
    static constexpr int Gap=6;
    //! The thumb's slide from one end to the other, in seconds.
    static constexpr double SlideSeconds=0.15;

    //! On or off. A change fires onToggled, then onToggledOn or onToggledOff
    //! (and Button's toggle callers); the current value fires and marks
    //! nothing.
    void setOn(bool on) { setToggleState(on); }
    bool isOn() const { return getToggleState(); }
    void toggle() override { setToggleState(!getToggleState()); }

    void setToggleState(bool on) override;

    //! The label on the track's left (default: its right).
    void setLabelOnLeft(bool left);
    bool isLabelOnLeft() const { return m_labelOnLeft; }

    void setText(const String &str, bool doEvents=true) override;

    //! The track's rect in the switch's own units.
    Rect getTrackRect() const;
    //! The thumb's rect in the switch's own units, at its position.
    Rect getThumbRect() const;
    //! The thumb's place along its travel: 0 at the left, 1 at the right.
    float getThumbPosition() const { return m_thumbPosition; }
    //! Whether the thumb is on its way (its animation registered).
    bool isSliding() const { return m_slide != nullptr; }

    //! The slide's step, from its animation: the thumb's place at the clock's
    //! time, the arrival cancelling it.
    void slideStep();

    //! Disabled, it takes no keyboard focus (a click would give it the focus,
    //! as any control taking keys) and gives up the one it has.
    void setDisabled(bool disabled) override;

    //! Notes that it has been drawn: a toggle before that has nothing to
    //! slide from.
    void render(skin::Base *skin) override;

    void onPress(event::Info info) override;
    bool onKeySpace(bool down) override;
    bool onKeyReturn(bool down) override;
    void acceleratePressed() override;

    //! The label's colour: Colors.Label.Default (followed, so a theme edit
    //! reaches it), Colors.Button.Disabled while disabled.
    void updateColors() override;

    void calculateSize(skin::Base *skin, Dim dim) override;

    //! Fired once per real change of the state, from a click, a key or setOn.
    gweni::event::Caller onToggled;
    gweni::event::Caller onToggledOn;
    gweni::event::Caller onToggledOff;

private:
    //! The padding that keeps the track beside the label.
    void updatePadding();
    //! The thumb toward the state's end: a slide, or a jump.
    void moveThumb();
    //! The thumb at \p position, the switch marked if its rect changed.
    void placeThumb(float position);
    //! Ends the slide, cancelling its animation.
    void stopSlide();

    bool m_labelOnLeft=false;
    float m_thumbPosition=0.0f;
    //! The slide: from where, since when and for how long.
    float m_slideFrom=0.0f;
    double m_slideStart=0.0;
    double m_slideLength=0.0;
    //! The thumb's rect when the switch was last marked for it.
    Rect m_thumbMarked;
    anim::Animation *m_slide=nullptr;
    bool m_drawn=false;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_switch_h_
