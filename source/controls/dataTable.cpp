/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/dataTable.h>
#include <gweni/controls/canvas.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

namespace
{

// A part a control drops: hidden at once, deleted by the canvas once the
// frame is over (an event handler of the part itself may be running), or at
// once off any canvas.
void dropPart(Base *part)
{
    part->setHidden(true);
    part->delayedDelete();
}

}//namespace

namespace internal
{

///////////////////////////////////////////////////////////////////////////
//DataTableColumnGrip

GWENI_CONTROL_CONSTRUCTOR(DataTableColumnGrip)
{
    setMouseInputEnabled(true);
    setCursor(cursor::SizeWE);
}

// A press holds the mouse and takes the column's width as the drag's start;
// each move gives the column that width plus how far the mouse went. The start
// is the width the column is laid out at, which for the stretched last column
// is more than it asks for: its edge follows the mouse from where it is drawn.
void DataTableColumnGrip::onMouseClickLeft(int x, int /*y*/, bool down)
{
    if(isDisabled() || !m_table)
        return;

    const bool wasDragging=m_dragging;

    if(down)
    {
        m_dragging=true;
        m_startX=x;
        m_startWidth=m_table->getColumnLaidOutWidth(m_column);
        MouseFocus=this;
    }
    else
    {
        m_dragging=false;
        MouseFocus=nullptr;
    }

    if(m_dragging != wasDragging)
        redraw();
}

void DataTableColumnGrip::onMouseMoved(int x, int /*y*/, int /*deltaX*/, int /*deltaY*/)
{
    if(!m_dragging || !m_table)
        return;

    m_table->setColumnWidth(m_column, m_startWidth+(x-m_startX));
}

///////////////////////////////////////////////////////////////////////////
//DataTableHeaderCell

// The caption reads from the left, with the chevron's column kept on the
// right whether or not the column is sorted.
GWENI_CONTROL_CONSTRUCTOR(DataTableHeaderCell)
{
    setTextPadding(Padding(6, 3, ChevronInset+ChevronSize+ChevronInset, 3));
    setTextAlignment(Alignment::Left|Alignment::CenterV);

    m_grip=newChild<DataTableColumnGrip>();
    m_grip->setPlacement(Placement::Owner);
}

void DataTableHeaderCell::onPress(event::Info info)
{
    ParentClass::onPress(info);

    if(m_table)
        m_table->headerCellPressed(m_column);
}

void DataTableHeaderCell::setSortDirection(SortDirection direction)
{
    if(m_sort == direction)
        return;

    m_sort=direction;
    // the chevron, this cell only
    addStateChange(StateChange_Background);
}

// The grip is the cell's right edge: placed in X at the width the header just
// gave the cell, in Y at its full height.
void DataTableHeaderCell::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    const int width=std::min(DataTableColumnGrip::Width, getWidth());

    placeChild(m_grip, getWidth()-width, m_grip->getY(), width, m_grip->getHeight());
}

void DataTableHeaderCell::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    placeChild(m_grip, m_grip->getX(), 0, m_grip->getWidth(), getHeight());
}

///////////////////////////////////////////////////////////////////////////
//DataTableHeader

GWENI_CONTROL_CONSTRUCTOR(DataTableHeader)
{
    setMouseInputEnabled(false);
}

// As wide as the columns ask for together and as high as the tallest caption;
// the cells are its owner-placed children, measured for their own subtrees.
void DataTableHeader::calculateSize(skin::Base *skin, Dim dim)
{
    int content=0;

    for(auto &&child : m_children)
    {
        if(child->hidden())
            continue;

        child->doCalculateSize(skin, dim);

        if(dim == Dim::Y)
            content=std::max(content, child->getPreferredSize().height);
    }

    if(dim == Dim::X)
    {
        content=(m_table?m_table->contentWidth():0)+m_padding.left+m_padding.right;
        m_preferredSize.width=preferredFromContent(content, Dim::X);
    }
    else
        m_preferredSize.height=preferredFromContent(content+m_padding.top+m_padding.bottom, Dim::Y);
}

