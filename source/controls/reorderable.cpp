/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include <gweni/controls/reorderable.h>
#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/virtualList.h>
#include <gweni/anim.h>
#include <gweni/dragAndDrop.h>
#include <gweni/platforms/platform.h>

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

// The ScrollControl a list is, if it is one
static ScrollControl *scrollerOf(Base *list)
{
    return gweni_cast<ScrollControl>(list);
}

#if GWENI_ANIMATE
// The auto-scroll's animation: registered on the list only while it scrolls,
// so a drag resting outside the bands runs none.
class ReorderAutoScroll: public anim::Animation
{
public:
    explicit ReorderAutoScroll(Reorderable *owner): m_owner(owner) {}

    void think() override
    {
        if(m_owner)
            m_owner->autoScrollStep();
    }

    bool finished() override { return m_owner == nullptr; }

    Reorderable *m_owner;
};
#else
class ReorderAutoScroll
{
};
#endif

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
    stopAutoScroll();

    if(m_group)
        m_group->remove(this);

    if(s_target == this)
        s_target=nullptr;

    // The source goes: the drag ends with nothing reported. The hooks the
    // cancel forwards to this find it in no drag and do nothing. The package
    // may still be the live one after endAsSource(): a drop handler that
    // takes this away (or deletes its list) runs before the core is done.
    if(s_source == this)
        s_source=nullptr;

    if(draganddrop::CurrentPackage == &m_package)
        draganddrop::cancel();

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
    // an edit of the items not seen yet (made earlier in this input phase) is
    // left for listArranged to see
    const bool editSeen=(treeSerialOf(m_list) == m_treeSerial);
    Look *control=gweni_cast<Look>(Base::getControlById(id));

    // made the first time it is needed (again, if the app cleared the panel)
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
    if(editSeen)
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
        const size_t count=itemCount();
        // Visibility changes do not move the canvas's tree serial, but
        // change a card's index. A hide/show pair can keep the count too:
        // check the dragged key and the card after the gap as well.
        const bool sourceMoved=(s_source == this) && (indexOf(m_dragKey) != m_dragIndex);
        const bool gapShifted=(s_target == this) && (m_gap != NoItem)
            && (m_gapAtEnd ? (m_gap != count) : (indexOf(m_gapKey) != m_gap));

        if((serial != m_treeSerial) || (count != m_itemCount) || sourceMoved || gapShifted)
        {
            m_treeSerial=serial;
            itemsChanged();
        }
    }

    // (the dragged item gone, the drag is over: the looks are parked below,
    // and the panel arranged again all the same)

    // the list scrolled under the resting cursor (the auto-scroll, a wheel):
    // the gap is the one at the cursor now, and the auto-scroll goes on or
    // stops (at the content's end)
    if(s_target == this)
    {
        ScrollControl *scroller=scrollerOf(m_list);

        if(scroller && (scroller->getVerticalScrollOffset() != m_hoverScrollOffset))
            hoverAt(m_lastCanvasPoint);
        updateAutoScroll();
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

// Where a look waits while its list is in the drag but it shows nothing: far
// above the panel (clipped away), a pure move, which lays nothing out again.
// A look is hidden only once its list is out of the drag, and a hide that
// comes during a layout pass (a drag cancelled by a list edit) is posted to
// the next update, as a visibility change in a pass dirties layout again.
static constexpr int ParkedY=-20000;

template<typename Look>
static void parkLook(Look *look)
{
    if(look->getY() != ParkedY)
        look->place(Rect(0, ParkedY, look->getWidth(), look->getHeight()));
}

template<typename Look>
static void hideLook(Look *look)
{
    if(!look || look->hidden())
        return;

    if(!Look::layingOut())
    {
        look->setHidden(true);
        return;
    }

    parkLook(look);

    const size_t id=look->getControlId();
    const unsigned serial=look->useSerial();

    if(Canvas *canvas=look->getCanvas())
    {
        canvas->post([id, serial]() {
            // taken into use by then for another drag (placed, or parked for
            // a gap a pass may allow): left alone
            if(auto *control=gweni_cast<Look>(Base::getControlById(id)))
            {
                if(control->useSerial() == serial)
                    control->setHidden(true);
            }
        });
    }
}

//! A look a drag uses from now on: shown (parked or placed) by the caller
template<typename Look>
static void takeIntoUse(Look *look)
{
    look->takeIntoUse();
    look->setHidden(false);
}

bool Reorderable::indicatorShown() const
{
    internal::DropIndicator *indicator=getIndicator();
    return indicator && indicator->visible() && (indicator->getY() != ParkedY);
}

bool Reorderable::veilShown() const
{
    internal::DragSourceVeil *veil=getSourceVeil();
    return veil && veil->visible() && (veil->getY() != ParkedY);
}

void Reorderable::placeIndicator()
{
    if(!showsGap())
    {
        internal::DropIndicator *indicator=getIndicator();

        // the target with no gap shown (a refused one): parked, so a gap shown
        // again in a layout pass changes no visibility
        if(indicator && (s_target == this) && !indicator->hidden())
        {
            indicator->takeIntoUse();
            parkLook(indicator);
        }
        else
            hideLook(indicator);
        return;
    }

    internal::DropIndicator *indicator=look<internal::DropIndicator>(m_indicatorId);

    indicator->place(Rect(0, gapPosition(m_gap)-IndicatorThickness/2, m_panel->getWidth(),
        IndicatorThickness));
    takeIntoUse(indicator);
}

void Reorderable::placeVeil()
{
    Rect rect;

    if((s_source == this) && (m_dragIndex != NoItem))
        rect=itemRect(m_dragIndex);

    if((rect.w <= 0) || (rect.h <= 0))
    {
        internal::DragSourceVeil *veil=getSourceVeil();

        // the item out of view while it is dragged: parked
        if(veil && (s_source == this) && !veil->hidden())
        {
            veil->takeIntoUse();
            parkLook(veil);
        }
        else
            hideLook(veil);
        return;
    }

    internal::DragSourceVeil *veil=look<internal::DragSourceVeil>(m_veilId);

    veil->place(rect);
    takeIntoUse(veil);
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

    const uint64_t key=keyAt(index);

    if(m_canDrag && !m_canDrag(key))
        return nullptr;

    m_package.key=key;
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
    if(cheapKeys())
        m_itemCount=itemCount();
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
        target->stopAutoScroll();
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
    if(cheapKeys())
        m_itemCount=itemCount();

    // Prepare the bar in the input phase, even for a refused gap. An
    // auto-scroll or item edit can first allow it during listArranged:
    // moving the parked bar then creates no child and changes no visibility.
    internal::DropIndicator *indicator=look<internal::DropIndicator>(m_indicatorId);
    parkLook(indicator);
    takeIntoUse(indicator);

    hover(package, x, y);
}

void Reorderable::hover(draganddrop::Package * /*package*/, int x, int y)
{
    if(s_target != this)
        return;

    hoverAt(Point(x, y));
    updateAutoScroll();
}

void Reorderable::hoverAt(Point canvasPoint)
{
    m_lastCanvasPoint=canvasPoint;
    m_lastPoint=m_panel->canvasPosToLocal(canvasPoint);

    if(ScrollControl *scroller=scrollerOf(m_list))
        m_hoverScrollOffset=scroller->getVerticalScrollOffset();

    updateGap(gapAt(m_lastPoint), false);
}

void Reorderable::updateAutoScroll()
{
    ScrollControl *scroller=scrollerOf(m_list);
    int direction=0;
    float rate=0.0f;

    if((s_target == this) && accepts(s_source) && scroller && scroller->canScrollVertical()
        && (m_autoScrollBand > 0))
    {
        const Point local=m_list->canvasPosToLocal(m_lastCanvasPoint);
        const Rect &viewport=scroller->getViewport();
        const int band=m_autoScrollBand;
        const int offset=scroller->getVerticalScrollOffset();
        int depth=0;

        if((local.x >= viewport.x) && (local.x < viewport.x+viewport.w))
        {
            // the cursor's depth into a band, the whole band past its edge
            if(local.y < viewport.y+band)
            {
                depth=viewport.y+band-local.y;
                direction=(offset > 0) ? -1 : 0;
            }
            else if(local.y >= viewport.y+viewport.h-band)
            {
                depth=local.y-(viewport.y+viewport.h-band)+1;
                direction=(offset < scroller->getMaxVerticalScrollOffset()) ? 1 : 0;
            }
        }

        rate=m_autoScrollSpeed*static_cast<float>(std::min(depth, band))/static_cast<float>(band);
    }

    if((direction == 0) || (rate <= 0.0f))
    {
        stopAutoScroll();
        return;
    }

#if GWENI_ANIMATE
    if(!m_autoScroll || (direction != m_scrollDirection))
    {
        stopAutoScroll();
        m_autoScroll=new ReorderAutoScroll(this);
        m_scrollTime=platform::getTimeInSeconds();
        m_scrollCarry=0.0;
        anim::add(m_list, m_autoScroll);
    }
    m_scrollDirection=direction;
    m_scrollRate=rate;
#else
    (void)direction;
#endif
}

void Reorderable::stopAutoScroll()
{
#if GWENI_ANIMATE
    if(m_autoScroll)
    {
        m_autoScroll->m_owner=nullptr;
        anim::cancel(m_list, m_autoScroll);
        m_autoScroll=nullptr;
    }
#endif
    m_scrollDirection=0;
    m_scrollRate=0.0f;
}

void Reorderable::autoScrollStep()
{
    ScrollControl *scroller=scrollerOf(m_list);

    if(!scroller || (m_scrollDirection == 0))
    {
        stopAutoScroll();
        return;
    }

    // the canvas clock's time since the last step, at the rate the cursor's
    // depth gives, whole units scrolled and the rest carried
    const double now=platform::getTimeInSeconds();

    m_scrollCarry+=m_scrollDirection*m_scrollRate*(now-m_scrollTime);
    m_scrollTime=now;

    const int step=static_cast<int>(m_scrollCarry);

    if(step == 0)
        return;

    m_scrollCarry-=step;

    // held to the content as the last layout gave it (a ScrollControl clamps a
    // plain content's offset only in its next layout)
    const int offset=scroller->getVerticalScrollOffset();
    const int wanted=std::clamp(offset+step, 0, scroller->getMaxVerticalScrollOffset());

    // at the content's end: nothing more to scroll
    if(wanted == offset)
    {
        stopAutoScroll();
        return;
    }

    scroller->setVerticalScrollOffset(wanted);
}

void Reorderable::hoverLeave(draganddrop::Package * /*package*/)
{
    if(s_target != this)
        return;

    // the gap is kept for handleDrop, which the core calls after the leave;
    // it is no longer shown
    s_target=nullptr;
    stopAutoScroll();
    gapMoved();
}

void Reorderable::updateGap(size_t gap, bool force)
{
    if(force || (gap != m_gap))
    {
        // asked again here, not only as the drag entered: the app may have
        // sorted a table since, which then takes none of its own items
        const bool accepted=accepts(s_source);
        bool allowed=(gap != NoItem) && accepted;

        if(!accepted)
            stopAutoScroll();

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

    // the gap stays before the item it was before, and is asked about again:
    // canDrop takes its index, which moved
    if((s_target == this) && (m_gap != NoItem))
        updateGap((at <= m_gap) ? m_gap+count : m_gap, true);
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

    if((s_target == this) && (m_gap != NoItem))
    {
        size_t gap=m_gap;

        if(gap > at)
            gap=(gap >= at+count) ? gap-count : at;
        updateGap(gap, true);
    }
}

void Reorderable::itemsChanged()
{
    if(cheapKeys() && ((s_source == this) || (s_target == this)))
        m_itemCount=itemCount();

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

        // Called on a copy: the handler may delete this Reorderable (its list
        // deleted, or reordering taken off it), and with it the member. Nothing
        // of this object is touched after the call.
        const DropHandler handler=m_onItemDropped;

        handler(drop);
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

VirtualListReorder::VirtualListReorder(VirtualList *list, KeyOf keyOf, IndexOf indexOf):
    Reorderable(list, const_cast<Base *>(list->getInnerPanel())),
    m_virtualList(list),
    m_keyOf(std::move(keyOf)),
    m_indexOf(std::move(indexOf))
{}

VirtualListReorder *VirtualListReorder::attach(VirtualList *list, KeyOf keyOf, IndexOf indexOf)
{
    auto reorder=std::make_unique<VirtualListReorder>(list, std::move(keyOf), std::move(indexOf));
    VirtualListReorder *result=reorder.get();

    list->setReorderable(std::move(reorder));
    return result;
}

size_t VirtualListReorder::itemCount() const
{
    return m_virtualList->getItemCount();
}

uint64_t VirtualListReorder::keyAt(size_t index) const
{
    return m_keyOf ? m_keyOf(index) : static_cast<uint64_t>(index);
}

size_t VirtualListReorder::indexOf(uint64_t key) const
{
    if(!m_indexOf)
        return (key < itemCount()) ? static_cast<size_t>(key) : NoItem;

    const size_t index=m_indexOf(key);
    return (index < itemCount()) ? index : NoItem;
}

// The rows are placed in the viewport (the inner panel does not scroll):
// item i's top is index*rowHeight less the offset, or in variable mode its
// height index's offset less the anchor's, as the last layout placed them.
int64_t VirtualListReorder::viewTop(size_t index) const
{
    if(m_virtualList->isVariableHeight())
    {
        const HeightIndex &heights=m_virtualList->getHeightIndex();
        const size_t anchor=std::min(m_virtualList->getAnchorItem(), heights.size());

        return heights.offsetOf(std::min(index, heights.size()))-heights.offsetOf(anchor)
            -m_virtualList->getAnchorOffset();
    }

    return static_cast<int64_t>(index)*m_virtualList->getRowHeight()
        -m_virtualList->getVerticalScrollOffset();
}

int VirtualListReorder::itemHeight(size_t index) const
{
    if(m_virtualList->isVariableHeight())
    {
        const HeightIndex &heights=m_virtualList->getHeightIndex();
        return (index < heights.size()) ? heights.height(index) : 0;
    }
    return m_virtualList->getRowHeight();
}

size_t VirtualListReorder::itemAtY(int y) const
{
    const size_t count=itemCount();

    if(count == 0)
        return 0;

    if(m_virtualList->isVariableHeight())
    {
        const HeightIndex &heights=m_virtualList->getHeightIndex();
        const size_t anchor=std::min(m_virtualList->getAnchorItem(), heights.size());
        const int64_t content=y+heights.offsetOf(anchor)+m_virtualList->getAnchorOffset();

        return std::min(heights.itemAt(content), count);
    }

    const int64_t content=static_cast<int64_t>(y)+m_virtualList->getVerticalScrollOffset();

    if(content < 0)
        return 0;
    return std::min(static_cast<size_t>(content/m_virtualList->getRowHeight()), count);
}

size_t VirtualListReorder::gapAt(Point local) const
{
    const size_t count=itemCount();
    const size_t item=itemAtY(local.y);

    if(item >= count)
        return count;

    // the half-height rule on the item under the cursor
    const int64_t top=viewTop(item);
    return (local.y < top+itemHeight(item)/2) ? item : item+1;
}

int VirtualListReorder::gapPosition(size_t gap) const
{
    const int64_t y=viewTop(std::min(gap, itemCount()));

    // kept inside the 16-bit range a placed control has, far off the panel
    return static_cast<int>(std::clamp<int64_t>(y, -30000, 30000));
}

Rect VirtualListReorder::itemRect(size_t index) const
{
    if(index >= itemCount())
        return Rect();

    const int64_t top=viewTop(index);
    const int height=itemHeight(index);
    const int viewport=m_virtualList->getViewportHeight();

    // out of view: no veil (no row shows it)
    if(top+height <= 0 || top >= viewport)
        return Rect();

    return Rect(0, static_cast<int>(top), getPanel()->getWidth(), height);
}

size_t VirtualListReorder::itemAt(Point local) const
{
    const size_t item=itemAtY(local.y);

    return (item < itemCount()) ? item : NoItem;
}

}//namespace controls
}//namespace gweni
