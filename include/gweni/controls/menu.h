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
    virtual void openAt(const Point &cursor);
    //! Shows it on the canvas and brings it to the front next to \p anchor (the
    //! MenuItem it is the menu of): on \p side of the anchor's canvas rect,
    //! \p gap units off it, flipped and kept inside the canvas by
    //! Popup::placement in every layout pass, so it opens where the anchor is
    //! laid out, not where it was at the call. From the first call on it is
    //! self-placed (Placement::Owner) on the canvas. A setPos()/setBounds()
    //! after it places it there instead, kept inside the canvas; hiding it or
    //! openAt() forgets the anchor. The anchor must outlive its being open.
    virtual void openNextTo(Base *anchor, PopupPlacement side, int gap);
    //! What openNextTo() placed it next to, while it does; null otherwise.
    Base *getAnchor() const
    {
        return m_anchor;
    }
    virtual void close();

    void setHidden(bool hidden) override;

    //! Called when it is shown or hidden (setHidden() with a new state), so
    //! the MenuItem it is the submenu of redraws its open highlight.
    gweni::event::Caller onVisibilityChanged;

    //! Where a menu of \p size opened at \p cursor goes in \p area: at the
    //! cursor, on its left where it would run past the right edge, above it
    //! where it would run past the bottom, then kept inside (keepInside()):
    //! against the left/top edge where it fits on neither side.
    static Point placement(const Point &cursor, const Point &size, const Rect &area);
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

    virtual bool shouldHoverOpenMenu()
    {
        return true;
    }

    virtual void onAddItem(MenuItem *item);

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

    //! The controls it is the context menu of (Base::setContextMenu).
    std::vector<Base *> m_contextOwners;
    //! The item it is the submenu of (MenuItem::getMenu), which owns it; null
    //! for any other menu, and once that item is being deleted.
    MenuItem *m_parentItem=nullptr;
    friend class Base;
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
