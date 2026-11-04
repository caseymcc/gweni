/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/menu.h>
#include <gweni/skin.h>
#include <gweni/utility.h>
#include <gweni/controls/canvas.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(Menu)
{
    setBounds(0, 0, 10, 10);
    setPadding(Padding(2, 2, 2, 2));
    setDisableIconMargin(false);
    setAutoHideBars(true);
    setScroll(false, true);
    setDeleteOnClose(false);
}

void Menu::renderUnder(skin::Base *skin)
{
    ParentClass::renderUnder(skin);
//    skin->drawShadow(this, skin::Generate);
//    skin->drawControl(this);
}

void Menu::layout(skin::Base *skin)
{
    int menuHeight=0;

    for(auto &&child : m_innerPanel->getChildren())
    {
        if(child)
            menuHeight+=child->getHeight();
    }

    // if tall menu descends off screen then clamp to screen height
    if(getY() + menuHeight > getCanvas()->getHeight())
    {
        const int dy=getCanvas()->getHeight() - getY();
        const int ytop=std::max(getY() - dy, 0);

        setPos(getX(), ytop);
        menuHeight=std::min(menuHeight, getCanvas()->getHeight());
    }

    setSize(getWidth(), menuHeight);

    ParentClass::layout(skin);
}

MenuItem *Menu::addItem(const String &name,
    const String &strIconName,
    const String &strAccelerator)
{
    MenuItem *item=newChild<MenuItem>();
    item->setPadding(Padding(2, 4, 4, 4));
    item->setText(name);
    item->setImage(strIconName);
    item->setAccelerator(strAccelerator);
    onAddItem(item);
    return item;
}

void Menu::onAddItem(MenuItem *item)
{
    item->setTextPadding(Padding(iconMarginDisabled()?0:24, 0, 16, 0));
    item->setDock(DockPosition::Top);
    item->sizeToContents();
    item->setAlignment(Alignment::CenterV|Alignment::Left);
    item->onHoverEnter.add(this, &Menu::onHoverItem);

    // Do this here - after Top Docking these values mean nothing in layout
    const int w=std::max(item->getWidth()+10+32, getWidth());

    setSize(w, getHeight());
}

void Menu::clearItems()
{
    for(auto &&child : m_innerPanel->getChildren())
    {
        if(!child)
            continue;

        child->delayedDelete();
    }
}

void Menu::closeAll()
{
    for(auto &&child : m_innerPanel->getChildren())
    {
        MenuItem *item=gweni_cast<MenuItem>(child);

        if(!item)
            continue;

        item->closeMenu();
    }
}

bool Menu::isMenuOpen()
{
    for(auto &&child : m_innerPanel->getChildren())
    {
        MenuItem *item=gweni_cast<MenuItem>(child);

        if(!item)
            continue;

        if(item->isMenuOpen())
            return true;
    }

    return false;
}

void Menu::onHoverItem(event::Info info)
{
    if(!shouldHoverOpenMenu())
        return;

    MenuItem *item=gweni_cast<MenuItem>(info.controlCaller);

    if(!item)
        return;

    if(item->isMenuOpen())
        return;

    closeAll();
    item->openMenu();
}

void Menu::open(Alignment pos)
{
    setHidden(false);
    bringToFront();
    gweni::Point MousePos=input::getMousePosition();
    setPos(MousePos.x, MousePos.y);
}

void Menu::close()
{
    setHidden(true);

    if(deleteOnClose())
        delayedDelete();
}

void Menu::closeMenus()
{
    ParentClass::closeMenus();
    closeAll();
    close();
}

void Menu::addDivider()
{
    MenuDivider *divider=newChild<MenuDivider>();
    divider->setDock(DockPosition::Top);
    divider->setMargin(Margin(iconMarginDisabled()?0:24, 0, 4, 0));
}

}//namespace controls
}//namespace gweni
