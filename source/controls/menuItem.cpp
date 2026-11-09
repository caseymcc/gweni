/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/menuItem.h>
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

MenuItem::~MenuItem()
{}

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

    ParentClass::updateColors();

    if(m_submenuArrow && !(getTextColor() == before))
        m_submenuArrow->redraw();
}

Menu *MenuItem::getMenu()
{
    if(!m_menu)
    {
        m_menu=getCanvas()->newChild<Menu>();
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
        onMenuItemSelected.call(this);
        getCanvas()->closeMenus();
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

    m_menu->setHidden(false);
    m_menu->bringToFront();
    gweni::Point p=localPosToCanvas(gweni::Point(0, 0));

    // Strip menus open downwards
    if(m_onStrip)
        m_menu->setPos(p.x, p.y+getHeight()+1);
    // Submenus open sidewards
    else
        m_menu->setPos(p.x+getWidth(), p.y);

    // TODO: Option this.
    // TODO: Make sure on screen, open the other side of the
    // parent if it's better...
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
