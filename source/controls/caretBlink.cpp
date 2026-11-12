/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/caretBlink.h>
#include <gweni/controls/canvas.h>
#include <gweni/anim.h>
#include <gweni/platforms/platform.h>

namespace gweni
{
namespace controls
{
namespace internal
{

#if GWENI_ANIMATE
namespace
{

// Its control's blink: a member-less animation, as cheap as TextBox's own was.
class CaretBlinkAnimation: public anim::Animation
{
public:
    void think() override
    {
        m_control->updateCaretBlink();
    }
};

}//namespace
#endif // if GWENI_ANIMATE

void CaretBlink::start(Base *control)
{
#if GWENI_ANIMATE
    if(m_animation)
        return;

    m_animation=new CaretBlinkAnimation();
    anim::add(control, m_animation);
#else
    (void)control;
#endif
}

void CaretBlink::stop(Base *control)
{
#if GWENI_ANIMATE
    if(!m_animation)
        return;

    anim::cancel(control, m_animation);
    m_animation=nullptr;
#else
    (void)control;
#endif
}

void CaretBlink::keepLit(Base *control)
{
    m_next=platform::getTimeInSeconds()+SolidTime;

    if(m_visible)
        return;

    m_visible=true;

    if(control->isFocussed())
        control->addStateChange(StateChange_Caret);
}

// Unfocused, the caret is not drawn and nothing here changes. Focused, each
// blink edge is one StateChange_Caret, which only shows or hides the caret.
void CaretBlink::step(Base *control)
{
    // the focus went without onLostKeyboardFocus (~Base clearing it, a
    // direct KeyboardFocus assignment): stop here
    if(!control->isFocussed())
    {
        stop(control);
        return;
    }

    const double now=platform::getTimeInSeconds();

    if(m_next > now)
        return;

    m_next=now+Period;
    m_visible=!m_visible;
    control->addStateChange(StateChange_Caret);

    if(Canvas *canvas=control->getCanvas())
        canvas->redraw();
}

}//namespace internal
}//namespace controls
}//namespace gweni
