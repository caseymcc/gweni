/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/dropDownButton.h>
#include <gweni/controls/canvas.h>
#include <gweni/inputHandler.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(DropDownButton)
{
    setKeyboardInputEnabled(true);
    setTabable(true);
    // Button's padding, the chevron's room added on the right
    setTextPadding(Padding(3, 0, 3+ChevronRoom, 0));
}

// The menu is a canvas child, not this button's: it goes delayed, closed
// first, as a MenuItem's submenu does. A canvas tearing down deletes it with
// its other children.
DropDownButton::~DropDownButton()
{
    Menu *menu=gweni_cast<Menu>(getControlById(m_menuId));

    if(!menu)
        return;

    m_menuId=0;
    // unhooked before the hide, which would redraw this button
    menu->onVisibilityChanged.removeHandler(this);

    Canvas *canvas=menu->getCanvas();

    if(canvas && canvas->releasingChildren())
        return;

    // read first: close() deletes a deleteOnClose menu off any canvas at once
    const bool deletesItself=menu->deleteOnClose();

    menu->closeAll();
    menu->close();
    if(!deletesItself)
        menu->delayedDelete();
}

Menu *DropDownButton::getMenu()
{
    if(Menu *menu=gweni_cast<Menu>(getControlById(m_menuId)))
        return menu;

    Canvas *canvas=getCanvas();

    if(!canvas)
        return nullptr;

    Menu *menu=canvas->newChild<Menu>();

    menu->setHidden(true);
    menu->onVisibilityChanged.add(this, &DropDownButton::onMenuVisibilityChanged);
    m_menuId=menu->getControlId();
    return menu;
}

bool DropDownButton::isMenuOpen() const
{
    Menu *menu=gweni_cast<Menu>(getControlById(m_menuId));

    return menu && !menu->hidden();
}

void DropDownButton::openMenu(MenuFocus focus)
{
    if(isDisabled() || !visible())
        return;

    Menu *menu=getMenu();
    Canvas *canvas=getCanvas();

    if(!menu || !canvas)
        return;

    // one menu open at a time, as a press anywhere else closes them
    canvas->closeMenus();
    menu->openNextTo(menuAnchor(), PopupPlacement::Below, 1, focus);
}

void DropDownButton::closeMenu()
{
    Menu *menu=gweni_cast<Menu>(getControlById(m_menuId));

    if(!menu)
        return;

    menu->closeAll();
    menu->close();
}

void DropDownButton::setDisabled(bool disabled)
{
    const bool focusable=!disabled && takesKeyboardFocus();

    // off before the menu closes, whose hand-back would focus it again
    setKeyboardInputEnabled(focusable);
    setTabable(focusable);
    if(disabled)
        closeMenu();

    ParentClass::setDisabled(disabled);
    if(disabled && (KeyboardFocus == this))
        unfocus();
}

bool DropDownButton::isDepressed() const
{
    return ParentClass::isDepressed() || isMenuOpen();
}

void DropDownButton::onMenuVisibilityChanged(event::Info)
{
    redraw();
}

void DropDownButton::onPress(event::Info info)
{
    if(isDisabled())
        return;

    if(isMenuOpen())
        closeMenu();
    else
        openMenu(MenuFocus::Menu);

    ParentClass::onPress(info);
}

bool DropDownButton::onKeySpace(bool down)
{
    if(down)
        toggleMenuFromKey();
    return true;
}

bool DropDownButton::onKeyReturn(bool down)
{
    if(down)
        toggleMenuFromKey();
    return true;
}

bool DropDownButton::onKeyDown(bool down)
{
    if(!input::isKeyDown(keys::Alt))
        return ParentClass::onKeyDown(down);

    if(down)
        toggleMenuFromKey();
    return true;
}

bool DropDownButton::onKeyF4(bool down)
{
    if(down)
        toggleMenuFromKey();
    return true;
}

// The menu open while the button has the focus (a press on the open button let
// go off it): the key closes it, as a second press would.
void DropDownButton::toggleMenuFromKey()
{
    if(isMenuOpen())
        closeMenu();
    else
        openMenu(MenuFocus::FirstItem);
}

bool DropDownButton::onKeyEscape(bool down)
{
    if(!isMenuOpen())
        return ParentClass::onKeyEscape(down);

    if(down)
        closeMenu();
    return true;
}

Rect DropDownButton::chevronRect(const Rect &bounds)
{
    return Rect(bounds.x+bounds.w-ChevronInset-ChevronSize, bounds.y+(bounds.h-ChevronSize)/2,
        ChevronSize, ChevronSize);
}

}//namespace controls
}//namespace gweni