const Rect *DataTableHeader::getChildClipRect(const Base * /*child*/) const
{
    return &m_clip;
}

// Set by the DataTable once the body is laid out, before this header is
// arranged: what it changes is placement. A new clip with the same cell
// bounds still re-stamps the cells' slots with it.
void DataTableHeader::setTrack(int origin, int width, int clipX, int clipWidth)
{
    const Rect clip(clipX, 0, clipWidth, getHeight());

    if(m_origin != origin || m_trackWidth != width)
    {
        m_origin=origin;
        m_trackWidth=width;

        PlacementScope placement;
        invalidate();
    }

    if(!(clip == m_clip))
    {
        m_clip=clip;

        for(DataTableHeaderCell *cell : m_cells)
            cell->addStateChangeRecursive(StateChange_Geometry);
    }
}

// Each cell at its column of the body's content, which starts at m_origin.
void DataTableHeader::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    if(!m_table)
        return;

    std::vector<int> x, w;

    m_table->columnSpans(m_trackWidth, x, w);

    for(size_t i=0; i < m_cells.size() && i < x.size(); ++i)
    {
        DataTableHeaderCell *cell=m_cells[i];

        placeChild(cell, m_origin+x[i], cell->getY(), w[i], cell->getHeight());
    }
}

void DataTableHeader::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    for(DataTableHeaderCell *cell : m_cells)
        placeChild(cell, cell->getX(), 0, cell->getWidth(), getHeight());
}

///////////////////////////////////////////////////////////////////////////
//DataTableBody

// The rows meet the header and the table's edges: no inset around them.
GWENI_CONTROL_CONSTRUCTOR(DataTableBody)
{
    setScroll(true, true);
    setAutoHideBars(true);
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
}

///////////////////////////////////////////////////////////////////////////
//DataTableVirtualBody

// A VirtualList that scrolls both ways: the rows sideways past the viewport
// when the columns are wider than it, as the row-mode body's do.
GWENI_CONTROL_CONSTRUCTOR(DataTableVirtualBody)
{
    setScroll(true, true);
}

// The pooled rows are owner-placed, so the inner panel's preferred width is
// none of theirs: the content is as wide as the columns ask for together,
// which is what a row asks for (DataTableRow::calculateSize).
Size DataTableVirtualBody::contentSize() const
{
    Size content=ParentClass::contentSize();

    if(m_table)
        content.width=m_table->contentWidth();
    return content;
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//DataTableRow

// The row's own string is not shown: its cells are its text.
GWENI_CONTROL_CONSTRUCTOR(DataTableRow)
{
    m_text->setHidden(true);
}

DataTableRow::~DataTableRow()
{
    if(m_table)
        m_table->rowDeleted(this);
}

void DataTableRow::setCellCount(int count)
{
    count=std::max(count, 0);

    if(static_cast<int>(m_cells.size()) == count)
        return;

    while(static_cast<int>(m_cells.size()) > count)
    {
        Label *cell=m_cells.back();

        m_cells.pop_back();
        dropPart(cell);
    }

    while(static_cast<int>(m_cells.size()) < count)
    {
        Label *cell=newChild<Label>();

        cell->setPlacement(Placement::Owner);
        cell->setPadding(Padding(CellPaddingX, CellPaddingY, CellPaddingX, CellPaddingY));
        cell->setTextAlignment(Alignment::Left|Alignment::CenterV);
        cell->setTextColor(getTextColor());
        m_cells.push_back(cell);
    }

    invalidate();
    // the column rules, one per cell but the last
    addStateChange(StateChange_Background);
}

void DataTableRow::setCellText(int column, const String &text)
{
    if(Label *cell=getCell(column))
        cell->setText(text);
}

const String &DataTableRow::getCellText(int column) const
{
    static const String empty;

    if(Label *cell=getCell(column))
        return cell->getText();
    return empty;
}

void DataTableRow::setCellContents(int column, Base *control)
{
    Label *cell=getCell(column);

    if(!cell || !control)
        return;

    control->setParent(cell);
}

Label *DataTableRow::getCell(int column) const
{
    if(column < 0 || column >= static_cast<int>(m_cells.size()))
        return nullptr;
    return m_cells[column];
}

void DataTableRow::setStriped(bool striped)
{
    if(m_striped == striped)
        return;

    m_striped=striped;
    // the fill, this row only
    addStateChange(StateChange_Background);
}

// As wide as the table's columns ask for together (the body scrolls sideways
// past that), as high as its tallest cell. The cells are owner-placed: they
// are measured for their own subtrees, but do not count as Base counts them.
void DataTableRow::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        const int content=(m_table?m_table->contentWidth():0)+m_padding.left+m_padding.right;

        m_preferredSize.width=preferredFromContent(content, Dim::X);
        return;
    }

    int height=0;

    for(Label *cell : m_cells)
    {
        if(!cell->hidden())
            height=std::max(height, cell->getPreferredSize().height);
    }

    m_preferredSize.height=preferredFromContent(height+m_padding.top+m_padding.bottom, Dim::Y);
}

