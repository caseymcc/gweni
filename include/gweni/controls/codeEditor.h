/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_codeeditor_h_
#define _gweni_controls_codeeditor_h_

#include <gweni/controls/text.h>
#include <gweni/controls/virtualList.h>
#include <gweni/text/document.h>

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

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:
    void arrangeHorizontal(skin::Base *skin) override;
    void arrangeVertical(skin::Base *skin) override;

private:
    friend class CodeEditor;

    //! Shows the line, with the text it holds now.
    void bind(size_t line, const String &text);
    void setCurrent(bool current);
    //! The editor's font changed (in place): the run is laid out again.
    void fontChanged();

    internal::Text *m_text=nullptr;
    CodeEditor *m_editor=nullptr;
    size_t m_line=NoLine;
    bool m_current=false;
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

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:
    //! The rows are owner-placed, so the inner panel's preferred width is
    //! none of theirs: the content is as wide as the longest line, its cells
    //! and one more for a caret after its end, inset on both sides.
    Size contentSize() const override;

private:
    friend class CodeLine;

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
