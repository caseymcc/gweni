/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_codeeditor_h_
#define _gweni_controls_codeeditor_h_

#include <gweni/controls/caretBlink.h>
#include <gweni/controls/label.h>
#include <gweni/controls/text.h>
#include <gweni/controls/virtualList.h>
#include <gweni/text/document.h>
#include <gweni/text/highlightCache.h>
#include <gweni/text/undoStack.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace gweni
{
namespace controls
{

class CodeEditor;
class CodeLine;

namespace internal
{

//! A CodeEditor's line-number column: the editor's own child, left of the
//! viewport in the room the editor's padding keeps for it, so it never
//! scrolls sideways. It draws its backdrop (the CodeGutter surface); its
//! number Labels are pooled with the editor's rows.
class GWENI_EXPORT CodeGutter: public Base
{
public:
    GWENI_CONTROL(CodeGutter, Base);
};

//! A number in the gutter, right-aligned, in the skin's
//! Colors.CodeEditor.LineNumber, or LineNumberCurrent on the caret's line.
class GWENI_EXPORT CodeLineNumber: public Label
{
public:
    GWENI_CONTROL(CodeLineNumber, Label);

public:
    static constexpr size_t NoLine=static_cast<size_t>(-1);

    //! The line whose number it shows, NoLine while hidden.
    size_t getLine() const { return m_line; }
    bool isCurrentLine() const { return m_current; }

    void updateColors() override;

private:
    friend class controls::CodeEditor;

    void setCurrent(bool current);
    //! The editor's font changed (in place): the number is laid out again.
    void fontChanged();

    size_t m_line=NoLine;
    bool m_current=false;
};

//! A CodeLine's text: one run drawn with the colour spans of the line's
//! tokens (CodeEditor::setLanguage), in Colors.CodeEditor's Plain colour
//! outside them, by a skin renderer of its own (the textured skin's
//! CodeLineText, through renderText's span overload).
class GWENI_EXPORT CodeLineText: public Text
{
public:
    GWENI_CONTROL(CodeLineText, Text);

public:
    const std::vector<renderer::ColorSpan> &getColorSpans() const { return m_spans; }

private:
    friend class controls::CodeLine;

    //! Takes \p spans (swapped in), marking StateChange_Text only when they
    //! differ from the ones it had: the string is not laid out again.
    void setColorSpans(std::vector<renderer::ColorSpan> &spans);

    std::vector<renderer::ColorSpan> m_spans;
};

}//namespace internal

//! One row of a CodeEditor: the line it shows, as one text run (a Text in the
//! editor's font, coloured by the line's tokens), with a line background
//! behind it (shown on the current line) and a selection rect behind the
//! glyphs at the text's z. The editor
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
    const String &getText() const { return m_rowStarts.size() > 1?m_lineText:m_text->getText(); }
    internal::Text *getTextControl() const { return m_text; }

    //! The visual rows the line is shown in: 1 unwrapped, else the rows it
    //! wraps into at the editor's wrap width (CodeEditor::setWrap,
    //! text::wrapCells), each a text run of its own, one line height high,
    //! one below the other.
    size_t getRowCount() const { return m_rowStarts.empty()?1:m_rowStarts.size(); }
    //! The byte of the line visual row \p row starts at (0 for the first).
    size_t getRowStart(size_t row) const { return row < m_rowStarts.size()?m_rowStarts[row]:0; }
    //! The Text drawing visual row \p row: getTextControl() for the first.
    internal::Text *getRowText(size_t row) const { return row == 0?m_text:m_pieces[row-1]; }
    CodeEditor *getEditor() const { return m_editor; }
    //! The colour spans its run is drawn with: the bound line's tokens in
    //! the skin's colours (none without a language).
    //! Wrapped, the first visual row's: each row's Text holds the spans over
    //! its bytes, from its own start.
    const std::vector<renderer::ColorSpan> &getColorSpans() const { return m_text->getColorSpans(); }

    //! Whether it shows the line with the caret: its line background shows.
    bool isCurrentLine() const { return m_current; }

    //! A diff line's kind, which a CodeView's diff gives its row background.
    enum class DiffKind
    {
        None,       //!< context, a file header or no diff: no background
        Added,      //!< a '+' line in a hunk
        Removed,    //!< a '-' line in a hunk
        Hunk        //!< an "@@ ... @@" hunk header
    };
    //! The bound line's kind, from its tokens when it is bound or its tokens
    //! change (CodeEditor::showsDiffRows()); None otherwise.
    DiffKind getDiffKind() const { return m_diffKind; }
    //! The z its glyphs are drawn in front of, and its selection rect at.
    int getTextZIndex() const { return m_text->getZIndex(); }

    //! The cells of the line the editor's selection covers, [begin, end):
    //! the end is one cell past the last glyph when the line's end (its
    //! line break) is selected; begin == end when it covers none.
    size_t getSelectionBegin() const { return m_selectionBegin; }
    size_t getSelectionEnd() const { return m_selectionEnd; }
    //! The selection's rect in the row's coordinates, a line high from the
    //! begin cell's x to the end cell's (CodeEditor::columnX); empty when the
    //! line has none. Wrapped, the first of getSelectionRects().
    Rect getSelectionRect() const;
    //! The selection's rects in the row's coordinates, one per visual row it
    //! covers, top to bottom, each a line high on its row (empty when the
    //! line has none; one at most unwrapped, getSelectionRect()). Wrapped, a
    //! row's rect runs between the selection's cells on that row, counted
    //! from the row's start (CodeEditor::columnX of them): a row the
    //! selection goes on past reaches its last cell, at most the wrap cells
    //! (blanks hanging past the edge are cut there), and the line's last row
    //! one cell past its last glyph when the line break is selected too. So
    //! the rects are in units, the same at any canvas scale.
    void getSelectionRects(std::vector<Rect> &rects) const;

    void calculateSize(skin::Base *skin, Dim dim) override;
    //! The skin's colours again: the spans are built from them anew.
    void updateColors() override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class CodeEditor;

    //! The run's spans from the editor's tokens of the bound line.
    void updateColorSpans();

    //! Shows the line, with the text it holds now, wrapped at \p wrapCells
    //! cells (0: unwrapped, one row).
    void bind(size_t line, const String &text, size_t wrapCells);
    void setCurrent(bool current);
    void setSelection(size_t begin, size_t end);
    //! The selection's rect on visual row \p row, if it covers any of it.
    bool selectionRectOnRow(size_t row, Rect &rect) const;
    //! The editor's font changed (in place): the run is laid out again.
    void fontChanged();

    internal::CodeLineText *m_text=nullptr;
    //! The Texts of the visual rows after the first, and where each row
    //! starts in the line (empty unwrapped: no allocation for a fixed row).
    std::vector<internal::CodeLineText *> m_pieces;
    std::vector<size_t> m_rowStarts;
    //! The whole line while it is shown in more than one row.
    String m_lineText;
    CodeEditor *m_editor=nullptr;
    size_t m_line=NoLine;
    bool m_current=false;
    DiffKind m_diffKind=DiffKind::None;
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
//! cell to spare. The latest scroll request in a frame decides the view: of
//! two moves the later one, and a scroll the app or the user asks for after
//! a move (a scrollToItem(), an offset, a wheel notch, the bar), or in a
//! resize frame, is kept over it.
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
//!
//! setWrap(true) wraps each line at the viewport's width, on word
//! boundaries where it has any and at any cell where it has none
//! (text::wrapCells, counted in cells with no measuring), and takes the
//! horizontal bar away. The list goes to its variable-height mode: a line is
//! one item, its height its visual rows times the line height, measured by
//! the wrap at the rows' width (O(line) per edit) and estimated from the
//! line's cells (O(1)) until the list has measured it, so a new width costs
//! the lines on screen at once and the idle re-measure the rest.
//!
//! setAutoHeight(minLines, maxLines) makes the editor as tall as its content:
//! its preferred height is its visual rows (its lines, unwrapped) clamped to
//! [minLines, maxLines], times the line height, plus its padding, counted in
//! the Y measuring at its final width; it scrolls only past maxLines, so it
//! shows no vertical bar until then. A row more or fewer is a new preferred
//! height of the editor's, which its parent lays out in the same pass. The
//! count stops past maxLines: a layout of the editor costs the wrap of the
//! lines up to that many rows, whatever the document's length.
//!
//! setPlain(true) is the form-field preset, a textarea: wrapped, no gutter
//! and no current-line background, the theme's text-field look (the TextBox
//! surfaces), and Tab moving the focus (setTabInsertsSpaces(false)). It is
//! still monospace: every column is a cell of the mono face.
//!
//! setLanguage("yaml") (any name of text::findLanguage(), or a tokenizer of
//! the app's, setTokenizer) highlights the lines: a text::HighlightCache over
//! the document keeps each line's tokens, and each row's run is drawn with a
//! colour span per token in the skin's Colors.CodeEditor colour of its kind
//! (renderer::Base::renderText's colour spans: one text id a line whatever
//! its tokens). The editor tells the cache of an edit before it binds a row,
//! and the cache which lines are on screen as it lays them out, so an edit
//! tokenizes the lines it changed, and one that changes the state the next
//! line starts in (an opened block scalar) the lines on screen below it and
//! no more: the rows whose tokens changed get new spans, which rewrite the
//! glyph slots whose colour changed and lay nothing out. The lines below are
//! tokenized when they are bound. Wrapped, each visual row's run takes
//! the spans over its bytes.
//!
//! setLineNumbers(true) adds a gutter left of the viewport: one number
//! Label per pooled row, bound with the rows (the numbers of the lines on
//! screen alone), right-aligned in a column as wide as the line count's
//! digits in cells plus GutterInset on each side. The editor's left padding
//! keeps that room, so the gutter does not scroll sideways, and a new digit
//! count (999 to 1000 lines) is a new padding of the editor's: it lays out
//! the editor alone.
class GWENI_EXPORT CodeEditor: public VirtualList
{
public:
    GWENI_CONTROL(CodeEditor, VirtualList);

public:
    ~CodeEditor() override;

    //! The units between a row's left edge and its first cell.
    static constexpr int TextInset=4;
    //! The padding inside the field's border, on every side.
    static constexpr int FieldPadding=2;
    //! The units on each side of the gutter's numbers.
    static constexpr int GutterInset=6;
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
    //! The visual row of its line the caret is drawn on (0 unwrapped), and
    //! its column in cells on that row.
    size_t getCaretRow() const;
    size_t getCaretRowColumn() const;
    //! Whether the caret, at a wrap point (the byte a visual row starts at),
    //! is drawn at the end of the row above rather than the start of its own:
    //! End, a click past a row's end and Up or Down landing past it put it
    //! there; any other move puts it at the start.
    bool isCaretAtRowEnd() const { return m_caretUpstream; }
    bool hasSelection() const { return m_caret != m_anchor; }
    //! The selection from its first position to its last.
    text::Range getSelection() const;
    String getSelectedText() const;
    void selectAll();

    //! The caret stop under a point in the editor's coordinates, as the
    //! last layout scrolled and measured it: the nearest one on the line (the
    //! visual row, wrapped) the point is on, the first or last line above or
    //! below every line, column 0 (the row's start) left of the viewport (the
    //! gutter) however far it is scrolled sideways. Wrapped, \p rowEnd (if
    //! given) says whether it is the end of the point's row at a wrap point.
    text::Position positionAt(const Point &local, bool *rowEnd=nullptr) const;

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

    //! Wraps the lines at the viewport's width (see the class comment), or
    //! not. Default off.
    void setWrap(bool wrap);
    bool isWrapped() const { return m_wrap; }
    //! The cells a visual row holds, as the last layout wrapped the rows at
    //! (0 unwrapped, or before a layout).
    size_t getWrapCells() const { return m_wrapCells; }
    //! The cells a row \p width units wide holds: its width less TextInset
    //! on both sides, in whole cells, at least 1 (0 before the metrics are
    //! read).
    size_t wrapCellsFor(int width) const;
    //! The visual rows line \p line takes at the last layout's wrap width.
    size_t getLineRows(size_t line) const;

    //! The line-number gutter, shown or not. Default off.
    void setLineNumbers(bool show);
    bool hasLineNumbers() const { return m_lineNumbers; }
    //! The gutter, made by the first setLineNumbers(true); null before.
    internal::CodeGutter *getGutter() const { return m_gutter; }
    //! The gutter's width in units, as the last layout gave it (0 hidden).
    int getGutterWidth() const { return m_gutterWidth; }
    //! The gutter's number Labels, pooled with the rows, in pool order.
    const std::vector<internal::CodeLineNumber *> &getLineNumberLabels() const { return m_numbers; }

    //! Sizes the editor to its visual rows, at least \p minLines (1 at least)
    //! and at most \p maxLines of them, scrolling past that; \p maxLines 0
    //! turns it off (the editor then takes the height it is given).
    void setAutoHeight(int minLines, int maxLines);
    bool hasAutoHeight() const { return m_autoMax > 0; }
    int getAutoHeightMin() const { return m_autoMin; }
    int getAutoHeightMax() const { return m_autoMax; }
    //! The visual rows the last Y measuring counted with auto-height on, at
    //! most getAutoHeightMax()+1 (it stops there); 0 before one.
    size_t getContentRows() const { return m_contentRows; }

    //! The form-field preset (see the class comment), or back to the code
    //! look: setPlain(false) gives back the current line, the CodeEditor
    //! surfaces and Tab's editing, and leaves the wrap and the gutter as they
    //! are.
    void setPlain(bool plain);
    bool isPlain() const { return m_plain; }

    //! Highlights the lines in the registry's language \p name
    //! (text::findLanguage: "yaml", "json", "markdown", "markdown+jinja",
    //! "diff", "bash"), or none for "": false, and no highlighting, for a name
    //! the registry does not have.
    bool setLanguage(const String &name);
    const String &getLanguage() const { return m_language; }
    //! Highlights the lines with \p tokenizer (which must outlive the editor
    //! or be set again first), none for null; getLanguage() is "" after it.
    void setTokenizer(const text::Tokenizer *tokenizer);
    const text::Tokenizer *getTokenizer() const { return m_highlight.tokenizer(); }
    //! The lines' tokens, as the rows were bound with them.
    text::HighlightCache &getHighlightCache() { return m_highlight; }

    //! No caret, no current line and no edits; selection and copying stay.
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return m_readOnly; }

    //! Whether the caret is lit now (it blinks while the editor has the
    //! focus; never read-only).
    bool isCaretVisible() const { return !m_readOnly && m_caretBlink.visible(); }
    //! The caret's rect in the editor's coordinates, from the last layout's
    //! scroll and metrics, cut to the viewport: empty when its line or
    //! column is scrolled out.
    Rect getCaretBounds() const;
    //! One frame of the caret's blink (its animation's).
    void updateCaretBlink() override;
    double nextCaretBlinkTime() override { return m_caretBlink.nextEdge(this); }
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

    //! Whether the rows take a diff kind from their lines' tokens
    //! (CodeLine::getDiffKind): none in the editor, a CodeView's with the
    //! diff language.
    virtual bool showsDiffRows() const { return false; }

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
    //! The gutter's width for the line count and the cell width (0 without
    //! line numbers), and the padding that keeps it: set while the editor
    //! lays out (under a PlacementScope), when it changed.
    int gutterWidthFor() const;
    void updateGutterPadding();
    //! The number Labels for the rows contentsPlaced() has just placed.
    void placeLineNumbers(skin::Base *skin);
    //! The lines the selection covers: its first to its last, less a last
    //! line it only reaches the start of.
    void selectedLines(size_t &first, size_t &last) const;

    //! Replaces \p range with \p text, puts the caret after it and records
    //! the edit as made by \p kind.
    void replaceRange(const text::Range &range, const String &text, text::EditKind kind);
    void insertText(const String &text, text::EditKind kind);
    //! Moves the caret (and the anchor too unless \p extend), keeping
    //! m_goalColumn unless \p keepGoal is false; updates the rows' current
    //! line, lights the caret and puts it in view. \p rowEnd puts it at the
    //! end of the row above when it lands on a wrap point.
    void moveCaret(const text::Position &position, bool extend, bool keepGoal=false, bool reveal=true,
        bool rowEnd=false);
    //! The caret moved or the view scrolled: the caret is drawn again.
    void caretChanged();
    void makeCaretVisible();
    //! Its column, scrolled sideways into view (unwrapped).
    void makeCaretColumnVisible();
    //! Wrapped: its line by the next layout, and of a line taller than the
    //! viewport its visual row at the wrap width that layout gives.
    void revealCaretRow();
    //! The caret's line by the next layout (scrollToItem), its visual row
    //! too when \p row is true, recorded as the caret's request.
    void requestCaretScroll(bool row);
    //! The scroll request just made of the list is the caret's (its visual
    //! row's when \p row is true).
    void recordCaretRequest(bool row);
    //! The last scroll request made of the list is the caret's: no later
    //! one (the app's, the user's, an earlier caret move's) replaced it.
    bool caretOwnsScroll() const;
    //! The caret's line is on screen (some row shows it).
    bool caretLineShown() const;
    //! The bytes line \p line's visual rows start at, the first 0: {0}
    //! unwrapped (text::wrapCells at the wrap cells otherwise).
    void rowStarts(size_t line, std::vector<size_t> &starts) const;
    //! The row of \p starts holding \p byte, the row above at a wrap point
    //! with \p rowEnd.
    static size_t rowOf(const std::vector<size_t> &starts, size_t byte, bool rowEnd);
    //! The byte row \p row of line \p line (with those starts) ends at.
    size_t rowEndByte(size_t line, const std::vector<size_t> &starts, size_t row) const;
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
    //! A point's x in px from the start of the text, 0 left of the viewport.
    double textX(const Point &local) const;
    //! The positions one word to the left and right of \p position.
    text::Position wordLeft(const text::Position &position) const;
    text::Position wordRight(const text::Position &position) const;
    //! Moves up or down by \p rows visual rows (lines, unwrapped) at the
    //! goal column, a column in cells on the row.
    void moveRows(int64_t rows, bool extend);
    //! The rows a page moves: the viewport's whole rows, at least one.
    int pageLines() const;
    //! Whether line \p line draws the current-line look.
    bool showsCurrentLine(size_t line) const { return !m_readOnly && !m_plain && line == m_caret.line; }
    //! The visual rows of the document's lines at a row \p width units wide,
    //! counted until they pass \p limit (so at most limit+1).
    size_t countRows(int width, size_t limit) const;
    //! Vertical scrolling on, or off while auto-height shows every row.
    void setVerticalScrolling(bool scrolls);

    //! The lines an edit replaced: their widths counted again and the rows
    //! of the lines it changed bound again.
    void linesChanged(const text::LineChange &change);
    //! One more (or one fewer) line of \p cells cells.
    void addWidth(size_t cells);
    void removeWidth(size_t cells);
    void bindRow(Base *row, size_t line);
    //! Wrapped, the height of line \p line at a row \p width units wide:
    //! measured (its wrap) or estimated (from its cells).
    int measureLine(size_t line, int width) const;
    int estimateLine(size_t line) const;
    //! Puts the list in variable mode with the wrap's measurer, every line
    //! estimated again from its cells.
    void applyWrapMeasurer();
    //! The colour spans of line \p line's tokens, in the skin's colours, into
    //! \p spans (emptied first; none without a tokenizer).
    void tokenSpans(size_t line, std::vector<renderer::ColorSpan> &spans);
    //! Line \p line's diff kind: its first token's, when that starts the line
    //! and is Added, Removed or Hunk, with showsDiffRows(); None otherwise.
    CodeLine::DiffKind diffKind(size_t line);
    //! Tells the highlight cache which lines the pooled rows show.
    void syncHighlightWindow();
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
    //! At a wrap point: drawn at the end of the row above (isCaretAtRowEnd).
    bool m_caretUpstream=false;
    //! The list's scrollRequestSerial() after the caret's last request of
    //! it (requestCaretScroll(), or an offset makeCaretVisible() set), and
    //! after the last layout: the latest request in a frame is the one kept,
    //! the caret's or another's, so the caret's holds only while the list's
    //! serial is still its own.
    uint64_t m_caretRequestSerial=0;
    uint64_t m_placedRequestSerial=0;
    //! The caret's request is for its visual row (revealCaretRow()): the
    //! next layout scrolls it into view while caretOwnsScroll().
    bool m_revealCaretRow=false;
    //! Row starts, kept for their capacity.
    mutable std::vector<size_t> m_rowScratch;
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
    double m_autoScrollTime=0.0;
    //! The units of auto-scroll not scrolled yet, per axis.
    float m_autoScrollCarry[2]={0.0f, 0.0f};

    bool m_wrap=false;
    size_t m_wrapCells=0;
    //! The line height the lines were last estimated at, and the viewport's
    //! height the caret's line was last kept on screen at.
    int m_wrapLineHeight=0;
    int m_wrapViewportHeight=-1;

    int m_autoMin=1;
    int m_autoMax=0;
    size_t m_contentRows=0;
    bool m_verticalScrolls=true;
    bool m_plain=false;

    bool m_lineNumbers=false;
    internal::CodeGutter *m_gutter=nullptr;
    //! One number per pooled row, in pool order, and the row each is for: a
    //! row the pool drops takes its number with it, and a fresh row gets a
    //! fresh one, so after a resize the gutter holds what a gutter built at
    //! that size holds.
    std::vector<internal::CodeLineNumber *> m_numbers;
    //! Each number's row by its control id (never reused, unlike an address).
    std::vector<size_t> m_numberRows;
    int m_gutterWidth=0;

    text::HighlightCache m_highlight;
    String m_language;
    //! The lines an edit's highlight pass gave new tokens, whose rows get new
    //! spans once the edit has moved the rows (kept for its capacity).
    std::vector<size_t> m_recolor;
    //! The spans a row's are built in (CodeLine::updateColorSpans).
    std::vector<renderer::ColorSpan> m_spanScratch;
    //! A wrapped row's share of them (CodeLine::updateColorSpans).
    std::vector<renderer::ColorSpan> m_rowSpanScratch;

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
