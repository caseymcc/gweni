/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_controls_logview_h_
#define _gweni_controls_logview_h_

#include <gweni/controls/button.h>
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
class MenuItem;

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
    //! What of the row it shows is selected, in bytes of the text its Label
    //! shows: the union of every range's part of it.
    struct Selection
    {
        //! A range touches the row.
        bool selected=false;
        //! It covers all of it, alone or inside a range running on both
        //! sides: the row's bounds are the highlight (as a click's row).
        bool whole=false;
        //! A range runs on from the row before it, past its end to the next.
        bool fromBefore=false;
        bool continues=false;
        size_t begin=0;
        size_t end=0;

        bool operator==(const Selection &other) const
        {
            return selected == other.selected && whole == other.whole && fromBefore == other.fromBefore
                && continues == other.continues && begin == other.begin && end == other.end;
        }
    };

    //! Whether a selected range touches the row it shows.
    bool isSelected() const { return m_selection.selected; }
    const Selection &getSelection() const { return m_selection; }
    //! The highlight's rects in the row's coordinates, which its renderer
    //! draws at getTextZIndex(), behind every glyph: the row's bounds when it
    //! is selected whole (or shows an app kind's content), else one per
    //! wrapped line the selection covers, from the character it starts at
    //! (the row's left edge for one coming from above) to the one it ends
    //! at (the row's right edge where it runs on past the line), the line's
    //! rows of the view's line height, the row's top and bottom padding
    //! included where the selection runs on into the row before or after.
    void getSelectionRects(std::vector<Rect> &rects);
    int getTextZIndex() const { return m_text->getZIndex(); }

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class controls::LogView;

    //! Marks StateChange_Background on a change, and with redraw on a row
    //! selected in part (its rects follow its text, which a bind changes).
    void setSelection(const Selection &selection, bool redraw);

    LogView *m_view=nullptr;
    int m_kind=0;
    Selection m_selection;
    LogViewChevron *m_chevron=nullptr;
    //! The content shown, of m_contents.
    Base *m_content=nullptr;
    //! Each app kind's content this row made, by kind.
    std::vector<std::pair<int, Base *>> m_contents;
};

//! The "jump to latest" button a detached LogView shows over the bottom of
//! its viewport, with the count of rows appended since it detached: a press
//! follows the log again. It never takes the keyboard focus from the view.
class GWENI_EXPORT LogViewJumpButton: public Button
{
public:
    GWENI_CONTROL(LogViewJumpButton, Button);

public:
    void onPress(event::Info info) override;

private:
    friend class controls::LogView;

    LogView *m_view=nullptr;
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
    //! The jump button's gap above the viewport's bottom edge, and its
    //! height over the line height.
    static constexpr int JumpGap=6;
    static constexpr int JumpPadY=8;

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
    void clear() override;

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
    //! The view's height that shows exactly `rows` one-line rows (a small
    //! excerpt view's): the rows, their padding and the view's own.
    int heightForRows(int rows);

    //! A block row's look. Collapsed, it shows its title with the count of
    //! lines it shows and holds, "title (4 of 120 lines)", and its first
    //! getBlockPreviewLines() lines; expanded, "title (120 lines)" and every
    //! line. A block of no more lines than that shows them all either way.
    static String blockText(const LogRow &row, size_t previewLines);
    //! The lines a collapsed block shows. Default 4. One measure generation,
    //! as a new width: the next layout measures the rows on screen, the idle
    //! re-measure the rest.
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
    //! measured and bound again; a kind replaced deletes the pooled rows'
    //! content of the old factory, and the new one makes it again.
    void setRowKind(LogRow::Kind kind, RowKind handler);

    //! The pooled row showing the row, or null when it is not on screen.
    internal::LogViewRow *rowControl(RowId id) const;
    //! The row under a canvas point, and that point in the row's
    //! coordinates; NoRow where none is.
    RowId rowAt(const Point &canvasPoint, Point *local=nullptr) const;

    //! A selection endpoint: a row and a byte offset into its rowText().
    struct Position
    {
        RowId row=NoRow;
        size_t character=0;

        bool operator==(const Position &other) const
        {
            return row == other.row && character == other.character;
        }
    };
    //! A selected range, from the anchor (where it started) to the focus
    //! (where Shift+click or a drag moved it), in either order. A whole row
    //! is selected from character 0 to its rowText()'s length.
    struct SelectionRange
    {
        Position anchor;
        Position focus;
    };

    //! The text a row copies: a plain row's text, a block's title and every
    //! line (collapsed or not), joined by '\n'; an app kind's text. Empty for
    //! a row the view does not hold.
    String rowText(RowId id) const;
    //! A byte of the text a row shows (its Label's: a block's blockText())
    //! as a byte of its rowText(): a block's count after its title is the
    //! title's end, the lines after it are their own bytes. Other rows show
    //! their rowText().
    size_t shownToRowCharacter(RowId id, size_t shown) const;
    //! The inverse: a byte of rowText() as a byte of the shown text, a
    //! collapsed block's hidden lines at its shown text's end.
    size_t rowToShownCharacter(RowId id, size_t character) const;

