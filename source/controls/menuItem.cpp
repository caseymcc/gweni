/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/comboBox.h>
#include <gweni/skin.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/imagePanel.h>

namespace gweni
{
namespace controls
{

class GWENI_EXPORT RightArrow: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(RightArrow, controls::Base)
    {
        setMouseInputEnabled(false);
    }
};


GWENI_CONTROL_CONSTRUCTOR(MenuItem)
{
    setTabable(false);
}

// Its submenu is a canvas child, not its own: it goes delayed, as a menu is
// deleted (this may run from one of its items' events), hidden first with
// every submenu open under it. The canvas is the submenu's: this item's own
// parent chain may be half destroyed (its Menu's ~Base deleting it). A canvas
// tearing down deletes the submenu with its other children, before or after
// this item.
MenuItem::~MenuItem()
{
    Menu *menu=m_menu;

    if(!menu)
        return;

    m_menu=nullptr;
    menu->m_parentItem=nullptr;
    if(menu->m_anchor == this)
        menu->m_anchor=nullptr;
    // unhooked before the hide, which would redraw this item
    menu->onVisibilityChanged.removeHandler(this);

    Canvas *canvas=menu->getCanvas();

    if(canvas && canvas->releasingChildren())
        return;

    // close() deletes a deleteOnClose() menu itself (at once off any canvas),
    // so it goes last and the menu is not deleted again
    const bool deletesOnClose=menu->deleteOnClose();

    menu->closeAll();
    menu->close();
    if(!deletesOnClose)
        // at once off any canvas
        menu->delayedDelete();
}

void MenuItem::render(skin::Base *skin)
{
    // Base::render: generate on the first render, update, and count the change
    // (ChangeStats); the textured renderer draws the highlight and the check
    ParentClass::render(skin);

    // HACK!
    if(m_accelerator)
        m_accelerator->setTextColorOverride(getTextColor());
}

void MenuItem::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
}

void MenuItem::updateColors()
{
    const gweni::Color before=getTextColor();

    // while its menu navigates by keyboard the highlight, not the hover, has
    // the hover colour (Button's order otherwise)
    Menu *menu=parentMenu();

    if(menu && menu->isKeyboardNavigating() && !isDisabled() && !isDepressed() && !getToggleState())
        setTextColor((menu->getHighlightedItem() == this) ? getSkin()->Colors.Button.Hover
            : getSkin()->Colors.Button.Normal);
    else
        ParentClass::updateColors();

    if(m_submenuArrow && !(getTextColor() == before))
        m_submenuArrow->redraw();
}

Menu *MenuItem::parentMenu() const
{
    for(Base *parent=getParent(); parent; parent=parent->getParent())
    {
        if(Menu *menu=gweni_cast<Menu>(parent))
            return menu;
    }
    return nullptr;
}

bool MenuItem::isHighlighted()
{
    if(isDisabled())
        return false;

    if(isDepressed() || isMenuOpen())
        return true;

    Menu *menu=parentMenu();

    if(menu && menu->isKeyboardNavigating())
        return menu->getHighlightedItem() == this;

    return isHovered();
}

Menu *MenuItem::getMenu()
{
    if(!m_menu)
    {
        m_menu=getCanvas()->newChild<Menu>();
        m_menu->m_parentItem=this;
        m_menu->setHidden(true);
        m_menu->onVisibilityChanged.add(this, &MenuItem::onMenuVisibilityChanged);

        if(!m_onStrip)
        {
            m_submenuArrow=newChild<RightArrow>();
            m_submenuArrow->setSize(15, 15);
            m_submenuArrow->setAlignment(Alignment::Right|Alignment::CenterV);
        }

        invalidate();
    }

    return m_menu;
}

// The arrow goes delayed: the submenu may be deleted from an event of this
// item's own menu, inside a walk of its tree. A canvas tearing down deletes it
// with the item.
void MenuItem::submenuDeleted()
{
    m_menu=nullptr;

    Canvas *canvas=getCanvas();

    if(canvas && canvas->releasingChildren())
        return;

    if(m_submenuArrow)
    {
        // hidden now, so a frame before the delete neither lays it out nor
        // draws it
        m_submenuArrow->setHidden(true);

        if(canvas)
            m_submenuArrow->delayedDelete();
        else
            delete m_submenuArrow;
        m_submenuArrow=nullptr;
    }

    invalidate();
    // an open submenu's highlight goes: no visibility change tells the row
    redraw();
}

