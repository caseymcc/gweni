/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_datatable_h_
#define _gweni_controls_datatable_h_

#include <gweni/controls/button.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/selectable.h>
#include <gweni/controls/virtualList.h>

#include <functional>
#include <vector>

namespace gweni
{
namespace controls
{

class DataTable;

//! A column's sort order, as its header shows it with a chevron.
enum class SortDirection
{
    None,
    Ascending,
    Descending
};

//! The rules a DataTable draws between its body cells: along each row's bottom,
//! between the columns, or both. The header always has its column rules (they
//! are where a column is resized).
enum class DataTableBorders
{
    None=0,
    Rows=1,
    Columns=2,
    All=3
};

namespace internal
{

//! The grab zone at a header cell's right edge: dragging it resizes the
//! column. It draws the column's rule along that edge, lit while hovered or
//! dragged, and shows the resize cursor.
class GWENI_EXPORT DataTableColumnGrip: public controls::Base
{
public:
    GWENI_CONTROL(DataTableColumnGrip, controls::Base);

public:
    //! The zone's width, in units, inside the cell's right edge.
    static constexpr int Width=5;

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

    bool isDragging() const { return m_dragging; }
    int getColumn() const { return m_column; }

private:
    friend class controls::DataTable;

    DataTable *m_table=nullptr;
    int m_column=0;
    bool m_dragging=false;
    int m_startX=0;
    int m_startWidth=0;
};

//! A column's header: its caption, and a chevron (up ascending, down
//! descending) in a column it always keeps at its right, so sorting never
//! changes its size. A click outside the grip sorts by the column.
class GWENI_EXPORT DataTableHeaderCell: public Button
{
public:
    GWENI_CONTROL(DataTableHeaderCell, Button);

public:
    //! The chevron's side, and the inset on each side of it: the right text
    //! padding is ChevronInset+ChevronSize+ChevronInset.
    static constexpr int ChevronSize=8;
    static constexpr int ChevronInset=6;

    int getColumn() const { return m_column; }
    SortDirection getSortDirection() const { return m_sort; }
    DataTableColumnGrip *getGrip() const { return m_grip; }

    void onPress(event::Info info) override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class controls::DataTable;

    void setSortDirection(SortDirection direction);

    DataTable *m_table=nullptr;
    DataTableColumnGrip *m_grip=nullptr;
    int m_column=0;
    SortDirection m_sort=SortDirection::None;
};

//! The header strip: one cell per column, placed at the columns of the body's
//! scrolled content (the DataTable hands it the content's origin and width
//! after laying the body out), and clipped to the body's viewport.
class GWENI_EXPORT DataTableHeader: public controls::Base
{
public:
    GWENI_CONTROL(DataTableHeader, controls::Base);

public:
    void calculateSize(skin::Base *skin, Dim dim) override;

    //! The cells are drawn and hit-tested only above the body's viewport.
    const Rect *getChildClipRect(const Base *child) const override;
    bool hasChildClipRects() const override { return true; }

    const std::vector<DataTableHeaderCell *> &getCells() const { return m_cells; }

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class controls::DataTable;

    //! Where the body's content starts (its inner panel's x, so the scroll
    //! offset is in it), how wide it is, and the viewport's x and width.
    void setTrack(int origin, int width, int clipX, int clipWidth);

    DataTable *m_table=nullptr;
    std::vector<DataTableHeaderCell *> m_cells;
    int m_origin=0;
    int m_trackWidth=0;
    Rect m_clip;
};

//! The ScrollControl the rows scroll in, with no inset: the rows meet the
//! header and the table's edges.
class GWENI_EXPORT DataTableBody: public ScrollControl
{
public:
    GWENI_CONTROL(DataTableBody, ScrollControl);

public:
    //! A container, as a ScrollControl: its rows and its bars draw.
    GWENI_DRAWS_NOTHING
};

//! The body of a DataTable in source mode: a VirtualList of pooled
//! DataTableRows, which also scrolls sideways when the columns are wider than
//! its viewport. Its rows are owner-placed and do not count in the inner
//! panel's size, so the content's width is the table's columns'. It draws
//! nothing: the table's backdrop shows through, as under the row-mode body.
class GWENI_EXPORT DataTableVirtualBody: public VirtualList
{
public:
    GWENI_CONTROL(DataTableVirtualBody, VirtualList);

protected:
    Size contentSize() const override;

private:
    friend class controls::DataTable;

