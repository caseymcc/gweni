/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_virtuallist_h_
#define _gweni_controls_virtuallist_h_

#include <gweni/controls/heightIndex.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/selectable.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gweni
{
namespace anim
{
class Animation;
}

namespace controls
{

class VirtualList;

namespace internal
{

class VirtualListRemeasure;

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
//!
//! Variable mode: a list given an item measurer (setItemMeasurer) gives each
//! item its own height, the measurer's at the rows' width (wrapped text: see
//! Base::wrappedTextHeight), kept in a HeightIndex with the generation of
//! the width it was measured at. The content is the sum of the heights, and
//! an item not measured yet counts at the estimator's height, or the last
//! measured one, or the row height. The scroll position is an anchor: the
//! item at the viewport's top and the offset into it. Each layout measures
//! the items on screen from the anchor down (and up from the end when the end
//! is in view), places them one below the other from it, and works the
//! offset out from the anchor through the index; a bar, wheel or
//! setVerticalScrollOffset() move since the last layout anchors at the new
//! offset, and one to the end at the end (the last item's bottom at the
//! viewport's, as measured). So an item above the anchor changing height moves the offset and
//! the thumb, not the rows on screen. The pool is the viewport at the
//! smallest height measured: ceil(viewport/minHeight)+1 rows. The vertical
//! bar always shows (disabled while everything fits), so the width the items
//! are measured at never depends on their heights and a layout is one pass.
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
    //! The height of the item at the given index for a row of the given
    //! width, in units (at least 1 is used). The measurer must give the
    //! height the bound row lays out at; the estimator what it is likely to
    //! be, for an item not measured yet. Both are called during layout.
    using ItemMeasurer=std::function<int(size_t index, int width)>;
    using ItemEstimator=std::function<int(size_t index, int width)>;

    static constexpr size_t NoItem=static_cast<size_t>(-1);

    //! The height of every row, in units (at least 1). Default 20. In
    //! variable mode the height of an item added before any was measured,
    //! with no estimator, and the wheel's and the bar's step.
    void setRowHeight(int height);
    int getRowHeight() const { return m_rowHeight; }

    //! How many items there are. It may change at any time: the rows still
    //! showing their item keep it, the offset is kept inside the new content
    //! (scrolled up when the end moved above the viewport), and rows past the
    //! last item are hidden. The content is itemCount*rowHeight units, capped
    //! at INT_MAX: items past that cannot be scrolled to.
    void setItemCount(size_t count);
    size_t getItemCount() const { return m_itemCount; }
    //! Adds count items before item at (at == getItemCount() appends), or
    //! removes the count items from at on (clipped to the end). A row
    //! showing an item that moved shows it at its new index without being
    //! bound again, and one whose item was removed is hidden or given another.
    //! The item at the viewport's top stays there with the rows below it,
    //! however many items were added or removed above it (one removed takes
    //! the item after the removed ones there), the top being where the bar
    //! is now, moved since the last layout or not; a list stuck to its end
    //! keeps its end. O(log N) in the item count, plus the items added or removed
    //! (in variable mode the estimator is asked for each one added).
    void insertItems(size_t at, size_t count);
    void removeItems(size_t at, size_t count);
    //! The item's content changed: the row showing it is bound again by the
    //! next layout, and in variable mode the item is measured again now (by
    //! the next layout if none has given a width yet), so a new height above
    //! the viewport's top moves the offset and not the rows on screen.
    void itemChanged(size_t index);

    //! Stuck, every layout puts the last item's bottom at the viewport's
    //! bottom (the first item at its top when they all fit), through
    //! appends, removals and new heights, until unstuck. Only the app sticks
    //! and unsticks it: a scroll does not (a stuck list scrolled by the user
    //! goes back to its end). Unstuck, it stays where it is.
    void setStickToEnd(bool stick);
    bool isStuckToEnd() const { return m_stuckToEnd; }

    //! Variable mode with a measurer, fixed mode (every row getRowHeight())
    //! with a null one. Turning it on counts every item at the default height
    //! (the estimator's, else the row height) and measures the ones on screen
    //! by the next layout; it keeps the vertical bar shown (setAutoHideBars,
    //! given back by turning it off).
    void setItemMeasurer(ItemMeasurer measurer);
    bool isVariableHeight() const { return static_cast<bool>(m_measurer); }
    //! The height an item added from now on counts at until it is measured
    //! (items already there keep theirs). Null: the last measured height, or
    //! the row height before any.
    void setItemEstimator(ItemEstimator estimator);
    //! In variable mode, the index of the items' heights (for tests and
    //! tools); empty in fixed mode.
    const HeightIndex &getHeightIndex() const { return m_heights; }
    //! The width the items are measured at: the rows' width the last layout
    //! gave (-1 before one, or in fixed mode).
    int getMeasureWidth() const { return m_measureWidth; }
    //! The generation the items on screen are measured at; it goes up by one
    //! with each new width and each new text layout (the renderer's
    //! textLayoutSerial(), a runtime text AA switch), not with a canvas scale.
    uint32_t getMeasureGeneration() const { return m_generation; }
    //! Items measured a frame, in the update phase, while any is not measured
    //! at this generation (a new width, items added or never shown): from
    //! the item at the top down, then from the first, until every item is.
    //! Each frame's new heights move the offset and the thumb, not the rows
    //! on screen, and once every item is measured nothing runs. 0 measures
    //! only what shows. Default 64.
    void setRemeasureBudget(size_t items);
    size_t getRemeasureBudget() const { return m_remeasureBudget; }
    //! Whether that idle re-measure runs (an animation is registered).
    bool isRemeasuring() const { return m_remeasure != nullptr; }

    //! Replaces the factory (a null one gives back the default VirtualListRow);
    //! the pool is made again by the next layout.
    void setRowFactory(RowFactory factory);
    //! Replaces the binder; every row on screen is bound again (and in
    //! variable mode every item measured again, as refresh() does).
    void setRowBinder(RowBinder binder);

    //! The items changed: every row on screen is bound again by the next
    //! layout. In variable mode every item is measured again too, as for a
    //! new width (one generation up): the items on screen by that layout, the
    //! rest by the idle re-measure. refreshItem() binds only the row showing
    //! that item, if any (itemChanged() also measures it again).
    void refresh();
    void refreshItem(size_t index);

    //! Scrolls as little as puts the item fully in view: to its top if it is
    //! above the viewport, to its bottom if below; an item at least as tall
    //! as the viewport (variable mode) to its top, so calling it again on the
    //! item changes nothing. It is resolved by the next
    //! layout, with the viewport that layout gives, so it may be called before
    //! the first one (an app restoring a position) or in the frame of a resize;
    //! until then getVerticalScrollOffset() is the old offset.
    void scrollToItem(size_t index);
    //! scrollToItem() for the part of the item \p partHeight units high
    //! \p partTop units below its top (a wrapped line's visual row): an item
    //! that fits the viewport is put fully in view as scrollToItem() puts it,
    //! one at least as tall as the viewport only as far as puts that part in
    //! view (its top at the viewport's top where the part is above it, its
    //! bottom at the viewport's bottom where below it, nothing where in view).
    //! In fixed mode it is scrollToItem().
    void scrollToItem(size_t index, int partTop, int partHeight);
    //! Scrolls by whole rows, down for a positive count; it drops a
    //! scrollToItem() not yet laid out. In variable mode it moves the anchor
    //! by that many items.
    void scrollByRows(int rows);
    //! The item of the scrollToItem() the next layout is to resolve, or
    //! NoItem: none asked for, or a later request dropped it (scrollByRows(),
    //! an offset, a wheel notch, a bar move).
    size_t getPendingItem() const { return m_pendingItem; }
    //! Counts the scroll requests made of the list: each scrollToItem() and
    //! scrollByRows() call, each setVerticalScrollOffset() that moves the
    //! offset or replaces a pending item, each wheel notch it takes and each
    //! move of its bar by the user (the thumb, the arrows, the track). The
    //! latest of them in a frame decides the view: an offset, a notch or a
    //! bar move drops a scrollToItem() still pending, as scrollByRows()
    //! does. An edit keeping the rows on screen where they are
    //! (insertItems() above them, say) is not one, nor is a layout. So a
    //! caller that recorded it after its own request knows, while it is
    //! unchanged, that no later one replaced it (CodeEditor's caret).
    uint64_t scrollRequestSerial() const { return m_scrollRequests; }
    //! Counted in scrollRequestSerial(), and drops a pending item.
    void setVerticalScrollOffset(int offset) override;

    //! The item at the top of the viewport (partly scrolled out or not), as
    //! the scroll offset gives it; 0 for an empty list.
    size_t firstVisibleItem() const;
    //! In variable mode, the anchor: the item at the viewport's top and how
    //! many units of it are scrolled out above, as the last layout (or the
    //! last edit) left them.
    size_t getAnchorItem() const { return m_anchorItem; }
    int getAnchorOffset() const { return m_anchorOffset; }
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

    //! In variable mode, a shown row whose laid-out height (its bounds, or
    //! the height it asks for itself) is not its item's measure: the
    //! measurer and the row disagree.
    void verifyOwnLayout(std::vector<std::string> &problems) override;

    //! A default row (VirtualListRow) was pressed: the item's index is the
    //! info's `integer`, the row its `control`.
    gweni::event::Caller onItemPressed;

    //! The Y arrange takes a bar the user moved first (takeBarScroll()).
    void arrange(skin::Base *skin, Dim dim) override;

protected:
    void contentsPlaced(skin::Base *skin) override;

    //! The pooled rows in pool order and the item each shows now (NoItem for a
    //! spare), an edit's shift included: for a subclass whose rows keep what
    //! they show in step with the items (CodeEditor's lines).
    size_t poolSize() const { return m_rows.size(); }
    Base *poolRow(size_t i) const { return m_rows[i].control; }
    size_t poolItem(size_t i) const { return m_rows[i].item; }

    //! A bar the user moved since the last layout (its thumb, its arrows, its
    //! track) becomes the scroll offset, at the content it was moved against,
    //! and barScrollTaken() is told. Whatever needs the offset first takes
    //! it: an edit, a scroll request, the idle re-measure or the layout.
    void takeBarScroll();
    //! The user's move of the vertical bar was just taken as the offset,
    //! `amount` the bar's scrolled amount (1 at the end), wherever it was
    //! taken (takeBarScroll()). Nothing by default.
    virtual void barScrollTaken(float /*amount*/) {}

private:
    struct Row
    {
        Base *control=nullptr;
        size_t item=NoItem;
        bool stale=true;   //!< bind again, even showing the same item
    };

    friend class internal::VirtualListRow;
    friend class internal::VirtualListRemeasure;

    void updateContentHeight();
    void clearPool();
    //! Variable mode's part of contentsPlaced: resolves the scroll requests
    //! into the anchor, measures the items on screen and gives the items in
    //! view, [first, end), their tops in the viewport (m_viewTops). Sets the
    //! content height, the bar and the offset from the anchor.
    void placeVariable(size_t &first, size_t &end);
    //! The height a new item at index counts at until measured.
    int defaultHeight(size_t index);
    //! Item index's height at m_measureWidth, measured if it was not at this
    //! generation.
    int measureItem(size_t index);
    //! Anchors so that item's bottom is at the viewport's bottom, measuring
    //! the items above it that show; at the top when they all fit.
    void anchorAbove(size_t item, int viewport);
    //! A scroll offset set since the last layout (the bar, the wheel,
    //! setVerticalScrollOffset) becomes the anchor.
    void anchorAtOffset();
    //! Sets the scroll offset from the anchor, at the last layout's viewport
    //! (after an edit outside layout).
    void offsetFromAnchor();
    //! Registers the idle re-measure if an item is not measured at this
    //! generation and it does not run yet; stops it otherwise.
    void updateRemeasure();
    //! One frame of the idle re-measure: up to the budget's items measured.
    //! Whether every item is measured now.
    bool remeasureStep();
    //! The row shows another item, or none: it gives up the mouse and the focus.
    void releaseRow(Base *control);
    // an edit the list cannot describe to a reorder drag: it looks its item up
    void reorderItemsChanged();
    void rowPressed(Base *row);
    //! The pooled rows' items, and a pending scrollToItem, after count items
    //! were inserted at at (count > 0) or removed from it (count < 0).
    void shiftItems(size_t at, int64_t count);
    //! The pool size the viewport needs: ceil(viewport/rowHeight)+1.
    size_t poolSizeFor(int viewport) const;

    RowFactory m_factory;
    RowBinder m_binder;
    std::vector<Row> m_rows;
    size_t m_itemCount=0;
    //! The item a scrollToItem() asked for, resolved by the next layout.
    size_t m_pendingItem=NoItem;
    uint64_t m_scrollRequests=0;
    //! The part of it to show, a height of -1 for the whole item.
    int m_pendingPartTop=0;
    int m_pendingPartHeight=-1;
    int m_rowHeight=20;
    int m_wheelRows=3;
    bool m_stuckToEnd=false;
    //! An edit hid or took the item of a shown row: the next layout tells the
    //! canvas the hover may have moved.
    bool m_rowsEdited=false;

    // Variable mode
    ItemMeasurer m_measurer;
    ItemEstimator m_estimator;
    HeightIndex m_heights;
    //! The generation of m_measureWidth; an item stamped with an older one
    //! (0: never measured) is measured again when it shows.
    uint32_t m_generation=1;
    int m_measureWidth=-1;
    //! The renderer and its textLayoutSerial() the heights were measured
    //! under: another one wraps the text otherwise (a runtime text AA
    //! switch), so it is a new generation too, as a new width is.
    const renderer::Base *m_measureRenderer=nullptr;
    unsigned m_measureSerial=0;
    //! The smallest and the last height measured (0: none yet).
    int m_minHeight=0;
    int m_lastMeasured=0;
    size_t m_anchorItem=0;
    int m_anchorOffset=0;
    //! The offset the anchor gave when m_scrollOffset[1] was last set from it:
    //! any other offset was set by a scroll since.
    int m_anchoredOffset=0;
    //! The offset was scrolled to its end since the last layout: the next one
    //! puts the last item's bottom at the viewport's bottom.
    bool m_anchorToEnd=false;
    //! The tops of the items in view in the viewport, from the first.
    std::vector<int> m_viewTops;
    size_t m_remeasureBudget=64;
    //! Where the idle re-measure goes on from.
    size_t m_remeasureCursor=0;
    anim::Animation *m_remeasure=nullptr;
    bool m_autoHideBefore=true;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_virtuallist_h_
