/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/controls/reorderable.h>
#include <gweni/controls/base.h>
#include <gweni/dragAndDrop.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

const char *const Reorderable::PackageName="gweni:ReorderItem";

// One drag at a time: its source and the list showing its gap.
static Reorderable *s_source=nullptr;
static Reorderable *s_target=nullptr;

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

Reorderable::Reorderable(Base *list):
    m_list(list)
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
    dragChanged();
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
        target->gapChanged();
    }

    dragChanged();
}

void Reorderable::hoverEnter(draganddrop::Package *package, int x, int y)
{
    s_target=this;
    m_gap=NoItem;
    m_gapAllowed=false;
    hover(package, x, y);
}

void Reorderable::hover(draganddrop::Package * /*package*/, int x, int y)
{
    if(s_target != this)
        return;

    m_lastPoint=m_list->canvasPosToLocal(Point(x, y));
    updateGap(gapAt(m_lastPoint), false);
}

void Reorderable::hoverLeave(draganddrop::Package * /*package*/)
{
    if(s_target != this)
        return;

    // the gap is kept for handleDrop, which the core calls after the leave;
    // it is no longer shown
    s_target=nullptr;
    gapChanged();
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
    gapChanged();
}

void Reorderable::itemsInserted(size_t at, size_t count)
{
    if(count == 0)
        return;

    if((s_source == this) && (m_dragIndex != NoItem) && (at <= m_dragIndex))
        m_dragIndex+=count;

    // the gap stays before the item it was before
    if((s_target == this) && (m_gap != NoItem) && (at <= m_gap))
    {
        m_gap+=count;
        gapChanged();
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
            m_dragIndex-=count;
    }

    if((s_target == this) && (m_gap != NoItem) && (m_gap > at))
    {
        m_gap=(m_gap >= at+count) ? m_gap-count : at;
        gapChanged();
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
    }

    // the gap is found again where the cursor was, and checked again (the
    // indexes it gives may be others)
    if(s_target == this)
        updateGap(gapAt(m_lastPoint), true);
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

}//namespace controls
}//namespace gweni
