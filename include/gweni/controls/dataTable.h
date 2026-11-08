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

    //! The rows were put in another order: the inner panel docks them again.
    //! (A child's bringToFront() invalidates its logical parent, this control,
    //! and not the inner panel that holds it.)
    void rowOrderChanged();
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
//! is as wide as the columns, so a new width changes every row's bounds and
//! marks its whole subtree). Dragging a header cell's right edge (its grip)
//! resizes the column from the width it is laid out at, so the stretched last
//! column's edge follows the mouse (dragged left, the column still fills the
//! body, at the narrower width it then asks for). A click elsewhere on the
//! cell sorts by it, ascending first and then
//! alternating, shows its chevron and fires onSortChanged. The table does not
//! sort: the app orders its records and shows them, by setting the rows' cells
//! again or by putting the rows in the new order (setRowOrder).
//!
//! The rows (addRow) are DataTableRows, one cell per column, single-selected
//! through the table's SelectableGroup. setStriped fills every odd row (by its
//! place in the row order), setBorders draws rules along the rows and between
//! the columns.
class GWENI_EXPORT DataTable: public controls::Base
{
public:
    GWENI_CONTROL(DataTable, controls::Base);

public:
    static constexpr int DefaultColumnWidth=100;
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

    DataTableRow *getSelectedRow() const;
    //! Selects the row (null: none).
    void setSelectedRow(DataTableRow *row);
    //! Fired once per change of getSelectedRow(), with the row (or null) as
    //! the info's `control`.
    gweni::event::Caller onSelectionChange;

    internal::DataTableHeader *getHeader() const { return m_header; }
    ScrollControl *getBody() const { return m_body; }

    //! Column x offsets and widths across `width` units: each column at its
    //! width, the last one stretched to fill when it stretches. Written to x
    //! and w, which are sized to the column count.
    void columnSpans(int width, std::vector<int> &x, std::vector<int> &w) const;
    //! The width every column asks for together: the content's width.
    int contentWidth() const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

protected:
    //! A requested size is the table's outer size: the body scrolls inside.
    bool sizedByRequest(Dim dim) const override { return hasRequestedSize(dim); }

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
    void headerCellPressed(int column);
    void rowDeleted(DataTableRow *row);
    void restripe();
    void groupSelectionChanged(event::Info info);

    std::vector<Column> m_columns;
    std::vector<DataTableRow *> m_rows;
    SelectableGroup m_selection;

    internal::DataTableHeader *m_header=nullptr;
    internal::DataTableBody *m_body=nullptr;

    int m_sortColumn=-1;
    SortDirection m_sortDirection=SortDirection::None;
    bool m_stretchLast=true;
    bool m_striped=true;
    DataTableBorders m_borders=DataTableBorders::None;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_datatable_h_
