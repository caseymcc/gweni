/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/skin.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(MenuStrip)
{
    setBounds(0, 0, 200, 22);
    setDock(DockPosition::Top);
    // One row that never scrolls: no bars, and no inset but the 5 units
    // before the first item, so the items are the strip's full height (their
    // own padding makes it, onAddItem).
    setScroll(false, false);
    setPadding(Padding(0, 0, 0, 0));
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    m_innerPanel->setPadding(Padding(5, 0, 0, 0));
}

void MenuStrip::layout(skin::Base * /*skin*/)
{
    // TODO: We don't want to do vertical sizing the same as Menu, do nothing
    // for now
}

void MenuStrip::onAddItem(MenuItem *item)
{
    item->setDock(DockPosition::Left);
    item->setTextPadding(Padding(5, 0, 5, 0));
    item->setPadding(Padding(10, StripItemInset, 10, StripItemInset));
    item->sizeToContents();
    item->setOnStrip(true);
    item->onHoverEnter.add(this, &Menu::onHoverItem);
}

bool MenuStrip::shouldHoverOpenMenu()
{
    return isMenuOpen();
}

}//namespace controls
}//namespace gweni