// The cells at the table's columns across this row's width, which is the
// body's content width: the header places its cells at the same spans.
void DataTableRow::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    if(!m_table)
        return;

    std::vector<int> x, w;

    m_table->columnSpans(getWidth()-m_padding.left-m_padding.right, x, w);

    bool moved=false;

    for(size_t i=0; i < m_cells.size() && i < x.size(); ++i)
    {
        Label *cell=m_cells[i];

        if(placeChild(cell, m_padding.left+x[i], cell->getY(), w[i], cell->getHeight()))
            moved=true;
    }

    // the column rules are drawn at the cells' edges
    if(moved && m_table->hasColumnBorders())
        addStateChange(StateChange_Background);
}

void DataTableRow::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int height=std::max(0, getHeight()-m_padding.top-m_padding.bottom);

    for(Label *cell : m_cells)
        placeChild(cell, cell->getX(), m_padding.top, cell->getWidth(), height);
}

void DataTableRow::onPress(event::Info info)
{
    if(!m_table || !m_table->isSourceMode())
    {
        ParentClass::onPress(info);
        return;
    }

    if(isDisabled())
        return;

    m_table->itemRowPressed(this);
    Button::onPress(info);
}

void DataTableRow::updateColors()
{
    ParentClass::updateColors();

    const Color &color=getTextColor();

    for(Label *cell : m_cells)
        cell->setTextColor(color);
}

///////////////////////////////////////////////////////////////////////////
//DataTable

// The header and the body are placed by the table (arrange), the header above
// the body. A default size, which the app overrides or a dock replaces: with
// no request the table would be as big as all its rows.
GWENI_CONTROL_CONSTRUCTOR(DataTable)
{
    setMouseInputEnabled(false);
    setPadding(Padding(1, 1, 1, 1));

    m_header=newChild<internal::DataTableHeader>();
    m_header->m_table=this;
    m_header->setPlacement(Placement::Owner);

    m_rowBody=newChild<internal::DataTableBody>();
    m_rowBody->setPlacement(Placement::Owner);
    m_body=m_rowBody;

    m_selection.onSelectionChange.add(this, &DataTable::groupSelectionChanged);

    setSize(400, 200);
}

// The rows outlive this body (Base deletes them): they no longer report to it.
DataTable::~DataTable()
{
    for(DataTableRow *row : bodyRows())
        row->m_table=nullptr;
}

std::vector<DataTableRow *> DataTable::bodyRows() const
{
    if(!m_virtualBody)
        return m_rows;

    std::vector<DataTableRow *> rows;

    for(Base *control : m_virtualBody->getRows())
    {
        if(DataTableRow *row=gweni_cast<DataTableRow>(control))
            rows.push_back(row);
    }
    return rows;
}