    DataTable *m_table=nullptr;
};

}//namespace internal

//! A body row of a DataTable: one cell per column, each a Label, placed at the
//! table's columns. It is a Selectable in the table's SelectableGroup, so it
//! highlights while hovered and a click selects it; an idle row is filled only
//! when the table is striped and it is an odd row.
class GWENI_EXPORT DataTableRow: public Selectable
{
public:
    GWENI_CONTROL(DataTableRow, Selectable);

public:
    //! A cell's text padding: CellPaddingX on the left and right, CellPaddingY
    //! above and below.
    static constexpr int CellPaddingX=6;
    static constexpr int CellPaddingY=3;

    ~DataTableRow() override;

    //! The cell's text. Out of range does nothing (an empty string).
    void setCellText(int column, const String &text);
    const String &getCellText(int column) const;
    //! Puts a control in the cell (its parent becomes the cell's Label). The
    //! cell never takes the mouse, the control does as its own
    //! setMouseInputEnabled says: a click on a Button in a cell presses the
    //! Button and does not select the row, while a click on a control with no
    //! mouse input (a Label, an ImagePanel) or elsewhere on the cell does.
    void setCellContents(int column, Base *control);
    //! The cell's Label, or null out of range.
    Label *getCell(int column) const;
    int getCellCount() const { return static_cast<int>(m_cells.size()); }

    //! Whether the row is filled as an odd row of a striped table.
    bool isStriped() const { return m_striped; }
    //! The table the row belongs to, null once it is removed.
    DataTable *getTable() const { return m_table; }

    void calculateSize(skin::Base *skin, Dim dim) override;
    //! The row's colours, and its cells' text colours with them.
    void updateColors() override;

    //! In a source-mode table a press selects the item the row shows (through
    //! the table, not the row's own selection, which only shows it); in row
    //! mode it is a Selectable's press.
    void onPress(event::Info info) override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class DataTable;

    //! Makes or drops cells so there are `count`.
    void setCellCount(int count);
    void setStriped(bool striped);

    DataTable *m_table=nullptr;
    std::vector<Label *> m_cells;
    bool m_striped=false;
};

//! A table of records (imgui's BeginTable): a header row frozen above a body
//! of rows that scrolls in a ScrollControl, the header following the body's
//! horizontal scroll.
//!
//! Columns have a width (setColumnWidth, at least the column's minimum) and a
//! caption. With setLastColumnStretch (the default) the last column takes what
//! the others leave of the body's width, at least its own width; the stretch is
//! the last column's so that a column's new width moves only it and the columns
//! after it, while the columns fit the body (once they overflow it, every row
//! is as wide as the columns, so a new width changes every row's size, which
//! marks the row alone, not the cells before the column, while the body is
//! scrolled to its left edge). Dragging a header
//! cell's right edge (its grip) resizes the column from the width it is laid
//! out at, so the stretched last column's edge follows the mouse (dragged
//! left, the column still fills the body, at the narrower width it then asks
//! for). A click elsewhere on the cell sorts by it, ascending first and then
//! alternating, shows its chevron and fires onSortChanged. The table does not
//! sort: the app orders its records and shows them, by setting the rows' cells
//! again or by putting the rows in the new order (setRowOrder).
//!
//! The rows (addRow) are DataTableRows, one cell per column, single-selected
//! through the table's SelectableGroup. setStriped fills every odd row (by its
//! place in the row order), setBorders draws rules along the rows and between
//! the columns.
//!
//! Source mode: instead of a row per record, the table is given an item count
//! (setItemCount) and a row binder (setRowBinder), and its body is a
//! VirtualList of pooled DataTableRows, the rows on screen plus one, so a table
//! of 100 000 items has the controls, ids and slots of the rows it shows. The
//! binder fills a pooled row for an item, as the VirtualList's does: its cells'
//! text (setCellText) or contents. The rows are setRowHeight high. Everything
//! but the row API works as in row mode, by item index: the stripes are the
//! odd items', the selection is an item (getSelectedItem, setSelectedItem),
//! shown on whichever pooled row holds it, and a sort leaves the order to the
//! app, which reorders its own data and calls rebind() (only the rows on screen
//! are bound again). The first setItemCount or setRowBinder turns source mode
//! on for good: the rows addRow made are removed, and the row API (addRow,
//! removeRow, setRowOrder) does nothing.
class GWENI_EXPORT DataTable: public controls::Base
{
public:
    GWENI_CONTROL(DataTable, controls::Base);

public:
    static constexpr int DefaultColumnWidth=100;
    //! A source-mode row's height: a row-mode row's at the default skin's text.
    static constexpr int DefaultRowHeight=21;
    static constexpr size_t NoItem=VirtualList::NoItem;

