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
    //! Clears it from every control it is the context menu of.
    ~Menu() override;

public:
    //void render(skin::Base *skin) override;
    void renderUnder(skin::Base *skin) override;

    void calculateSize(skin::Base *skin, Dim dim) override;
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
    virtual void close();

    void setHidden(bool hidden) override;

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
    //! Where layout() last put it, so a position requested since is told apart.
    Point m_placed;

    //! The controls it is the context menu of (Base::setContextMenu).
    std::vector<Base *> m_contextOwners;
    friend class Base;
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