void DataTable::setColumnCount(int count)
{
    count=std::max(count, 0);

    if(getColumnCount() == count)
        return;

    m_columns.resize(static_cast<size_t>(count));

    std::vector<internal::DataTableHeaderCell *> &cells=m_header->m_cells;

    while(static_cast<int>(cells.size()) > count)
    {
        internal::DataTableHeaderCell *cell=cells.back();

        cells.pop_back();
        dropPart(cell);
    }

    while(static_cast<int>(cells.size()) < count)
    {
        internal::DataTableHeaderCell *cell=m_header->newChild<internal::DataTableHeaderCell>();
        const int column=static_cast<int>(cells.size());

        cell->setPlacement(Placement::Owner);
        cell->m_table=this;
        cell->m_column=column;
        cell->m_grip->m_table=this;
        cell->m_grip->m_column=column;
        cell->setText(m_columns[column].caption);
        cells.push_back(cell);
    }

    for(DataTableRow *row : bodyRows())
        row->setCellCount(count);

    // a new column's cells are empty until the binder fills them
    if(m_virtualBody)
        m_virtualBody->refresh();

    if(m_sortColumn >= count)
        setSortColumn(-1, SortDirection::None, false);

    columnsChanged();
}

void DataTable::setColumnCaption(int column, const String &caption)
{
    if(column < 0 || column >= getColumnCount())
        return;

    m_columns[column].caption=caption;
    m_header->m_cells[column]->setText(caption);
}

const String &DataTable::getColumnCaption(int column) const
{
    static const String empty;

    if(column < 0 || column >= getColumnCount())
        return empty;
    return m_columns[column].caption;
}

void DataTable::setColumnWidth(int column, int width)
{
    if(column < 0 || column >= getColumnCount())
        return;

    Column &entry=m_columns[column];

    width=std::max(width, entry.minWidth);

    if(entry.width == width)
        return;

    entry.width=width;
    columnsChanged();
}

int DataTable::getColumnWidth(int column) const
{
    if(column < 0 || column >= getColumnCount())
        return 0;
    return m_columns[column].width;
}

void DataTable::setColumnMinWidth(int column, int width)
{
    if(column < 0 || column >= getColumnCount())
        return;

    Column &entry=m_columns[column];

    entry.minWidth=std::max(width, 0);

    if(entry.width < entry.minWidth)
    {
        entry.width=entry.minWidth;
        columnsChanged();
    }
}

int DataTable::getColumnMinWidth(int column) const
{
    if(column < 0 || column >= getColumnCount())
        return 0;
    return m_columns[column].minWidth;
}

int DataTable::getColumnLaidOutWidth(int column) const
{
    if(column < 0 || column >= getColumnCount())
        return 0;
    return m_header->m_cells[column]->getWidth();
}

void DataTable::setLastColumnStretch(bool stretch)
{
    if(m_stretchLast == stretch)
        return;

    m_stretchLast=stretch;
    columnsChanged();
}

void DataTable::columnSpans(int width, std::vector<int> &x, std::vector<int> &w) const
{
    const size_t count=m_columns.size();
    int at=0;

    x.resize(count);
    w.resize(count);

    for(size_t i=0; i < count; ++i)
    {
        x[i]=at;
        w[i]=m_columns[i].width;
        at+=w[i];
    }

    if(m_stretchLast && count > 0 && at < width)
        w[count-1]+=width-at;
}

int DataTable::contentWidth() const
{
    int width=0;

    for(const Column &column : m_columns)
        width+=column.width;
    return width;
}

