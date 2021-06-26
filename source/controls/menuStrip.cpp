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
    dock(Position::Top);
    m_innerPanel->setPadding(Padding(5, 0, 0, 0));
}

void MenuStrip::render(skin::Base *skin)
{
//    skin->drawMenuStrip(this, skin::Generate);
    skin->drawControl(this);
}

void MenuStrip::layout(skin::Base * /*skin*/)
{
    // TODO: We don't want to do vertical sizing the same as Menu, do nothing
    // for now
}

void MenuStrip::onAddItem(MenuItem *item)
{
    item->dock(Position::Left);
    item->setTextPadding(Padding(5, 0, 5, 0));
    item->setPadding(Padding(10, 0, 10, 0));
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
