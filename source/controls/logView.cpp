/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/logView.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/text.h>
#include <gweni/anim.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skin.h>

#include <algorithm>
#include <climits>
#include <map>

namespace gweni
{
namespace controls
{

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(LogViewChevron)
{
    setMouseInputEnabled(false);
}

void LogViewChevron::setLook(bool expanded, const Color &color)
{
    if(m_expanded == expanded && m_color == color)
        return;

    m_expanded=expanded;
    m_color=color;
    addStateChange(StateChange_Background);
}

GWENI_CONTROL_CONSTRUCTOR(LogViewRow)
{
    setWrap(true);
    setPadding(Padding(LogView::RowPadX, LogView::RowPadY, LogView::RowPadX, LogView::RowPadY));
    setTextPadding(Padding(0, 0, 0, 0));
}

void LogViewRow::fontChanged()
{
    m_text->textChanged();
    m_text->addStateChange(StateChange_Text);
    m_text->invalidate();
}

void LogViewRow::setSelected(bool selected)
{
    if(m_selected == selected)
        return;

    m_selected=selected;
    addStateChange(StateChange_Background);
}

// A row of an app's kind is as high as the kind's measurer says, its padding
// included: the measure is the height the bound content lays out at (the
// kind's contract), and the list holds it.
void LogViewRow::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(!m_content || dim != Dim::Y || !m_view)
        return;

    const size_t item=m_view->itemForRow(this);

    if(item != VirtualList::NoItem && item < m_view->getHeightIndex().size())
        m_preferredSize.height=preferredFromContent(m_view->getHeightIndex().height(item), Dim::Y);
}

// The chevron sits in the padding left of the title line; an app kind's
// content fills the row inside its padding.
void LogViewRow::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    if(m_chevron && !m_chevron->hidden())
        placeChild(m_chevron, LogView::RowPadX, m_chevron->getY(), LogView::ChevronColumn, m_chevron->getHeight());
    if(m_content)
        placeChild(m_content, m_padding.left, m_content->getY(),
            std::max(0, getWidth()-m_padding.left-m_padding.right), m_content->getHeight());
}

void LogViewRow::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    if(m_chevron && !m_chevron->hidden())
    {
        const int line=m_view?m_view->getLineHeight():m_chevron->getHeight();

        placeChild(m_chevron, m_chevron->getX(), m_padding.top, m_chevron->getWidth(), line);
    }
    if(m_content)
        placeChild(m_content, m_content->getX(), m_padding.top, m_content->getWidth(),
            std::max(0, getHeight()-m_padding.top-m_padding.bottom));
}

GWENI_CONTROL_CONSTRUCTOR(LogViewJumpButton)
{
    setKeyboardInputEnabled(false);
    setTabable(false);
}

