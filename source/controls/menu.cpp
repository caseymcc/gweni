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
#include <gweni/controls/popup.h>

namespace gweni
{
namespace controls
{

namespace
{
bool samePoint(const Point &a, const Point &b)
{
    return (a.x == b.x) && (a.y == b.y);
}
}


GWENI_CONTROL_CONSTRUCTOR(Menu)
{
    // The padding is the inner panel's, with no margin: ScrollControl places
    // the inner panel at its viewport, which with neither is the whole menu,
    // and the panel's padding keeps the items 2 units off the backdrop's
    // border on every side.
    m_innerPanel->setPadding(Padding(2, 2, 2, 2));
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    setDisableIconMargin(false);
    setAutoHideBars(true);
    setScroll(false, true);
    setDeleteOnClose(false);
}

Menu::~Menu()
{
    for(Base *owner : m_contextOwners)
        owner->contextMenuDeleted();

    if(m_parentItem)
        m_parentItem->submenuDeleted();
}

void Menu::renderUnder(skin::Base *skin)
{
    ParentClass::renderUnder(skin);
//    skin->drawShadow(this, skin::Generate);
//    skin->drawControl(this);
}

// As high as its items, which are docked Top, up to the canvas's height: a
// preferred height its parent gives it, not a resize from its own layout.
void Menu::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::Y)
        m_preferredSize.height=std::min(m_preferredSize.height, getCanvas()->getHeight());
}

void Menu::layout(skin::Base *skin)
{
    // Kept inside the canvas, flipped from the cursor when openAt() placed it.
    // Only the position changes, which nothing laid out in this pass has read;
    // layout() runs again in the Y arrange once the height is final. Where it
    // was asked to open is the request; where it is kept is computed, from
    // the request each pass, so a canvas that grows again gets it back.
    Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas))
    {
        const Rect area(0, 0, canvas->getWidth(), canvas->getHeight());
        const Point size(getWidth(), getHeight());
        const Point at(getRequestedBounds().x, getRequestedBounds().y);

        // a position requested since openAt() wins over the cursor
        if(m_atCursor && !samePoint(at, m_cursor))
            m_atCursor=false;

        const Point pos=m_atCursor ? placement(m_cursor, size, area) : keepInside(at, size, area);

        if(!samePoint(pos, Point(getX(), getY())))
            setArrangedBounds(pos.x, pos.y, getWidth(), getHeight());
    }

    ParentClass::layout(skin);
}

// The cursor is a zero-size anchor: below it, flipped to its left and above
// it, with no gap.
Point Menu::placement(const Point &cursor, const Point &size, const Rect &area)
{
    return Popup::placement(Rect(cursor.x, cursor.y, 0, 0), size, area, PopupPlacement::Below, 0);
}

Point Menu::keepInside(const Point &pos, const Point &size, const Rect &area)
{
    return Point(std::max(area.x, std::min(pos.x, area.x+area.w-size.x)),
        std::max(area.y, std::min(pos.y, area.y+area.h-size.y)));
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
    // after the icon column, clear of the accelerator docked on the right
    item->setTextAlignment(Alignment::Left|Alignment::CenterV);
    item->setDock(DockPosition::Top);
    item->sizeToContents();
    item->onHoverEnter.add(this, &Menu::onHoverItem);

    // Do this here - after Top Docking these values mean nothing in layout
    const int w=std::max(item->getWidth()+10+32, getWidth());

    // The width only: a height asked for would cap the menu (ScrollControl),
    // which is as high as its items.
    setWidth(w);
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

void Menu::open(Alignment /*pos*/)
{
    openAt(input::getMousePosition());
}

void Menu::openAt(const Point &cursor)
{
    setHidden(false);
    bringToFront();
    m_atCursor=true;
    m_cursor=cursor;
    setPos(cursor.x, cursor.y);
}

void Menu::setHidden(bool hidden)
{
    if(hidden)
        m_atCursor=false;

    const bool changed=(hidden != this->hidden());

    ParentClass::setHidden(hidden);

    if(changed)
        onVisibilityChanged.call(this);
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
    // 3 units of backdrop above and below the rule
    divider->setMargin(Margin(iconMarginDisabled()?0:24, 3, 4, 3));
}

}//namespace controls
}//namespace gweni