// The rows' and the header's preferred widths are the columns' sum, and they
// place their cells at the columns: each is laid out again. A row's cells
// before the changed column keep their bounds, so they are not marked.
void DataTable::columnsChanged()
{
    m_header->invalidate();

    for(DataTableRow *row : bodyRows())
        row->invalidate();

    // the content's width, which the source-mode body takes from the columns
    // and not from its rows (an empty table has none)
    if(m_virtualBody)
        m_virtualBody->invalidate();
}

void DataTable::headerCellPressed(int column)
{
    const SortDirection direction=(column == m_sortColumn && m_sortDirection == SortDirection::Ascending)?
        SortDirection::Descending:SortDirection::Ascending;

    setSortColumn(column, direction, true);
}

void DataTable::setSortColumn(int column, SortDirection direction, bool doEvents)
{
    if(column < 0 || column >= getColumnCount() || direction == SortDirection::None)
    {
        column=-1;
        direction=SortDirection::None;
    }

    if(column == m_sortColumn && direction == m_sortDirection)
        return;

    const std::vector<internal::DataTableHeaderCell *> &cells=m_header->m_cells;

    if(m_sortColumn >= 0 && m_sortColumn < static_cast<int>(cells.size()))
        cells[m_sortColumn]->setSortDirection(SortDirection::None);

    m_sortColumn=column;
    m_sortDirection=direction;

    if(column >= 0)
        cells[column]->setSortDirection(direction);

    if(!doEvents)
        return;

    event::Info info(this);

    info.integer=(column >= 0)?static_cast<size_t>(column):static_cast<size_t>(-1);
    info.point=Point(column, direction == SortDirection::Ascending?1:
        (direction == SortDirection::Descending?-1:0));
    onSortChanged.call(this, info);
}

void DataTable::setStriped(bool striped)
{
    if(m_striped == striped)
        return;

    m_striped=striped;
    restripe();
}

void DataTable::setBorders(DataTableBorders borders)
{
    if(m_borders == borders)
        return;

    m_borders=borders;

    for(DataTableRow *row : bodyRows())
        row->addStateChange(StateChange_Background);
}

bool DataTable::hasRowBorders() const
{
    return (static_cast<int>(m_borders) & static_cast<int>(DataTableBorders::Rows)) != 0;
}

bool DataTable::hasColumnBorders() const
{
    return (static_cast<int>(m_borders) & static_cast<int>(DataTableBorders::Columns)) != 0;
}

// Docked Top in the body's inner panel (a Selectable docks Top), after the
// other rows.
DataTableRow *DataTable::addRow()
{
    if(isSourceMode())
        return nullptr;

    DataTableRow *row=m_rowBody->newChild<DataTableRow>();

    row->m_table=this;
    row->setCellCount(getColumnCount());
    m_selection.add(row);
    m_rows.push_back(row);
    row->setStriped(m_striped && (m_rows.size()%2) == 0);
    return row;
}

void DataTable::removeRow(DataTableRow *row)
{
    auto it=std::find(m_rows.begin(), m_rows.end(), row);

    if(it == m_rows.end())
        return;

    m_rows.erase(it);
    row->m_table=nullptr;
    // onSelectionChange fires if it was the selected row
    m_selection.remove(row);
    dropPart(row);
    restripe();
}

void DataTable::clear()
{
    std::vector<DataTableRow *> rows;

    rows.swap(m_rows);

    if(m_selection.getSelected())
        setSelectedRow(nullptr);

    for(DataTableRow *row : rows)
    {
        row->m_table=nullptr;
        m_selection.remove(row);
        dropPart(row);
    }
}

DataTableRow *DataTable::getRow(int index) const
{
    if(index < 0 || index >= rowCount())
        return nullptr;
    return m_rows[index];
}

int DataTable::getRowIndex(const DataTableRow *row) const
{
    auto it=std::find(m_rows.begin(), m_rows.end(), row);

    return (it == m_rows.end())?-1:static_cast<int>(it-m_rows.begin());
}

