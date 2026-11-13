/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/virtualList.h>
#include <gweni/controls/canvas.h>
#include <gweni/anim.h>
#include <gweni/skin.h>
#include <gweni/platforms/baseRender.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>

namespace gweni
{
namespace controls
{

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(VirtualListRow)
{}

// A press highlights the row (Button's press handling) and goes to the list;
// it never selects the row, which shows another item once the list scrolls.
void VirtualListRow::onPress(event::Info info)
{
    if(isDisabled())
        return;

    Button::onPress(info);

    for(Base *parent=getParent(); parent; parent=parent->getParent())
    {
        if(VirtualList *list=gweni_cast<VirtualList>(parent))
        {
            list->rowPressed(this);
            break;
        }
    }
}

// The idle re-measure: registered while an item is not measured at the list's
// generation, a budget of them a frame, and finished once every item is.
class VirtualListRemeasure: public anim::Animation
{
public:
    void think() override
    {
        VirtualList *list=static_cast<VirtualList *>(m_control);

        m_done=list->remeasureStep();
        if(m_done)
            list->m_remeasure=nullptr;
    }

    bool finished() override
    {
        return m_done;
    }

private:
    bool m_done=false;
};

}//namespace internal

// Vertical scrolling only, over virtual content: the rows sit in the inner
// panel, which is the viewport, with nothing around them. The size is a
// default the app overrides (or a dock replaces): with no request a
// ScrollControl asks for its content, which here is every item's row.
GWENI_CONTROL_CONSTRUCTOR(VirtualList)
{
    setScroll(false, true);
    setAutoHideBars(true);
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    m_verticalScrollBar->setNudgeAmount(m_rowHeight);
    setVirtualContentHeight(0);
    setSize(200, 8*m_rowHeight);
}

void VirtualList::setRowHeight(int height)
{
    height=std::max(height, 1);

    if(m_rowHeight == height)
        return;

    m_rowHeight=height;
    m_verticalScrollBar->setNudgeAmount(m_rowHeight);
    updateContentHeight();
    invalidate();
}

void VirtualList::setItemCount(size_t count)
{
    if(m_itemCount == count)
        return;

    if(isVariableHeight())
    {
        // a scroll not laid out yet is anchored at the heights it was made at
        anchorAtOffset();

        if(count > m_itemCount)
        {
            if(m_estimator)
            {
                for(size_t i=m_itemCount; i < count; ++i)
                    m_heights.append(1, defaultHeight(i), 0);
            }
            else
                m_heights.append(count-m_itemCount, defaultHeight(m_itemCount), 0);
        }
        else
        {
            m_heights.erase(count, m_itemCount-count);
            // the item at the top went: past the end, as removeItems leaves it
            if(m_anchorItem >= count)
            {
                m_anchorItem=count;
                m_anchorOffset=0;
            }
        }

        m_itemCount=count;
        updateContentHeight();
        offsetFromAnchor();
        invalidate();
        return;
    }

    m_itemCount=count;
    // which keeps the offset inside the new content
    updateContentHeight();
    invalidate();
}

void VirtualList::insertItems(size_t at, size_t count)
{
    at=std::min(at, m_itemCount);
    if(count == 0)
        return;

    if(isVariableHeight())
    {
        anchorAtOffset();

        if(m_estimator)
        {
            for(size_t i=0; i < count; ++i)
                m_heights.insert(at+i, 1, defaultHeight(at+i), 0);
        }
        else
            m_heights.insert(at, count, defaultHeight(at), 0);

        // added above the item at the top (or before it): it stays there; an
        // anchor past the end (its item removed) stays past it
        if(at < m_anchorItem || (at == m_anchorItem && m_anchorItem < m_itemCount))
            m_anchorItem+=count;

        m_itemCount+=count;
        shiftItems(at, static_cast<int64_t>(count));
        updateContentHeight();
        offsetFromAnchor();
        invalidate();
        return;
    }

    takeBarScroll();

    const int offset=getVerticalScrollOffset();
    const size_t top=static_cast<size_t>(offset/m_rowHeight);
    const bool above=at <= top && top < m_itemCount;

    m_itemCount+=count;
    shiftItems(at, static_cast<int64_t>(count));
    updateContentHeight();
    if(above)
        setVerticalScrollOffset(static_cast<int>(std::min<uint64_t>(
            static_cast<uint64_t>(offset)+static_cast<uint64_t>(count)*m_rowHeight, INT_MAX)));
    invalidate();
}

void VirtualList::removeItems(size_t at, size_t count)
{
    if(at >= m_itemCount || count == 0)
        return;

    count=std::min(count, m_itemCount-at);

    const size_t end=at+count;

    if(isVariableHeight())
    {
        anchorAtOffset();
        m_heights.erase(at, count);

        // removed above the item at the top: it stays there; the item at the
        // top removed: the one after the removed ones takes its place
        if(m_anchorItem >= end)
            m_anchorItem-=count;
        else if(m_anchorItem >= at)
        {
            m_anchorItem=at;
            m_anchorOffset=0;
        }

        m_itemCount-=count;
        shiftItems(at, -static_cast<int64_t>(count));
        updateContentHeight();
        offsetFromAnchor();
        invalidate();
        return;
    }

    takeBarScroll();

    const int offset=getVerticalScrollOffset();
    const size_t top=static_cast<size_t>(offset/m_rowHeight);
    int64_t newOffset=offset;

    if(top >= end)
        newOffset-=static_cast<int64_t>(count)*m_rowHeight;
    else if(top >= at)
        newOffset=static_cast<int64_t>(at)*m_rowHeight;

    m_itemCount-=count;
    shiftItems(at, -static_cast<int64_t>(count));
    updateContentHeight();
    if(newOffset != offset)
        setVerticalScrollOffset(static_cast<int>(std::min<int64_t>(newOffset, INT_MAX)));
    invalidate();
}

// A row keeps its item through the shift, at the item's new index; a row of
// a removed item gives up its press and focus now, while that is still what
// it shows, and is free for the next layout.
void VirtualList::shiftItems(size_t at, int64_t count)
{
    const size_t removedEnd=count < 0?at+static_cast<size_t>(-count):at;

    for(Row &row : m_rows)
    {
        if(row.item == NoItem || row.item < at)
            continue;

        if(count > 0)
            row.item+=static_cast<size_t>(count);
        else if(row.item < removedEnd)
        {
            releaseRow(row.control);
            row.item=NoItem;
            row.stale=true;
            m_rowsEdited=true;
        }
        else
            row.item-=removedEnd-at;
    }

    if(m_pendingItem != NoItem && m_pendingItem >= at)
    {
        if(count > 0)
            m_pendingItem+=static_cast<size_t>(count);
        else if(m_pendingItem < removedEnd)
        {
            m_pendingItem=at;
            m_pendingPartHeight=-1;
        }
        else
            m_pendingItem-=removedEnd-at;
    }

    // the idle re-measure goes on from the item it had reached
    if(m_remeasureCursor >= at)
    {
        if(count > 0)
            m_remeasureCursor+=static_cast<size_t>(count);
        else if(m_remeasureCursor < removedEnd)
            m_remeasureCursor=at;
        else
            m_remeasureCursor-=removedEnd-at;
    }
}

void VirtualList::itemChanged(size_t index)
{
    if(index >= m_itemCount)
        return;

    refreshItem(index);

    if(!isVariableHeight())
        return;

    anchorAtOffset();

    const int before=m_heights.height(index);

    // not measured at this width any more
    m_heights.set(index, before, 0);
    if(m_measureWidth < 0)
    {
        invalidate();
        return;
    }

    const int after=measureItem(index);

    if(after == before)
        return;

    if(index == m_anchorItem)
        m_anchorOffset=std::min(m_anchorOffset, after-1);

    updateContentHeight();
    offsetFromAnchor();
    invalidate();
}

void VirtualList::setStickToEnd(bool stick)
{
    if(m_stuckToEnd == stick)
        return;

    m_stuckToEnd=stick;

    // unstuck, it stays where the last layout put it
    if(!stick)
        return;

    if(isVariableHeight())
    {
        anchorAtOffset();
        offsetFromAnchor();
    }
    invalidate();
}

void VirtualList::updateContentHeight()
{
    if(isVariableHeight())
    {
        setVirtualContentHeight(static_cast<int>(std::min<int64_t>(m_heights.totalHeight(), INT_MAX)));
        return;
    }

    const uint64_t height=static_cast<uint64_t>(m_itemCount)*static_cast<uint64_t>(m_rowHeight);

    setVirtualContentHeight(static_cast<int>(std::min<uint64_t>(height, INT_MAX)));
}

// Every item counts at the default height until it shows; the anchor is the
// item the fixed offset showed at the top, so the view stays where it was as
// far as the heights allow. Leaving variable mode, the fixed offset is the
// anchor's item's row, as far into it as the anchor was into the item.
void VirtualList::setItemMeasurer(ItemMeasurer measurer)
{
    const bool wasVariable=isVariableHeight();

    // the position, from one place: a scroll not laid out yet included
    if(wasVariable)
        anchorAtOffset();

    const size_t top=wasVariable?std::min(m_anchorItem, m_itemCount > 0?m_itemCount-1:0):firstVisibleItem();
    const int into=wasVariable?m_anchorOffset:getVerticalScrollOffset()%m_rowHeight;

    m_measurer=std::move(measurer);

    if(isVariableHeight())
    {
        if(!wasVariable)
        {
            m_autoHideBefore=m_autoHideBars;
            setAutoHideBars(false);
        }

        m_heights.clear();
        m_generation=1;
        m_measureWidth=-1;
        m_minHeight=0;
        m_lastMeasured=0;
        if(m_estimator)
        {
            for(size_t i=0; i < m_itemCount; ++i)
                m_heights.append(1, defaultHeight(i), 0);
        }
        else
            m_heights.append(m_itemCount, defaultHeight(0), 0);

        m_anchorItem=top;
        m_anchorOffset=into;
        updateContentHeight();
        offsetFromAnchor();
    }
    else if(wasVariable)
    {
        updateRemeasure();
        setAutoHideBars(m_autoHideBefore);
        m_heights.clear();
        m_measureWidth=-1;
        m_viewTops.clear();
        updateContentHeight();
        setVerticalScrollOffset(static_cast<int>(std::min<uint64_t>(
            static_cast<uint64_t>(top)*m_rowHeight+gweni::clamp(into, 0, m_rowHeight-1), INT_MAX)));
    }

    refresh();
}

void VirtualList::setItemEstimator(ItemEstimator estimator)
{
    m_estimator=std::move(estimator);
}

void VirtualList::setRemeasureBudget(size_t items)
{
    m_remeasureBudget=items;
    updateRemeasure();
}

void VirtualList::updateRemeasure()
{
    const bool needed=isVariableHeight() && m_measureWidth >= 0 && m_remeasureBudget > 0
        && m_heights.minGeneration() < m_generation;

    if(needed && !m_remeasure)
    {
        m_remeasure=new internal::VirtualListRemeasure();
        anim::add(this, m_remeasure);
    }
    else if(!needed && m_remeasure)
    {
        anim::cancel(this, m_remeasure);
        m_remeasure=nullptr;
    }
}

// From the update phase, before layout. The items on screen were measured at
// this generation by the layout that showed them, so this measures only
// those off it, nearest below the top first. A new height moves the offset
// through the anchor (offsetFromAnchor), so the next layout places the rows on
// screen where they were.
bool VirtualList::remeasureStep()
{
    if(!isVariableHeight() || m_measureWidth < 0 || m_remeasureBudget == 0)
        return true;

    anchorAtOffset();

    const size_t count=m_heights.size();
    bool changed=false;

    for(size_t n=0; n < m_remeasureBudget; ++n)
    {
        size_t item=m_heights.firstStale(m_remeasureCursor, m_generation);

        if(item >= count)
        {
            item=m_heights.firstStale(0, m_generation);
            if(item >= count)
                break;
        }

        const int before=m_heights.height(item);

        if(measureItem(item) != before)
            changed=true;
        m_remeasureCursor=item+1;
    }

    if(changed)
    {
        if(m_anchorItem < count)
            m_anchorOffset=std::min(m_anchorOffset, m_heights.height(m_anchorItem)-1);
        updateContentHeight();
        offsetFromAnchor();
    }

    return m_heights.minGeneration() >= m_generation;
}

void VirtualList::verifyOwnLayout(std::vector<std::string> &problems)
{
    if(!isVariableHeight() || !visible())
        return;

    for(const Row &row : m_rows)
    {
        if(row.item == NoItem || row.control->hidden())
            continue;

        const int measured=m_heights.height(row.item);
        const int laidOut=row.control->getHeight();
        const int asked=row.control->getPreferredSize().height;

        if(laidOut == measured && asked == measured)
            continue;

        problems.push_back("row " + std::string(row.control->getTypeName()) + "#"
            + std::to_string(row.control->getControlId()) + " of item " + std::to_string(row.item)
            + ": bounds.h " + std::to_string(laidOut) + ", preferred.height " + std::to_string(asked)
            + ", measured " + std::to_string(measured));
    }
}

int VirtualList::defaultHeight(size_t index)
{
    if(m_estimator)
    {
        const int width=m_measureWidth >= 0?m_measureWidth:m_innerPanel->getWidth();

        return std::max(1, m_estimator(index, width));
    }

    return m_lastMeasured > 0?m_lastMeasured:m_rowHeight;
}

int VirtualList::measureItem(size_t index)
{
    if(m_heights.generation(index) == m_generation)
        return m_heights.height(index);

    const int height=std::max(1, m_measurer(index, m_measureWidth));

    m_heights.set(index, height, m_generation);
    m_lastMeasured=height;
    m_minHeight=(m_minHeight > 0)?std::min(m_minHeight, height):height;
    return height;
}

void VirtualList::anchorAbove(size_t item, int viewport)
{
    int64_t need=static_cast<int64_t>(viewport)-measureItem(item);
    size_t top=item;

    while(need > 0 && top > 0)
    {
        --top;
        need-=measureItem(top);
    }

    if(need > 0)
    {
        // everything down to the item fits: from the top
        m_anchorItem=0;
        m_anchorOffset=0;
        return;
    }

    m_anchorItem=top;
    m_anchorOffset=static_cast<int>(-need);
}

void VirtualList::takeBarScroll()
{
    // A bar moved by the user since the last layout (its thumb, its buttons)
    // is taken at the content it was moved against, as placeContents takes
    // it, before an edit moves the offset (and the bar with it).
    const float amount=m_verticalScrollBar->getScrolledAmount();

    if(amount != m_shownAmount[1])
    {
        m_scrollOffset[1]=gweni::clamp(static_cast<int>(std::lround(
            static_cast<double>(amount)*m_scrollRange[1])), 0, getMaxVerticalScrollOffset());
        m_shownAmount[1]=amount;
        barScrollTaken(amount);
    }
}

void VirtualList::anchorAtOffset()
{
    takeBarScroll();

    const int offset=getVerticalScrollOffset();

    if(offset == m_anchoredOffset)
        return;

    m_anchoredOffset=offset;
    // scrolled to the bottom: the end, whatever the heights measured next
    m_anchorToEnd=offset > 0 && offset == getMaxVerticalScrollOffset();

    if(m_heights.empty())
    {
        m_anchorItem=0;
        m_anchorOffset=0;
        return;
    }

    int64_t top=0;
    size_t item=m_heights.itemAt(offset, &top);

    if(item >= m_heights.size())
    {
        item=m_heights.size()-1;
        top=m_heights.offsetOf(item);
    }

    m_anchorItem=item;
    m_anchorOffset=static_cast<int>(std::max<int64_t>(0, offset-top));
}

void VirtualList::offsetFromAnchor()
{
    const size_t item=std::min(m_anchorItem, m_heights.size());
    const int64_t position=m_stuckToEnd?INT64_MAX
        :m_heights.offsetOf(item)+(item < m_heights.size()?m_anchorOffset:0);
    const int offset=static_cast<int>(gweni::clamp<int64_t>(position, 0, getMaxVerticalScrollOffset()));

    m_anchoredOffset=offset;

    if(offset == m_scrollOffset[1])
        return;

    m_scrollOffset[1]=offset;
    showScrollOffset(Dim::Y);
    invalidate();
}

void VirtualList::setRowFactory(RowFactory factory)
{
    clearPool();
    m_factory=std::move(factory);
    invalidate();
}

void VirtualList::setRowBinder(RowBinder binder)
{
    m_binder=std::move(binder);
    refresh();
}

void VirtualList::refresh()
{
    for(Row &row : m_rows)
        row.stale=true;

    // their heights may have changed too: a new generation, O(1), so the next
    // layout measures the items on screen and the idle re-measure the rest
    if(isVariableHeight())
    {
        ++m_generation;
        m_remeasureCursor=m_anchorItem;
    }

    invalidate();
}

void VirtualList::refreshItem(size_t index)
{
    for(Row &row : m_rows)
    {
        if(row.item == index)
        {
            row.stale=true;
            invalidate();
            return;
        }
    }
}

void VirtualList::clearPool()
{
    for(Row &row : m_rows)
        delete row.control;    // ~Base takes it out of the children, gives its ids back

    m_rows.clear();
}

// Resolved by the next layout (contentsPlaced), which knows the viewport: so a
// call made before the first layout, or in the frame of a resize, scrolls for
// the viewport the item is shown in.
void VirtualList::scrollToItem(size_t index)
{
    scrollToItem(index, 0, -1);
}

void VirtualList::scrollToItem(size_t index, int partTop, int partHeight)
{
    if(index >= m_itemCount)
        return;

    // the same item pending: its layout is asked for already, this only
    // says which part of it
    const bool pending=(m_pendingItem == index);

    m_pendingItem=index;
    m_pendingPartTop=std::max(0, partTop);
    m_pendingPartHeight=(partHeight < 0)?-1:std::max(1, partHeight);
    if(!pending)
        invalidate();
}

void VirtualList::scrollByRows(int rows)
{
    m_pendingItem=NoItem;

    if(isVariableHeight())
    {
        anchorAtOffset();
        m_anchorToEnd=false;
        if(m_heights.empty())
            return;

        const int64_t item=static_cast<int64_t>(m_anchorItem)+rows;

        m_anchorItem=static_cast<size_t>(gweni::clamp<int64_t>(item, 0,
            static_cast<int64_t>(m_heights.size())-1));
        // as far into the new item as into the old, inside it, so the offset
        // shows that item until the next layout (a second call goes on from it)
        m_anchorOffset=std::min(m_anchorOffset, m_heights.height(m_anchorItem)-1);
        offsetFromAnchor();
        invalidate();
        return;
    }

    const int64_t offset=static_cast<int64_t>(getVerticalScrollOffset())
        +static_cast<int64_t>(rows)*m_rowHeight;

    setVerticalScrollOffset(static_cast<int>(gweni::clamp<int64_t>(offset, 0, INT_MAX)));
}

// GLFW's translator gives 60 per wheel notch. A wheel the list cannot use
// (at its end, or nothing to scroll) goes on to the parent.
bool VirtualList::onMouseWheeled(int iDelta)
{
    const int before=getVerticalScrollOffset();
    const long delta=std::lround(-static_cast<double>(iDelta)*m_wheelRows*m_rowHeight/60.0);

    setVerticalScrollOffset(static_cast<int>(gweni::clamp<int64_t>(
        static_cast<int64_t>(before)+delta, 0, INT_MAX)));

    if(getVerticalScrollOffset() != before)
        return true;

    return Base::onMouseWheeled(iDelta);
}

size_t VirtualList::firstVisibleItem() const
{
    if(m_itemCount == 0)
        return 0;

    if(isVariableHeight())
        return std::min(m_heights.itemAt(getVerticalScrollOffset()), m_itemCount-1);

    return std::min(static_cast<size_t>(getVerticalScrollOffset()/m_rowHeight), m_itemCount-1);
}

Base *VirtualList::rowForItem(size_t index) const
{
    if(index == NoItem)
        return nullptr;

    for(const Row &row : m_rows)
    {
        if(row.item == index)
            return row.control;
    }

    return nullptr;
}

size_t VirtualList::itemForRow(const Base *control) const
{
    for(const Row &row : m_rows)
    {
        if(row.control == control)
            return row.item;
    }

    return NoItem;
}

std::vector<Base *> VirtualList::getRows() const
{
    std::vector<Base *> rows;

    rows.reserve(m_rows.size());
    for(const Row &row : m_rows)
        rows.push_back(row.control);

    return rows;
}

void VirtualList::rowPressed(Base *control)
{
    const size_t item=itemForRow(control);

    if(item == NoItem)
        return;

    event::Info info(control);

    info.integer=item;
    onItemPressed.call(this, info);
}

size_t VirtualList::poolSizeFor(int viewport) const
{
    if(viewport <= 0)
        return 1;

    const int height=(isVariableHeight() && m_minHeight > 0)?m_minHeight:m_rowHeight;

    return static_cast<size_t>((viewport+height-1)/height)+1;
}

// The rows' width is final here: a new one is a new generation, so every item
// is measured again as it shows. The scroll requests since the last layout
// become the anchor (a scrollToItem, else an offset the bar, the wheel or the
// app set), and the items on screen are measured from the anchor down until
// they fill the viewport. If the end comes first, the last item's bottom goes
// to the viewport's bottom, measuring upwards. Only then are the content
// height, the bar and the offset worked out, from the anchor: every item in
// view has been measured, so the rows sit one below the other from it, and an
// item above it changing height moves the offset, not them. Items with no
// measure at this width keep their old or estimated height in the sum.
void VirtualList::placeVariable(size_t &first, size_t &end)
{
    const int viewport=getViewportHeight();
    const int width=m_innerPanel->getWidth();
    const size_t count=m_heights.size();
    const renderer::Base *renderer=getSkin()?getSkin()->getRenderer():nullptr;
    const unsigned serial=renderer?renderer->textLayoutSerial():0;
    // the bar as placeContents left it, at the range it took it against
    const float amount=m_verticalScrollBar->getScrolledAmount();
    const int range=m_scrollRange[1];

    anchorAtOffset();

    // every item is measured again, by this layout if it shows, by the idle
    // re-measure from the top down otherwise: at a new width, or under a new
    // text layout (the canvas lays the whole tree out again for it, and a
    // Text wraps again; a canvas scale leaves the measures as they are)
    if(width != m_measureWidth || renderer != m_measureRenderer || serial != m_measureSerial)
    {
        if(m_measureWidth >= 0)
            ++m_generation;
        m_measureWidth=width;
        m_measureRenderer=renderer;
        m_measureSerial=serial;
        m_remeasureCursor=m_anchorItem;
    }

    if(count == 0)
    {
        m_anchorItem=0;
        m_anchorOffset=0;
        m_pendingItem=NoItem;
    }
    else
    {
        if(m_anchorItem >= count)
        {
            m_anchorItem=count-1;
            m_anchorOffset=0;
        }

        if(m_pendingItem != NoItem)
        {
            const size_t item=std::min(m_pendingItem, count-1);

            m_pendingItem=NoItem;
            m_anchorToEnd=false;
            m_anchorOffset=gweni::clamp(m_anchorOffset, 0, measureItem(m_anchorItem)-1);

            const int itemHeight=measureItem(item);

            if(itemHeight >= viewport && m_pendingPartHeight > 0)
            {
                // too tall to show whole, a part of it asked for: that part,
                // judged from the measures down to it as below
                const int partTop=std::min(m_pendingPartTop, itemHeight-1);
                const int partBottom=std::min(partTop+m_pendingPartHeight, itemHeight);
                int64_t top=-m_anchorOffset;
                size_t at=m_anchorItem;

                if(item < m_anchorItem)
                    top=INT64_MIN/2;
                else
                {
                    for(; at < item && top < viewport; ++at)
                        top+=measureItem(at);
                    if(at < item)
                        top=INT64_MAX/2;
                }

                if(top+partTop < 0)
                {
                    m_anchorItem=item;
                    m_anchorOffset=partTop;
                }
                else if(top+partBottom > viewport)
                {
                    m_anchorItem=item;
                    m_anchorOffset=std::max(0, partBottom-viewport);
                }
            }
            else if(item < m_anchorItem || (item == m_anchorItem && m_anchorOffset > 0)
                || itemHeight >= viewport)
            {
                // above the viewport's top, or too tall to show whole: to its
                // top, so a second call on it changes nothing
                m_anchorItem=item;
                m_anchorOffset=0;
            }
            else
            {
                // Whether it is in view is judged from measures, not estimates:
                // the items from the anchor down to it are measured, or down to
                // the viewport's bottom if it is further (those show anyway).
                int64_t top=-m_anchorOffset;
                size_t at=m_anchorItem;

                for(; at < item && top < viewport; ++at)
                    top+=measureItem(at);

                if(at < item || top+measureItem(item) > viewport)
                    anchorAbove(item, viewport);
            }
        }

        // stuck, whatever was asked
        if(m_stuckToEnd)
            m_anchorToEnd=true;

        const int height=measureItem(m_anchorItem);

        m_anchorOffset=gweni::clamp(m_anchorOffset, 0, height-1);

        int64_t covered=height-m_anchorOffset;

        if(!m_anchorToEnd)
        {
            for(size_t item=m_anchorItem+1; covered < viewport && item < count; ++item)
                covered+=measureItem(item);
        }

        if(m_anchorToEnd || covered < viewport)
            anchorAbove(count-1, viewport);
    }
    m_anchorToEnd=false;

    const int content=static_cast<int>(std::min<int64_t>(m_heights.totalHeight(), INT_MAX));
    const bool required=content > viewport;
    const int maxOffset=required?content-viewport:0;

    m_virtualHeight=content;
    m_verticalScrollBar->setContentSize(content);
    setVerticalScrollRequired(required);
    m_scrollRange[1]=maxOffset;

    const int64_t position=count > 0?m_heights.offsetOf(m_anchorItem)+m_anchorOffset:0;
    const int offset=static_cast<int>(gweni::clamp<int64_t>(position, 0, maxOffset));

    m_scrollOffset[1]=offset;
    m_anchoredOffset=offset;

    // A bar the user moved to between two units keeps its amount while it
    // still rounds to the offset at the same range, so moves of under half a
    // unit add up, as placeContents keeps them (#512).
    if(maxOffset == range && std::lround(static_cast<double>(amount)*maxOffset) == offset)
        m_shownAmount[1]=amount;
    else
        showScrollOffset(Dim::Y);

    // the items in view and their tops
    first=m_anchorItem;
    m_viewTops.clear();

    int64_t y=(count > 0?m_heights.offsetOf(first):0)-offset;
    size_t item=first;

    for(; item < count && y < viewport; ++item)
    {
        m_viewTops.push_back(static_cast<int>(y));
        y+=m_heights.height(item);
    }
    end=item;

    updateRemeasure();
}

// The offset that puts the item fully in view, scrolling as little as it can:
// to the item's top if it is above the viewport, to its bottom if below.
static int64_t offsetShowing(size_t item, int rowHeight, int64_t offset, int64_t viewport)
{
    const int64_t top=static_cast<int64_t>(item)*rowHeight;
    const int64_t bottom=top+rowHeight;

    if(top < offset)
        return top;
    if(bottom > offset+viewport)
        return bottom-viewport;
    return offset;
}

// Whether control is row or inside it.
static bool inRow(const Base *control, const Base *row)
{
    for(; control; control=control->getParent())
    {
        if(control == row)
            return true;
    }
    return false;
}

// A row that shows another item now, or none: the mouse it holds (a press on
// its old item) and the keyboard focus are taken from it, so neither acts on
// the item it shows now. A dropped press is a capture ending: the canvas
// hit-tests the hover again.
void VirtualList::releaseRow(Base *control)
{
    if(MouseFocus && inRow(MouseFocus, control))
    {
        Base *held=MouseFocus;

        MouseFocus=nullptr;
        if(Button *button=gweni_cast<Button>(held))
            button->setDepressed(false);
    }

    if(KeyboardFocus && inRow(KeyboardFocus, control))
        KeyboardFocus->unfocus();
}

// The viewport and the offset are final here (placeContents), and nothing in
// the inner panel has been laid out yet. A pending scrollToItem is resolved
// first, with the viewport now known. Each item the viewport shows any of gets
// a row: a row already showing an item that is still in view keeps it,
// unbound; the rows left over take the items that came into view, in order,
// and are bound, and the rest (the spare row while the offset is a whole
// number of rows, the rows past the last item) are hidden. Every shown row is
// placed at its item's place.
// The pool is ceil(viewport/rowHeight)+1 rows. In a frame where that changes
// (a resize), the rows the items do not reach, left with no item, are deleted,
// whichever they are, and fresh rows are appended up to the new size, so after
// a shrink the spare is a row that was never bound, measured, placed or drawn,
// as a list built at that size has, not one shown at the larger size (issue
// #479); the kept rows stay in their order. A frame with the pool's size
// unchanged creates and deletes nothing.
// What is bound was measured in X with its old content, so it is measured
// again. A row given another item, or deleted, gives up its press and focus.
// When a shown row moved, was bound to another item, or was shown, hidden or
// deleted, what is under the resting mouse may be another row: the canvas
// hit-tests the hover again once the layout is done.
void VirtualList::contentsPlaced(skin::Base *skin)
{
    const int viewport=getViewportHeight();
    const bool variable=isVariableHeight();
    size_t poolSize=0;
    size_t first=0;
    size_t end=0;
    bool moved=false;

    if(variable)
    {
        placeVariable(first, end);
        poolSize=poolSizeFor(viewport);
        // every item in view is at least the smallest height high: this
        // only guards the pool
        end=std::min(end, first+poolSize);
    }
    else
    {
        poolSize=poolSizeFor(viewport);

        if(m_pendingItem != NoItem)
        {
            const size_t item=std::min(m_pendingItem, m_itemCount > 0?m_itemCount-1:0);
            const int64_t offset=offsetShowing(item, m_rowHeight, m_scrollOffset[1], viewport);

            m_pendingItem=NoItem;
            m_scrollOffset[1]=static_cast<int>(gweni::clamp<int64_t>(offset, 0, getMaxVerticalScrollOffset()));
            showScrollOffset(Dim::Y);
        }

        if(m_stuckToEnd)
        {
            m_scrollOffset[1]=getMaxVerticalScrollOffset();
            showScrollOffset(Dim::Y);
        }

        const int offset=getVerticalScrollOffset();

        first=static_cast<size_t>(offset/m_rowHeight);
        // the items the viewport shows any of: the pool's size at most
        end=std::min(m_itemCount, std::min(first+poolSize,
            static_cast<size_t>((static_cast<int64_t>(offset)+viewport+m_rowHeight-1)/m_rowHeight)));
    }

    const int offset=getVerticalScrollOffset();
    const size_t inView=end > first?end-first:0;

    if(m_rowsEdited)
    {
        // a row an edit took the item of shows something else or nothing
        moved=true;
        m_rowsEdited=false;
    }
    std::vector<bool> held(inView, false);
    std::vector<bool> keeps(m_rows.size(), false);
    size_t kept=0;

    // Rows still showing an item in view keep it.
    for(size_t i=0; i < m_rows.size(); ++i)
    {
        const size_t item=m_rows[i].item;

        if(item != NoItem && item >= first && item < end && !held[item-first])
        {
            held[item-first]=true;
            keeps[i]=true;
            ++kept;
        }
    }

    if(m_rows.size() != poolSize)
    {
        // The items that came into view go to the first free rows; the free
        // rows past those are left with no item and deleted. What is left is
        // kept+min(free, incoming), which is inView at most, so the pool is
        // only ever topped up after.
        const size_t incoming=inView-kept;
        size_t freeSeen=0;
        size_t to=0;

        for(size_t i=0; i < m_rows.size(); ++i)
        {
            if(!keeps[i] && freeSeen++ >= incoming)
            {
                releaseRow(m_rows[i].control);
                delete m_rows[i].control;    // ~Base takes it out of the children, gives its ids back
                moved=true;
                continue;
            }

            m_rows[to]=m_rows[i];
            keeps[to]=keeps[i];
            ++to;
        }
        m_rows.resize(to);
        keeps.resize(to);

        while(m_rows.size() < poolSize)
        {
            Base *control=nullptr;

            {
                // a row's constructor sets its defaults, as anywhere
                PartScope part;

                control=m_factory?m_factory(m_innerPanel):m_innerPanel->newChild<internal::VirtualListRow>();
            }

            if(control->getParent() != m_innerPanel)
                control->setParent(m_innerPanel);

            control->setPlacement(Placement::Owner);
            m_rows.push_back({control, NoItem, true});
            keeps.push_back(false);
        }
    }

    std::vector<size_t> previous(m_rows.size());

    for(size_t i=0; i < m_rows.size(); ++i)
    {
        previous[i]=m_rows[i].item;
        if(!keeps[i])
            m_rows[i].item=NoItem;
    }

    // The items that came into view go to the free rows.
    size_t freeRow=0;

    for(size_t i=0; i < inView; ++i)
    {
        if(held[i])
            continue;

        while(m_rows[freeRow].item != NoItem)
            ++freeRow;

        m_rows[freeRow].item=first+i;
        m_rows[freeRow].stale=true;
    }

    const int width=m_innerPanel->getWidth();

    for(size_t i=0; i < m_rows.size(); ++i)
    {
        Row &row=m_rows[i];
        Base *control=row.control;

        if(row.item != previous[i])
        {
            releaseRow(control);
            moved=true;
        }

        if(row.item == NoItem)
        {
            // bound again whatever it shows when it comes back
            row.stale=true;
            control->setHidden(true);
            continue;
        }

        const bool wasHidden=control->hidden();
        const bool bind=row.stale;

        control->setHidden(false);
        row.stale=false;

        if(bind && m_binder)
            m_binder(control, row.item);

        if(bind || wasHidden)
            control->doCalculateSize(skin, Dim::X);

        const int64_t y=variable?m_viewTops[row.item-first]
            :static_cast<int64_t>(row.item)*m_rowHeight-offset;
        const int height=variable?m_heights.height(row.item):m_rowHeight;

        if(placeChild(control, 0, static_cast<int>(y), width, height) || wasHidden)
            moved=true;
    }

    if(moved)
    {
        if(Canvas *canvas=getCanvas())
            canvas->hoverMayHaveMoved();
    }
}

}//namespace controls
}//namespace gweni