// However the submenu opens or closes (openMenu(), Menu::closeAll(), the
// canvas closing every menu), the row's highlight follows it.
void MenuItem::onMenuVisibilityChanged(event::Info)
{
    redraw();
}

void MenuItem::setCheckable(bool check)
{
    if(check == m_checkable)
        return;

    m_checkable=check;
    updateImageVisibility();
    redraw();
}

void MenuItem::setChecked(bool check)
{
    if(check == m_checked)
        return;

    m_checked=check;
    updateImageVisibility();
    redraw();
    onCheckChange.call(this);

    if(check)
        onChecked.call(this);
    else
        onUnChecked.call(this);
}

void MenuItem::setImage(const String &name, bool center)
{
    ParentClass::setImage(name, center);
    updateImageVisibility();
}

void MenuItem::updateImageVisibility()
{
    if(m_image)
        m_image->setHidden(m_checkable && m_checked);
}

void MenuItem::onPress(event::Info info)
{
    if(m_menu)
    {
        toggleMenu();
    }
    else if(!m_onStrip)
    {
        if(m_checkable)
            setChecked(!m_checked);
        // Closed first, so a navigating menu has given the focus back before
        // the handler runs: a dialog it opens records that control as its
        // focus return, not a menu that is hidden by then.
        if(Canvas *canvas=getCanvas())
            canvas->closeMenus();
        onMenuItemSelected.call(this);
    }

    ParentClass::onPress(info);
}

void MenuItem::toggleMenu()
{
    if(isMenuOpen())
        closeMenu();
    else
        openMenu();
}

bool MenuItem::isMenuOpen()
{
    if(!m_menu)
        return false;

    return !m_menu->hidden();
}

void MenuItem::openMenu()
{
    if(!m_menu)
        return;

    // A strip menu below the item, 1 unit off it, a submenu on its right, top
    // to top: placed from the item in every layout pass, flipped (above it,
    // on its left) and kept inside the canvas.
    // Under a navigating menu the submenu navigates too, with nothing
    // highlighted (a mouse open): it takes the focus from its parent menu, so
    // a press on one of its items keeps the focus in the chain that gives it
    // back to the opener, and its keys work after a press let go off it.
    if(m_onStrip)
        m_menu->openNextTo(this, PopupPlacement::Below, 1);
    else
    {
        Menu *parent=parentMenu();
        const MenuFocus focus=(parent && parent->isKeyboardNavigating()) ? MenuFocus::Menu : MenuFocus::None;

        m_menu->openNextTo(this, PopupPlacement::Right, 0, focus);
    }
}

void MenuItem::closeMenu()
{
    if(!m_menu)
        return;

    m_menu->close();
    m_menu->closeAll();
}

void MenuItem::setAccelerator(const String &strAccelerator)
{
    if(m_accelerator)
    {
        m_accelerator->delayedDelete();
        m_accelerator=nullptr;
    }

    if(strAccelerator.empty())
        return;

    m_accelerator=newChild<Label>();
    m_accelerator->setDock(DockPosition::Right);
    m_accelerator->setText(strAccelerator);
    m_accelerator->setMargin(Margin(0, 0, 16, 0));
    // TODO.
}

void MenuItem::setText(const String &str, bool doEvents)
{
    // Label's returns early on the same string, which reaches nothing.
    const String before=getText();

    ParentClass::setText(str, doEvents);
    if(getText() == before)
        return;

    // read after the events, which may have moved the item
    if(auto *list=gweni_cast<internal::ComboBoxList>(getParent()))
        list->itemTextChanged(this);
}

void MenuItem::sizeToContents()
{
    ParentClass::sizeToContents();

    if(m_accelerator)
    {
        m_accelerator->sizeToContents();
        setWidth(getWidth()+m_accelerator->getWidth());
    }
}

}//namespace controls
}//namespace gweni
