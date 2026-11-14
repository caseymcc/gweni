/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_menu_h_
#define _gweni_controls_menu_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/separator.h>

namespace gweni
{
namespace controls
{
class MenuItem;

//! Whether a Menu opened by openNextTo() or openAt() navigates by keyboard
//! (Menu::startKeyboardNavigation): None, as every menu did before, or
//! FirstItem, taking the keyboard focus with its first enabled item
//! highlighted.
enum class MenuFocus
{
    None,
    FirstItem
};

class GWENI_EXPORT Menu: public ScrollControl
{
public:

    GWENI_CONTROL(Menu, ScrollControl);
    //! Clears it from every control it is the context menu of, and from the
    //! MenuItem it is the submenu of (MenuItem::submenuDeleted).
    ~Menu() override;

public:
    //void render(skin::Base *skin) override;
    void renderUnder(skin::Base *skin) override;

    void calculateSize(skin::Base *skin, Dim dim) override;
    //! Places it on the canvas once openNextTo() has made it self-placed
    //! (Placement::Owner), one axis per arrange, as Popup::arrange does.
    void arrange(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;

    virtual MenuItem *addItem(const String &name,
        const String &strIconName="",
        const String &strAccelerator="");

    virtual void addDivider();

    void onHoverItem(event::Info info);
    void closeAll();
    bool isMenuOpen();
    void clearItems();

    //! Opens it at the mouse, as openAt() does.
    virtual void open(Alignment pos);
    //! Shows it and brings it to the front with its top-left at \p cursor
    //! (canvas coordinates), flipped and kept inside the canvas by placement()
    //! once its size is known (in layout()). A setPos()/setBounds() after it
    //! places it there instead, kept inside the canvas.
    //! \p focus FirstItem starts keyboard navigation once it is shown
    //! (startKeyboardNavigation()).
    virtual void openAt(const Point &cursor, MenuFocus focus=MenuFocus::None);
    //! Shows it on the canvas and brings it to the front next to \p anchor (the
    //! MenuItem it is the menu of): on \p side of the anchor's canvas rect,
    //! \p gap units off it, flipped and kept inside the canvas by
    //! Popup::placement in every layout pass, so it opens where the anchor is
    //! laid out, not where it was at the call. From the first call on it is
    //! self-placed (Placement::Owner) on the canvas. A setPos()/setBounds()
    //! after it places it there instead, kept inside the canvas; hiding it or
    //! openAt() forgets the anchor. While it is open the canvas follows it
    //! (Canvas::anchoredMenus()): it is laid out again when the anchor moves,
    //! and closed when the anchor is hidden or deleted. A Right one whose
    //! anchor is in a menu that isLeftward() is placed as Left instead, so a
    //! submenu chain that flipped left goes on opening left. \p focus
    //! FirstItem starts keyboard navigation once it is shown.
    virtual void openNextTo(Base *anchor, PopupPlacement side, int gap,
        MenuFocus focus=MenuFocus::None);

    //! Keyboard navigation, for a shown menu: it takes the keyboard focus,
    //! keeping the control that had it to give it back when it is hidden, and
    //! highlights its first enabled item. Up/Down move the highlight over the
    //! enabled, shown items (wrapping), Home/End go to the first/last,
    //! Return/Space choose the highlighted item as a click does (a submenu's
    //! item opens its submenu as Right does), Right opens the highlighted
    //! item's submenu navigating by keyboard in turn, Left closes a submenu
    //! back to its item, Escape closes it, and Tab is taken (the focus stays).
    //! The highlight is the hover's look: while it navigates, an item shows
    //! hovered only while it is the highlighted one, and the mouse moving
    //! onto an enabled item highlights it, so there is one highlight. Ends
    //! when it is hidden. A menu that never starts it takes no focus and no
    //! key, as before.
    void startKeyboardNavigation();
    bool isKeyboardNavigating() const
    {
        return m_keyboardNavigation;
    }
    //! The item the keyboard highlights while it navigates; null otherwise.
    MenuItem *getHighlightedItem() const;
    //! Moves the keyboard highlight to \p item (one of its enabled items, or
    //! null), redrawing the item it leaves and the one it reaches; nothing
    //! while it does not navigate.
    void setHighlightedItem(MenuItem *item);

    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyReturn(bool down) override;
    bool onKeySpace(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyLeft(bool down) override;
    bool onKeyEscape(bool down) override;
    bool onKeyTab(bool down) override;
    //! What openNextTo() placed it next to, while it does; null otherwise.
    Base *getAnchor() const
    {
        return m_anchor;
    }
    //! Whether its placement on the canvas in the last layout pass went left
    //! of where its side starts it (Popup::placement's leftward): next to its
    //! anchor, on the anchor's left or right-aligned with it, or at the
    //! cursor, on its left. False at a requested position or off the canvas.
    bool isLeftward() const
    {
        return m_leftward;
    }
    virtual void close();

    void setHidden(bool hidden) override;

    //! Called when it is shown or hidden (setHidden() with a new state), so
    //! the MenuItem it is the submenu of redraws its open highlight.
    gweni::event::Caller onVisibilityChanged;

    //! Where a menu of \p size opened at \p cursor goes in \p area: at the
    //! cursor, on its left where it would run past the right edge, above it
    //! where it would run past the bottom, then kept inside (keepInside()):
    //! against the left/top edge where it fits on neither side. \p leftward,
    //! when given, is set to whether it went to the cursor's left.
    static Point placement(const Point &cursor, const Point &size, const Rect &area,
        bool *leftward=nullptr);
    //! \p pos moved the least that puts a \p size rect inside \p area; at
    //! the area's left/top edge where it is wider/higher than the area.
    static Point keepInside(const Point &pos, const Point &size, const Rect &area);

    bool isMenuComponent() override
    {
        return true;
    }

    void closeMenus() override;

    bool iconMarginDisabled()
    {
        return m_disableIconMargin;
    }

    void setDisableIconMargin(bool bDisable)
    {
        m_disableIconMargin=bDisable;
    }

    bool deleteOnClose()
    {
        return m_deleteOnClose;
    }

    void setDeleteOnClose(bool b)
    {
        m_deleteOnClose=b;
    }

protected:

    //! Where it goes on the canvas (\p area) at its size: next to its anchor,
    //! at the cursor, or at its requested position, kept inside. A request
    //! made since openNextTo() or openAt() drops that mode.
    Point canvasPosition(const Rect &area);
    //! The side canvasPosition() places it on next to m_anchor: m_anchorSide,
    //! but Left for a Right one whose anchor is in a menu that isLeftward().
    PopupPlacement anchorSide() const;

    virtual bool shouldHoverOpenMenu()
    {
        return true;
    }

    virtual void onAddItem(MenuItem *item);

    //! Its enabled, shown items in order, which the keyboard highlights.
    std::vector<MenuItem *> navigableItems() const;
    //! The highlight \p step items on (wrapping), or the first/last item for
    //! a step from none.
    void moveHighlight(int step);
    //! Chooses the highlighted item: opens its submenu navigating by
    //! keyboard, or presses it as a click does.
    void chooseHighlighted();
    //! Opens the highlighted item's submenu, navigating by keyboard; false
    //! when it has none.
    bool openHighlightedSubmenu();
    //! Ends keyboard navigation on a hide: the highlight cleared and the focus,
    //! if this menu still holds it, given back to the control that had it at
    //! the start (through a hidden menu to the one it took it from), else
    //! dropped.
    void endKeyboardNavigation();

    bool m_keyboardNavigation=false;
    //! getKeyboardInputEnabled() before navigation turned it on.
    bool m_keyboardWasEnabled=false;
    //! getHighlightedItem()'s control id (0 for none): an id, not a pointer,
    //! so a deleted item is no highlight.
    size_t m_highlighted=0;
    //! The id of the control that had the keyboard focus when navigation
    //! started (0 for none), kept after it ends so a submenu closing after it
    //! finds where its own focus goes.
    size_t m_focusReturn=0;

    bool m_disableIconMargin;
    bool m_deleteOnClose;

    //! openAt() was the last placement: layout() places it from m_cursor.
    bool m_atCursor=false;
    Point m_cursor;

    //! openNextTo() was the last placement: arrange() places it on
    //! m_anchorSide of m_anchor's canvas rect, m_anchorGap off it.
    //! m_anchorRequest is its requested position at the call, which a later
    //! request replaces; m_placedFor the anchor rect it was last placed for.
    Base *m_anchor=nullptr;
    PopupPlacement m_anchorSide=PopupPlacement::Below;
    int m_anchorGap=0;
    Point m_anchorRequest;
    Rect m_placedFor;
    //! isLeftward(), set by canvasPosition().
    bool m_leftward=false;
    //! The canvas following it (Canvas::followAnchoredMenu), which has it in
    //! its anchoredMenus(); null for none.
    Canvas *m_followedBy=nullptr;

    //! Canvas::updateAnchoredMenus, for a menu \p canvas follows: false once
    //! it is no longer open next to an anchor on that canvas, which it closes
    //! when the anchor is no longer visible() there; invalidated when the
    //! anchor's canvas rect is not m_placedFor (moved with no layout pass: a
    //! pass enters the menu anyway, Canvas::enterFollowers).
    bool followAnchor(Canvas *canvas);

    //! The controls it is the context menu of (Base::setContextMenu).
    std::vector<Base *> m_contextOwners;
    //! The item it is the submenu of (MenuItem::getMenu), which owns it; null
    //! for any other menu, and once that item is being deleted.
    MenuItem *m_parentItem=nullptr;
    friend class Base;
    friend class Canvas;
    friend class MenuItem;
};


//! The rule between a Menu's items (Menu::addDivider). It is a horizontal
//! Separator, so the skin draws it with the Separator renderer: a renderer is
//! looked up by type name up the parent types, and one renderer class is
//! registered under one name.
class GWENI_EXPORT MenuDivider: public Separator
{
public:

    GWENI_CONTROL_INLINE(MenuDivider, Separator)
    {
    }
};


}

}
#endif//_gweni_controls_menu_h_
