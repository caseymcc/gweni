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

#include <algorithm>
#include <vector>

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

bool sameRect(const Rect &a, const Rect &b)
{
    return (a.x == b.x) && (a.y == b.y) && (a.w == b.w) && (a.h == b.h);
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
    if(m_followedBy)
        m_followedBy->anchoredMenuDeleted(this);

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
// preferred height its parent gives it, not a resize from its own layout. The
// canvas's height is its parent's size, which a new size invalidates it for,
// so only a menu on the canvas reads it: a MenuStrip, which is docked wherever
// the app puts it, is as high as its items (MenuStrip::calculateSize).
void Menu::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    Canvas *canvas=getCanvas();

    if((dim == Dim::Y) && canvas && (getParent() == canvas))
        m_preferredSize.height=std::min(m_preferredSize.height, canvas->getHeight());
}

// Self-placed (openNextTo()): the canvas's arrange skips it, so it takes the
// size the canvas would give a dock-None child (policyResize) and the
// position canvasPosition() gives it, before the subtree is arranged.
// Popup::placement, keepInside and placement() each work an axis on its own,
// so the X arrange places it in X from the widths, and the Y arrange in both
// axes: the anchor was arranged before it in this pass, as the menu comes
// after the anchor's top-level ancestor among the canvas's children
// (openNextTo brings it to the front), but a ScrollControl arranges its
// children in X in its Y arrange (a MenuStrip's items, a Menu's), so the
// anchor's x is final only then. A move needs no arrange of the subtree.
void Menu::arrange(skin::Base *skin, Dim dim)
{
    Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas) && (getPlacement() == Placement::Owner))
    {
        policyResize(getPreferredSize(), dim);

        const Rect area(0, 0, canvas->getWidth(), canvas->getHeight());
        const Point pos=canvasPosition(area);

        if(dim == Dim::X)
            setArrangedBounds(pos.x, getY(), getWidth(), getHeight());
        else
            setArrangedBounds(pos.x, pos.y, getWidth(), getHeight());
    }

    ParentClass::arrange(skin, dim);
}

void Menu::layout(skin::Base *skin)
{
    // Kept inside the canvas, flipped from the cursor when openAt() placed it,
    // when the canvas places it (a self-placed one places itself in
    // arrange()). Only the position changes, which nothing laid out in this
    // pass has read; layout() runs again in the Y arrange once the height is
    // final. Where it was asked to open is the request; where it is kept is
    // computed, from the request each pass, so a canvas that grows again gets
    // it back.
    Canvas *canvas=getCanvas();

    if(canvas && (getParent() == canvas) && (getPlacement() != Placement::Owner))
    {
        const Rect area(0, 0, canvas->getWidth(), canvas->getHeight());
        const Point pos=canvasPosition(area);

        if(!samePoint(pos, Point(getX(), getY())))
            setArrangedBounds(pos.x, pos.y, getWidth(), getHeight());
    }

    ParentClass::layout(skin);
}

Point Menu::canvasPosition(const Rect &area)
{
    const Point size(getWidth(), getHeight());
    const Point at(getRequestedBounds().x, getRequestedBounds().y);

    // a position requested since openAt() or openNextTo() wins
    if(m_atCursor && !samePoint(at, m_cursor))
        m_atCursor=false;

    if(m_anchor && !samePoint(at, m_anchorRequest))
        m_anchor=nullptr;

    if(m_anchor)
    {
        m_placedFor=m_anchor->getCanvasBounds();
        return Popup::placement(m_placedFor, size, area, anchorSide(), m_anchorGap, &m_leftward);
    }

    if(m_atCursor)
        return placement(m_cursor, size, area, &m_leftward);

    m_leftward=false;
    return keepInside(at, size, area);
}

// A submenu opens on the side the menu holding its item went in this pass
// (Win32's and GTK's rule), read from where that menu was placed: it was
// arranged before this one, which comes after it among the canvas's children
// (openNextTo brings it to the front), and is entered in every pass
// (Canvas::enterFollowers), so a chain follows its first menu's side back.
// The item sits in the holding menu's ScrollControl panel, not on the menu.
PopupPlacement Menu::anchorSide() const
{
    if(m_anchorSide != PopupPlacement::Right)
        return m_anchorSide;

    for(Base *parent=m_anchor->getParent(); parent; parent=parent->getParent())
    {
        if(Menu *menu=gweni_cast<Menu>(parent))
            return menu->isLeftward() ? PopupPlacement::Left : PopupPlacement::Right;
    }
    return m_anchorSide;
}