    //! The row and character under a canvas point on a row on screen: the
    //! closest caret of the row's wrapped text (Text::getClosestCharacter,
    //! on whichever wrapped line is under it), as a byte of rowText(). An app
    //! kind's row is its start left of its middle and its end right of it.
    //! NoRow where no row is.
    Position positionAt(const Point &canvasPoint);

    //! The row alone (a click).
    void selectRow(RowId id);
    //! The last range's focus moved to the row, the rows between it and the
    //! anchor selected whole; with no selection, selectRow().
    void extendSelectionTo(RowId id);
    //! One range from the anchor to the focus, their characters clamped to
    //! their rows' text; nothing for a row the view does not hold.
    void selectRange(const Position &anchor, const Position &focus);
    //! The last range's focus moved to the character (Shift+click, a drag),
    //! its anchor unit kept whole: the character a drag was pressed on, the
    //! word a double-click selected or the row a click selected, from its far
    //! end when the focus is before it. With no selection, selectRow().
    void extendSelectionTo(const Position &focus);
    //! The word at the character (a double-click): its run of letters,
    //! digits, '_' and non-ASCII codepoints, of blanks or of punctuation, the
    //! one before it at a line's end. A row with no word there, selectRow().
    void selectWordAt(const Position &at);
    //! The row out of the selection if it is in it, else in it as a range of
    //! its own (Ctrl+click).
    void toggleRowSelected(RowId id);
    //! Every row (Ctrl+A), from the first to the last held now.
    void selectAll();
    //! Nothing selected (Escape).
    void clearSelection();
    bool hasSelection() const { return !m_selection.empty(); }
    bool isRowSelected(RowId id) const;
    //! The ranges, in the order they were made, the last one the one
    //! Shift+click extends. Ranges keep their ids: rows dropped by the cap
    //! or cleared leave the rows that remain, a replace clamps the characters
    //! on that row (one at the row's end stays at its end).
    const std::vector<SelectionRange> &getSelection() const { return m_selection; }
    //! Each selected row's selected text, in row order, joined by '\n'.
    String getSelectedText() const;
    //! getSelectedText() on the clipboard (platform::setClipboardText):
    //! Ctrl+C and the context menu's Copy. False with nothing selected.
    bool copySelection();

    //! A press on a block's chevron or title line toggles it; anywhere else
    //! on a row it selects the row whole, Shift extending the selection to
    //! the character under it and Ctrl toggling the row. A drag from there
    //! selects from the pressed character to the one under the mouse once
    //! it leaves it (back on it, the pressed row whole again), across wrapped
    //! lines and rows, scrolling past the viewport's top or bottom edge (a
    //! scroll up detaches the view; past an edge the focus is the top row's
    //! start or the bottom row's end). A press under the last row clears it.
    void onMouseClickLeft(int x, int y, bool down) override;
    //! A double-click on a row selects the word under it (selectWordAt), and
    //! a drag from there extends it with the word kept whole; with Shift or
    //! Ctrl, or on a block's title line, it is a press.
    void onMouseDoubleClickLeft(int x, int y) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    //! A right press on a row not selected selects it; the context menu
    //! (Copy, Select all) is made the first time.
    void onMouseClickRight(int x, int y, bool down) override;
    bool onKeyEscape(bool down) override;

    //! The selection changed (a click, a key, a drag or the app's call).
    gweni::event::Caller onSelectionChanged;

    //! A drag past the viewport's edge scrolls AutoScrollBase plus
    //! AutoScrollRate units a second per unit past it.
    static constexpr float AutoScrollBase=60.0f;
    static constexpr float AutoScrollRate=10.0f;

    //! Following, every layout keeps the last row's bottom at the viewport's
    //! bottom (VirtualList::setStickToEnd), appends included, in the frame
    //! they are made. A scroll up detaches it: a wheel notch up, the bar's
    //! thumb dragged or its arrows or track pressed above the end, Up,
    //! PageUp, Home, scrollToRow() of a row above the last. Detached, appends
    //! leave every row on screen where it is, and the jump button shows
    //! over the bottom of the viewport with the count of rows appended since.
    //! A press on it, End, scrollToRow() of the last row or a scroll down to
    //! the end follows again and hides it. Following by default.
    void setFollowing(bool follow);
    bool isFollowing() const { return m_following; }
    //! Rows appended since the view detached (0 while following).
    size_t getRowsSinceDetached() const { return m_sinceDetached; }
    //! The jump button, made the first time the view detaches; null before.
    internal::LogViewJumpButton *getJumpButton() const { return m_jump; }
    //! The label the jump button shows for a count of new rows: set by the
    //! layout after the appends, once however many there were.
    static String jumpText(size_t newRows);
    //! Scrolls the row into view (VirtualList::scrollToItem): the last row
    //! follows the log, any other detaches it. False for a row the view
    //! does not hold.
    bool scrollToRow(RowId id);