void LogViewJumpButton::onPress(event::Info info)
{
    ParentClass::onPress(info);
    if(m_view)
        m_view->setFollowing(true);
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//LogView

namespace
{

String lineCount(size_t lines)
{
    return std::to_string(lines)+((lines == 1)?" line":" lines");
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(LogView)
{
    setPadding(Padding(FieldPadding, FieldPadding, FieldPadding, FieldPadding));
    // the view takes the mouse over its rows: they and the panel take none
    setMouseInputEnabled(true);
    m_innerPanel->setMouseInputEnabled(false);

    if(skin::Base *skin=getSkin())
        m_font=skin->getDefaultFont();

    setRowFactory([this](Base *parent) -> Base *
        {
            internal::LogViewRow *row=parent->newChild<internal::LogViewRow>();

            row->m_view=this;
            // the row's Text holds a pointer to m_font: setFont changes it in place
            row->setFont(m_font);
            return row;
        });
    setRowBinder([this](Base *row, size_t position) { bindRow(row, position); });
    setItemMeasurer([this](size_t position, int width) { return measureRow(position, width); });

    setKeyboardInputEnabled(true);
    addAccelerator("Ctrl + C", &LogView::onCopy);
    addAccelerator("Ctrl + A", &LogView::onSelectAll);
    setStickToEnd(true);
}

LogView::RowId LogView::append(const LogRow &row)
{
    // a bar the user moved since the last layout is taken by the list's
    // edits first (barScrollTaken())
    applyCap();

    const RowId id=m_nextId++;

    m_rows.push_back(Entry{id, row});
    insertItems(m_rows.size()-1, 1);

    // the count is relabelled once a frame, by the layout the append asked for
    if(!m_following)
    {
        ++m_sinceDetached;
        m_jumpCountStale=true;
    }
    return id;
}

bool LogView::replace(RowId id, const LogRow &row)
{
    const size_t position=rowPosition(id);

    if(position == NoItem)
        return false;

    const size_t before=rowLength(id);

    m_rows[position].row=row;
    itemChanged(position);

    // the characters on this row clamped to its new text, a range's high end
    // at its end kept at its end (its low end is not moved there: a selected
    // empty row has both ends at 0, its end)
    if(!m_selection.empty())
    {
        const size_t after=rowLength(id);
        bool changed=false;

        for(SelectionRange &range : m_selection)
        {
            const Position *high=forward(range)?&range.focus:&range.anchor;

            for(Position *end : {&range.anchor, &range.focus})
            {
                if(end->row != id)
                    continue;

                const size_t character=(end == high && end->character >= before)?after:std::min(end->character, after);

                changed=changed || character != end->character;
                end->character=character;
            }
        }
        if(changed)
            onSelectionChanged.call(this);
    }
    return true;
}

LogView::RowId LogView::prepend(const std::vector<LogRow> &rows)
{
    if(rows.empty())
        return NoRow;

    // rows[0], the oldest, gets the lowest id: below every one given so far
    const RowId first=m_lowestId-static_cast<RowId>(rows.size());

    m_lowestId=first;
    for(size_t i=rows.size(); i-- > 0;)
        m_rows.push_front(Entry{first+static_cast<RowId>(i), rows[i]});
    insertItems(0, rows.size());
    return first;
}

void LogView::clear()
{
    // an empty log has nothing to jump to
    setFollowing(true);

    if(m_rows.empty())
        return;

    m_rows.clear();
    removeItems(0, getItemCount());
    clearSelection();
}

void LogView::setMaxRows(size_t rows)
{
    m_maxRows=rows;
}

void LogView::applyCap()
{
    if(m_maxRows == 0 || m_rows.size() < m_maxRows)
        return;

    const size_t drop=m_rows.size()-(m_maxRows-1);

    m_rows.erase(m_rows.begin(), m_rows.begin()+static_cast<std::ptrdiff_t>(drop));
    removeItems(0, drop);

    // a range that held dropped rows keeps the rows that remain
    if(pruneSelection())
    {
        refreshRowSelection();
        onSelectionChanged.call(this);
    }
}

LogView::RowId LogView::firstRowId() const
{
    return m_rows.empty()?NoRow:m_rows.front().id;
}

LogView::RowId LogView::lastRowId() const
{
    return m_rows.empty()?NoRow:m_rows.back().id;
}

// Ids rise with the positions, and the appended rows (every id from the last
// drop on) are one run at the end, the prepended ones one run per prepend at
// the start: an id is its distance from the last row's or the first row's,
// and only an id between two prepends' runs with rows dropped from the
// earlier one needs the binary search.
size_t LogView::rowPosition(RowId id) const
{
    if(m_rows.empty() || id < m_rows.front().id || id > m_rows.back().id)
        return NoItem;

    const RowId fromBack=m_rows.back().id-id;

    if(static_cast<uint64_t>(fromBack) < m_rows.size())
    {
        const size_t position=m_rows.size()-1-static_cast<size_t>(fromBack);

        if(m_rows[position].id == id)
            return position;
    }

    const RowId fromFront=id-m_rows.front().id;

    if(static_cast<uint64_t>(fromFront) < m_rows.size())
    {
        const size_t position=static_cast<size_t>(fromFront);

        if(m_rows[position].id == id)
            return position;
    }

    const auto found=std::lower_bound(m_rows.begin(), m_rows.end(), id,
        [](const Entry &entry, RowId value) { return entry.id < value; });

    if(found == m_rows.end() || found->id != id)
        return NoItem;
    return static_cast<size_t>(found-m_rows.begin());
}

LogView::RowId LogView::rowIdAt(size_t position) const
{
    return (position < m_rows.size())?m_rows[position].id:NoRow;
}

const LogRow *LogView::getRow(RowId id) const
{
    const size_t position=rowPosition(id);

    return (position == NoItem)?nullptr:&m_rows[position].row;
}

void LogView::setFont(const gweni::Font &font)
{
    if(m_font == font)
        return;

    m_font=font;
    for(Base *row : getRows())
    {
        if(internal::LogViewRow *logRow=gweni_cast<internal::LogViewRow>(row))
            logRow->fontChanged();
    }
    refresh();
}

int LogView::getLineHeight()
{
    skin::Base *skin=getSkin();
    const renderer::Base *renderer=skin?skin->getRenderer():nullptr;

    if(m_lineHeight > 0 && m_metricsFont == m_font && m_metricsRenderer == renderer && renderer
        && m_metricsSerial == renderer->textLayoutSerial())
        return m_lineHeight;

    m_metricsFont=m_font;
    m_metricsRenderer=renderer;
    m_metricsSerial=renderer?renderer->textLayoutSerial():0;
    m_lineHeight=std::max(1, measureLineHeight(m_font));
    return m_lineHeight;
}

int LogView::heightForRows(int rows)
{
    return std::max(0, rows)*(getLineHeight()+2*RowPadY)+getPadding().top+getPadding().bottom;
}

String LogView::blockText(const LogRow &row, size_t previewLines)
{
    const size_t total=row.lines.size();
    const size_t shown=(row.collapsed && total > previewLines)?previewLines:total;
    String text=row.text;

    if(total > 0)
    {
        text+=(shown < total)?" ("+std::to_string(shown)+" of "+lineCount(total)+")"
            :" ("+lineCount(total)+")";
    }
    for(size_t i=0; i < shown; ++i)
        text+="\n"+row.lines[i];
    return text;
}

void LogView::setBlockPreviewLines(size_t lines)
{
    if(m_previewLines == lines)
        return;

    // a new measure generation, O(1): the next layout measures the rows on
    // screen and the idle re-measure the rest, as for a new width
    m_previewLines=lines;
    refresh();
}

bool LogView::setBlockExpanded(RowId id, bool expanded)
{
    const size_t position=rowPosition(id);

    if(position == NoItem || m_rows[position].row.kind != LogRow::Block)
        return false;

    LogRow &row=m_rows[position].row;

    if(row.collapsed != expanded)
        return true;

    row.collapsed=!expanded;
    itemChanged(position);
    return true;
}

bool LogView::toggleBlock(RowId id)
{
    const LogRow *row=getRow(id);

    return row && setBlockExpanded(id, row->collapsed);
}

void LogView::setRowKind(LogRow::Kind kind, RowKind handler)
{
    if(kind < LogRow::FirstAppKind)
        return;

    auto found=std::find_if(m_kinds.begin(), m_kinds.end(),
        [kind](const std::pair<LogRow::Kind, RowKind> &entry) { return entry.first == kind; });

    if(found != m_kinds.end())
    {
        found->second=std::move(handler);

        // the old factory's content is not the new binder's to fill: the
        // pooled rows make the kind's content again when they next show it
        for(size_t i=0; i < poolSize(); ++i)
        {
            internal::LogViewRow *row=gweni_cast<internal::LogViewRow>(poolRow(i));

            if(!row)
                continue;
            for(auto made=row->m_contents.begin(); made != row->m_contents.end();)
            {
                if(made->first != kind)
                {
                    ++made;
                    continue;
                }
                if(row->m_content == made->second)
                    row->m_content=nullptr;
                delete made->second;    // ~Base takes it out of the row, gives its ids back
                made=row->m_contents.erase(made);
            }
        }
    }
    else
        m_kinds.emplace_back(kind, std::move(handler));
    refresh();
}

const LogView::RowKind *LogView::rowKind(LogRow::Kind kind) const
{
    if(kind < LogRow::FirstAppKind)
        return nullptr;
    for(const auto &entry : m_kinds)
    {
        if(entry.first == kind)
            return (entry.second.factory && entry.second.measurer)?&entry.second:nullptr;
    }
    return nullptr;
}

internal::LogViewRow *LogView::rowControl(RowId id) const
{
    const size_t position=rowPosition(id);

    if(position == NoItem)
        return nullptr;
    return gweni_cast<internal::LogViewRow>(rowForItem(position));
}

LogView::RowId LogView::rowAt(const Point &canvasPoint, Point *local) const
{
    for(Base *row : getRows())
    {
        if(row->hidden())
            continue;

        const size_t position=itemForRow(row);

        if(position == NoItem || position >= m_rows.size())
            continue;

        const Point point=row->canvasPosToLocal(canvasPoint);

        if(point.x < 0 || point.y < 0 || point.x >= row->getWidth() || point.y >= row->getHeight())
            continue;
        if(local)
            *local=point;
        return m_rows[position].id;
    }
    return NoRow;
}

// A press on a block's title line (its chevron's height, the first line of
// its text) toggles it; anywhere else on a row it selects, and starts a drag.
void LogView::onMouseClickLeft(int x, int y, bool down)
{
    if(!down)
    {
        if(m_dragging)
        {
            m_dragging=false;
            if(MouseFocus == this)
                MouseFocus=nullptr;
            stopAutoScroll();
        }
        return;
    }

    Point local;
    const RowId id=rowAt(Point(x, y), &local);
    const LogRow *row=getRow(id);

    if(row && row->kind == LogRow::Block && local.y < RowPadY+getLineHeight())
    {
        toggleBlock(id);
        return;
    }

    if(!row)
    {
        if(!input::isShiftDown() && !input::isControlDown())
            clearSelection();
        return;
    }

    if(input::isControlDown())
    {
        toggleRowSelected(id);
        return;
    }

    if(input::isShiftDown())
        extendSelectionTo(id);
    else
        selectRow(id);

    m_dragging=true;
    m_dragPoint=canvasPosToLocal(Point(x, y));
    MouseFocus=this;
}

// The selection's focus follows the row under the mouse; past the viewport's
// top or bottom edge the auto-scroll moves the view, so the move itself
// scrolls nothing.
void LogView::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);

    if(!m_dragging || MouseFocus != this)
        return;

    m_dragPoint=canvasPosToLocal(Point(x, y));

    const RowId id=rowAtDrag(m_dragPoint);

    if(id != NoRow)
        extendSelectionTo(id);

    if(dragOverflow() != 0 && !m_autoScroll)
    {
#if GWENI_ANIMATE
        class AutoScroll: public anim::Animation
        {
        public:
            void think() override
            {
                static_cast<LogView *>(m_control)->autoScrollStep();
            }
        };

        m_autoScrollTime=platform::getTimeInSeconds();
        m_autoScrollCarry=0.0f;
        m_autoScroll=new AutoScroll();
        anim::add(this, m_autoScroll);
#endif
    }
}

int LogView::dragOverflow() const
{
    const Rect &viewport=getViewport();

    if(m_dragPoint.y < viewport.y)
        return m_dragPoint.y-viewport.y;
    if(m_dragPoint.y >= viewport.y+viewport.h)
        return m_dragPoint.y-(viewport.y+viewport.h-1);
    return 0;
}

// The item at the viewport's height under the point (clamped into it), by
// the height index from the scroll offset: right before the layout that
// places the rows there too.
LogView::RowId LogView::rowAtDrag(const Point &local) const
{
    const Rect &viewport=getViewport();
    const HeightIndex &heights=getHeightIndex();

    if(m_rows.empty() || heights.size() != m_rows.size() || viewport.h <= 0)
        return NoRow;

    const int y=gweni::clamp(local.y, viewport.y, viewport.y+viewport.h-1)-viewport.y;
    const size_t item=heights.itemAt(static_cast<int64_t>(getVerticalScrollOffset())+y);

    return m_rows[std::min(item, m_rows.size()-1)].id;
}

void LogView::stopAutoScroll()
{
#if GWENI_ANIMATE
    if(!m_autoScroll)
        return;

    anim::cancel(this, m_autoScroll);
    m_autoScroll=nullptr;
#endif
}

// Each frame the view moves by the time since the last one (the canvas
// clock, at most a tenth of a second of it) times the speed past the edge;
// the whole units are scrolled, the rest kept for the next frame. A scroll up
// detaches the view, one down to the end follows again; the selection's focus
// goes to the row under the mouse in the new view.
void LogView::autoScrollStep()
{
    const int overflow=dragOverflow();

    if(!m_dragging || MouseFocus != this || overflow == 0)
    {
        stopAutoScroll();
        return;
    }

    const float now=platform::getTimeInSeconds();
    const float elapsed=std::min(std::max(now-m_autoScrollTime, 0.0f), 0.1f);
    const float speed=AutoScrollBase+AutoScrollRate*static_cast<float>(std::abs(overflow));

    m_autoScrollTime=now;
    m_autoScrollCarry+=((overflow < 0)?-speed:speed)*elapsed;

    const int step=static_cast<int>(m_autoScrollCarry);

    m_autoScrollCarry-=static_cast<float>(step);
    if(step == 0)
        return;

    if(step < 0 && !fitsInView())
        setFollowing(false);
    if(!m_following)
    {
        setVerticalScrollOffset(std::max(0, getVerticalScrollOffset()+step));
        followIfAtEnd();
    }

    const RowId id=rowAtDrag(m_dragPoint);

    if(id != NoRow)
        extendSelectionTo(id);
}

// A right press selects the row under it unless it is selected already, so
// the menu's Copy copies what was pressed; the menu is the view's, made once.
void LogView::onMouseClickRight(int x, int y, bool down)
{
    if(!down)
        return;

    const RowId id=rowAt(Point(x, y));

    if(id != NoRow && !isRowSelected(id))
        selectRow(id);

    if(!getContextMenu())
    {
        if(Menu *menu=createContextMenu())
        {
            m_copyItem=menu->addItem("Copy");
            m_copyItem->setAction(this, &LogView::onCopy);
            menu->addItem("Select all")->setAction(this, &LogView::onSelectAll);
        }
    }
    if(m_copyItem)
        m_copyItem->setDisabled(!hasSelection());
}

bool LogView::onKeyEscape(bool down)
{
    if(!hasSelection())
        return false;
    if(down)
        clearSelection();
    return true;
}

void LogView::onCopy(event::Info)
{
    copySelection();
}

void LogView::onSelectAll(event::Info)
{
    selectAll();
}

String LogView::rowText(RowId id) const
{
    const LogRow *row=getRow(id);

    if(!row)
        return String();

    if(row->kind != LogRow::Block)
        return row->text;

    String text=row->text;

    for(const String &line : row->lines)
        text+="\n"+line;
    return text;
}

size_t LogView::rowLength(RowId id) const
{
    return rowText(id).size();
}

size_t LogView::positionAtOrAfter(RowId id) const
{
    const auto found=std::lower_bound(m_rows.begin(), m_rows.end(), id,
        [](const Entry &entry, RowId value) { return entry.id < value; });

    return static_cast<size_t>(found-m_rows.begin());
}

bool LogView::forward(const SelectionRange &range)
{
    return (range.anchor.row < range.focus.row)
        || (range.anchor.row == range.focus.row && range.anchor.character <= range.focus.character);
}

void LogView::ordered(const SelectionRange &range, Position &low, Position &high)
{
    const bool isForward=forward(range);

    low=isForward?range.anchor:range.focus;
    high=isForward?range.focus:range.anchor;
}

void LogView::selectRow(RowId id)
{
    if(!hasRow(id))
        return;

    const SelectionRange range{Position{id, 0}, Position{id, rowLength(id)}};

    if(m_selection.size() == 1 && m_selection[0].anchor == range.anchor && m_selection[0].focus == range.focus)
        return;

    m_selection.assign(1, range);
    selectionChanged();
}

// Whole rows: the end further down the log takes the row's length, the one
// further up character 0, so the range covers both rows whole either way.
void LogView::extendSelectionTo(RowId id)
{
    if(!hasRow(id))
        return;
    if(m_selection.empty())
    {
        selectRow(id);
        return;
    }

    SelectionRange &range=m_selection.back();
    SelectionRange extended=range;

    extended.focus.row=id;
    if(id >= range.anchor.row)
    {
        extended.anchor.character=0;
        extended.focus.character=rowLength(id);
    }
    else
    {
        extended.anchor.character=rowLength(range.anchor.row);
        extended.focus.character=0;
    }
    if(extended.anchor.row == id)
    {
        extended.anchor.character=0;
        extended.focus.character=rowLength(id);
    }

    if(extended.anchor == range.anchor && extended.focus == range.focus)
        return;

    range=extended;
    selectionChanged();
}

// A row in a range splits it round the row: the held rows before it and
// after it stay selected, as ranges of their own.
void LogView::toggleRowSelected(RowId id)
{
    const size_t position=rowPosition(id);

    if(position == NoItem)
        return;

    if(!isRowSelected(id))
    {
        m_selection.push_back(SelectionRange{Position{id, 0}, Position{id, rowLength(id)}});
        selectionChanged();
        return;
    }

    std::vector<SelectionRange> kept;

    for(const SelectionRange &range : m_selection)
    {
        Position low, high;

        ordered(range, low, high);
        if(id < low.row || id > high.row)
        {
            kept.push_back(range);
            continue;
        }
        if(low.row < id && position > 0)
        {
            const RowId before=m_rows[position-1].id;

            kept.push_back(SelectionRange{low, Position{before, rowLength(before)}});
        }
        if(high.row > id && position+1 < m_rows.size())
        {
            const RowId after=m_rows[position+1].id;

            kept.push_back(SelectionRange{Position{after, 0}, high});
        }
    }
    m_selection=std::move(kept);
    selectionChanged();
}

void LogView::selectAll()
{
    if(m_rows.empty())
        return;

    const RowId last=m_rows.back().id;

    m_selection.assign(1, SelectionRange{Position{m_rows.front().id, 0}, Position{last, rowLength(last)}});
    selectionChanged();
}

void LogView::clearSelection()
{
    if(m_selection.empty())
        return;

    m_selection.clear();
    selectionChanged();
}

bool LogView::isRowSelected(RowId id) const
{
    for(const SelectionRange &range : m_selection)
    {
        Position low, high;

        ordered(range, low, high);
        if(id >= low.row && id <= high.row)
            return true;
    }
    return false;
}

// Each row's selected span is the union of what each range covers of it: a
// row inside a range whole, an endpoint row from or to its character.
String LogView::getSelectedText() const
{
    std::map<size_t, std::pair<size_t, size_t>> spans;

    for(const SelectionRange &range : m_selection)
    {
        Position low, high;

        ordered(range, low, high);
        for(size_t position=positionAtOrAfter(low.row);
            position < m_rows.size() && m_rows[position].id <= high.row; ++position)
        {
            const RowId id=m_rows[position].id;
            const size_t length=rowLength(id);
            const size_t begin=(id == low.row)?std::min(low.character, length):0;
            const size_t end=(id == high.row)?std::min(high.character, length):length;
            auto found=spans.find(position);

            if(found == spans.end())
                spans.emplace(position, std::make_pair(begin, std::max(begin, end)));
            else
            {
                found->second.first=std::min(found->second.first, begin);
                found->second.second=std::max(found->second.second, end);
            }
        }
    }

    String text;
    bool first=true;

    for(const auto &span : spans)
    {
        if(!first)
            text+="\n";
        first=false;
        text+=rowText(m_rows[span.first].id).substr(span.second.first, span.second.second-span.second.first);
    }
    return text;
}

bool LogView::copySelection()
{
    if(!hasSelection())
        return false;

    platform::setClipboardText(getSelectedText());
    return true;
}

bool LogView::pruneSelection()
{
    if(m_selection.empty())
        return false;

    if(m_rows.empty())
    {
        m_selection.clear();
        return true;
    }

    const RowId first=m_rows.front().id;
    std::vector<SelectionRange> kept;
    bool changed=false;

    for(SelectionRange range : m_selection)
    {
        Position low, high;

        ordered(range, low, high);

        const size_t from=positionAtOrAfter(low.row);

        // no row of it held any more
        if(from >= m_rows.size() || m_rows[from].id > high.row)
        {
            changed=true;
            continue;
        }

        // a low end the cap dropped: the first row held, from its start
        if(low.row < first)
        {
            Position &end=(range.anchor == low)?range.anchor:range.focus;

            end=Position{first, 0};
            changed=true;
        }
        kept.push_back(range);
    }
    m_selection=std::move(kept);
    return changed;
}

void LogView::selectionChanged()
{
    pruneSelection();
    refreshRowSelection();
    onSelectionChanged.call(this);
}

void LogView::refreshRowSelection()
{
    for(Base *control : getRows())
    {
        internal::LogViewRow *row=gweni_cast<internal::LogViewRow>(control);
        const size_t position=itemForRow(control);

        if(row && position != NoItem && position < m_rows.size())
            row->setSelected(isRowSelected(m_rows[position].id));
    }
}

void LogView::setFollowing(bool follow)
{
    // a bar the user moved and nothing took yet is the user's say first, so
    // it is not taken (and acted on) inside this change
    takeBarScroll();

    if(m_following == follow)
        return;

    m_following=follow;
    m_sinceDetached=0;
    setStickToEnd(follow);
    updateJumpButton();

    event::Info info(this);

    info.integer=follow?1:0;
    onFollowingChanged.call(this, info);
}

String LogView::jumpText(size_t newRows)
{
    if(newRows == 0)
        return "Jump to latest";
    return "Jump to latest ("+std::to_string(newRows)+" new)";
}

void LogView::updateJumpButton()
{
    if(!m_jump)
    {
        if(m_following)
            return;

        // the view's own child, over the inner panel (which a new child
        // would otherwise go into, and scroll with the rows)
        Base *inner=m_innerPanel;
        PartScope part;

        m_innerPanel=nullptr;
        m_jump=newChild<internal::LogViewJumpButton>();
        m_innerPanel=inner;
        m_jump->m_view=this;
        m_jump->setPlacement(Placement::Owner);
    }

    m_jump->setHidden(m_following);
    if(!m_following)
        m_jump->setText(jumpText(m_sinceDetached));
    m_jumpCountStale=false;
}

bool LogView::scrollToRow(RowId id)
{
    const size_t position=rowPosition(id);

    if(position == NoItem)
        return false;

    // the last row, or any row of a log that fits, is at the end
    if(position+1 == m_rows.size() || fitsInView())
    {
        setFollowing(true);
        return true;
    }

    setFollowing(false);
    scrollToItem(position);
    return true;
}

// Wherever the list took the move (an edit, the idle re-measure, the layout):
// at the end the view follows, above it it detaches.
void LogView::barScrollTaken(float amount)
{
    setFollowing(amount >= 1.0f);
}

bool LogView::fitsInView() const
{
    return getMaxVerticalScrollOffset() == 0;
}

void LogView::followIfAtEnd()
{
    if(!m_following && getVerticalScrollOffset() >= getMaxVerticalScrollOffset())
        setFollowing(true);
}

void LogView::scrollByUnits(int delta)
{
    if(delta < 0 && !fitsInView())
        setFollowing(false);
    if(m_following)
        return;

    const int64_t offset=static_cast<int64_t>(getVerticalScrollOffset())+delta;

    setVerticalScrollOffset(static_cast<int>(gweni::clamp<int64_t>(offset, 0, INT_MAX)));
    followIfAtEnd();
}

// A notch up detaches before the list scrolls (stuck, it would scroll back
// to the end); a notch down that reaches the end follows again.
bool LogView::onMouseWheeled(int delta)
{
    const int before=getVerticalScrollOffset();

    if(delta > 0 && before > 0)
        setFollowing(false);

    const bool used=ParentClass::onMouseWheeled(delta);

    if(delta < 0)
        followIfAtEnd();
    return used;
}

bool LogView::onKeyUp(bool down)
{
    if(down)
        scrollByUnits(-getLineHeight());
    return true;
}

bool LogView::onKeyDown(bool down)
{
    if(down)
        scrollByUnits(getLineHeight());
    return true;
}

bool LogView::onKeyPageUp(bool down)
{
    if(down)
        scrollByUnits(-std::max(getLineHeight(), getViewportHeight()-getLineHeight()));
    return true;
}

bool LogView::onKeyPageDown(bool down)
{
    if(down)
        scrollByUnits(std::max(getLineHeight(), getViewportHeight()-getLineHeight()));
    return true;
}

bool LogView::onKeyHome(bool down)
{
    if(down && !m_rows.empty() && !fitsInView())
    {
        setFollowing(false);
        scrollToItem(0);
    }
    return true;
}

bool LogView::onKeyend(bool down)
{
    if(down)
        setFollowing(true);
    return true;
}

// The thumb dragged (its arrows and track do not come here: contentsPlaced
// sees them).
void LogView::onVeritcalBarMoved(event::Info info)
{
    takeBarScroll();
    ParentClass::onVeritcalBarMoved(info);
}

// The bar moved by its arrows or its track since the last layout, which
// nothing told the view of and nothing took since, is taken before
// placeContents() would: above the end it detaches the view
// (barScrollTaken()), so the list does not put it back at the end. What that
// changes (the stick, the jump button) is this pass's to lay out.
void LogView::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
    {
        PlacementScope placement;

        takeBarScroll();
    }
    ParentClass::arrange(skin, dim);
}

