/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/menuItem.h>
#include <gweni/skin.h>
#include <gweni/controls/canvas.h>

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

public:
    void render(skin::Base *skin) override
    {
//        skin->drawMenuRightArrow(this, skin::Generate);
//        skin->drawControl(this);
        if(getStateChange() != StateChange_Nothing)
            m_skinControl->update(skin->getRenderer(), this);
    }

};


GWENI_CONTROL_CONSTRUCTOR(MenuItem)
{
    m_menu=nullptr;
    m_onStrip=false;
    m_submenuArrow=nullptr;
    m_accelerator=nullptr;
    setTabable(false);
    setCheckable(false);
    setChecked(false);
}

MenuItem::~MenuItem()
{}

void MenuItem::render(skin::Base *skin)
{
//    skin->drawMenuItem(this, skin::Generate, isMenuOpen(), m_checkable?m_checked:false);
//    skin->drawControl(this);
    if(getStateChange() != StateChange_Nothing)
        m_skinControl->update(skin->getRenderer(), this);

    // HACK!
    if(m_accelerator)
        m_accelerator->setTextColorOverride(getTextColor());
}

void MenuItem::layout(skin::Base *skin)
{
    if(m_submenuArrow)
        m_submenuArrow->setPosition(Position::Right|Position::CenterV, 4, 0);

    ParentClass::layout(skin);
}

Menu *MenuItem::getMenu()
{
    if(!m_menu)
    {
        m_menu=getCanvas()->newChild<Menu>();
        m_menu->setHidden(true);

        if(!m_onStrip)
        {
            m_submenuArrow=newChild<RightArrow>();
            m_submenuArrow->setSize(15, 15);
        }

        invalidate();
    }

    return m_menu;
}

void MenuItem::setChecked(bool check)
{
    if(check == m_checked)
        return;

    m_checked=check;
    onCheckChange.call(this);

    if(check)
        onChecked.call(this);
    else
        onUnChecked.call(this);
}

void MenuItem::onPress(event::Info info)
{
    if(m_menu)
    {
        toggleMenu();
    }
    else if(!m_onStrip)
    {
        setChecked(!cetChecked());
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
    m_accelerator->dock(Position::Right);
    m_accelerator->setAlignment(Position::Right|Position::CenterV);
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
