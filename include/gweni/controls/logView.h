/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_logview_h_
#define _gweni_controls_logview_h_

#include <gweni/controls/label.h>
#include <gweni/controls/virtualList.h>

#include <cstdint>
#include <deque>
#include <vector>

namespace gweni
{
namespace controls
{

class LogView;

//! One row of a LogView, as the app gives it. A plain row is its text,
//! wrapped at the view's width; a block row (kind Block) is a title and its
//! lines; an app's own kinds start at FirstAppKind.
struct GWENI_EXPORT LogRow
{
    using Kind=int;

    static constexpr Kind Plain=0;
    static constexpr Kind Block=1;
    static constexpr Kind FirstAppKind=2;

    Kind kind=Plain;
    //! A plain row's text, a block's title.
    String text;
    //! A block's lines.
    std::vector<String> lines;
    //! The text's colour when hasColor, else the skin's label colour.
    Color color;
    bool hasColor=false;
    //! A block shows only its first lines and a count while collapsed.
    bool collapsed=true;

    LogRow()=default;
    LogRow(const String &text): text(text) {}
    LogRow(const String &text, const Color &color): text(text), color(color), hasColor(true) {}

    static LogRow block(const String &title, std::vector<String> lines)
    {
        LogRow row(title);

        row.kind=Block;
        row.lines=std::move(lines);
        return row;
    }
};

namespace internal
{

//! A LogView's pooled row: a wrapped Label showing the row it is bound to,
//! in the view's font (held by pointer, so a new font is fontChanged()).
class GWENI_EXPORT LogViewRow: public Label
{
public:
    GWENI_CONTROL(LogViewRow, Label);

public:
    //! The view's font changed in place: the text is measured again.
    void fontChanged();
};

}//namespace internal

//! An append-only log of wrapped rows, virtualized: a VirtualList in
//! variable mode over a row store the view owns, so a frame costs what the
//! rows on screen cost however long the log is.
//!
//! Every row has a RowId, a running sequence number never given twice:
//! append() numbers up from 0, prepend() (older rows before the first, a
//! "Show all") down from -1, below every id given so far. Ids rise with the
//! rows' positions, so an id is found from the last row's (an appended one)
//! or the first row's in O(1), and only where rows were dropped between two
//! prepends by a binary search. replace() is by id, since positions move as
//! rows are prepended and dropped.
class GWENI_EXPORT LogView: public VirtualList
{
public:
    GWENI_CONTROL(LogView, VirtualList);

public:
    using RowId=int64_t;

    static constexpr RowId NoRow=INT64_MIN;
    //! The rows' padding, inside which the text wraps.
    static constexpr int RowPadX=4;
    static constexpr int RowPadY=1;

    //! Adds a row after the last one and returns its id. With a cap
    //! (setMaxRows) the oldest rows past it are dropped first.
    RowId append(const LogRow &row);
    //! Gives the row its new content. It is measured again alone (the rows on
    //! screen stay where they are when it is above them) and bound again if
    //! it shows. False, changing nothing, for an id the view does not hold
    //! (dropped by the cap, cleared, or never given).
    bool replace(RowId id, const LogRow &row);
    //! Adds older rows before the first one, rows[0] the oldest, numbered
    //! below every id given so far; what is on screen stays where it is.
    //! Returns the first one's id (NoRow for none). The cap does not apply
    //! until the next append.
    RowId prepend(const std::vector<LogRow> &rows);
    //! Removes every row. Ids go on from where they were.
    void clear();

    //! At most this many rows: an append drops the oldest past it. 0, the
    //! default, keeps every row.
    void setMaxRows(size_t rows);
    size_t getMaxRows() const { return m_maxRows; }

    size_t getRowCount() const { return m_rows.size(); }
    //! The first (oldest) and last rows' ids; NoRow when there are none.
    RowId firstRowId() const;
    RowId lastRowId() const;
    //! The row's position (its item in the list), or NoItem when the view
    //! does not hold it.
    size_t rowPosition(RowId id) const;
    //! The id of the row at a position, or NoRow past the end.
    RowId rowIdAt(size_t position) const;
    //! The row, or null when the view does not hold it.
    const LogRow *getRow(RowId id) const;
    bool hasRow(RowId id) const { return rowPosition(id) != NoItem; }

    //! The font the rows are drawn in; the skin's default one until set.
    void setFont(const gweni::Font &font);
    const gweni::Font &getFont() const { return m_font; }

    //! The pooled row showing the row, or null when it is not on screen.
    internal::LogViewRow *rowControl(RowId id) const;

private:
    struct Entry
    {
        RowId id;
        LogRow row;
    };

    void bindRow(Base *row, size_t position);
    int measureRow(size_t position, int width);
    //! Drops the oldest rows past the cap, keeping room for one more.
    void applyCap();

    std::deque<Entry> m_rows;
    //! The next id append() gives, and the lowest prepend() gave.
    RowId m_nextId=0;
    RowId m_lowestId=0;
    size_t m_maxRows=0;
    gweni::Font m_font;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_logview_h_