// A detached log whose rows all fit now (emptied, capped, replaced shorter,
// made taller) has nothing to jump to and follows again; what that hides is
// this pass's. The jump button is placed over the bottom of the viewport,
// centred, measured in X here when it was shown or relabelled after the X
// measuring.
void LogView::contentsPlaced(skin::Base *skin)
{
    ParentClass::contentsPlaced(skin);

    if(!m_following && fitsInView())
    {
        PlacementScope placement;

        setFollowing(true);
    }

    if(!m_jump || m_jump->hidden())
        return;

    if(m_jumpCountStale)
    {
        m_jump->setText(jumpText(m_sinceDetached));
        m_jumpCountStale=false;
    }
    m_jump->doCalculateSize(skin, Dim::X);

    const Rect &viewport=getViewport();
    const int width=std::min(viewport.w, m_jump->getPreferredSize().width);
    const int height=getLineHeight()+JumpPadY;

    placeChild(m_jump, viewport.x+(viewport.w-width)/2, viewport.y+viewport.h-height-JumpGap, width, height);
}

// The binder only sets the row's content: its text and colour, its padding,
// which children show and what they show (VirtualList's rule); the list lays
// the row out after it.
void LogView::bindRow(Base *control, size_t position)
{
    internal::LogViewRow *row=gweni_cast<internal::LogViewRow>(control);

    if(!row || position >= m_rows.size())
        return;

    const LogRow &content=m_rows[position].row;
    const RowKind *kind=rowKind(content.kind);

    row->setSelected(isRowSelected(m_rows[position].id));
    const bool block=(content.kind == LogRow::Block);
    Base *shown=nullptr;

    row->m_kind=content.kind;

    if(kind)
    {
        for(const auto &made : row->m_contents)
        {
            if(made.first == content.kind)
                shown=made.second;
        }
        if(!shown)
        {
            {
                PartScope part;

                shown=kind->factory(row);
            }
            if(shown->getParent() != row)
                shown->setParent(row);
            shown->setPlacement(Placement::Owner);
            row->m_contents.emplace_back(content.kind, shown);
        }
    }

    for(const auto &made : row->m_contents)
        made.second->setHidden(made.second != shown);
    row->m_content=shown;

    if(shown)
    {
        // the text is not drawn: it holds no glyphs
        row->setText(String());
        row->m_text->setHidden(true);
        if(kind->binder)
            kind->binder(shown, content);
    }
    else
    {
        row->m_text->setHidden(false);
        row->setText(block?blockText(content, m_previewLines):content.text);
    }

    if(content.hasColor)
        row->setTextColor(content.color);
    else
        row->makeColorNormal();

    const int left=RowPadX+(block?ChevronColumn:0);

    if(row->getPadding().left != left)
        row->setPadding(Padding(left, RowPadY, RowPadX, RowPadY));

    if(block && !row->m_chevron)
    {
        PartScope part;

        row->m_chevron=row->newChild<internal::LogViewChevron>();
        row->m_chevron->setPlacement(Placement::Owner);
    }
    if(row->m_chevron)
    {
        row->m_chevron->setHidden(!block);
        if(block)
            row->m_chevron->setLook(!content.collapsed, row->getTextColor());
    }
}

// The wrap the row's Label does inside its padding, or the kind's measure.
int LogView::measureRow(size_t position, int width)
{
    if(position >= m_rows.size())
        return getRowHeight();

    const LogRow &row=m_rows[position].row;

    if(const RowKind *kind=rowKind(row.kind))
        return std::max(1, kind->measurer(row, std::max(0, width-2*RowPadX)))+2*RowPadY;
    if(row.kind == LogRow::Block)
        return wrappedTextHeight(m_font, blockText(row, m_previewLines), width-2*RowPadX-ChevronColumn)
            +2*RowPadY;
    return wrappedTextHeight(m_font, row.text, width-2*RowPadX)+2*RowPadY;
}

}//namespace controls
}//namespace gweni
