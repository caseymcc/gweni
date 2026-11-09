/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/virtualList.h>
#include <gweni/controls/canvas.h>

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

    m_itemCount=count;
    updateContentHeight();
    // kept inside the new content with the viewport the last layout gave;
    // the next layout clamps it again to the one it gives
    setVerticalScrollOffset(getVerticalScrollOffset());
    invalidate();
}

void VirtualList::updateContentHeight()
{
    const uint64_t height=static_cast<uint64_t>(m_itemCount)*static_cast<uint64_t>(m_rowHeight);

    setVirtualContentHeight(static_cast<int>(std::min<uint64_t>(height, INT_MAX)));
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
    if(index >= m_itemCount)
        return;

    m_pendingItem=index;
    invalidate();
}

void VirtualList::scrollByRows(int rows)
{
    m_pendingItem=NoItem;

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

    return static_cast<size_t>((viewport+m_rowHeight-1)/m_rowHeight)+1;
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
    const size_t poolSize=poolSizeFor(viewport);
    bool moved=false;

    if(m_pendingItem != NoItem)
    {
        const size_t item=std::min(m_pendingItem, m_itemCount > 0?m_itemCount-1:0);
        const int64_t offset=offsetShowing(item, m_rowHeight, m_virtualOffset, viewport);

        m_pendingItem=NoItem;
        m_virtualOffset=static_cast<int>(gweni::clamp<int64_t>(offset, 0, getMaxVerticalScrollOffset()));
        showVirtualOffset();
    }

    const int offset=getVerticalScrollOffset();
    const size_t first=static_cast<size_t>(offset/m_rowHeight);
    // the items the viewport shows any of: the pool's size at most
    const size_t end=std::min(m_itemCount, std::min(first+poolSize,
        static_cast<size_t>((static_cast<int64_t>(offset)+viewport+m_rowHeight-1)/m_rowHeight)));
    const size_t inView=end > first?end-first:0;
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

        const int64_t y=static_cast<int64_t>(row.item)*m_rowHeight-offset;

        if(placeChild(control, 0, static_cast<int>(y), width, m_rowHeight) || wasHidden)
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