    bool onMouseWheeled(int delta) override;
    //! Up, Down, PageUp and PageDown scroll by a line and a page less a
    //! line; Home goes to the first row, End follows the log.
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;

    //! Following was turned on or off (by the user or setFollowing):
    //! `integer` 1 or 0. It may be called from a layout pass (the bar's arrows
    //! or track pressed), so a handler only reads the view and acts later.
    gweni::event::Caller onFollowingChanged;

protected:
    void onVeritcalBarMoved(event::Info info) override;
    //! The user's move of the bar, wherever the list took it: at its end the
    //! view follows, above it it detaches.
    void barScrollTaken(float amount) override;
    void contentsPlaced(skin::Base *skin) override;

private:
    struct Entry
    {
        RowId id;
        LogRow row;
    };

    friend class internal::LogViewRow;

    //! Scrolls by delta units from where the view is (detaching it for a
    //! scroll up, following again once it reaches the end).
    void scrollByUnits(int delta);
    //! Follows again when the offset is at the end.
    void followIfAtEnd();
    //! Every row fits in the viewport (as the last layout placed them):
    //! there is nothing to scroll up to, so nothing detaches the view.
    bool fitsInView() const;
    //! Makes the jump button the first time; shows it with the count, or
    //! hides it.
    void updateJumpButton();

    //! rowText()'s length.
    size_t rowLength(RowId id) const;
    //! The position of the first row the view holds at or after the id
    //! (getRowCount() when none).
    size_t positionAtOrAfter(RowId id) const;
    //! Whether a range runs from its anchor to its focus (an empty one does).
    static bool forward(const SelectionRange &range);
    //! A range's endpoints in row order.
    static void ordered(const SelectionRange &range, Position &low, Position &high);
    //! Ranges with no row held any more dropped, a low end the cap dropped
    //! moved to the first row held. Whether it changed anything.
    bool pruneSelection();
    //! pruneSelection(), the rows on screen told, onSelectionChanged.
    void selectionChanged();
    //! Tells every pooled row on screen what of its row is selected.
    void refreshRowSelection();
    //! What of the row at the position is selected, as its row shows it.
    internal::LogViewRow::Selection rowSelection(size_t position) const;
    void onCopy(event::Info);
    void onSelectAll(event::Info);
    //! The item under a point in the view's coordinates, its y clamped into
    //! the viewport; NoRow for none.
    RowId rowAtDrag(const Point &local) const;
    //! The character of the row on screen under the canvas point (see
    //! positionAt()); 0 for a row not on screen.
    size_t characterAt(RowId id, const Point &canvasPoint);
    //! Where the drag's focus goes for m_dragPoint: the character under it,
    //! its x clamped into the viewport; past the viewport's top the start of
    //! the row at its top, past its bottom (or under the last row) the end of
    //! the row there.
    Position positionAtDrag();
    //! The drag's focus to the position: nothing until it leaves the pressed
    //! character, then extendSelectionTo(), the pressed row whole again when
    //! a drag from a plain press comes back to its character.
    void dragTo(const Position &at);
    //! The last range's anchor unit (see extendSelectionTo()).
    void setAnchorUnit(const Position &low, const Position &high);
    //! The anchor unit, its characters clamped to its row; the last range's
    //! anchor alone when the unit is not that range's anchor any more (the
    //! cap dropped its row, a replace or an app's call moved it).
    void anchorUnit(Position &low, Position &high) const;
    static bool before(const Position &a, const Position &b);
    //! How far the drag point is past the viewport's top (< 0) or bottom.
    int dragOverflow() const;
    void autoScrollStep();
    void stopAutoScroll();

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
    bool m_following=true;
    size_t m_sinceDetached=0;
    //! Rows were appended since the jump button last showed the count.
    bool m_jumpCountStale=false;
    internal::LogViewJumpButton *m_jump=nullptr;
    std::vector<SelectionRange> m_selection;
    //! What the last range's anchor stands for, kept whole by an extension:
    //! the pressed character (low == high), a word or a row.
    Position m_anchorLow;
    Position m_anchorHigh;
    //! The character the drag was pressed on.
    Position m_pressPosition;
    //! A press on a row started a drag: the mouse is this view's.
    bool m_dragging=false;
    //! The drag moves the focus by characters: it left the pressed character
    //! (or started with Shift, or from a double-click).
    bool m_charDrag=false;
    //! The drag started with a plain press, which selected the row whole.
    bool m_dragFromRow=false;
    Point m_dragPoint;
    anim::Animation *m_autoScroll=nullptr;
    double m_autoScrollTime=0.0;
    MenuItem *m_copyItem=nullptr;
    //! getLineHeight()'s, and what it was measured with.
    int m_lineHeight=0;
    //! The auto-scroll's fraction of a unit to come; beside m_lineHeight, in
    //! the padding before m_metricsFont, so the double time costs no size.
    float m_autoScrollCarry=0.0f;
    gweni::Font m_metricsFont;
    const renderer::Base *m_metricsRenderer=nullptr;
    unsigned m_metricsSerial=0;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_logview_h_
