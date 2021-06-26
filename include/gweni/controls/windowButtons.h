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
class GWENI_EXPORT WindowCloseButton: public Button
{
    GWENI_CONTROL_INLINE(WindowCloseButton, Button)
    {
        m_window=nullptr;
        setSize(31, 31);
        setText("");
    }

    void render(skin::Base *skin) override
    {
        if(!m_window)
            return;

//        skin->drawWindowCloseButton(this, skin::Generate,
//            isDepressed() && isHovered(),
//            isHovered() && shouldDrawHover(),
//            isDisabled());
        skin->drawControl(this);
    }

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

    void render(skin::Base *skin) override
    {
        if(!m_window)
            return;

//        skin->drawWindowMaximizeButton(this, skin::Generate,
//            isDepressed() && isHovered(),
//            isHovered() && shouldDrawHover(),
//            isDisabled(),
//            m_maximized);
        skin->drawControl(this);
    }

    virtual void setMaximized(bool b)
    {
        m_maximized=b;
    }

protected:

    bool m_maximized;
};


class GWENI_EXPORT WindowMinimizeButton: public WindowCloseButton
{
    GWENI_CONTROL_INLINE(WindowMinimizeButton, WindowCloseButton)
    {}

    void render(skin::Base *skin) override
    {
        if(!m_window)
            return;

//        skin->drawWindowMinimizeButton(this, skin::Generate,
//            isDepressed() && isHovered(),
//            isHovered() && shouldDrawHover(),
//            isDisabled());
        skin->drawControl(this);
    }

};


}
}

#endif//_gweni_controls_windows_buttons_h_
