/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/controls/reorderable.h>
#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/dragAndDrop.h>

#include <algorithm>
#include <memory>

namespace gweni
{
namespace controls
{

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(DropIndicator)
{
    setPlacement(Placement::Owner);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
}

GWENI_CONTROL_CONSTRUCTOR(DragSourceVeil)
{
    setPlacement(Placement::Owner);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
}

}//namespace internal

const char *const Reorderable::PackageName="gweni:ReorderItem";

// One drag at a time: its source and the list showing its gap.
static Reorderable *s_source=nullptr;
static Reorderable *s_target=nullptr;

// The tree serial of \p list's canvas, 0 off a canvas
static unsigned treeSerialOf(Base *list)
{
    Canvas *canvas=list->getCanvas();
    return canvas ? canvas->treeSerial() : 0;
}

ReorderGroup::~ReorderGroup()
{
    for(Reorderable *member : m_members)
        member->m_group=nullptr;
}

void ReorderGroup::add(Reorderable *member)
{
    if(!member || member->m_group == this)
        return;

    if(member->m_group)
        member->m_group->remove(member);

    m_members.push_back(member);
    member->m_group=this;
}

void ReorderGroup::remove(Reorderable *member)
{
    if(!member || member->m_group != this)
        return;

    m_members.erase(std::find(m_members.begin(), m_members.end(), member));
    member->m_group=nullptr;
}

Reorderable::Reorderable(Base *list, Base *panel):
    m_list(list),
    m_panel(panel ? panel : list)
{
    m_package.name=PackageName;
    m_package.draggable=true;
    m_package.sourceList=list;
    m_package.userdata=this;
}

Reorderable::~Reorderable()
{
    if(m_group)
        m_group->remove(this);

    if(s_target == this)
        s_target=nullptr;

    // The source goes: the drag ends with nothing reported. The hooks the
    // cancel forwards to this find it in no drag and do nothing.
    if(s_source == this)
    {
        s_source=nullptr;

        if(draganddrop::CurrentPackage == &m_package)
            draganddrop::cancel();
    }

    delete getIndicator();
    delete getSourceVeil();
}

internal::DropIndicator *Reorderable::getIndicator() const
{
    return gweni_cast<internal::DropIndicator>(Base::getControlById(m_indicatorId));
}

internal::DragSourceVeil *Reorderable::getSourceVeil() const
{
    return gweni_cast<internal::DragSourceVeil>(Base::getControlById(m_veilId));
}

template<typename Look>
Look *Reorderable::look(size_t &id)
{
    Look *control=gweni_cast<Look>(Base::getControlById(id));

    // made the first time it shows (again, if the app cleared the panel)
    if(!control || (control->getActualParent() != m_panel))
    {
        control=m_panel->newChild<Look>();
        id=control->getControlId();
    }

    // Kept after the panel's items, so numbered after them, the indicator
    // after the veil: reordered only when something was added after them.
    Base *tail[2]={nullptr, nullptr};
    size_t count=0;

    if(internal::DragSourceVeil *veil=getSourceVeil())
        tail[count++]=veil;
    if(internal::DropIndicator *indicator=getIndicator())
        tail[count++]=indicator;

    const Base::List &children=m_panel->getChildren();
    bool inPlace=(children.size() >= count);
    auto at=children.rbegin();

    for(size_t i=count; inPlace && (i > 0); --i, ++at)
        inPlace=(*at == tail[i-1]);

    if(!inPlace)
    {
        for(size_t i=0; i < count; ++i)
            tail[i]->bringToFront();
    }

    // the looks' own changes to the tree are no edit of the items
    m_treeSerial=treeSerialOf(m_list);
    return control;
}

void Reorderable::listArranged(skin::Base *skin)
{
    if((s_source != this) && (s_target != this))
        return;

    if(cheapKeys())
    {
        const unsigned serial=treeSerialOf(m_list);

        if(serial != m_treeSerial)
        {
            m_treeSerial=serial;
            itemsChanged();

            // the dragged item was gone: the drag is over
            if((s_source != this) && (s_target != this))
            {
                placeLooks();
                return;
            }
        }
    }

    placeLooks();

    // Placed after the panel was arranged, which marked the way down to them:
    // the panel is arranged again in this pass, entering only them (its items
    // are clean), so the pass ends with nothing left dirty.
    if(m_panel != m_list)
        m_panel->doArrange(skin, Dim::Y);
}

void Reorderable::placeLooks()
{
    if((s_target == this) || getIndicator())
        placeIndicator();
    if((s_source == this) || getSourceVeil())
        placeVeil();
}

void Reorderable::placeIndicator()
{
    if(!showsGap())
    {
        if(internal::DropIndicator *indicator=getIndicator())
            indicator->setHidden(true);
        return;
    }

    internal::DropIndicator *indicator=look<internal::DropIndicator>(m_indicatorId);

    indicator->place(Rect(0, gapPosition(m_gap)-IndicatorThickness/2, m_panel->getWidth(),
        IndicatorThickness));
    indicator->setHidden(false);
}

void Reorderable::placeVeil()
{
    Rect rect;

    if((s_source == this) && (m_dragIndex != NoItem))
        rect=itemRect(m_dragIndex);

    if((rect.w <= 0) || (rect.h <= 0))
    {
        if(internal::DragSourceVeil *veil=getSourceVeil())
            veil->setHidden(true);
        return;
    }

    internal::DragSourceVeil *veil=look<internal::DragSourceVeil>(m_veilId);

    veil->place(rect);
    veil->setHidden(false);
}

void Reorderable::gapMoved()
{
    placeIndicator();
    gapChanged();
}

void Reorderable::dragMoved()
{
    placeVeil();
    dragChanged();
}

bool Reorderable::accepts(const Reorderable *source) const
{
    if(!source)
        return false;

    if(source == this)
        return m_reorderWithin && reorderWithinNow();

    return m_acceptsFromGroup && m_group && (source->m_group == m_group);
}

size_t Reorderable::targetIndexFor(const Reorderable *source, size_t sourceIndex,
    const Reorderable *target, size_t gap)
{
    if(gap == NoItem)
        return NoItem;

    // within one list the items after the dragged one move up once it has left
    if((source == target) && (sourceIndex != NoItem) && (gap > sourceIndex))
        return gap-1;

    return gap;
}

Reorderable *Reorderable::dragSource()
{
    return s_source;
}

Reorderable *Reorderable::dropTarget()
{
    return s_target;
}

Reorderable *Reorderable::fromPackage(const draganddrop::Package *package)
{
    if(!package || (package->name != PackageName))
        return nullptr;

    return static_cast<Reorderable *>(package->userdata);
}

draganddrop::Package *Reorderable::beginDragAt(int x, int y)
{
    return beginDrag(itemAt(m_panel->canvasPosToLocal(Point(x, y))));
}

draganddrop::Package *Reorderable::beginDrag(size_t index)
{
    if(index >= itemCount())
        return nullptr;

    m_package.key=keyAt(index);
    m_pendingIndex=index;
    return &m_package;
}

bool Reorderable::canAcceptPackage(draganddrop::Package *package)
{
    return accepts(fromPackage(package));
}

void Reorderable::startDragging()
{
    s_source=this;
    m_dragKey=m_package.key;
    m_dragIndex=m_pendingIndex;
    m_pendingIndex=NoItem;
    m_treeSerial=treeSerialOf(m_list);
    dragMoved();
}

void Reorderable::endDragging(bool /*success*/)
{
    if(s_source == this)
        endAsSource();
}

void Reorderable::endAsSource()
{
    s_source=nullptr;
    m_dragIndex=NoItem;

    // a target the core did not tell (it always does) shows nothing more
    if(Reorderable *target=s_target)
    {
        s_target=nullptr;
        target->gapMoved();
    }

    dragMoved();
}

void Reorderable::hoverEnter(draganddrop::Package *package, int x, int y)
{
    s_target=this;
    m_gap=NoItem;
    m_gapAllowed=false;
    m_treeSerial=treeSerialOf(m_list);
    hover(package, x, y);
}

void Reorderable::hover(draganddrop::Package * /*package*/, int x, int y)
{
    if(s_target != this)
        return;

    m_lastPoint=m_panel->canvasPosToLocal(Point(x, y));
    updateGap(gapAt(m_lastPoint), false);
}

void Reorderable::hoverLeave(draganddrop::Package * /*package*/)
{
    if(s_target != this)
        return;

    // the gap is kept for handleDrop, which the core calls after the leave;
    // it is no longer shown
    s_target=nullptr;
    gapMoved();
}

void Reorderable::updateGap(size_t gap, bool force)
{
    if(force || (gap != m_gap))
    {
        bool allowed=(gap != NoItem) && s_source;

        if(allowed && m_canDrop)
        {
            allowed=m_canDrop(s_source, s_source->m_dragKey, this,
                targetIndexFor(s_source, s_source->m_dragIndex, this, gap));
        }
        setGapState(gap, allowed);
    }

    draganddrop::setDropRefused(!m_gapAllowed);
}

void Reorderable::setGapState(size_t gap, bool allowed)
{
    if((gap == m_gap) && (allowed == m_gapAllowed))
        return;

    m_gap=gap;
    m_gapAllowed=allowed;

    // held as the item after it, for an edit the list cannot describe
    if(cheapKeys() && (gap != NoItem))
    {
        m_gapAtEnd=(gap >= itemCount());
        m_gapKey=m_gapAtEnd ? 0 : keyAt(gap);
    }
    gapMoved();
}

void Reorderable::itemsInserted(size_t at, size_t count)
{
    if(count == 0)
        return;

    if((s_source == this) && (m_dragIndex != NoItem) && (at <= m_dragIndex))
    {
        m_dragIndex+=count;
        placeVeil();
    }

    // the gap stays before the item it was before
    if((s_target == this) && (m_gap != NoItem) && (at <= m_gap))
    {
        m_gap+=count;
        gapMoved();
    }
}

void Reorderable::itemsRemoved(size_t at, size_t count)
{
    if(count == 0)
        return;

    if((s_source == this) && (m_dragIndex != NoItem))
    {
        // the dragged item itself is gone
        if((m_dragIndex >= at) && (m_dragIndex-at < count))
        {
            draganddrop::cancel();
            return;
        }

        if(m_dragIndex >= at+count)
        {
            m_dragIndex-=count;
            placeVeil();
        }
    }

    if((s_target == this) && (m_gap != NoItem) && (m_gap > at))
    {
        m_gap=(m_gap >= at+count) ? m_gap-count : at;
        gapMoved();
    }
}

void Reorderable::itemsChanged()
{
    if(s_source == this)
    {
        m_dragIndex=indexOf(m_dragKey);

        if(m_dragIndex == NoItem)
        {
            draganddrop::cancel();
            return;
        }
        placeVeil();
    }

    // The gap is found again, before the item it was before when the keys are
    // cheap (else where the cursor was), and checked again: the indexes it
    // gives may be others.
    if(s_target == this)
    {
        size_t gap=NoItem;

        if(cheapKeys() && (m_gap != NoItem))
            gap=m_gapAtEnd ? itemCount() : indexOf(m_gapKey);
        if(gap == NoItem)
            gap=gapAt(m_lastPoint);
        updateGap(gap, true);
    }
}

bool Reorderable::handleDrop(draganddrop::Package *package, int /*x*/, int /*y*/)
{
    Reorderable *source=fromPackage(package);

    if(!source || (source != s_source) || !accepts(source))
        return false;

    if((m_gap == NoItem) || !m_gapAllowed)
        return false;

    // the dragged item's index as the drop happens
    const size_t sourceIndex=source->indexOf(source->m_dragKey);

    if(sourceIndex == NoItem)
        return false;

    const size_t targetIndex=targetIndexFor(source, sourceIndex, this, m_gap);

    // dropped where it was: nothing to report
    if((source == this) && (targetIndex == sourceIndex))
        return false;

    // The drag is over before the app hears of it: the handler applies the drop
    // (removes the item from the source, inserts it here), which is a plain
    // edit then, not the dragged item going away.
    source->endAsSource();

    if(m_onItemDropped)
    {
        ItemDrop drop;
        drop.source=source;
        drop.key=source->m_dragKey;
        drop.sourceIndex=sourceIndex;
        drop.target=this;
        drop.targetIndex=targetIndex;
        m_onItemDropped(drop);
    }
    return true;
}

CardReorder::CardReorder(ScrollControl *list):
    Reorderable(list, const_cast<Base *>(list->getInnerPanel()))
{}

CardReorder *CardReorder::attach(ScrollControl *list)
{
    auto reorder=std::make_unique<CardReorder>(list);
    CardReorder *result=reorder.get();

    list->setReorderable(std::move(reorder));
    return result;
}

// a child of the panel that is a card: shown, and none of the looks
static bool isCard(Base *child)
{
    return !child->hidden() && !gweni_cast<internal::DropIndicator>(child)
        && !gweni_cast<internal::DragSourceVeil>(child);
}

std::vector<Base *> CardReorder::cards() const
{
    std::vector<Base *> result;

    for(Base *child : getPanel()->getActualChildren())
    {
        if(isCard(child))
            result.push_back(child);
    }
    return result;
}

size_t CardReorder::itemCount() const
{
    size_t count=0;

    for(Base *child : getPanel()->getActualChildren())
        count+=isCard(child) ? 1 : 0;
    return count;
}

uint64_t CardReorder::keyAt(size_t index) const
{
    for(Base *child : getPanel()->getActualChildren())
    {
        if(!isCard(child))
            continue;
        if(index == 0)
            return keyOf(child);
        --index;
    }
    return 0;
}

size_t CardReorder::indexOf(uint64_t key) const
{
    size_t index=0;

    for(Base *child : getPanel()->getActualChildren())
    {
        if(!isCard(child))
            continue;
        if(keyOf(child) == key)
            return index;
        ++index;
    }
    return NoItem;
}

size_t CardReorder::gapAt(Point local) const
{
    size_t index=0;

    // before the first card whose middle the cursor is above
    for(Base *child : getPanel()->getActualChildren())
    {
        if(!isCard(child))
            continue;

        const Rect &bounds=child->getBounds();

        if(local.y < bounds.y+bounds.h/2)
            return index;
        ++index;
    }
    return index;
}

int CardReorder::gapPosition(size_t gap) const
{
    const std::vector<Base *> items=cards();

    if(items.empty())
        return IndicatorThickness/2;

    // half-way between the cards either side, inside the panel at the ends
    if(gap == 0)
        return std::max(IndicatorThickness/2, items.front()->getY()/2);

    if(gap >= items.size())
    {
        const Base *last=items.back();
        return last->getY()+last->getHeight()+last->getMargin().bottom/2+IndicatorThickness/2;
    }

    const Base *above=items[gap-1];
    const Base *below=items[gap];

    // the middle of the space between them, rounded down
    return (above->getY()+above->getHeight()+below->getY())/2;
}

Rect CardReorder::itemRect(size_t index) const
{
    for(Base *child : getPanel()->getActualChildren())
    {
        if(!isCard(child))
            continue;
        if(index == 0)
            return child->getBounds();
        --index;
    }
    return Rect();
}

size_t CardReorder::itemAt(Point local) const
{
    size_t index=0;

    for(Base *child : getPanel()->getActualChildren())
    {
        if(!isCard(child))
            continue;

        const Rect &bounds=child->getBounds();

        if(local.x >= bounds.x && local.x < bounds.x+bounds.w && local.y >= bounds.y
            && local.y < bounds.y+bounds.h)
            return index;
        ++index;
    }
    return NoItem;
}

}//namespace controls
}//namespace gweni
