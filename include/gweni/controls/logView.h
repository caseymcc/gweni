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
#include <functional>
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

//! A block row's chevron, in the row's left padding beside the title line:
//! right while the block is collapsed, down while it is expanded, in the
//! row's text colour. The mouse goes through it to the view.
class GWENI_EXPORT LogViewChevron: public Base
{
public:
    GWENI_CONTROL(LogViewChevron, Base);

public:
    bool isExpanded() const { return m_expanded; }
    const Color &getColor() const { return m_color; }

private:
    friend class controls::LogView;

    //! Marks StateChange_Background only when the look changes.
    void setLook(bool expanded, const Color &color);

    bool m_expanded=false;
    Color m_color;
};

//! A LogView's pooled row: a wrapped Label showing the row it is bound to, in
//! the view's font (held by pointer, so a new font is fontChanged()). A block
//! row adds a chevron in a wider left padding; a row of an app's kind hides
//! the text and shows that kind's content (made the first time the row shows
//! that kind), placed inside the padding.
class GWENI_EXPORT LogViewRow: public Label
{
public:
    GWENI_CONTROL(LogViewRow, Label);

public:
    //! The view's font changed in place: the text is measured again.
    void fontChanged();

    //! The kind of the row it shows.
    int getKind() const { return m_kind; }
    //! The chevron, made the first time the row shows a block; null before.
    LogViewChevron *getChevron() const { return m_chevron; }
    //! The content of the app's kind it shows, or null for a plain or block row.
    Base *getKindContent() const { return m_content; }

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class controls::LogView;

    LogView *m_view=nullptr;
    int m_kind=0;
    LogViewChevron *m_chevron=nullptr;
    //! The content shown, of m_contents.
    Base *m_content=nullptr;
    //! Each app kind's content this row made, by kind.
    std::vector<std::pair<int, Base *>> m_contents;
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
    //! A block row's chevron column, left of its text, after RowPadX.
    static constexpr int ChevronColumn=14;
    //! The field's inset, inside its border.
    static constexpr int FieldPadding=2;

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
    //! The font's line height, as the rows' text lays its lines out.
    int getLineHeight();

    //! A block row's look. Collapsed, it shows its title with the count of
    //! lines it shows and holds, "title (4 of 120 lines)", and its first
    //! getBlockPreviewLines() lines; expanded, "title (120 lines)" and every
    //! line. A block of no more lines than that shows them all either way.
    static String blockText(const LogRow &row, size_t previewLines);
    //! The lines a collapsed block shows. Default 4. Every block on screen
    //! is measured again.
    void setBlockPreviewLines(size_t lines);
    size_t getBlockPreviewLines() const { return m_previewLines; }
    //! Expands or collapses a block row: that row alone is measured and bound
    //! again (the rows above it on screen stay where they are). False for a
    //! row the view does not hold or that is no block.
    bool setBlockExpanded(RowId id, bool expanded);
    bool toggleBlock(RowId id);

    //! A row kind of the app's (or a view built on LogView): what a row of
    //! that kind shows in place of the text, inside the row's padding.
    struct RowKind
    {
        //! Makes the kind's content as a child of the pooled row, once per
        //! pooled row, the first time it shows a row of the kind.
        std::function<Base *(Base *row)> factory;
        //! Fills the content for a row: it only sets content (VirtualList's
        //! rule for a binder), never moves, resizes or deletes the control.
        std::function<void(Base *content, const LogRow &row)> binder;
        //! The content's height at that width (the row's less its padding):
        //! the height the bound content lays out at.
        std::function<int(const LogRow &row, int width)> measurer;
    };
    //! Registers or replaces a kind, from LogRow::FirstAppKind on (a row of
    //! a kind with no handler shows its text, as a plain row). Every row is
    //! measured and bound again.
    void setRowKind(LogRow::Kind kind, RowKind handler);

    //! The pooled row showing the row, or null when it is not on screen.
    internal::LogViewRow *rowControl(RowId id) const;
    //! The row under a canvas point, and that point in the row's
    //! coordinates; NoRow where none is.
    RowId rowAt(const Point &canvasPoint, Point *local=nullptr) const;

    //! A press on a block's chevron or title line toggles it.
    void onMouseClickLeft(int x, int y, bool down) override;

private:
    struct Entry
    {
        RowId id;
        LogRow row;
    };

    friend class internal::LogViewRow;

    void bindRow(Base *row, size_t position);
    int measureRow(size_t position, int width);
    const RowKind *rowKind(LogRow::Kind kind) const;
    //! Drops the oldest rows past the cap, keeping room for one more.
    void applyCap();

    std::deque<Entry> m_rows;
    //! The next id append() gives, and the lowest prepend() gave.
    RowId m_nextId=0;
    RowId m_lowestId=0;
    size_t m_maxRows=0;
    size_t m_previewLines=4;
    gweni::Font m_font;
    std::vector<std::pair<LogRow::Kind, RowKind>> m_kinds;
    //! getLineHeight()'s, and what it was measured with.
    int m_lineHeight=0;
    gweni::Font m_metricsFont;
    const renderer::Base *m_metricsRenderer=nullptr;
    unsigned m_metricsSerial=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_logview_h_