    //! Fills a pooled row for the item at the given index (source mode).
    using RowBinder=std::function<void(DataTableRow *row, size_t item)>;
    //! Room for a short caption and the header's chevron column.
    static constexpr int DefaultMinColumnWidth=40;

    ~DataTable() override;

    void setColumnCount(int count);
    int getColumnCount() const { return static_cast<int>(m_columns.size()); }

    void setColumnCaption(int column, const String &caption);
    const String &getColumnCaption(int column) const;

    //! The width the column asks for, clamped to its minimum. A new width
    //! moves only this column's cells and the following columns' (and with the
    //! last column stretching, shrinks or grows that one).
    void setColumnWidth(int column, int width);
    int getColumnWidth(int column) const;
    void setColumnMinWidth(int column, int width);
    int getColumnMinWidth(int column) const;
    //! The column's width as the last layout placed it: the stretched last
    //! column's is what the body's width gave it.
    int getColumnLaidOutWidth(int column) const;

    void setLastColumnStretch(bool stretch);
    bool getLastColumnStretch() const { return m_stretchLast; }

    //! Sorts by the column (-1 or SortDirection::None for no sort column):
    //! its header shows the chevron, the previous one's goes, and with
    //! doEvents onSortChanged fires. The same column and direction again does
    //! nothing.
    void setSortColumn(int column, SortDirection direction, bool doEvents=true);
    int getSortColumn() const { return m_sortColumn; }
    SortDirection getSortDirection() const { return m_sortDirection; }

    //! Fired once per change of the sort column or direction: the info's
    //! `integer` is the column, its `point` (column, direction) with direction
    //! 1 ascending, -1 descending and 0 none.
    gweni::event::Caller onSortChanged;

    void setStriped(bool striped);
    bool isStriped() const { return m_striped; }

    void setBorders(DataTableBorders borders);
    DataTableBorders getBorders() const { return m_borders; }
    bool hasRowBorders() const;
    bool hasColumnBorders() const;

    //! A new row at the end, with one empty cell per column.
    DataTableRow *addRow();
    //! The row leaves the table (and the selection, if it was the selected
    //! one: onSelectionChange fires) and is deleted by the canvas.
    void removeRow(DataTableRow *row);
    void clear();
    int rowCount() const { return static_cast<int>(m_rows.size()); }
    //! The row at a place in the row order, or null.
    DataTableRow *getRow(int index) const;
    int getRowIndex(const DataTableRow *row) const;
    const std::vector<DataTableRow *> &getRows() const { return m_rows; }
    //! Puts the rows in this order (the same rows, each once; anything else is
    //! ignored). The stripes follow the new order; the selection stays with
    //! its row.
    void setRowOrder(const std::vector<DataTableRow *> &rows);

    //! The selected row. In source mode, the pooled row showing the selected
    //! item, or null when it is not on screen.
    DataTableRow *getSelectedRow() const;
    //! Selects the row (null: none). In source mode, the item it shows.
    void setSelectedRow(DataTableRow *row);
    //! Fired once per change of the selection, with the row (or null) as the
    //! info's `control`; in source mode the item is its `integer` (NoItem for
    //! none) and the control the row showing it, if any.
    gweni::event::Caller onSelectionChange;

    //! Source mode (see the class comment): how many items there are. It may
    //! change at any time, as a VirtualList's: the rows still showing their
    //! item keep it, the view stays inside the new count, and a selected item
    //! past the end is deselected (onSelectionChange fires).
    void setItemCount(size_t count);
    //! The item count in source mode, the row count in row mode.
    size_t getItemCount() const;
    //! Source mode: the binder that fills a pooled row for an item. Every row
    //! on screen is bound again.
    void setRowBinder(RowBinder binder);
    bool isSourceMode() const { return m_virtualBody != nullptr; }

    //! Source mode: the items changed (a sort reordered them, say): the rows
    //! on screen are bound again by the next layout, and only those.
    //! rebindItem binds only the row showing that item, if any.
    void rebind();
    void rebindItem(size_t item);

    //! Source mode: every row's height, in units (at least 1).
    void setRowHeight(int height);
    int getRowHeight() const { return m_rowHeight; }

