/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_controls_reorderable_h_
#define _gweni_controls_reorderable_h_

#include <gweni/uiTypes.h>
#include <gweni/controls/base.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace gweni
{
namespace controls
{

class Reorderable;
class ReorderGroup;

namespace internal
{

//! A reorder drag's bar in the gap the item would land in: a child of the
//! target list's panel, owner-placed by its Reorderable, numbered last, one
//! slot (the textured skin's DropIndicator surface). Takes no mouse input.
class GWENI_EXPORT DropIndicator: public controls::Base
{
public:
    GWENI_CONTROL(DropIndicator, controls::Base);

public:
    //! Its bounds in its panel, as its Reorderable places it
    void place(const Rect &bounds) { placeChild(this, bounds); }
    static bool layingOut() { return inLayoutPass(); }
};

//! The veil over a reorder drag's item where it is in the source list (the
//! DragSource look): a child of the source list's panel, owner-placed over the
//! item's rect by its Reorderable, numbered last, one slot. It is in front of
//! the item in depth, so the item shows as a placeholder: its text, drawn after
//! every slot, fails the depth test there. Placed from the
//! item's index, not attached to the row that shows it, so on a list of
//! pooled rows it is over whichever row holds the item, and hidden while the
//! item is out of view.
class GWENI_EXPORT DragSourceVeil: public controls::Base
{
public:
    GWENI_CONTROL(DragSourceVeil, controls::Base);

public:
    void place(const Rect &bounds) { placeChild(this, bounds); }
    static bool layingOut() { return inLayoutPass(); }
};

}//namespace internal

//! One reorder drop, as Reorderable::setOnItemDropped's handler gets it: the
//! item `key` of `source`, at `sourceIndex` there as the drop happens, dropped
//! on `target` so that it lands at `targetIndex`, the index it has in the
//! target once it has left the source (SortableJS's newIndex: within one list,
//! the indexes after the item are one lower). gweni moves nothing: the app
//! applies the drop to its data.
struct GWENI_EXPORT ItemDrop
{
    Reorderable *source=nullptr;
    uint64_t key=0;
    size_t sourceIndex=0;
    Reorderable *target=nullptr;
    size_t targetIndex=0;
};

//! The lists whose items may be dragged from one to another (a board's
//! columns). A plain object like SelectableGroup, not a control: whoever makes
//! it owns it. A member deleted before the group leaves it, and the group's
//! destructor releases its members, so either may go first.
class GWENI_EXPORT ReorderGroup
{
public:

    ReorderGroup()=default;
    ~ReorderGroup();

    ReorderGroup(const ReorderGroup &)=delete;
    ReorderGroup &operator=(const ReorderGroup &)=delete;

    //! Makes the list a member, leaving any other group.
    void add(Reorderable *member);
    void remove(Reorderable *member);

    const std::vector<Reorderable *> &getMembers() const { return m_members; }

private:

    std::vector<Reorderable *> m_members;
};

//! The reorder part a list owns: the drag of one of its items, keyed by the
//! app's stable key, and the drops of items on it. The list forwards its
//! Base::dragAndDrop_* hooks to it (beginDrag from its getPackage,
//! canAcceptPackage, hoverEnter, hover, hoverLeave, handleDrop, startDragging,
//! endDragging), and a subclass, the list's adapter, says what the items are:
//! their count, key(index), indexOf(key) and the gap nearer a point.
//!
//! The gap is an index 0..itemCount(), before the item of that index, found
//! from the list's geometry (gapAt), not from a hit test. The indexes the drag
//! holds (the source item's, the gap) are kept by arithmetic as the app tells
//! the list of inserts and removals (itemsInserted, itemsRemoved), so a move or
//! a scroll frame calls neither key() nor indexOf(); an edit the list cannot
//! describe (itemsChanged) looks the dragged item up again, and finds the gap
//! from the cursor again. The drag is cancelled, with no drop reported, only
//! when the dragged item itself is gone.
//!
//! The accept rules: a list takes its own items while getReorderWithin() (and
//! the adapter's reorderWithinNow(): a sorted table says no), and another
//! list's while both are in one ReorderGroup and getAcceptsFromGroup(); a
//! list that does not take the item is no target (no gap, cursor::No). A gap
//! it takes is checked against setCanDrop's function when the gap changes; a
//! refused gap shows nothing, the cursor is cursor::No and a release there
//! cancels.
//!
//! The looks are two controls in the list's panel (the control whose
//! coordinates the adapter's geometry is in: a ScrollControl's inner panel),
//! made when needed and kept as its last children, so they are
//! numbered after (drawn over) its rows: an internal::DropIndicator, a
//! IndicatorThickness bar across the panel at the shown gap's position, and an
//! internal::DragSourceVeil over the dragged item's rect while it has one.
//! The indicator is prepared as the target is entered, parked even for a
//! refused gap, so a gap first allowed during layout only moves it.
//! Each is one slot: a gap change writes the bar's slot alone, and a move that
//! keeps the gap writes nothing. The list calls placeLooks() once its items
//! have moved (its layout, a scroll), so they follow them.
class GWENI_EXPORT Reorderable
{
public:

    //! The drop indicator's height, its middle at the gap's position
    static constexpr int IndicatorThickness=2;
    //! The auto-scroll's bands, at the viewport's top and bottom (units)
    static constexpr int DefaultAutoScrollBand=24;
    //! The auto-scroll's speed at a band's outer edge (units a second); it is
    //! the depth into the band's share of it
    static constexpr float DefaultAutoScrollSpeed=600.0f;

    //! No item, no gap
    static constexpr size_t NoItem=std::numeric_limits<size_t>::max();

    //! May the item `key` of `source` land at `index` (after its removal from
    //! the source) in `target`?
    using CanDrop=std::function<bool(Reorderable *source, uint64_t key, Reorderable *target,
        size_t index)>;
    using DropHandler=std::function<void(const ItemDrop &)>;

    //! The package name of a reorder drag
    static const char *const PackageName;

    //! \p panel is the control the gaps and items are placed in, the list
    //! itself if null.
    explicit Reorderable(Base *list, Base *panel=nullptr);
    virtual ~Reorderable();

    Reorderable(const Reorderable &)=delete;
    Reorderable &operator=(const Reorderable &)=delete;

    //! The list control this is the reorder part of, the drag's source list
    Base *getList() const { return m_list; }
    //! The control the gaps and items are in, which holds the looks
    Base *getPanel() const { return m_panel; }

    //! The looks, null until the first target entry (indicator) or shown item
    //! drag (veil)
    internal::DropIndicator *getIndicator() const;
    internal::DragSourceVeil *getSourceVeil() const;
    //! Whether the looks show now: visible, and not parked (a look whose list
    //! is in the drag but shows nothing waits far above the panel, so a layout
    //! pass that shows or hides it moves it only)
    bool indicatorShown() const;
    bool veilShown() const;

    //! Places the looks again from the adapter's geometry: the list's items
    //! moved (its layout, a scroll) while it is the drag's source or target.
    //! Does nothing otherwise.
    void placeLooks();

    //! The list's layout pass has placed its items (ScrollControl::arrange
    //! calls it last): while this list is in a drag, an edit the list's
    //! cheapKeys() adapter did not describe is resolved (the tree changed,
    //! Canvas::treeSerial()), and the looks are placed again and the panel
    //! arranged again in the pass, entering only them, so the pass leaves
    //! nothing dirty. Does nothing otherwise.
    void listArranged(skin::Base *skin);

    //! Whether it takes its own items back at another place (default true)
    void setReorderWithin(bool reorder) { m_reorderWithin=reorder; }
    bool getReorderWithin() const { return m_reorderWithin; }

    //! Whether it takes the items of its group's other lists (default true)
    void setAcceptsFromGroup(bool accepts) { m_acceptsFromGroup=accepts; }
    bool getAcceptsFromGroup() const { return m_acceptsFromGroup; }

    //! The group it is a member of, or null (ReorderGroup::add/remove)
    ReorderGroup *getGroup() const { return m_group; }

    //! Auto-scroll, while this list is the drop target and it is a
    //! ScrollControl that scrolls vertically: with the cursor in the top or
    //! bottom band of its viewport (and the content not at that end), one
    //! animation scrolls it every frame on the canvas clock, by the speed times
    //! the cursor's depth into the band over its height, the gap found again at
    //! the resting cursor after each layout. It ends at the content's end, when
    //! the cursor leaves the band and when the drag ends. A band of 0 turns it
    //! off.
    void setAutoScrollBand(int units) { m_autoScrollBand=units; }
    int getAutoScrollBand() const { return m_autoScrollBand; }
    void setAutoScrollSpeed(float unitsPerSecond) { m_autoScrollSpeed=unitsPerSecond; }
    float getAutoScrollSpeed() const { return m_autoScrollSpeed; }
    //! Where the auto-scroll is going: -1 up, 1 down, 0 not scrolling
    int autoScrollDirection() const { return m_scrollDirection; }
    //! Its speed now, in units a second
    float autoScrollRate() const { return m_scrollRate; }

    //! Checked when the gap changes; none takes every gap
    void setCanDrop(CanDrop canDrop) { m_canDrop=std::move(canDrop); }

    //! Called once per drop on this list (the target's handler), not for a
    //! drop that leaves the item where it was
    void setOnItemDropped(DropHandler handler) { m_onItemDropped=std::move(handler); }

    //! Whether this list takes an item of \p source now (the rules above,
    //! without canDrop, which depends on the gap)
    bool accepts(const Reorderable *source) const;

    //! The index the item at \p sourceIndex of \p source lands at in \p target
    //! dropped in its gap \p gap: within one list a gap after the item is one
    //! lower once the item has left.
    static size_t targetIndexFor(const Reorderable *source, size_t sourceIndex,
        const Reorderable *target, size_t gap);

    //! The live drag's source, null when no reorder drag is live
    static Reorderable *dragSource();
    //! The list the cursor is over that takes the live drag's item, or null
    static Reorderable *dropTarget();
    //! The reorder part of a package, null for another drag's package
    static Reorderable *fromPackage(const draganddrop::Package *package);

    //! While this is the drag's source: the dragged item's key and its index
    //! now (NoItem otherwise)
    uint64_t dragKey() const { return m_dragKey; }
    size_t dragIndex() const { return (dragSource() == this) ? m_dragIndex : NoItem; }
    //! Whether \p key is the item being dragged from this list (the DragSource
    //! look)
    bool isDragged(uint64_t key) const { return (dragSource() == this) && (key == m_dragKey); }

    //! While this is the drop target: the gap the cursor is nearer, NoItem
    //! when it is not the target; and whether canDrop took it
    size_t gap() const { return (dropTarget() == this) ? m_gap : NoItem; }
    bool gapAllowed() const { return (dropTarget() == this) && m_gapAllowed; }
    //! A gap is shown: the target, a gap, taken
    bool showsGap() const { return gapAllowed() && (m_gap != NoItem); }

    //! The list's items changed during a drag. The list calls these: \p count
    //! items inserted at \p at, removed from \p at, or an edit it cannot
    //! describe. Nothing happens while the list is in no live drag.
    void itemsInserted(size_t at, size_t count);
    void itemsRemoved(size_t at, size_t count);
    void itemsChanged();

    // The list's drag-and-drop hooks, forwarded

    //! The package for a drag of the item at \p index, from the pressed row's
    //! or the list's dragAndDrop_getPackage; null when there is no such item
    draganddrop::Package *beginDrag(size_t index);
    //! beginDrag for the item under the canvas point (x, y), from the list's
    //! own dragAndDrop_getPackage
    draganddrop::Package *beginDragAt(int x, int y);
    bool canAcceptPackage(draganddrop::Package *package);
    void hoverEnter(draganddrop::Package *package, int x, int y);
    void hover(draganddrop::Package *package, int x, int y);
    void hoverLeave(draganddrop::Package *package);
    bool handleDrop(draganddrop::Package *package, int x, int y);
    void startDragging();
    void endDragging(bool success);

protected:

    // The adapter

    virtual size_t itemCount() const=0;
    virtual uint64_t keyAt(size_t index) const=0;
    //! The item's index, NoItem when it is gone
    virtual size_t indexOf(uint64_t key) const=0;
    //! The gap (0..itemCount()) nearer \p local, a point in the panel's
    //! coordinates, by the half-height rule
    virtual size_t gapAt(Point local) const=0;
    //! The y, in the panel, the bar of gap \p gap is centred on: the boundary
    //! between the items either side of it
    virtual int gapPosition(size_t gap) const=0;
    //! The item's rect in the panel, empty while it is not shown (a pooled
    //! list's item out of view)
    virtual Rect itemRect(size_t index) const=0;
    //! Whether it may take its own items at this moment (a sorted table: no)
    virtual bool reorderWithinNow() const { return true; }
    //! The item under \p local, a point in the panel, NoItem over none: the
    //! one a press starts dragging
    virtual size_t itemAt(Point local) const=0;
    //! Whether keyAt() and indexOf() are the list's own and cheap (a card's key
    //! is its control id): then the gap is held as the item after it, so an
    //! edit the list cannot describe (itemsChanged) keeps it before that item;
    //! otherwise it is found from the cursor again. An app's key functions are
    //! called only at a drag's start, its drop and a list change.
    virtual bool cheapKeys() const { return false; }

    // The looks

    //! The shown gap changed (showsGap(), gap()), after the indicator was
    //! placed for it
    virtual void gapChanged() {}
    //! This list's item drag began or ended, after the veil was placed for it
    virtual void dragChanged() {}

private:

    friend class ReorderGroup;

    // the gap at the cursor's last point, checked against canDrop
    void updateGap(size_t gap, bool force);
    void setGapState(size_t gap, bool allowed);
    void endAsSource();
    // the gap at the cursor, from the canvas point it was last at
    void hoverAt(Point canvasPoint);
    // the auto-scroll follows the cursor: started, its rate set, or stopped
    void updateAutoScroll();
    void stopAutoScroll();
    // one frame of it, from its animation
    void autoScrollStep();
    friend class ReorderAutoScroll;
    // the looks follow the state, then the adapter hears of it
    void gapMoved();
    void dragMoved();
    void placeIndicator();
    void placeVeil();
    // a look made and kept after every other child of the panel
    template<typename Look>
    Look *look(size_t &id);

    Base *m_list;
    Base *m_panel;
    size_t m_indicatorId=0;
    size_t m_veilId=0;
    draganddrop::Package m_package;
    ReorderGroup *m_group=nullptr;
    bool m_reorderWithin=true;
    bool m_acceptsFromGroup=true;
    CanDrop m_canDrop;
    DropHandler m_onItemDropped;

    // as the source
    uint64_t m_dragKey=0;
    size_t m_dragIndex=NoItem;
    size_t m_pendingIndex=NoItem;

    // as the target
    size_t m_gap=NoItem;
    bool m_gapAllowed=false;
    Point m_lastPoint;
    Point m_lastCanvasPoint;
    // the auto-scroll: its animation while registered, its direction, rate,
    // last step's clock and the part of a unit not yet scrolled
    int m_autoScrollBand=DefaultAutoScrollBand;
    float m_autoScrollSpeed=DefaultAutoScrollSpeed;
    class ReorderAutoScroll *m_autoScroll=nullptr;
    int m_scrollDirection=0;
    float m_scrollRate=0.0f;
    double m_scrollTime=0.0;
    double m_scrollCarry=0.0;
    // the list's scroll offset when the gap was last found from the cursor
    int m_hoverScrollOffset=0;
    // with cheapKeys(): the item the gap is before, or the end
    uint64_t m_gapKey=0;
    bool m_gapAtEnd=false;
    // the canvas's tree serial the last listArranged() saw
    unsigned m_treeSerial=0;
};

class ScrollControl;

//! The Reorderable of a column of cards: the visible children of a
//! ScrollControl's inner panel (cards docked Top, say), in their order, each
//! keyed by its control id (keyOf, cardFor). The gap is found from the cards'
//! bounds, the half-height rule on the card under the cursor; its bar is centred
//! between two cards, in the gap above the first and below the last. A card
//! pressed anywhere, a part of it included, starts its drag (once the press has
//! moved 5 units), and the app's cards added, removed or reordered during a drag
//! are found again in the list's next layout (cheapKeys(): the gap stays before
//! the card it was before); the dragged card deleted cancels the drag.
//!
//! \code
//! auto *todo=CardReorder::attach(todoColumn);
//! todo->setOnItemDropped([](const ItemDrop &drop) { ... move the card ... });
//! \endcode
class GWENI_EXPORT CardReorder: public Reorderable
{
public:
    explicit CardReorder(ScrollControl *list);

    //! Makes one for \p list, gives it to the list (setReorderable) and
    //! returns it
    static CardReorder *attach(ScrollControl *list);

    static uint64_t keyOf(const Base *card) { return const_cast<Base *>(card)->getControlId(); }
    //! The card of \p key, null once it is deleted
    static Base *cardFor(uint64_t key) { return Base::getControlById(static_cast<size_t>(key)); }

    //! The cards, in order: the panel's visible children but the looks
    std::vector<Base *> cards() const;

protected:
    size_t itemCount() const override;
    uint64_t keyAt(size_t index) const override;
    size_t indexOf(uint64_t key) const override;
    size_t gapAt(Point local) const override;
    int gapPosition(size_t gap) const override;
    Rect itemRect(size_t index) const override;
    size_t itemAt(Point local) const override;
    bool cheapKeys() const override { return true; }
};

class VirtualList;

//! The Reorderable of a VirtualList: its items, keyed by the app's own
//! functions, key(index) and indexOf(key) (NoItem once the item is gone),
//! asked only as a drag starts, as it drops and when the list changes during
//! it (setItemCount, refresh, and insertItems/removeItems, which keep the
//! dragged index and the gap by arithmetic and ask nothing). The gap and the
//! looks come from the list's geometry, index*rowHeight less the offset (in
//! variable mode its height index from the anchor), never from the pooled
//! rows: the veil is over whichever row shows the dragged item, follows it
//! as rows are bound to other items, and is hidden while it is out of view.
//! A move or a scroll frame calls neither function.
class GWENI_EXPORT VirtualListReorder: public Reorderable
{
public:
    using KeyOf=std::function<uint64_t(size_t index)>;
    using IndexOf=std::function<size_t(uint64_t key)>;

    VirtualListReorder(VirtualList *list, KeyOf keyOf, IndexOf indexOf);

    //! Makes one for \p list, gives it to the list (setReorderable) and
    //! returns it
    static VirtualListReorder *attach(VirtualList *list, KeyOf keyOf, IndexOf indexOf);

protected:
    size_t itemCount() const override;
    uint64_t keyAt(size_t index) const override;
    size_t indexOf(uint64_t key) const override;
    size_t gapAt(Point local) const override;
    int gapPosition(size_t gap) const override;
    Rect itemRect(size_t index) const override;
    size_t itemAt(Point local) const override;

private:
    // where the item's top is in the panel (the viewport), and its height
    int64_t viewTop(size_t index) const;
    int itemHeight(size_t index) const;
    // the item whose span holds the panel's y (itemCount() past the last)
    size_t itemAtY(int y) const;

    VirtualList *m_virtualList;
    KeyOf m_keyOf;
    IndexOf m_indexOf;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_reorderable_h_
