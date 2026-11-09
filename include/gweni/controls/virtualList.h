/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_virtuallist_h_
#define _gweni_controls_virtuallist_h_

#include <gweni/controls/scrollControl.h>
#include <gweni/controls/selectable.h>

#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{

class VirtualList;

namespace internal
{

//! The row a VirtualList makes when no row factory is set: a Selectable that
//! highlights while hovered and pressed but never stays selected (a row is
//! bound to another item as the list scrolls, so a selection held by the row
//! would move with it). A press fires the list's onItemPressed.
class GWENI_EXPORT VirtualListRow: public Selectable
{
public:
    GWENI_CONTROL(VirtualListRow, Selectable);

public:
    void onPress(event::Info info) override;
    void toggle() override {}
};

}//namespace internal

//! A vertical list of fixed-height rows bound from an item count (imgui's
//! ListClipper): it holds row controls for the rows on screen plus one, a
//! pool, and binds each to the item it shows through the row binder. So a
//! list of 100 000 items has the controls, primitive ids and text ids of the
//! handful of rows it shows, and a frame costs what those rows cost.
//!
//! It is a ScrollControl with virtual content (setVirtualContentHeight): the
//! vertical bar is ranged from itemCount*rowHeight, and the rows are placed in
//! the viewport at index*rowHeight-offset, so nothing drawn is far off screen
//! however long the list is. Scrolling moves the rows and binds only the ones
//! that come into view; a row still showing its item is not bound again. A
//! row given another item gives up a press and the keyboard focus it held,
//! and once the rows moved the hover is hit-tested again
//! (Canvas::hoverMayHaveMoved), so the row under a resting mouse is the
//! hovered one and a click without a move presses the item under it.
//! A resize that changes the pool's size deletes the rows the items do not
//! reach and appends fresh ones, so a list grown and shrunk back with no
//! scroll between holds the pool a list built at that size does (issue #479).
//!
//! Rows are bound in the layout pass that places them (the Y arrange), so the
//! binder must only set the row's content (its text, colours, children), not
//! move, resize or delete it. It does not scroll sideways.
class GWENI_EXPORT VirtualList: public ScrollControl
{
public:
    GWENI_CONTROL(VirtualList, ScrollControl);

public:
    //! Makes one pooled row as a child of the given parent (the list's inner
    //! panel) and returns it. The list places it (it is made owner-placed).
    using RowFactory=std::function<Base *(Base *parent)>;
    //! Fills a row for the item at the given index.
    using RowBinder=std::function<void(Base *row, size_t index)>;

    static constexpr size_t NoItem=static_cast<size_t>(-1);

    //! The height of every row, in units (at least 1). Default 20.
    void setRowHeight(int height);
    int getRowHeight() const { return m_rowHeight; }

    //! How many items there are. It may change at any time: the rows still
    //! showing their item keep it, the offset is kept inside the new content
    //! (scrolled up when the end moved above the viewport), and rows past the
    //! last item are hidden. The content is itemCount*rowHeight units, capped
    //! at INT_MAX: items past that cannot be scrolled to.
    void setItemCount(size_t count);
    size_t getItemCount() const { return m_itemCount; }

    //! Replaces the factory (a null one gives back the default VirtualListRow);
    //! the pool is made again by the next layout.
    void setRowFactory(RowFactory factory);
    //! Replaces the binder; every row on screen is bound again.
    void setRowBinder(RowBinder binder);

    //! The items changed: every row on screen is bound again by the next
    //! layout. refreshItem() binds only the row showing that item, if any.
    void refresh();
    void refreshItem(size_t index);

    //! Scrolls as little as puts the item fully in view: to its top if it is
    //! above the viewport, to its bottom if below. It is resolved by the next
    //! layout, with the viewport that layout gives, so it may be called before
    //! the first one (an app restoring a position) or in the frame of a resize;
    //! until then getVerticalScrollOffset() is the old offset.
    void scrollToItem(size_t index);
    //! Scrolls by whole rows, down for a positive count; it drops a
    //! scrollToItem() not yet laid out.
    void scrollByRows(int rows);

    //! The item at the top of the viewport (partly scrolled out or not), as
    //! the scroll offset gives it; 0 for an empty list.
    size_t firstVisibleItem() const;
    //! The row showing the item, as the last layout bound it, or null when
    //! that item is not on screen.
    Base *rowForItem(size_t index) const;
    //! The item a pooled row shows, or NoItem (a hidden spare row).
    size_t itemForRow(const Base *row) const;
    //! The pooled rows, shown or not, in no particular order.
    std::vector<Base *> getRows() const;

    //! Rows a wheel notch scrolls. Default 3.
    void setWheelRows(int rows) { m_wheelRows=rows; }
    bool onMouseWheeled(int iDelta) override;

    //! A default row (VirtualListRow) was pressed: the item's index is the
    //! info's `integer`, the row its `control`.
    gweni::event::Caller onItemPressed;

protected:
    void contentsPlaced(skin::Base *skin) override;

private:
    struct Row
    {
        Base *control=nullptr;
        size_t item=NoItem;
        bool stale=true;   //!< bind again, even showing the same item
    };

    friend class internal::VirtualListRow;

    void updateContentHeight();
    void clearPool();
    //! The row shows another item, or none: it gives up the mouse and the focus.
    void releaseRow(Base *control);
    void rowPressed(Base *row);
    //! The pool size the viewport needs: ceil(viewport/rowHeight)+1.
    size_t poolSizeFor(int viewport) const;

    RowFactory m_factory;
    RowBinder m_binder;
    std::vector<Row> m_rows;
    size_t m_itemCount=0;
    //! The item a scrollToItem() asked for, resolved by the next layout.
    size_t m_pendingItem=NoItem;
    int m_rowHeight=20;
    int m_wheelRows=3;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_virtuallist_h_
