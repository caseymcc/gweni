/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_controls_reorderable_h_
#define _gweni_controls_reorderable_h_

#include <gweni/uiTypes.h>
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

class Base;
class Reorderable;
class ReorderGroup;

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
class GWENI_EXPORT Reorderable
{
public:

    //! No item, no gap
    static constexpr size_t NoItem=std::numeric_limits<size_t>::max();

    //! May the item `key` of `source` land at `index` (after its removal from
    //! the source) in `target`?
    using CanDrop=std::function<bool(Reorderable *source, uint64_t key, Reorderable *target,
        size_t index)>;
    using DropHandler=std::function<void(const ItemDrop &)>;

    //! The package name of a reorder drag
    static const char *const PackageName;

    explicit Reorderable(Base *list);
    virtual ~Reorderable();

    Reorderable(const Reorderable &)=delete;
    Reorderable &operator=(const Reorderable &)=delete;

    //! The list control this is the reorder part of, the drag's source list
    Base *getList() const { return m_list; }

    //! Whether it takes its own items back at another place (default true)
    void setReorderWithin(bool reorder) { m_reorderWithin=reorder; }
    bool getReorderWithin() const { return m_reorderWithin; }

    //! Whether it takes the items of its group's other lists (default true)
    void setAcceptsFromGroup(bool accepts) { m_acceptsFromGroup=accepts; }
    bool getAcceptsFromGroup() const { return m_acceptsFromGroup; }

    //! The group it is a member of, or null (ReorderGroup::add/remove)
    ReorderGroup *getGroup() const { return m_group; }

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
    //! The gap (0..itemCount()) nearer \p local, a point in the list's own
    //! coordinates, by the half-height rule
    virtual size_t gapAt(Point local) const=0;
    //! Whether it may take its own items at this moment (a sorted table: no)
    virtual bool reorderWithinNow() const { return true; }

    // The looks

    //! The shown gap changed (showsGap(), gap()): the indicator follows it
    virtual void gapChanged() {}
    //! This list's item drag began or ended: the dragged item's look follows it
    virtual void dragChanged() {}

private:

    friend class ReorderGroup;

    // the gap at the cursor's last point, checked against canDrop
    void updateGap(size_t gap, bool force);
    void setGapState(size_t gap, bool allowed);
    void endAsSource();

    Base *m_list;
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
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_reorderable_h_