// The rows are docked Top in the inner panel, in its child order: each is
// brought to its end in turn, which leaves them in the new order.
void DataTable::setRowOrder(const std::vector<DataTableRow *> &rows)
{
    if(isSourceMode() || rows.size() != m_rows.size() || rows == m_rows)
        return;

    std::vector<DataTableRow *> mine=m_rows;
    std::vector<DataTableRow *> given=rows;

    std::sort(mine.begin(), mine.end());
    std::sort(given.begin(), given.end());

    if(mine != given)
        return;

    for(DataTableRow *row : rows)
        row->bringToFront();

    m_rows=rows;
    restripe();
}

DataTableRow *DataTable::getSelectedRow() const
{
    if(isSourceMode())
        return rowForItem(m_selectedItem);

    return gweni_cast<DataTableRow>(m_selection.getSelected());
}

void DataTable::setSelectedRow(DataTableRow *row)
{
    if(isSourceMode())
    {
        if(!row)
            setSelectedItem(NoItem);
        else if(row->m_table == this)
            setSelectedItem(itemForRow(row));
        return;
    }

    if(!row)
    {
        if(Selectable *selected=m_selection.getSelected())
            selected->setSelected(false);
        return;
    }

    if(row->m_table == this)
        row->setSelected(true);
}

void DataTable::groupSelectionChanged(event::Info)
{
    event::Info info(this);

    info.control=getSelectedRow();
    onSelectionChange.call(this, info);
}

// A row deleted outright, not through removeRow(): the group forgets it
// itself (without the event).
void DataTable::rowDeleted(DataTableRow *row)
{
    auto it=std::find(m_rows.begin(), m_rows.end(), row);

    if(it == m_rows.end())
        return;

    m_rows.erase(it);
    restripe();
}

// Every odd row of the row order is filled (in source mode, every row showing
// an odd item); a row whose stripe does not change is not marked.
void DataTable::restripe()
{
    if(m_virtualBody)
    {
        for(DataTableRow *row : bodyRows())
        {
            const size_t item=itemForRow(row);

            row->setStriped(m_striped && item != NoItem && (item%2) == 1);
        }
        return;
    }

    for(size_t i=0; i < m_rows.size(); ++i)
        m_rows[i]->setStriped(m_striped && (i%2) == 1);
}

///////////////////////////////////////////////////////////////////////////
// Source mode

// The row-mode body goes with its rows (deleted with it once the frame is
// over); the selection they held is dropped first, with its event. The
// VirtualList body takes its place, after the header in the child order as
// the old one was, and makes DataTableRows of the table's columns.
void DataTable::enterSourceMode()
{
    if(m_virtualBody)
        return;

    if(m_selection.getSelected())
        setSelectedRow(nullptr);

    for(DataTableRow *row : m_rows)
    {
        row->m_table=nullptr;
        m_selection.remove(row);
    }
    m_rows.clear();

    dropPart(m_rowBody);
    m_rowBody=nullptr;

    m_virtualBody=newChild<internal::DataTableVirtualBody>();
    m_virtualBody->m_table=this;
    m_virtualBody->setPlacement(Placement::Owner);
    m_virtualBody->setRowHeight(m_rowHeight);
    m_virtualBody->setRowFactory([this](Base *parent) -> Base *
        {
            DataTableRow *row=newSourceRow(parent);

            // the table holds the keyboard focus, not a pooled row, which
            // gives it up when it is bound to another item
            row->setKeyboardInputEnabled(false);
            row->setTabable(false);
            row->m_table=this;
            row->setCellCount(getColumnCount());
            return row;
        });
    m_virtualBody->setRowBinder([this](Base *row, size_t item) { bindRow(row, item); });
    m_body=m_virtualBody;

    // a press on a row focuses the table (its first ancestor taking keys),
    // which takes the row keys
    setKeyboardInputEnabled(true);
    setTabable(true);
}

DataTableRow *DataTable::newSourceRow(Base *parent)
{
    return parent->newChild<DataTableRow>();
}