    //! Source mode: the selected item, or NoItem.
    size_t getSelectedItem() const { return m_selectedItem; }
    //! Selects the item (NoItem or past the end: none). With doEvents
    //! onSelectionChange fires if the selection changed.
    void setSelectedItem(size_t item, bool doEvents=true);
    //! Source mode: the pooled row showing the item, null when it is not on
    //! screen; and the item a pooled row shows, NoItem for none.
    DataTableRow *rowForItem(size_t item) const;
    size_t itemForRow(const DataTableRow *row) const;
    //! Source mode: scrolls as little as puts the item fully in view (resolved
    //! by the next layout, VirtualList::scrollToItem).
    void scrollToItem(size_t item);

    internal::DataTableHeader *getHeader() const { return m_header; }
    //! The body the rows scroll in: a DataTableBody in row mode, the
    //! VirtualList in source mode.
    ScrollControl *getBody() const { return m_body; }
    //! The source-mode body, null in row mode.
    VirtualList *getVirtualBody() const { return m_virtualBody; }

    //! Column x offsets and widths across `width` units: each column at its
    //! width, the last one stretched to fill when it stretches. Written to x
    //! and w, which are sized to the column count.
    void columnSpans(int width, std::vector<int> &x, std::vector<int> &w) const;
    //! The width every column asks for together: the content's width.
    int contentWidth() const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

    //! Source mode's row keys, the table holding the keyboard focus (a press
    //! on a row focuses it; Tab reaches it): Up and Down move the selection a
    //! row, Home and End to the first and the last item, PageUp and PageDown
    //! by pageRows(), each scrolling the new selection into view and firing
    //! onSelectionChange. With nothing selected they select the first item
    //! (End the last). In row mode, and on an empty table, they do nothing.
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    //! Source mode: the rows the viewport shows whole, at least 1.
    size_t pageRows() const;

protected:
    //! Source mode: the selection moved to the item (setSelectedItem, the
    //! row showing it or null): fires onSelectionChange with doEvents. A
    //! subclass that selects something else through the item (a TreeTable's
    //! node) tells it here.
    virtual void selectedItemChanged(DataTableRow *row, size_t item, bool doEvents);
    //! Source mode: selects the item with events and scrolls it into view, as
    //! a row key does.
    void moveSelectionTo(size_t item);

    //! A requested size is the table's outer size: the body scrolls inside.
    bool sizedByRequest(Dim dim) const override { return hasRequestedSize(dim); }
    //! Source mode: makes one pooled row in the body's inner panel (the table
    //! then gives it its cells). A subclass makes a subclass of DataTableRow,
    //! so every pooled row has its parts from the start.
    virtual DataTableRow *newSourceRow(Base *parent);

private:
    friend class internal::DataTableColumnGrip;
    friend class internal::DataTableHeaderCell;
    friend class DataTableRow;

    struct Column
    {
        String caption;
        int width=DefaultColumnWidth;
        int minWidth=DefaultMinColumnWidth;
    };

    //! What changes with a column's width: the rows' and the header's sizes.
    void columnsChanged();
    //! The rows a column, border or stripe change reaches: the table's rows
    //! in row mode, the pooled ones in source mode.
    std::vector<DataTableRow *> bodyRows() const;
    //! Turns source mode on: the rows go, the VirtualList body replaces the
    //! row-mode one.
    void enterSourceMode();
    //! The VirtualList's binder: the row's stripe and selection by its item,
    //! then the app's binder.
    void bindRow(controls::Base *row, size_t item);
    void itemRowPressed(DataTableRow *row);
    bool rowKey(int key, bool down);
    void headerCellPressed(int column);
    void rowDeleted(DataTableRow *row);
    void restripe();
    void groupSelectionChanged(event::Info info);

    std::vector<Column> m_columns;
    std::vector<DataTableRow *> m_rows;
    SelectableGroup m_selection;

    internal::DataTableHeader *m_header=nullptr;
    //! The body in use, and which kind it is: the row-mode one until source
    //! mode replaces it with the VirtualList.
    ScrollControl *m_body=nullptr;
    internal::DataTableBody *m_rowBody=nullptr;
    internal::DataTableVirtualBody *m_virtualBody=nullptr;

    RowBinder m_rowBinder;
    size_t m_selectedItem=NoItem;
    int m_rowHeight=DefaultRowHeight;

    int m_sortColumn=-1;
    SortDirection m_sortDirection=SortDirection::None;
    bool m_stretchLast=true;
    bool m_striped=true;
    DataTableBorders m_borders=DataTableBorders::None;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_datatable_h_
