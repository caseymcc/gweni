/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_splitButton_h_
#define _gweni_controls_splitButton_h_

#include <gweni/controls/button.h>
#include <gweni/controls/dropDownButton.h>

namespace gweni
{
namespace controls
{
class MenuItem;
class SplitButton;

namespace internal
{

//! A SplitButton's main part: a Button that never takes the keyboard focus
//! (the SplitButton takes it for both parts), drawn square on its right.
class GWENI_EXPORT SplitButtonMain: public Button
{
public:

    GWENI_CONTROL(SplitButtonMain, Button);

public:
    //! The SplitButton it is the main part of.
    SplitButton *getSplitButton() const;
};

//! A SplitButton's arrow part: a DropDownButton (its menu, its toggle and its
//! pressed look while the menu is open) that never takes the keyboard focus,
//! drawn square on its left with the chevron centred. Its menu opens below the
//! SplitButton.
class GWENI_EXPORT SplitButtonArrow: public DropDownButton
{
public:

    GWENI_CONTROL(SplitButtonArrow, DropDownButton);

public:
    SplitButton *getSplitButton() const;

protected:
    //! The menu opens below the whole SplitButton, left-aligned with it.
    Base *menuAnchor() override;
    bool takesKeyboardFocus() const override
    {
        return false;
    }
};

}//namespace internal

//! A button with an action and a menu in one outline (WinUI's SplitButton and
//! ToggleSplitButton, Qt's QToolButton with MenuButtonPopup, Material's split
//! button): a main part that acts on its own and an arrow part that opens
//! getMenu() below the control.
//!
//! The parts are two child Buttons in one row, the main part as wide as its
//! content (text, image) and the arrow part ArrowWidth units, with a Divider
//! unit between them that the SplitButton itself draws (the SplitButtonDivider
//! surface). The main part is drawn square on its right with no border there,
//! the arrow part square on its left, so the two are one outline; each has its
//! own hover and press look (Button's surfaces), and a hover of one writes its
//! slot alone.
//!
//! The main part fires onPress (and, a toggle with setIsToggle(true), flips
//! its state and fires onToggle first); the arrow part opens and closes the
//! menu as a DropDownButton does (a click with MenuFocus::Menu), with the
//! arrow's pressed look while it is open. Neither part fires the other's.
//!
//! The control takes the keyboard focus as one (the parts never do), drawing
//! both parts' borders in ButtonFocus's colour: Space and Return press the
//! main part, Alt+Down and F4 open the menu with its first item highlighted;
//! the menu gives the focus back to the control when it closes. Disabled, it
//! takes neither (setDisabled disables both parts).
//!
//! setMainItem(item) makes a menu item the main part's action (the "last
//! used" pattern): the main part shows the item's text and its press fires the
//! item's onMenuItemSelected, as choosing it from the menu does.
class GWENI_EXPORT SplitButton: public Base
{
public:

    GWENI_CONTROL(SplitButton, Base);

public:
    //! The arrow part's width and the divider's, in units.
    static constexpr int ArrowWidth=20;
    static constexpr int Divider=1;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

    //! The main part's text and image.
    void setText(const String &text);
    const String &getText();
    void setImage(const String &name, bool center=false);

    //! The menu the arrow part opens, made on first use as a hidden canvas
    //! child. Null off a canvas.
    Menu *getMenu();
    bool isMenuOpen() const;
    void openMenu(MenuFocus focus=MenuFocus::FirstItem);
    void closeMenu();

    //! The main part as a toggle (ToggleSplitButton): a press flips its state.
    void setIsToggle(bool toggle);
    bool isToggle() const;
    bool getToggleState() const;
    void setToggleState(bool on);

    //! \p item (one of getMenu()'s, or null for none) becomes the main part's
    //! action: its text is the main part's, and a press on the main part fires
    //! its onMenuItemSelected (after onPress). Held by id, so a deleted item is
    //! none.
    void setMainItem(MenuItem *item);
    MenuItem *getMainItem() const;

    void setDisabled(bool disabled) override;

    internal::SplitButtonMain *getMainPart() const
    {
        return m_main;
    }
    internal::SplitButtonArrow *getArrowPart() const
    {
        return m_arrow;
    }

    void onKeyboardFocus() override;
    void onLostKeyboardFocus() override;
    bool onKeySpace(bool down) override;
    bool onKeyReturn(bool down) override;
    //! Alt+Down opens the menu (closes it if open); a plain Down is not
    //! taken.
    bool onKeyDown(bool down) override;
    bool onKeyF4(bool down) override;
    //! Closes the open menu (one the control kept the focus from: a press on
    //! the arrow let go off it); otherwise not taken.
    bool onKeyEscape(bool down) override;

    //! The main part was pressed (a click, Space, Return).
    gweni::event::Caller onPress;
    //! The main part's toggle state changed.
    gweni::event::Caller onToggle;

protected:
    void onMainPressed(event::Info info);
    void onMainToggled(event::Info info);
    //! Alt+Down and F4: open the menu with its first item highlighted, or
    //! close it if it is open.
    void toggleMenuFromKey();
    //! Space and Return: close a menu left open, then the main part's action.
    void pressMainFromKey();

    internal::SplitButtonMain *m_main=nullptr;
    internal::SplitButtonArrow *m_arrow=nullptr;
    //! getMainItem()'s control id, 0 for none.
    size_t m_mainItem=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_splitButton_h_