// The row takes the stripe and the selection of its item, as the row-mode
// rows have them by their place, before the app fills its cells.
void DataTable::bindRow(Base *control, size_t item)
{
    DataTableRow *row=gweni_cast<DataTableRow>(control);

    if(!row)
        return;

    row->setStriped(m_striped && (item%2) == 1);
    row->setSelected(item == m_selectedItem, false);

    if(m_rowBinder)
        m_rowBinder(row, item);
}

void DataTable::itemRowPressed(DataTableRow *row)
{
    const size_t item=itemForRow(row);

    if(item != NoItem)
        setSelectedItem(item);
}

void DataTable::setItemCount(size_t count)
{
    enterSourceMode();
    m_virtualBody->setItemCount(count);

    if(m_selectedItem != NoItem && m_selectedItem >= count)
        setSelectedItem(NoItem);
}

size_t DataTable::getItemCount() const
{
    return m_virtualBody?m_virtualBody->getItemCount():m_rows.size();
}

void DataTable::setRowBinder(RowBinder binder)
{
    enterSourceMode();
    m_rowBinder=std::move(binder);
    m_virtualBody->refresh();
}

void DataTable::rebind()
{
    if(m_virtualBody)
        m_virtualBody->refresh();
}

void DataTable::rebindItem(size_t item)
{
    if(m_virtualBody)
        m_virtualBody->refreshItem(item);
}

void DataTable::setRowHeight(int height)
{
    m_rowHeight=std::max(height, 1);

    if(m_virtualBody)
        m_virtualBody->setRowHeight(m_rowHeight);
}

// The selection is the item: the row showing the old one (if it is on
// screen) shows it no more, the row showing the new one does, and a row that
// comes to show either later is bound with it (bindRow). The rows' own
// selection has no group and fires nothing.
void DataTable::setSelectedItem(size_t item, bool doEvents)
{
    if(!m_virtualBody)
        return;

    if(item >= getItemCount())
        item=NoItem;

    if(item == m_selectedItem)
        return;

    if(DataTableRow *previous=rowForItem(m_selectedItem))
        previous->setSelected(false, false);

    m_selectedItem=item;

    DataTableRow *row=rowForItem(item);

    if(row)
        row->setSelected(true, false);

    selectedItemChanged(row, item, doEvents);
}

void DataTable::selectedItemChanged(DataTableRow *row, size_t item, bool doEvents)
{
    if(!doEvents)
        return;

    event::Info info(this);

    info.control=row;
    info.integer=item;
    onSelectionChange.call(this, info);
}

///////////////////////////////////////////////////////////////////////////
// Source mode's row keys

size_t DataTable::pageRows() const
{
    if(!m_virtualBody)
        return 1;

    return static_cast<size_t>(std::max(1, m_virtualBody->getViewportHeight()/m_rowHeight));
}

void DataTable::moveSelectionTo(size_t item)
{
    setSelectedItem(item, true);
    scrollToItem(m_selectedItem);
}

// Up and Down a row, Home and End to the first and the last, PageUp and
// PageDown the rows the viewport shows whole, the selection kept in view. With
// nothing selected every one of them selects the first item but End, which
// selects the last. A release of one of them is taken too, so it does not go
// on to an ancestor; in row mode and on an empty table they go on.
bool DataTable::rowKey(int key, bool down)
{
    const size_t count=getItemCount();

    if(!m_virtualBody || count == 0)
        return false;
    if(!down)
        return true;

    const size_t last=count-1;
    const size_t at=m_selectedItem;
    const bool none=(at == NoItem);
    size_t item=none?0:at;

    switch(key)
    {
    case keys::Up:
        if(!none && at > 0)
            item=at-1;
        break;
    case keys::Down:
        if(!none)
            item=std::min(at+1, last);
        break;
    case keys::PageUp:
        if(!none)
            item=(at > pageRows())?at-pageRows():0;
        break;
    case keys::PageDown:
        if(!none)
            item=std::min(at+pageRows(), last);
        break;
    case keys::Home:
        item=0;
        break;
    case keys::End:
        item=last;
        break;
    default:
        return false;
    }

    moveSelectionTo(item);
    return true;
}

