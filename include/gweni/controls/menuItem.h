/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_menuitem_h_
#define _gweni_controls_menuitem_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/menu.h>

namespace gweni
{
namespace controls
{
class Menu;

class GWENI_EXPORT MenuItem: public Button
{
public:

    GWENI_CONTROL(MenuItem, Button);

    virtual ~MenuItem();

    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;

    void sizeToContents() override;

    virtual void onPress(event::Info) override;

    Menu *getMenu();

    bool isMenuOpen();
    void openMenu();
    void closeMenu();
    void toggleMenu();

    void setOnStrip(bool b)
    {
        m_onStrip=b;
    }

    bool onStrip()
    {
        return m_onStrip;
    }

    virtual void setCheckable(bool check)
    {
        m_checkable=check;
    }

    virtual void setChecked(bool check);
    virtual bool cetChecked()
    {
        return m_checked;
    }

    template <typename T>
    MenuItem *setAction(gweni::event::Handler *handler, T fn)
    {
        if(m_accelerator)
            addAccelerator(m_accelerator->getText(), fn, handler);

        onMenuItemSelected.add(handler, fn);
        return this;
    }

    void setAccelerator(const String &strAccelerator);

    gweni::event::Caller onMenuItemSelected;
    gweni::event::Caller onChecked;
    gweni::event::Caller onUnChecked;
    gweni::event::Caller onCheckChange;

private:

    Menu *m_menu;
    bool m_onStrip;
    bool m_checkable;
    bool m_checked;

    Label *m_accelerator;


    controls::Base *m_submenuArrow;
};


}

}
#endif//_gweni_controls_menuitem_h_