// The cursor is a zero-size anchor: below it, flipped to its left and above
// it, with no gap.
Point Menu::placement(const Point &cursor, const Point &size, const Rect &area,
    bool *leftward)
{
    return Popup::placement(Rect(cursor.x, cursor.y, 0, 0), size, area, PopupPlacement::Below, 0,
        leftward);
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
    // A copy: off a canvas delayedDelete() deletes the item at once, out of
    // the list being walked.
    Base::List items=m_innerPanel->getChildren();

    for(auto &&child : items)
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
    MenuItem *item=gweni_cast<MenuItem>(info.controlCaller);

    // the mouse moves the one highlight; a disabled item takes none
    if(item && m_keyboardNavigation && !item->isDisabled())
        setHighlightedItem(item);

    if(!shouldHoverOpenMenu())
        return;

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

void Menu::openAt(const Point &cursor, MenuFocus focus)
{
    setHidden(false);
    bringToFront();
    m_anchor=nullptr;
    m_atCursor=true;
    m_cursor=cursor;
    setPos(cursor.x, cursor.y);

    if(focus == MenuFocus::FirstItem)
        startKeyboardNavigation();
}

void Menu::openNextTo(Base *anchor, PopupPlacement side, int gap, MenuFocus focus)
{
    Canvas *canvas=anchor ? anchor->getCanvas() : nullptr;

    if(!canvas)
        return;

    if(getParent() != canvas)
        setParent(canvas);

    setPlacement(Placement::Owner);
    setHidden(false);
    bringToFront();
    m_atCursor=false;
    m_anchor=anchor;
    m_anchorSide=side;
    m_anchorGap=gap;
    m_anchorRequest=Point(getRequestedBounds().x, getRequestedBounds().y);
    m_placedFor=Rect();
    invalidate();
    canvas->followAnchoredMenu(this);

    if(focus == MenuFocus::FirstItem)
        startKeyboardNavigation();
}

bool Menu::followAnchor(Canvas *canvas)
{
    if(!m_anchor || hidden() || (getParent() != canvas))
        return false;

    if(!m_anchor->visible() || (m_anchor->getCanvas() != canvas))
    {
        // as its item closes it, with every submenu open under it
        close();
        closeAll();
        return false;
    }

    // moved with no layout pass (a ZoomPanel's pan or zoom); one a pass
    // moves is placed in that pass (Canvas::enterFollowers)
    if(!sameRect(m_anchor->getCanvasBounds(), m_placedFor))
        invalidate();

    return true;
}

void Menu::setHidden(bool hidden)
{
    if(hidden)
    {
        m_atCursor=false;
        m_anchor=nullptr;
        m_leftward=false;
    }

    const bool changed=(hidden != this->hidden());

    ParentClass::setHidden(hidden);

    if(hidden && m_keyboardNavigation)
        endKeyboardNavigation();

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

// Keyboard navigation (startKeyboardNavigation()).

void Menu::startKeyboardNavigation()
{
    if(hidden())
        return;

    if(!m_keyboardNavigation)
    {
        m_keyboardNavigation=true;
        // the canvas drops a focus that takes no keyboard input
        // (input::dropLostKeyboardFocus)
        m_keyboardWasEnabled=getKeyboardInputEnabled();
        setKeyboardInputEnabled(true);
        m_focusReturn=(KeyboardFocus && (KeyboardFocus != this)) ? KeyboardFocus->getControlId() : 0;

        // an item the mouse rests on drops its hover look, which only the
        // highlight shows now
        if(MenuItem *hovered=gweni_cast<MenuItem>(HoveredControl))
        {
            if(hovered->parentMenu() == this)
                hovered->redraw();
        }
    }

    focus();

    const std::vector<MenuItem *> items=navigableItems();

    setHighlightedItem(items.empty() ? nullptr : items.front());
}

void Menu::endKeyboardNavigation()
{
    MenuItem *highlighted=getHighlightedItem();

    m_keyboardNavigation=false;
    m_highlighted=0;
    setKeyboardInputEnabled(m_keyboardWasEnabled);

    // its look goes back to following the hover
    if(highlighted)
        highlighted->redraw();
    if(MenuItem *hovered=gweni_cast<MenuItem>(HoveredControl))
    {
        if((hovered != highlighted) && (hovered->parentMenu() == this))
            hovered->redraw();
    }

    if(KeyboardFocus != this)
        return;

    // Back to where the focus was at the start; a submenu took it from its
    // parent menu, which may be closing in the same closeMenus(), before or
    // after it, so a hidden menu passes it on to the control it took it from.
    Base *target=getControlById(m_focusReturn);

    for(int hops=0; target && (hops < 64); ++hops)
    {
        Menu *menu=gweni_cast<Menu>(target);

        if(!menu || !menu->hidden())
            break;
        target=getControlById(menu->m_focusReturn);
    }

    if(target && target->visible())
        target->focus();

    // refused (a modal), gone or hidden: nothing keeps it
    if(KeyboardFocus == this)
        unfocus();
}

MenuItem *Menu::getHighlightedItem() const
{
    if(!m_highlighted)
        return nullptr;

    return gweni_cast<MenuItem>(getControlById(m_highlighted));
}

void Menu::setHighlightedItem(MenuItem *item)
{
    if(!m_keyboardNavigation)
        return;

    MenuItem *previous=getHighlightedItem();

    if(previous == item)
        return;

    m_highlighted=item ? item->getControlId() : 0;

    if(previous)
        previous->redraw();
    if(item)
        item->redraw();
}

std::vector<MenuItem *> Menu::navigableItems() const
{
    std::vector<MenuItem *> items;

    for(Base *child : m_innerPanel->getChildren())
    {
        MenuItem *item=gweni_cast<MenuItem>(child);

        if(item && !item->hidden() && !item->isDisabled())
            items.push_back(item);
    }
    return items;
}

void Menu::moveHighlight(int step)
{
    const std::vector<MenuItem *> items=navigableItems();

    if(items.empty())
        return;

    const auto at=std::find(items.begin(), items.end(), getHighlightedItem());
    const int count=static_cast<int>(items.size());
    int index;

    if(at == items.end())
        index=(step > 0) ? 0 : count-1;
    else
        index=((static_cast<int>(at-items.begin())+step)%count+count)%count;

    // a submenu the mouse opened on the item left goes with it
    closeAll();
    setHighlightedItem(items[index]);
}

bool Menu::openHighlightedSubmenu()
{
    MenuItem *item=getHighlightedItem();

    if(!item || !item->m_menu)
        return false;

    if(!item->isMenuOpen())
    {
        closeAll();
        item->openMenu();
    }
    item->m_menu->startKeyboardNavigation();
    return true;
}

void Menu::chooseHighlighted()
{
    MenuItem *item=getHighlightedItem();

    if(!item)
        return;

    if(openHighlightedSubmenu())
        return;

    // as a click: a checkable item toggles, onMenuItemSelected fires and the
    // canvas closes every menu
    item->doAction();
}

bool Menu::onKeyUp(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyUp(down);

    if(down)
        moveHighlight(-1);
    return true;
}

bool Menu::onKeyDown(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyDown(down);

    if(down)
        moveHighlight(1);
    return true;
}

bool Menu::onKeyHome(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyHome(down);

    if(down)
    {
        const std::vector<MenuItem *> items=navigableItems();

        if(!items.empty())
        {
            closeAll();
            setHighlightedItem(items.front());
        }
    }
    return true;
}

bool Menu::onKeyend(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyend(down);

    if(down)
    {
        const std::vector<MenuItem *> items=navigableItems();

        if(!items.empty())
        {
            closeAll();
            setHighlightedItem(items.back());
        }
    }
    return true;
}

bool Menu::onKeyReturn(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyReturn(down);

    if(down)
        chooseHighlighted();
    return true;
}

bool Menu::onKeySpace(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeySpace(down);

    if(down)
        chooseHighlighted();
    return true;
}

bool Menu::onKeyRight(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyRight(down);

    if(down)
        openHighlightedSubmenu();
    return true;
}

bool Menu::onKeyLeft(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyLeft(down);

    // a submenu closes back to its item, which its parent menu highlights
    // still; a first menu stays open
    if(down && m_parentItem && !m_parentItem->onStrip())
    {
        closeAll();
        close();
    }
    return true;
}

bool Menu::onKeyEscape(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyEscape(down);

    // one level: a submenu gives the focus back to its parent menu
    if(down)
    {
        closeAll();
        close();
    }
    return true;
}

bool Menu::onKeyTab(bool down)
{
    if(!m_keyboardNavigation)
        return ParentClass::onKeyTab(down);

    return true;
}

}//namespace controls
}//namespace gweni
