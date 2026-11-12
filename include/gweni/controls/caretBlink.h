/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_caretblink_h_
#define _gweni_controls_caretblink_h_

#include <gweni/export.h>

namespace gweni
{
namespace anim
{
class Animation;
}

namespace controls
{
class Base;

namespace internal
{

//! A text caret's blink, for a control that draws a caret while it has the
//! keyboard focus (TextBox, CodeEditor): one animation, registered from the
//! focus arriving (start()) until it leaves (stop(), or a blink frame finding
//! the control unfocused), so an unfocused control costs its canvas's update
//! phase nothing. Each blink edge marks StateChange_Caret on the control
//! alone, which only shows or hides the caret. After an edit, a move or the
//! focus arriving the caret is lit and held lit for SolidTime (keepLit()).
//! The times are the platform clock's (platform::getTimeInSeconds()).
class GWENI_EXPORT CaretBlink
{
public:
    //! The blink's half period, and how long the caret stays lit after keepLit().
    static constexpr float Period=0.5f;
    static constexpr float SolidTime=1.5f;

    CaretBlink()=default;
    CaretBlink(const CaretBlink &)=delete;
    CaretBlink &operator=(const CaretBlink &)=delete;

    //! Whether the caret is lit now.
    bool visible() const { return m_visible; }
    //! Whether the animation is registered.
    bool running() const { return m_animation != nullptr; }

    //! Registers the blink for \p control, once.
    void start(Base *control);
    //! Cancels it, if registered.
    void stop(Base *control);
    //! Lights the caret and holds it lit for SolidTime; a caret that was dark
    //! marks StateChange_Caret while \p control has the focus.
    void keepLit(Base *control);
    //! One frame of the blink: stops once \p control has lost the focus,
    //! flips the caret at each edge. The animation calls the control's
    //! Base::updateCaretBlink(), which calls this on its own blink.
    void step(Base *control);

private:
    float m_next=0.0f;
    bool m_visible=true;
    anim::Animation *m_animation=nullptr;
};

}//namespace internal
}//namespace controls
}//namespace gweni

#endif//_gweni_controls_caretblink_h_
