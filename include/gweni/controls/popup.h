/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_popup_h_
#define _gweni_controls_popup_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{

//! Where a Popup goes next to its anchor.
enum class PopupPlacement
{
    //! Below the anchor, left edges aligned; above it where it would run past
    //! the bottom, right edges aligned where it would run past the right edge.
    Below,
    //! Right of the anchor, top edges aligned; on its left where it would run
    //! past the right edge, bottom edges aligned where it would run past the
    //! bottom.
    Right
};

//! A panel the canvas shows over everything else next to an anchor control
//! (imgui's BeginPopup): a colour picker under its swatch, a list of tabs
//! under its button. Its content is its children, added like any control's;
//! it is as big as they are (or as its requested size), and paints a panel
//! behind them.
//!
//! A canvas shows one popup at a time, and its context menu
//! (Base::setContextMenu) is one too: opening either closes the one shown
//! (Canvas::shownPopup()). In each layout pass it is placed by placement()
//! next to the anchor's canvas rect, flipped and kept inside the canvas, so
//! it follows the anchor and a canvas that shrinks or grows again. It is
//! closed by close(), setHidden(true), a press of either button outside it
//! and its anchor (and outside an open Menu, which a ComboBox in it may have
//! opened), Escape, its anchor hidden, disabled or deleted, and its own
//! deletion. A press on the anchor is left to the anchor, so a button can
//! toggle the popup it opened.
//!
//! Ownership: a popup is its creator's. Opening it moves it onto the canvas
//! (as a context menu set from outside is), and the canvas deletes it with
//! its other children; closing it only hides it.
//!
//! While open it is kept after every other child of the canvas but the open
//! Menus (one a ComboBox in it opened draws over it) and the ToolTip box, so
//! the z renumber puts it above the rest of the tree and a tooltip still
//! draws over it.
class GWENI_EXPORT Popup: public Base
{
public:

    GWENI_CONTROL(Popup, Base);

public:

    //! The logical units between the anchor and the popup.
    static constexpr int Gap=2;

    //! Shows it next to \p anchor, closing the popup or context menu the
    //! canvas showed before; onOpen fires once it is shown. Opening it again
    //! for another anchor moves it there (onClose, then onOpen). A null or
    //! canvas-less anchor opens nothing.
    virtual void open(Base *anchor, PopupPlacement side=PopupPlacement::Below);
    //! Hides it, as setHidden(true) does.
    virtual void close();
    //! It is the canvas's shown popup.
    bool isOpen();
    //! The control it is open next to, or null.
    Base *getAnchor() const { return m_anchor; }
    PopupPlacement getPlacement() const { return m_side; }

    //! Hiding an open popup closes it (onClose).
    void setHidden(bool hidden) override;

    //! Sizes itself (its preferred size, or its request, by its size flags)
    //! and places itself next to the anchor, one axis per arrange: it is
    //! owner-placed (Placement::Owner), so the canvas leaves both to it.
    void arrange(skin::Base *skin, Dim dim) override;

    //! Fired when it opens and when it closes, however it closes, but not
    //! when it is deleted.
    gweni::event::Caller onOpen;
    gweni::event::Caller onClose;

    //! Where a \p size popup goes next to \p anchor (both in canvas
    //! coordinates) inside \p area, \p gap units off it: see PopupPlacement,
    //! then kept inside the area (Menu::keepInside). A zero-size anchor is a
    //! point: Menu::placement is this at the cursor with no gap.
    static Point placement(const Rect &anchor, const Point &size, const Rect &area,
        PopupPlacement side, int gap=Gap);

    //! The anchor's canvas rect, a zero-size one at the origin with no anchor.
    Rect anchorRect() const;

    //! Canvas: the anchor's rect is not the one it was last placed by (the
    //! anchor or one of its ancestors moved): it is laid out again.
    void followAnchor();

protected:

    friend class Canvas;
    //! Canvas: the anchor it is open next to, or null once it closed.
    void setAnchor(Base *anchor, PopupPlacement side);

    Base *m_anchor=nullptr;
    PopupPlacement m_side=PopupPlacement::Below;
    //! The anchor rect arrange() last placed it by.
    Rect m_placedFor;
};

}
}
#endif//_gweni_controls_popup_h_
