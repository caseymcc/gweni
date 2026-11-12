/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_codeeditor_h_
#define _gweni_controls_codeeditor_h_

#include <gweni/controls/caretBlink.h>
#include <gweni/controls/text.h>
#include <gweni/controls/virtualList.h>
#include <gweni/text/document.h>
#include <gweni/text/undoStack.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace gweni
{
namespace controls
{

class CodeEditor;

//! One row of a CodeEditor: the line it shows, as one text run (a Text in the
//! editor's font), with a line background behind it (shown on the current
//! line) and a selection rect behind the glyphs at the text's z. The editor
//! makes and binds its rows (a pool, as a VirtualList's): an app never makes
//! one, and a row shows another line as the editor scrolls.
class GWENI_EXPORT CodeLine: public Base
{
public:
    GWENI_CONTROL(CodeLine, Base);

public:
    static constexpr size_t NoLine=static_cast<size_t>(-1);

    //! The line of the document the row shows, NoLine while it is a spare.
    size_t getLine() const { return m_line; }
    //! The line's text, as bound.
    const String &getText() const { return m_text->getText(); }
    internal::Text *getTextControl() const { return m_text; }
    CodeEditor *getEditor() const { return m_editor; }

    //! Whether it shows the line with the caret: its line background shows.
    bool isCurrentLine() const { return m_current; }
    //! The z its glyphs are drawn in front of, and its selection rect at.
    int getTextZIndex() const { return m_text->getZIndex(); }

    //! The cells of the line the editor's selection covers, [begin, end):
    //! the end is one cell past the last glyph when the line's end (its
    //! line break) is selected; begin == end when it covers none.
    size_t getSelectionBegin() const { return m_selectionBegin; }
    size_t getSelectionEnd() const { return m_selectionEnd; }
    //! The selection's rect in the row's coordinates, a line high from the
    //! begin cell's x to the end cell's (CodeEditor::columnX); empty when the
    //! line has none.
    Rect getSelectionRect() const;

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class CodeEditor;

    //! Shows the line, with the text it holds now.
    void bind(size_t line, const String &text);
    void setCurrent(bool current);
    void setSelection(size_t begin, size_t end);
    //! The editor's font changed (in place): the run is laid out again.
    void fontChanged();

    internal::Text *m_text=nullptr;
    CodeEditor *m_editor=nullptr;
    size_t m_line=NoLine;
    bool m_current=false;
    size_t m_selectionBegin=0;
    size_t m_selectionEnd=0;
};

//! A multi-line editor of monospace text in which only the lines on screen
//! are controls: a VirtualList of CodeLine rows over a text::Document, one row
//! per line in view plus one, each bound to the line it shows. So a document
//! of 100 000 lines has the controls, primitive ids and text ids of the
//! handful of lines on screen, and a frame costs what those lines cost.
//!
//! The rows are the document's lines, unwrapped, one line height high (the
//! font's lineHeight()), and the content scrolls both ways: its height is
//! every line's, its width the longest line's (kept as a count of lines per
//! width in cells, so an edit costs the lines it changed, not a scan of every
//! line). An edit of the document (getDocument(), whose observers are told
//! the lines an edit replaced) binds again only the rows showing the lines it
//! changed; lines it added or removed move the rows below them to their new
//! lines (VirtualList::insertItems / removeItems), and the rows that came
//! into view are bound.
//!
//! Columns are counted in cells, as a terminal counts them: every codepoint
//! is text::cellCount() cells (2 for a wide one, 0 for a combining mark), and
//! the font is a fixed-width face (by default the skin's mono face,
//! getDefaultMonoFont()), whose runs lay every codepoint in whole cells. A
//! cell is getCellWidth() units wide, read from the font once per font,
//! renderer text layout and scale, so a column's x is TextInset plus the
//! column times the cell width on every line, a line with CJK in it
//! included.
//!
//! The caret and the selection's other end (the anchor) are document
//! positions (text::Position, a line and a byte offset), so they survive
//! scrolling and edits of other lines. The caret is drawn by the editor at
//! its overlay z, in front of every row, one unit wide and a line high at
//! its column, while the editor has the keyboard focus; it blinks
//! (internal::CaretBlink, TextBox's) and stays lit after a move or an edit.
//! A caret stop is a codepoint boundary not followed by a combining mark (a
//! codepoint of 0 cells), so a mark moves with its base. Up and Down (and
//! PageUp, PageDown) keep the column in cells a run of them started at, and
//! land before a wide glyph that column falls inside. Home goes to the
//! line's first non-blank, or to column 0 from there; Ctrl+Left/Right move
//! by words (letters, digits, '_' and every non-ASCII codepoint), runs of
//! other punctuation and line ends. Return keeps the line's indent (its
//! leading blanks, up to the caret). Every move puts the caret in view: its
//! line by the next layout (VirtualList::scrollToItem), its column with a
//! cell to spare.
//!
//! Every edit made through the editor (typing, the editing keys, the
//! clipboard, Tab) is recorded on a text::UndoStack with TextBox's
//! coalescing: typing joins one step until the caret moves, a line break is
//! typed, the kind of edit changes or a second passes. Ctrl+Z undoes a step
//! and Ctrl+Y or Ctrl+Shift+Z redoes it, giving back the text, the caret and
//! the selection, the caret put in view. An edit the app applies to the
//! document itself (getDocument(), setText()) clears the history, which no
//! longer fits the text.
//!
//! Tab inserts spaces to the next multiple of getTabWidth() cells; with a
//! selection over several lines it indents each of them by that many
//! spaces, and Shift+Tab outdents the caret's line or the selected lines to
//! the tab stop before their indent (a leading tab character is one stop).
//! Tab never moves the focus out of the editor, but Escape arms it to: the
//! next Tab or Shift+Tab, if no other key comes first, moves the focus as in
//! any control. setTabInsertsSpaces(false) gives Tab back to the focus
//! (CodeMirror's rule both ways). Read-only (setReadOnly), the editor draws
//! no caret and no current line and makes no edit, while the moves, the
//! selection, Ctrl+A and Ctrl+C still work and Tab moves the focus.
class GWENI_EXPORT CodeEditor: public VirtualList
{
public:
    GWENI_CONTROL(CodeEditor, VirtualList);

public:
    ~CodeEditor() override;

    //! The units between a row's left edge and its first cell.
    static constexpr int TextInset=4;
    //! The zeros the cell width is measured over, so a fractional advance
    //! (an SDF run) reads as its average, not one glyph's rounding.
    static constexpr int CellSample=32;

    //! Replaces the whole text (the document's setText: "\r\n" is read as
    //! "\n"), every row bound again.
    void setText(const String &text);
    //! The lines joined with '\n'.
    String getText() const { return m_document.text(); }

    //! The text the editor shows. An edit applied to it is shown by the next
    //! layout: the rows of the lines it changed are bound again.
    text::Document &getDocument() { return m_document; }
    const text::Document &getDocument() const { return m_document; }
    size_t getLineCount() const { return m_document.lineCount(); }

    //! The font every line is drawn in: a fixed-width face, the skin's mono
    //! face (getDefaultMonoFont()) unless set.
    void setFont(const gweni::Font &font);
    const gweni::Font &getFont() const { return m_font; }

    //! The width of one cell and the height of one line, in units, as the
    //! last layout read them from the font (0 before one).
    float getCellWidth() const { return m_cellWidth; }
    int getLineHeight() const { return m_lineHeight; }

    //! The cells line \p line takes (text::stringCells of it), and the most
    //! any line takes: what the content's width is made of.
    size_t getLineCells(size_t line) const { return line < m_lineCells.size()?m_lineCells[line]:0; }
    size_t getLongestLineCells() const { return m_longest; }

    //! The x of a cell column in a row, in units from the row's left edge:
    //! TextInset plus column cells, rounded down to a whole unit.
    int columnX(size_t column) const;

    //! The row showing the line, as the last layout bound it, or null when
    //! that line is not on screen.
    CodeLine *rowForLine(size_t line) const;

    //! \name Caret and selection
    //! \{

    //! Where the caret is, and the selection's other end (the caret itself
    //! when nothing is selected).
    const text::Position &getCaret() const { return m_caret; }
    const text::Position &getAnchor() const { return m_anchor; }
    //! Moves the caret to \p position (clamped to the document, snapped to
    //! a caret stop), keeping the anchor with \p extend, and puts it in view.
    void setCaret(const text::Position &position, bool extend=false);
    //! The caret's column in cells on its line.
    size_t getCaretColumn() const { return m_caretColumn; }
    bool hasSelection() const { return m_caret != m_anchor; }
    //! The selection from its first position to its last.
    text::Range getSelection() const;
    String getSelectedText() const;
    void selectAll();

    //! The caret stop under a point in the editor's coordinates, as the
    //! last layout scrolled and measured it: the nearest one on the line the
    //! point is on, the first or last line above or below every line.
    text::Position positionAt(const Point &local) const;

    //! Replaces the selection (or inserts at the caret) with \p text, as
    //! typing it does, and puts the caret after it; nothing when read-only.
    void insertText(const String &text);

    //! Undoes (redoes) the last step, giving back the text, the caret and
    //! the selection; false with none, or read-only.
    bool undo();
    bool redo();
    text::UndoStack &getUndoStack() { return m_undo; }
    const text::UndoStack &getUndoStack() const { return m_undo; }

    //! The cells a tab stop is every. Default 4 (at least 1).
    void setTabWidth(int cells) { m_tabWidth=std::max(1, cells); }
    int getTabWidth() const { return m_tabWidth; }
    //! Whether Tab edits (inserts spaces, indents, outdents) rather than
    //! moving the focus. Default true.
    void setTabInsertsSpaces(bool inserts) { m_tabInsertsSpaces=inserts; }
    bool getTabInsertsSpaces() const { return m_tabInsertsSpaces; }

    //! No caret, no current line and no edits; selection and copying stay.
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return m_readOnly; }

    //! Whether the caret is lit now (it blinks while the editor has the
    //! focus).
    bool isCaretVisible() const { return m_caretBlink.visible(); }
    //! The caret's rect in the editor's coordinates, from the last layout's
    //! scroll and metrics, cut to the viewport: empty when its line or
    //! column is scrolled out.
    Rect getCaretBounds() const;
    //! One frame of the caret's blink (its animation's).
    void updateCaretBlink();
    //! \}

    void calculateSize(skin::Base *skin, Dim dim) override;

    bool needsInputChars() override { return true; }
    bool AccelOnlyFocus() override { return true; }
    bool onChar(gweni::UnicodeChar c) override;
    bool onKeyReturn(bool down) override;
    bool onKeyBackspace(bool down) override;
    bool onKeyDelete(bool down) override;
    bool onKeyLeft(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyPageUp(bool down) override;
    bool onKeyPageDown(bool down) override;
    bool onKeyTab(bool down) override;
    //! Arms the next Tab to move the focus (and goes on to the parent).
    bool onKeyEscape(bool down) override;
    //! Any key but Tab, Escape and the modifiers disarms that.
    bool onKeyPress(int key, bool press=true) override;
    void onKeyboardFocus() override;
    void onLostKeyboardFocus() override;
    //! A press puts the caret at the nearest caret stop (Shift: extends the
    //! selection to it) and starts a drag: while the button is held the
    //! caret follows the mouse with the anchor kept, and with the mouse past
    //! an edge of the viewport the view scrolls on the canvas clock
    //! (AutoScrollBase plus AutoScrollRate units a second per unit past the
    //! edge), the caret with it, until the mouse comes back in or the button
    //! is let go.
    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;
    //! Selects the word under the mouse (letters, digits, '_' and every
    //! non-ASCII codepoint), or the run of punctuation or of blanks there.
    void onMouseDoubleClickLeft(int x, int y) override;

    //! Units a second the view scrolls by while a drag holds the mouse past
    //! the viewport's edge, plus AutoScrollRate times how far past it is.
    static constexpr float AutoScrollBase=60.0f;
    static constexpr float AutoScrollRate=10.0f;
    //! Whether that scroll runs (its animation is registered).
    bool isAutoScrolling() const { return m_autoScroll != nullptr; }
    //! One frame of it (its animation's).
    void autoScrollStep();

protected:
    void contentsPlaced(skin::Base *skin) override;

    //! The rows are owner-placed, so the inner panel's preferred width is
    //! none of theirs: the content is as wide as the longest line, its cells
    //! and one more for a caret after its end, inset on both sides.
    Size contentSize() const override;

private:
    friend class CodeLine;

    void onCopy(event::Info info);
    void onCut(event::Info info);
    void onPaste(event::Info info);
    void onSelectAll(event::Info info);
    void onUndo(event::Info info);
    void onRedo(event::Info info);

    //! An edit the user makes: not when read-only, and it disarms Escape.
    bool editable();
    //! The selection as the undo stack keeps it.
    text::Selection currentSelection() const { return text::Selection{m_anchor, m_caret}; }
    //! Puts the anchor and the caret back, the caret in view.
    void restoreSelection(const text::Selection &selection);
    //! Indents (outdents) the lines the selection covers, or the caret's.
    void indentLines(bool outdent);
    //! The lines the selection covers: its first to its last, less a last
    //! line it only reaches the start of.
    void selectedLines(size_t &first, size_t &last) const;

    //! Replaces \p range with \p text, puts the caret after it and records
    //! the edit as made by \p kind.
    void replaceRange(const text::Range &range, const String &text, text::EditKind kind);
    void insertText(const String &text, text::EditKind kind);
    //! Moves the caret (and the anchor too unless \p extend), keeping
    //! m_goalColumn unless \p keepGoal is false; updates the rows' current
    //! line, lights the caret and puts it in view.
    void moveCaret(const text::Position &position, bool extend, bool keepGoal=false, bool reveal=true);
    //! The caret moved or the view scrolled: the caret is drawn again.
    void caretChanged();
    void makeCaretVisible();
    //! Each shown row's current-line look, from the caret's line.
    void syncCurrentLine();
    //! The cells of line \p line the selection covers (see CodeLine).
    void lineSelection(size_t line, size_t &begin, size_t &end) const;
    //! Each shown row's selection span, from the selection; nothing to do
    //! while there is none and none was shown.
    void syncSelection();
    //! How far the drag point is past the viewport's edges, in units: <0
    //! before, >0 after, 0 inside, per axis.
    Point dragOverflow() const;
    void stopAutoScroll();
    //! The caret stop \p cells cells into \p line (before a wide glyph it
    //! falls inside).
    text::Position atColumn(size_t line, size_t cells) const;
    //! The start of the glyph under a point in the editor's coordinates
    //! (positionAt's line, the cell the point is in).
    text::Position glyphAt(const Point &local) const;
    //! The positions one word to the left and right of \p position.
    text::Position wordLeft(const text::Position &position) const;
    text::Position wordRight(const text::Position &position) const;
    //! Moves up or down by \p lines lines at the goal column.
    void moveLines(int64_t lines, bool extend);
    //! The lines a page moves: the viewport's whole lines, at least one.
    int pageLines() const;

    //! The lines an edit replaced: their widths counted again and the rows
    //! of the lines it changed bound again.
    void linesChanged(const text::LineChange &change);
    //! One more (or one fewer) line of \p cells cells.
    void addWidth(size_t cells);
    void removeWidth(size_t cells);
    void bindRow(Base *row, size_t line);
    //! Whether the cell width and line height were read for this font,
    //! renderer, text layout and scale.
    bool metricsCurrent(skin::Base *skin) const;
    void readMetrics(skin::Base *skin);

    text::Document m_document;
    size_t m_observer=0;
    gweni::Font m_font;

    //! The cells of each line, and how many lines take each width in cells.
    std::vector<size_t> m_lineCells;
    std::vector<size_t> m_widthLines;
    size_t m_longest=0;
    //! The new lines' widths while an edit is counted (kept for its capacity).
    std::vector<size_t> m_newCells;

    text::Position m_caret;
    text::Position m_anchor;
    size_t m_caretColumn=0;
    //! The column Up and Down keep, or NoGoal: the caret's own.
    static constexpr size_t NoGoal=static_cast<size_t>(-1);
    size_t m_goalColumn=NoGoal;
    internal::CaretBlink m_caretBlink;
    //! The scroll the caret was last drawn at.
    int m_caretScroll[2]={-1, -1};
    //! Some row was given a selection span since the selection was last empty.
    bool m_selectionShown=false;
    //! The mouse while a drag holds it, in the editor's coordinates.
    Point m_dragPoint;
    anim::Animation *m_autoScroll=nullptr;
    float m_autoScrollTime=0.0f;
    //! The units of auto-scroll not scrolled yet, per axis.
    float m_autoScrollCarry[2]={0.0f, 0.0f};

    text::UndoStack m_undo;
    //! The editor is applying an edit of its own (the history stays).
    bool m_ownEdit=false;
    bool m_readOnly=false;
    int m_tabWidth=4;
    bool m_tabInsertsSpaces=true;
    //! Escape was the last key: the next Tab moves the focus.
    bool m_escapeArmed=false;

    float m_cellWidth=0.f;
    int m_lineHeight=0;
    //! What the metrics were read for.
    gweni::Font m_metricsFont;
    const renderer::Base *m_metricsRenderer=nullptr;
    unsigned m_metricsSerial=0;
    float m_metricsScale=0.f;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_codeeditor_h_
