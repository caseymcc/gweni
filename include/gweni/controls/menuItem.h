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

public:
    virtual ~MenuItem();

    void render(skin::Base *skin) override;
    void layout(skin::Base *skin) override;
    //! Button's text colour, and a repaint of the submenu chevron, drawn in it,
    //! when that colour changes.
    void updateColors() override;

    void sizeToContents() override;

    //! Label's, and on a real change, an item of a ComboBox's list tells the
    //! list, so the box shows the new text if the item is its selection.
    void setText(const String &str, bool doEvents=true) override;

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

    //! Whether a click toggles the item's check. A change redraws the item.
    virtual void setCheckable(bool check);
    bool isCheckable() const
    {
        return m_checkable;
    }

    //! A change redraws the item and fires onCheckChange, then onChecked or
    //! onUnChecked.
    virtual void setChecked(bool check);
    bool isChecked() const
    {
        return m_checked;
    }

    //! Button's image; it is hidden while the item is checkable and checked,
    //! where the check takes its place in the icon column.
    void setImage(const String &name, bool center=false) override;

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
    //! Its submenu was shown or hidden: the open highlight (isMenuOpen()) changed.
    void onMenuVisibilityChanged(event::Info);
    //! Its submenu is being deleted (Menu::~Menu) before it: it is a plain item
    //! again, with no submenu arrow, so a press checks and selects it.
    void submenuDeleted();
    //! The image, if any, is hidden while checkable and checked, whatever order
    //! setImage, setCheckable and setChecked came in.
    void updateImageVisibility();

    Menu *m_menu=nullptr;
    bool m_onStrip=false;
    bool m_checkable=false;
    bool m_checked=false;

    Label *m_accelerator=nullptr;


    controls::Base *m_submenuArrow=nullptr;

    friend class Menu;
};


}

}
#endif//_gweni_controls_menuitem_h_
