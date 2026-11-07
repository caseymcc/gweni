/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_windows_buttons_h_
#define _gweni_controls_windows_buttons_h_

#include <gweni/gweni.h>
#include <gweni/controls/button.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{
// The title-bar buttons. They draw through their skin renderers, which the
// usual Base::render path generates, updates and removes like any control's;
// m_window is only the window they act on, drawing never needs it.
class GWENI_EXPORT WindowCloseButton: public Button
{
    GWENI_CONTROL_INLINE(WindowCloseButton, Button)
    {
        m_window=nullptr;
        setSize(31, 31);
        setText("");
    }

public:
    void setWindow(gweni::controls::Base *p)
    {
        m_window=p;
    }

protected:

    controls::Base *m_window;
};


class GWENI_EXPORT WindowMaximizeButton: public WindowCloseButton
{
    GWENI_CONTROL_INLINE(WindowMaximizeButton, WindowCloseButton)
    {
        m_maximized=false;
    }

public:
    //! The renderer draws the restore glyph while maximized, so a change repaints
    //! the button; the same value again marks nothing.
    virtual void setMaximized(bool b)
    {
        if(m_maximized == b)
            return;

        m_maximized=b;
        addStateChange(StateChange_Background);
    }

    bool isMaximized() const
    {
        return m_maximized;
    }

protected:

    bool m_maximized;
};


class GWENI_EXPORT WindowMinimizeButton: public WindowCloseButton
{
    GWENI_CONTROL_INLINE(WindowMinimizeButton, WindowCloseButton)
    {}
};


}
}

#endif//_gweni_controls_windows_buttons_h_
