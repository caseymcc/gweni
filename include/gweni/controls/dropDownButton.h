/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dropDownButton_h_
#define _gweni_controls_dropDownButton_h_

#include <gweni/controls/button.h>
#include <gweni/controls/menu.h>

namespace gweni
{
namespace controls
{

//! A button that opens a menu (WinUI's DropDownButton, Qt's QToolButton with
//! InstantPopup): a Button with a down chevron at its right, whose press opens
//! getMenu() below it.
//!
//! getMenu() is a Menu on the canvas, made on first use and filled by the app
//! (addItem, addDivider, a MenuItem's own getMenu() for a submenu). It opens
//! through Menu::openNextTo(this, PopupPlacement::Below, 1), so the canvas
//! places it below the button, flipped above it and kept inside the canvas,
//! and follows the button while it is open. A press while it is open closes
//! it (a toggle, as a Popup's anchor is); a press opening it closes every other
//! open menu first. The button is a menu component (isMenuComponent()), so a
//! press on it does not close every menu before its own press decides.
//!
//! A click opens it navigating by keyboard with nothing highlighted
//! (MenuFocus::Menu: the menu has the focus, Down highlights its first item);
//! Space, Return, Alt+Down and F4 on the focused button open it with its first
//! enabled item highlighted (MenuFocus::FirstItem). Either way the menu gives
//! the keyboard focus back to the button when it closes. Should the button
//! hold the focus with the menu open (a press on it let go off it), those keys
//! and Escape close the menu.
//!
//! While the menu is open the button shows its pressed look (isDepressed());
//! its opening and closing redraw the button alone. It takes the keyboard
//! focus and Tab; disabled, it opens nothing.
class GWENI_EXPORT DropDownButton: public Button
{
public:

    GWENI_CONTROL(DropDownButton, Button);

public:
    //! Closes and deletes its menu (delayed, as a MenuItem's submenu goes).
    ~DropDownButton() override;

    //! The menu its press opens, made on first use as a hidden canvas child.
    //! Null off a canvas.
    Menu *getMenu();
    //! Whether getMenu() is shown.
    bool isMenuOpen() const;
    //! Opens the menu below it (closing the other open menus first) with
    //! \p focus's keyboard navigation; nothing while it is disabled or hidden.
    void openMenu(MenuFocus focus=MenuFocus::FirstItem);
    //! Closes the menu, with every submenu open under it.
    void closeMenu();

    //! Disabling it closes its menu.
    void setDisabled(bool disabled) override;

    //! Pressed, or showing its menu.
    bool isDepressed() const override;

    bool isMenuComponent() override
    {
        return true;
    }

    //! Toggles the menu (Button's onPress callers fire after it).
    void onPress(event::Info info) override;
    bool onKeySpace(bool down) override;
    bool onKeyReturn(bool down) override;
    //! Alt+Down opens the menu; a plain Down is not taken.
    bool onKeyDown(bool down) override;
    bool onKeyF4(bool down) override;
    //! Closes the open menu (one the button kept the focus from: a press on
    //! it let go off it); otherwise not taken.
    bool onKeyEscape(bool down) override;

    //! The chevron's side and its room in the right text padding: the
    //! chevron's side plus a gap before the text.
    static constexpr int ChevronSize=8;
    static constexpr int ChevronRoom=ChevronSize+6;
    //! The chevron's right edge off the button's right edge.
    static constexpr int ChevronInset=6;

    //! Where the chevron goes in \p bounds (the button's render bounds): a
    //! ChevronSize square ChevronInset in from the right edge, centred
    //! vertically.
    static Rect chevronRect(const Rect &bounds);

protected:
    //! The menu was shown or hidden: the pressed look follows it.
    void onMenuVisibilityChanged(event::Info);
    //! Space, Return, Alt+Down and F4: open it with the first item
    //! highlighted, or close it if it is open.
    void toggleMenuFromKey();
    //! The control the menu opens below: this button (a SplitButton's arrow
    //! part gives the whole SplitButton).
    virtual Base *menuAnchor()
    {
        return this;
    }

    //! getMenu()'s control id (0 before it is made): an id, so a menu deleted
    //! by the app or the canvas is no menu.
    size_t m_menuId=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_dropDownButton_h_