bool DataTable::onKeyUp(bool down)
{
    return rowKey(keys::Up, down);
}

bool DataTable::onKeyDown(bool down)
{
    return rowKey(keys::Down, down);
}

bool DataTable::onKeyHome(bool down)
{
    return rowKey(keys::Home, down);
}

bool DataTable::onKeyend(bool down)
{
    return rowKey(keys::End, down);
}

bool DataTable::onKeyPageUp(bool down)
{
    return rowKey(keys::PageUp, down);
}

bool DataTable::onKeyPageDown(bool down)
{
    return rowKey(keys::PageDown, down);
}

DataTableRow *DataTable::rowForItem(size_t item) const
{
    if(!m_virtualBody || item == NoItem)
        return nullptr;

    return gweni_cast<DataTableRow>(m_virtualBody->rowForItem(item));
}

size_t DataTable::itemForRow(const DataTableRow *row) const
{
    return m_virtualBody?m_virtualBody->itemForRow(row):NoItem;
}

void DataTable::scrollToItem(size_t item)
{
    if(m_virtualBody)
        m_virtualBody->scrollToItem(item);
}

// The header and the body, stacked; with a size asked for that is the size.
void DataTable::calculateSize(skin::Base *skin, Dim dim)
{
    m_header->doCalculateSize(skin, dim);
    m_body->doCalculateSize(skin, dim);

    const Size header=m_header->hidden()?Size(0, 0):m_header->getPreferredSize();
    const Size &body=m_body->getPreferredSize();
    const Rect &requested=getRequestedBounds();

    if(dim == Dim::X)
    {
        const int content=std::max(header.width, body.width)+m_padding.left+m_padding.right;

        m_preferredSize.width=preferredFromContent(sizedByRequest(Dim::X)?requested.w:content, Dim::X);
    }
    else
    {
        const int content=header.height+body.height+m_padding.top+m_padding.bottom;

        m_preferredSize.height=preferredFromContent(sizedByRequest(Dim::Y)?requested.h:content, Dim::Y);
    }
}

// The header's cells follow the body's content, whose width and scroll
// position the body only knows once it is laid out, and the body lays its
// subtree out in its Y arrange (ScrollControl). So, like a ScrollControl, the
// table leaves its subtree alone in X and does it all in Y, once its own size
// is final: the header and the body placed, the body laid out, then the header
// handed the content's origin and width and laid out X and then Y.
void DataTable::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
        return;

    stampLayoutPass();

    if(needsLayout())
    {
        m_needsLayout=false;
        layout(skin);
    }

    const Rect area(m_padding.left, m_padding.top,
        std::max(0, getWidth()-m_padding.left-m_padding.right),
        std::max(0, getHeight()-m_padding.top-m_padding.bottom));
    const int headerHeight=m_header->hidden()?0:std::min(area.h, m_header->getPreferredSize().height);

    placeChild(m_header, area.x, area.y, area.w, headerHeight);
    placeChild(m_body, area.x, area.y+headerHeight, area.w, area.h-headerHeight);

    m_body->doArrange(skin, Dim::X);
    m_body->doArrange(skin, Dim::Y);

    if(!m_header->hidden())
    {
        const Base *panel=m_body->getInnerPanel();
        const Rect &viewport=m_body->getViewport();

        {
            PlacementScope placement;

            m_header->setTrack(panel->getX(), panel->getWidth(), viewport.x, viewport.w);
        }

        m_header->doArrange(skin, Dim::X);
        m_header->doArrange(skin, Dim::Y);
    }

    postLayout(skin);
}

}//namespace controls
}//namespace gweni
