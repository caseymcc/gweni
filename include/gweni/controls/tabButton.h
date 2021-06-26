/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_tabbutton_h_
#define _gweni_controls_tabbutton_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>

namespace gweni
{
namespace controls
{
class TabControl;

class GWENI_EXPORT TabButton: public Button
{
public:

    GWENI_CONTROL(TabButton, Button);
    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;

    void setPage(Base *page)
    {
        m_page=page;
    }

    Base *getPage()
    {
        return m_page;
    }

    void        setTabControl(TabControl *ctrl);
    TabControl *getTabControl()
    {
        return m_control;
    }

    bool isActive()
    {
        return m_page && m_page->visible();
    }

    bool dragAndDrop_shouldStartDrag() override;
    void dragAndDrop_startDragging(gweni::draganddrop::Package * /*package*/,
        int /*x*/, int /*y*/) override
    {
        setHidden(true);
    }

    void dragAndDrop_endDragging(bool /*bSuccess*/, int /*x*/, int /*y*/) override
    {
        setHidden(false); setDepressed(false);
    }

    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;

    void updateColors() override;

    bool shouldClip() override
    {
        return false;
    }

private:

    Base *m_page;
    TabControl *m_control;

};

}
}

#endif//_gweni_controls_tabbutton_h_
