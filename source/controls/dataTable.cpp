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
// frame is over (an event handler of the part itself may be running).
void dropPart(Base *part)
{
    part->setHidden(true);

    if(part->getCanvas())
        part->delayedDelete();
    else
        delete part;
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

void DataTableBody::rowOrderChanged()
{
    m_innerPanel->invalidate();
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
        cell->setPadding(Padding(6, 3, 6, 3));
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

    m_body=newChild<internal::DataTableBody>();
    m_body->setPlacement(Placement::Owner);

    m_selection.onSelectionChange.add(this, &DataTable::groupSelectionChanged);

    setSize(400, 200);
}

// The rows outlive this body (Base deletes them): they no longer report to it.
DataTable::~DataTable()
{
    for(DataTableRow *row : m_rows)
        row->m_table=nullptr;
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

    for(DataTableRow *row : m_rows)
        row->setCellCount(count);

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

    for(DataTableRow *row : m_rows)
        row->invalidate();
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

    for(DataTableRow *row : m_rows)
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
    DataTableRow *row=m_body->newChild<DataTableRow>();

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
    if(rows.size() != m_rows.size() || rows == m_rows)
        return;

    std::vector<DataTableRow *> mine=m_rows;
    std::vector<DataTableRow *> given=rows;

    std::sort(mine.begin(), mine.end());
    std::sort(given.begin(), given.end());

    if(mine != given)
        return;

    for(DataTableRow *row : rows)
        row->bringToFront();

    m_body->rowOrderChanged();
    m_rows=rows;
    restripe();
}

DataTableRow *DataTable::getSelectedRow() const
{
    return gweni_cast<DataTableRow>(m_selection.getSelected());
}

void DataTable::setSelectedRow(DataTableRow *row)
{
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

// Every odd row of the row order is filled; a row whose stripe does not
// change is not marked.
void DataTable::restripe()
{
    for(size_t i=0; i < m_rows.size(); ++i)
        m_rows[i]->setStriped(m_striped && (i%2) == 1);
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
