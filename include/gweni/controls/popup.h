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

//! Where a Popup goes next to its anchor (Popup::placement), and a Menu next
//! to its item (Menu::openNextTo). A submenu opened Right of an item in a
//! menu that went left (Menu::isLeftward()) is placed as Left, so a chain
//! that flipped at the canvas's right edge goes on opening left.
enum class PopupPlacement
{
    //! Below the anchor, left edges aligned; above it where it would run past
    //! the bottom, right edges aligned where it would run past the right edge.
    Below,
    //! Right of the anchor, top edges aligned; on its left where it would run
    //! past the right edge, bottom edges aligned where it would run past the
    //! bottom.
    Right,
    //! Left of the anchor, top edges aligned: Right mirrored. On its right
    //! where it would run past the left edge, bottom edges aligned where it
    //! would run past the bottom. Popup::open(anchor, Left) puts a popup there.
    Left
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
//! and its anchor (and outside a Menu opened over it, a ComboBox's list in
//! it; a menu bar, a MenuStrip, is outside it like any control), Escape, its
//! anchor hidden, disabled or deleted, and its own deletion. A press on the anchor is left to the anchor, so a button can
//! toggle the popup it opened.
//!
//! Ownership: a popup is a child of the canvas (opening one made elsewhere
//! moves it there), so the canvas deletes it with its other children, and
//! closing it only hides it. It is not a child of the control that made it,
//! so a control that makes one owns it instead: Base::createPopup() makes
//! one on the control's canvas owned by that control, and setOwner() makes
//! any popup a control's. Deleting the owner closes the popup if it is open
//! and deletes it on the popup's own canvas (delayed, as this can run from
//! an event of one of its controls; outright with no canvas; not at all
//! while that canvas is deleting its children, which deletes it then),
//! wherever the owner is, on that canvas or taken off it; deleting the
//! popup first takes it off its owner. So a control that makes a popup has
//! no cleanup of its own to do (ColorEdit, TabControl, PopupDemo).
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

    //! Takes itself off its owner's list.
    ~Popup() override;

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

    //! Makes it \p owner's: deleting \p owner closes it if it is open and
    //! deletes it, on the popup's own canvas, wherever \p owner is then
    //! (Base::createPopup() makes one owned). It moves off the list of the
    //! control that owned it before; null leaves it owned by none.
    void setOwner(Base *owner);
    //! The control that owns it, or null.
    Base *getOwner() const { return m_owner; }

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
    //!
    //! \p leftward, when given, is set to whether it went left of where its
    //! side starts it, decided by the side and its flip before it is kept
    //! inside the area: a Right flipped to the anchor's left, a Left that
    //! stayed there, a Below right-aligned with the anchor.
    static Point placement(const Rect &anchor, const Point &size, const Rect &area,
        PopupPlacement side, int gap=Gap, bool *leftward=nullptr);

    //! The anchor's canvas rect, a zero-size one at the origin with no anchor.
    Rect anchorRect() const;

    //! Canvas, before the frame's layout: the anchor's rect is not the one it
    //! was last placed by (moved with no layout pass, by a ZoomPanel's pan or
    //! zoom): it is laid out again. A pass that moves the anchor enters the
    //! popup anyway (Canvas::enterFollowers).
    void followAnchor();

protected:

    friend class Canvas;
    //! Canvas: the anchor it is open next to, or null once it closed.
    void setAnchor(Base *anchor, PopupPlacement side);

    Base *m_anchor=nullptr;
    PopupPlacement m_side=PopupPlacement::Below;
    //! The anchor rect arrange() last placed it by.
    Rect m_placedFor;

    friend class Base;
    Base *m_owner=nullptr;
    //! The next popup on m_owner's list (Base::m_ownedPopups).
    Popup *m_nextOwned=nullptr;
};

}
}
#endif//_gweni_controls_popup_h_
