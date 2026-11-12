/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/codeEditor.h>
#include <gweni/anim.h>
#include <gweni/controls/canvas.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/platform.h>
#include <gweni/skin.h>
#include <gweni/text/cells.h>
#include <gweni/utility.h>

#include <algorithm>
#include <climits>
#include <cmath>

namespace gweni
{
namespace controls
{

///////////////////////////////////////////////////////////////////////////
//CodeLine

// The row draws its backgrounds, its Text the line; neither takes the mouse,
// which is the editor's.
GWENI_CONTROL_CONSTRUCTOR(CodeLine)
{
    setMouseInputEnabled(false);

    m_text=newChild<internal::Text>();
    m_text->setPlacement(Placement::Owner);
    m_text->setMouseInputEnabled(false);
}

void CodeLine::bind(size_t line, const String &text)
{
    m_line=line;
    m_text->setText(text);
}

void CodeLine::setCurrent(bool current)
{
    if(m_current == current)
        return;

    m_current=current;
    addStateChange(StateChange_Background);
}

void CodeLine::setSelection(size_t begin, size_t end)
{
    if(end <= begin)
        begin=end=0;

    if(m_selectionBegin == begin && m_selectionEnd == end)
        return;

    m_selectionBegin=begin;
    m_selectionEnd=end;
    addStateChange(StateChange_Background);
}

Rect CodeLine::getSelectionRect() const
{
    if(!m_editor || m_selectionEnd <= m_selectionBegin)
        return Rect();

    const int x0=m_editor->columnX(m_selectionBegin);
    const int x1=m_editor->columnX(m_selectionEnd);

    return Rect(x0, 0, x1-x0, getHeight());
}

void CodeLine::fontChanged()
{
    m_text->textChanged();
    m_text->addStateChange(StateChange_Text);
    m_text->invalidate();
}

// As wide as its text after the inset, one line high: the editor places it
// at the content's width and the line height anyway.
void CodeLine::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    const Size &text=m_text->getPreferredSize();

    if(dim == Dim::X)
        m_preferredSize.width=preferredFromContent(CodeEditor::TextInset+text.width, Dim::X);
    else
        m_preferredSize.height=preferredFromContent(m_editor?m_editor->getLineHeight():text.height, Dim::Y);
}

void CodeLine::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);
    placeChild(m_text, CodeEditor::TextInset, m_text->getY(), m_text->getPreferredSize().width,
        m_text->getHeight());
}

void CodeLine::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);
    placeChild(m_text, m_text->getX(), 0, m_text->getWidth(), getHeight());
}

///////////////////////////////////////////////////////////////////////////
//CodeGutter, CodeLineNumber

namespace internal
{

// Its numbers are the editor's to place; the mouse goes through it to the
// editor (a press there is at column 0 of that line).
GWENI_CONTROL_CONSTRUCTOR(CodeGutter)
{
    setMouseInputEnabled(false);
}

GWENI_CONTROL_CONSTRUCTOR(CodeLineNumber)
{
    setMouseInputEnabled(false);
    setPadding(Padding(0, 0, 0, 0));
    setTextAlignment(Alignment::Right|Alignment::CenterV);
    updateColors();
}

void CodeLineNumber::updateColors()
{
    ParentClass::updateColors();

    if(skin::Base *skin=getSkin())
        setTextColor(m_current?skin->Colors.CodeEditor.LineNumberCurrent:skin->Colors.CodeEditor.LineNumber);
}

void CodeLineNumber::setCurrent(bool current)
{
    if(m_current == current)
        return;

    m_current=current;
    updateColors();
}

void CodeLineNumber::fontChanged()
{
    m_text->textChanged();
    m_text->addStateChange(StateChange_Text);
    m_text->invalidate();
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//CodeEditor

// The position clamped to the document and moved back to the caret stop it is
// in (a byte inside a codepoint, or between a codepoint and its marks).
static text::Position snapToStop(const text::Document &document, text::Position position)
{
    position=document.clamp(position);

    const String &line=document.line(position.line);

    if(position.byte < line.size())
        position.byte=text::prevCaretStop(line, text::nextCaretStop(line, position.byte));
    return position;
}

// Letters, digits, '_' and every non-ASCII codepoint make words; blanks
// separate; anything else is punctuation, a run of which is a word of its own
// for Ctrl+Left/Right.
enum class CharClass
{
    Blank,
    Word,
    Punctuation
};

static CharClass charClass(uint32_t codepoint)
{
    if(codepoint == ' ' || codepoint == '\t')
        return CharClass::Blank;
    if(codepoint >= 0x80 || codepoint == '_' || (codepoint >= '0' && codepoint <= '9')
        || (codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 'A' && codepoint <= 'Z'))
        return CharClass::Word;
    return CharClass::Punctuation;
}

// The leading blanks of a line, in bytes.
static size_t indentOf(const String &line)
{
    size_t i=0;

    while(i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        ++i;
    return i;
}

// Both ways over virtual content: the rows are the lines, the inner panel the
// viewport with nothing around it, inside a padding that keeps the rows off
// the rounded border the skin draws.
GWENI_CONTROL_CONSTRUCTOR(CodeEditor)
{
    setScroll(true, true);
    setPadding(Padding(FieldPadding, FieldPadding, FieldPadding, FieldPadding));
    // the editor takes the mouse over its rows: they and the panel take none
    setMouseInputEnabled(true);
    m_innerPanel->setMouseInputEnabled(false);
    setKeyboardInputEnabled(true);
    setTabable(true);
    addAccelerator("Ctrl + C", &CodeEditor::onCopy);
    addAccelerator("Ctrl + X", &CodeEditor::onCut);
    addAccelerator("Ctrl + V", &CodeEditor::onPaste);
    addAccelerator("Ctrl + A", &CodeEditor::onSelectAll);
    addAccelerator("Ctrl + Z", &CodeEditor::onUndo);
    addAccelerator("Ctrl + Y", &CodeEditor::onRedo);
    addAccelerator("Ctrl + Shift + Z", &CodeEditor::onRedo);

    if(skin::Base *skin=getSkin())
        m_font=skin->getDefaultMonoFont();
    else
    {
        m_font.facename="DejaVuSansMono";
        m_font.size=11.0f;
    }

    setRowFactory([this](Base *parent) -> Base *
        {
            CodeLine *row=parent->newChild<CodeLine>();

            row->m_editor=this;
            row->m_text->setFont(m_font);
            return row;
        });
    setRowBinder([this](Base *row, size_t line) { bindRow(row, line); });

    m_lineCells.assign(1, 0);
    m_widthLines.assign(1, 1);
    m_longest=0;
    setItemCount(1);

    m_observer=m_document.addObserver([this](const text::LineChange &change) { linesChanged(change); });
}

CodeEditor::~CodeEditor()
{
    m_document.removeObserver(m_observer);
}

void CodeEditor::setText(const String &text)
{
    m_document.setText(text);
}

void CodeEditor::setFont(const gweni::Font &font)
{
    if(m_font == font)
        return;

    // the rows' Texts hold a pointer to m_font: it changes in place
    m_font=font;

    for(Base *row : getRows())
    {
        if(CodeLine *line=gweni_cast<CodeLine>(row))
            line->fontChanged();
    }
    for(internal::CodeLineNumber *number : m_numbers)
        number->fontChanged();
    invalidate();
}

void CodeEditor::setLineNumbers(bool show)
{
    if(m_lineNumbers == show)
        return;

    m_lineNumbers=show;

    if(show && !m_gutter)
    {
        // the editor's own child, beside the inner panel (which a new child
        // would otherwise go into, and scroll with the content)
        Base *inner=m_innerPanel;
        PartScope part;

        m_innerPanel=nullptr;
        m_gutter=newChild<internal::CodeGutter>();
        m_innerPanel=inner;
        m_gutter->setPlacement(Placement::Owner);
    }

    if(m_gutter)
        m_gutter->setHidden(!show);
    invalidate();
}

int CodeEditor::gutterWidthFor() const
{
    if(!m_lineNumbers || m_cellWidth <= 0.f)
        return 0;

    int digits=1;

    for(size_t lines=m_document.lineCount(); lines >= 10; lines/=10)
        ++digits;

    return 2*GutterInset+static_cast<int>(std::ceil(digits*static_cast<double>(m_cellWidth)));
}

// The gutter sits in the editor's left padding, so the viewport (and the
// horizontal bar) start after it.
void CodeEditor::updateGutterPadding()
{
    m_gutterWidth=gutterWidthFor();

    Padding padding=getPadding();

    padding.left=FieldPadding+m_gutterWidth;
    setPadding(padding);
}

// One number per pooled row, in pool order, made and dropped with the pool;
// each shown row's number shows its line at its row's place, set (and
// measured in X, the X measuring being over) only when its line changed.
void CodeEditor::placeLineNumbers(skin::Base *skin)
{
    if(!m_gutter)
        return;

    if(!m_lineNumbers)
        return;

    const Rect &viewport=getViewport();

    placeChild(m_gutter, FieldPadding, viewport.y, m_gutterWidth, viewport.h);

    // the numbers of rows the pool dropped go with them
    for(size_t i=0; i < m_numbers.size();)
    {
        bool pooled=false;

        for(size_t j=0; j < poolSize() && !pooled; ++j)
            pooled=(poolRow(j)->getControlId() == m_numberRows[i]);

        if(pooled)
        {
            ++i;
            continue;
        }

        delete m_numbers[i];    // ~Base takes it out of the children, gives its ids back
        m_numbers.erase(m_numbers.begin()+static_cast<std::ptrdiff_t>(i));
        m_numberRows.erase(m_numberRows.begin()+static_cast<std::ptrdiff_t>(i));
    }

    // in pool order: a row's number found further on is moved to its place,
    // a row with none gets a new one
    for(size_t i=0; i < poolSize(); ++i)
    {
        size_t found=i;

        while(found < m_numbers.size() && m_numberRows[found] != poolRow(i)->getControlId())
            ++found;

        if(found < m_numbers.size())
        {
            std::swap(m_numbers[i], m_numbers[found]);
            std::swap(m_numberRows[i], m_numberRows[found]);
            continue;
        }

        internal::CodeLineNumber *number=nullptr;

        {
            PartScope part;

            number=m_gutter->newChild<internal::CodeLineNumber>();
        }
        number->setPlacement(Placement::Owner);
        number->setFont(m_font);
        number->setHidden(true);
        m_numbers.insert(m_numbers.begin()+static_cast<std::ptrdiff_t>(i), number);
        m_numberRows.insert(m_numberRows.begin()+static_cast<std::ptrdiff_t>(i), poolRow(i)->getControlId());
    }

    for(size_t i=0; i < poolSize(); ++i)
    {
        internal::CodeLineNumber *number=m_numbers[i];
        const Base *row=poolRow(i);
        const size_t line=poolItem(i);

        if(line == NoItem || row->hidden())
        {
            number->setHidden(true);
            number->m_line=internal::CodeLineNumber::NoLine;
            continue;
        }

        bool changed=number->hidden();

        number->setHidden(false);
        number->setCurrent(!m_readOnly && line == m_caret.line);

        if(number->m_line != line)
        {
            number->m_line=line;
            number->setText(std::to_string(line+1));
            changed=true;
        }

        if(changed)
            number->doCalculateSize(skin, Dim::X);

        placeChild(number, 0, row->getY(), m_gutterWidth-GutterInset, row->getHeight());
    }
}

int CodeEditor::columnX(size_t column) const
{
    return TextInset+static_cast<int>(std::floor(static_cast<double>(column)*m_cellWidth));
}

CodeLine *CodeEditor::rowForLine(size_t line) const
{
    return gweni_cast<CodeLine>(rowForItem(line));
}

void CodeEditor::bindRow(Base *row, size_t line)
{
    CodeLine *codeLine=gweni_cast<CodeLine>(row);

    if(!codeLine || line >= m_document.lineCount())
        return;

    codeLine->bind(line, m_document.line(line));
    codeLine->setCurrent(!m_readOnly && line == m_caret.line);

    size_t begin=0;
    size_t end=0;

    lineSelection(line, begin, end);
    codeLine->setSelection(begin, end);
}

void CodeEditor::addWidth(size_t cells)
{
    if(cells >= m_widthLines.size())
        m_widthLines.resize(cells+1, 0);

    ++m_widthLines[cells];
    m_longest=std::max(m_longest, cells);
}

void CodeEditor::removeWidth(size_t cells)
{
    if(cells >= m_widthLines.size() || m_widthLines[cells] == 0)
        return;

    --m_widthLines[cells];

    while(m_longest > 0 && m_widthLines[m_longest] == 0)
        --m_longest;
}

// Lines [first, first+removed) were replaced by [first, first+inserted). The
// new lines' widths are counted before the old ones are taken away, so a line
// that keeps the longest width never scans down for the next one. The rows:
// lines added or removed shift the rows below to their new lines, unbound
// (VirtualList::insertItems, removeItems), and the lines replaced in place
// are bound again; a line that came into view gets a row by the next layout.
void CodeEditor::linesChanged(const text::LineChange &change)
{
    const size_t first=change.first;

    // the app changed the text: the steps no longer fit it
    if(!m_ownEdit)
        m_undo.clear();

    m_newCells.resize(change.inserted);
    for(size_t i=0; i < change.inserted; ++i)
    {
        m_newCells[i]=text::stringCells(m_document.line(first+i));
        addWidth(m_newCells[i]);
    }

    for(size_t i=0; i < change.removed && first+i < m_lineCells.size(); ++i)
        removeWidth(m_lineCells[first+i]);

    if(change.inserted > change.removed)
    {
        // room to a power of two, as the document's own vector of lines
        // grows, so a new text leaves headroom and the first line break after
        // it does not copy every line's count
        const size_t needed=m_lineCells.size()+change.inserted-change.removed;

        if(needed > m_lineCells.capacity())
        {
            size_t capacity=16;

            while(capacity < needed)
                capacity*=2;
            m_lineCells.reserve(capacity);
        }
        m_lineCells.insert(m_lineCells.begin()+static_cast<std::ptrdiff_t>(first+change.removed),
            change.inserted-change.removed, 0);
    }
    else if(change.removed > change.inserted)
        m_lineCells.erase(m_lineCells.begin()+static_cast<std::ptrdiff_t>(first+change.inserted),
            m_lineCells.begin()+static_cast<std::ptrdiff_t>(first+change.removed));

    std::copy(m_newCells.begin(), m_newCells.end(), m_lineCells.begin()+static_cast<std::ptrdiff_t>(first));

    if(change.inserted > change.removed)
        insertItems(first+change.removed, change.inserted-change.removed);
    else if(change.removed > change.inserted)
        removeItems(first+change.inserted, change.removed-change.inserted);

    // the rows the shift moved show their line at its new index
    for(size_t i=0; i < poolSize(); ++i)
    {
        if(CodeLine *row=gweni_cast<CodeLine>(poolRow(i)))
            row->m_line=poolItem(i) == NoItem?CodeLine::NoLine:poolItem(i);
    }

    // The caret and the anchor stay on their text: past the lines replaced
    // they move with the lines, inside them they are kept in the new ones
    // (an editing step of the editor's own moves the caret after this).
    auto shift=[this, &change](text::Position &position)
        {
            if(position.line >= change.first+change.removed)
                position.line=position.line+change.inserted-change.removed;
            else if(position.line >= change.first)
                position.line=std::min(position.line, change.first+change.inserted-1);
            position=snapToStop(m_document, position);
        };

    shift(m_caret);
    shift(m_anchor);
    m_caretColumn=text::stringCells(m_document.line(m_caret.line), 0, m_caret.byte);
    syncCurrentLine();
    syncSelection();
    caretChanged();

    const size_t replaced=std::min(change.removed, change.inserted);

    // a whole new text replaces more lines than there are rows
    if(replaced > poolSize())
        refresh();
    else
    {
        for(size_t i=0; i < replaced; ++i)
            refreshItem(first+i);
    }

    // the content's width may have changed
    invalidate();
}

///////////////////////////////////////////////////////////////////////////
// Caret and selection

text::Range CodeEditor::getSelection() const
{
    return (m_anchor < m_caret)?text::Range{m_anchor, m_caret}:text::Range{m_caret, m_anchor};
}

String CodeEditor::getSelectedText() const
{
    return hasSelection()?m_document.textIn(getSelection()):String();
}

void CodeEditor::selectAll()
{
    m_anchor=text::Position();
    moveCaret(m_document.endPosition(), true);
}

void CodeEditor::setCaret(const text::Position &position, bool extend)
{
    moveCaret(position, extend);
}

void CodeEditor::moveCaret(const text::Position &position, bool extend, bool keepGoal, bool reveal)
{
    const size_t oldLine=m_caret.line;

    m_caret=snapToStop(m_document, position);
    if(!extend)
        m_anchor=m_caret;
    m_caretColumn=text::stringCells(m_document.line(m_caret.line), 0, m_caret.byte);

    if(!keepGoal)
        m_goalColumn=NoGoal;

    if(oldLine != m_caret.line)
        syncCurrentLine();

    syncSelection();
    m_caretBlink.keepLit(this);
    if(reveal)
        makeCaretVisible();
    caretChanged();
}

// A line inside the selection is covered from its begin's cell (0 below the
// first line) to its end's, or past its last glyph by one cell when its line
// break is selected too (every line but the last one of the selection).
void CodeEditor::lineSelection(size_t line, size_t &begin, size_t &end) const
{
    begin=end=0;

    if(!hasSelection())
        return;

    const text::Range range=getSelection();

    if(line < range.begin.line || line > range.end.line || line >= m_document.lineCount())
        return;

    const String &text=m_document.line(line);

    begin=(line == range.begin.line)?text::stringCells(text, 0, range.begin.byte):0;
    end=(line == range.end.line)?text::stringCells(text, 0, range.end.byte):getLineCells(line)+1;
}

void CodeEditor::syncSelection()
{
    const bool any=hasSelection();

    if(!any && !m_selectionShown)
        return;

    m_selectionShown=any;

    for(size_t i=0; i < poolSize(); ++i)
    {
        CodeLine *row=gweni_cast<CodeLine>(poolRow(i));
        size_t begin=0;
        size_t end=0;

        if(!row)
            continue;

        if(poolItem(i) != NoItem)
            lineSelection(poolItem(i), begin, end);
        row->setSelection(begin, end);
    }
}

void CodeEditor::caretChanged()
{
    addStateChange(StateChange_Caret);
}

void CodeEditor::syncCurrentLine()
{
    for(size_t i=0; i < poolSize(); ++i)
    {
        CodeLine *row=gweni_cast<CodeLine>(poolRow(i));

        if(row && poolItem(i) != NoItem)
            row->setCurrent(!m_readOnly && poolItem(i) == m_caret.line);
    }

    for(internal::CodeLineNumber *number : m_numbers)
    {
        if(number->m_line != internal::CodeLineNumber::NoLine)
            number->setCurrent(!m_readOnly && number->m_line == m_caret.line);
    }
}

// The line by the next layout, which knows the viewport (only when the last
// one left it out of view: a caret moving inside the view lays nothing out);
// the column now, at the last layout's viewport, kept inside the content by
// the next layout.
void CodeEditor::makeCaretVisible()
{
    const int lineHeight=m_lineHeight;

    if(lineHeight <= 0)
        scrollToItem(m_caret.line);
    else
    {
        const int64_t top=static_cast<int64_t>(m_caret.line)*lineHeight;
        const int64_t offset=getVerticalScrollOffset();

        if(top < offset || top+lineHeight > offset+getViewportHeight())
            scrollToItem(m_caret.line);
    }

    const int width=getViewport().w;

    if(width <= 0)
        return;

    const int x=columnX(m_caretColumn);
    const int cell=static_cast<int>(std::ceil(m_cellWidth));
    int offset=getHorizontalScrollOffset();

    if(x-TextInset < offset)
        offset=x-TextInset;
    else if(x+cell+TextInset > offset+width)
        offset=x+cell+TextInset-width;

    setHorizontalScrollOffset(std::max(offset, 0));
}

Rect CodeEditor::getCaretBounds() const
{
    if(m_lineHeight <= 0 || m_readOnly)
        return Rect();

    const Rect &viewport=getViewport();
    const int64_t y=static_cast<int64_t>(viewport.y)+static_cast<int64_t>(m_caret.line)*m_lineHeight
        -getVerticalScrollOffset();
    const int64_t x=static_cast<int64_t>(viewport.x)-getHorizontalScrollOffset()+columnX(m_caretColumn);
    const int64_t x0=std::max<int64_t>(x, viewport.x);
    const int64_t y0=std::max<int64_t>(y, viewport.y);
    const int64_t x1=std::min<int64_t>(x+1, viewport.x+viewport.w);
    const int64_t y1=std::min<int64_t>(y+m_lineHeight, viewport.y+viewport.h);

    if(x1 <= x0 || y1 <= y0)
        return Rect();

    return Rect(static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1-x0), static_cast<int>(y1-y0));
}

text::Position CodeEditor::positionAt(const Point &local) const
{
    if(m_lineHeight <= 0 || m_cellWidth <= 0.f)
        return m_caret;

    const Rect &viewport=getViewport();
    const int64_t y=static_cast<int64_t>(local.y)-viewport.y+getVerticalScrollOffset();
    const size_t last=m_document.lineCount()-1;

    if(y < 0)
        return text::Position{0, 0};

    const size_t line=static_cast<size_t>(y/m_lineHeight);

    if(line > last)
        return m_document.endPosition();

    const double x=static_cast<double>(local.x)-viewport.x+getHorizontalScrollOffset()-TextInset;

    return text::Position{line, text::byteNearestCell(m_document.line(line), x/m_cellWidth)};
}

text::Position CodeEditor::glyphAt(const Point &local) const
{
    const text::Position line=positionAt(local);

    if(m_cellWidth <= 0.f)
        return line;

    const double x=static_cast<double>(local.x)-getViewport().x+getHorizontalScrollOffset()-TextInset;
    const size_t cell=static_cast<size_t>(std::max(0.0, std::floor(x/m_cellWidth)));

    return text::Position{line.line, text::byteAtCell(m_document.line(line.line), cell)};
}

void CodeEditor::updateCaretBlink()
{
    m_caretBlink.step(this);
}

text::Position CodeEditor::atColumn(size_t line, size_t cells) const
{
    return text::Position{line, text::byteAtCell(m_document.line(line), cells)};
}

text::Position CodeEditor::wordRight(const text::Position &position) const
{
    const String &line=m_document.line(position.line);

    if(position.byte >= line.size())
        return (position.line+1 < m_document.lineCount())?text::Position{position.line+1, 0}:position;

    size_t i=position.byte;

    while(i < line.size() && charClass(text::utf8CodepointAt(line, i)) == CharClass::Blank)
        i=text::nextCaretStop(line, i);

    if(i < line.size())
    {
        const CharClass run=charClass(text::utf8CodepointAt(line, i));

        while(i < line.size() && charClass(text::utf8CodepointAt(line, i)) == run)
            i=text::nextCaretStop(line, i);
    }
    return text::Position{position.line, i};
}

text::Position CodeEditor::wordLeft(const text::Position &position) const
{
    if(position.byte == 0)
    {
        if(position.line == 0)
            return position;
        return text::Position{position.line-1, m_document.line(position.line-1).size()};
    }

    const String &line=m_document.line(position.line);
    size_t i=std::min(position.byte, line.size());

    while(i > 0 && charClass(text::utf8CodepointAt(line, text::prevCaretStop(line, i))) == CharClass::Blank)
        i=text::prevCaretStop(line, i);

    if(i > 0)
    {
        const CharClass run=charClass(text::utf8CodepointAt(line, text::prevCaretStop(line, i)));

        while(i > 0 && charClass(text::utf8CodepointAt(line, text::prevCaretStop(line, i))) == run)
            i=text::prevCaretStop(line, i);
    }
    return text::Position{position.line, i};
}

// Up from the first line goes to its start, down from the last to its end,
// as in a text field; anywhere else the goal column is kept for the next one.
void CodeEditor::moveLines(int64_t lines, bool extend)
{
    const size_t goal=(m_goalColumn == NoGoal)?m_caretColumn:m_goalColumn;
    const int64_t last=static_cast<int64_t>(m_document.lineCount())-1;
    const int64_t target=static_cast<int64_t>(m_caret.line)+lines;

    if(target < 0)
        moveCaret(text::Position{0, 0}, extend);
    else if(target > last)
        moveCaret(m_document.endPosition(), extend);
    else
    {
        moveCaret(atColumn(static_cast<size_t>(target), goal), extend, true);
        m_goalColumn=goal;
    }
}

int CodeEditor::pageLines() const
{
    return (m_lineHeight > 0)?std::max(1, getViewportHeight()/m_lineHeight):1;
}

void CodeEditor::replaceRange(const text::Range &range, const String &text, text::EditKind kind)
{
    const text::Selection before=currentSelection();
    text::Edit edit;

    m_ownEdit=true;
    edit=m_document.replace(range, text);
    m_ownEdit=false;

    moveCaret(edit.textEnd, false);
    m_undo.record(std::move(edit), kind, before, currentSelection());
}

void CodeEditor::insertText(const String &text, text::EditKind kind)
{
    replaceRange(hasSelection()?getSelection():text::Range{m_caret, m_caret}, text, kind);
}

void CodeEditor::insertText(const String &text)
{
    if(editable())
        insertText(text, text::EditKind::Insert);
}

bool CodeEditor::editable()
{
    m_escapeArmed=false;
    return !m_readOnly;
}

void CodeEditor::restoreSelection(const text::Selection &selection)
{
    m_anchor=snapToStop(m_document, selection.anchor);
    moveCaret(selection.caret, true);
}

// Each edit's inverse, from the last to the first, then the selection before
// the step; the history's own records, so nothing is recorded or cleared.
bool CodeEditor::undo()
{
    if(m_readOnly)
        return false;

    const text::UndoStep *step=m_undo.undo();

    if(!step)
        return false;

    m_ownEdit=true;
    for(size_t i=step->edits.size(); i-- > 0;)
    {
        text::Edit inverse=step->edits[i].inverse();

        m_document.apply(inverse);
    }
    m_ownEdit=false;

    restoreSelection(step->before);
    return true;
}

bool CodeEditor::redo()
{
    if(m_readOnly)
        return false;

    const text::UndoStep *step=m_undo.redo();

    if(!step)
        return false;

    m_ownEdit=true;
    for(const text::Edit &recorded : step->edits)
    {
        text::Edit edit;

        edit.range=recorded.range;
        edit.text=recorded.text;
        m_document.apply(edit);
    }
    m_ownEdit=false;

    restoreSelection(step->after);
    return true;
}

void CodeEditor::setReadOnly(bool readOnly)
{
    if(m_readOnly == readOnly)
        return;

    m_readOnly=readOnly;
    m_undo.seal();

    if(m_readOnly)
        m_caretBlink.stop(this);
    else if(isFocussed())
    {
        m_caretBlink.keepLit(this);
        m_caretBlink.start(this);
    }

    syncCurrentLine();
    caretChanged();
}

void CodeEditor::selectedLines(size_t &first, size_t &last) const
{
    const text::Range range=getSelection();

    first=range.begin.line;
    last=range.end.line;

    if(last > first && range.end.byte == 0)
        --last;
}

// Each line gets (or loses) its spaces as an edit of its own, one undo step
// together; the anchor and the caret move with the text of their lines.
void CodeEditor::indentLines(bool outdent)
{
    size_t first=0;
    size_t last=0;

    selectedLines(first, last);

    const text::Selection before=currentSelection();
    text::Position anchor=m_anchor;
    text::Position caret=m_caret;
    std::vector<text::Edit> edits;

    m_ownEdit=true;
    for(size_t line=first; line <= last; ++line)
    {
        const String &text=m_document.line(line);
        long change=0;

        if(!outdent)
        {
            edits.push_back(m_document.insert(text::Position{line, 0}, String(static_cast<size_t>(m_tabWidth), ' ')));
            change=m_tabWidth;
        }
        else
        {
            size_t remove=0;

            if(!text.empty() && text[0] == '\t')
                remove=1;
            else
            {
                size_t spaces=0;

                while(spaces < text.size() && text[spaces] == ' ')
                    ++spaces;
                if(spaces > 0)
                    remove=(spaces-1)%static_cast<size_t>(m_tabWidth)+1;
            }

            if(remove == 0)
                continue;

            edits.push_back(m_document.erase({text::Position{line, 0}, text::Position{line, remove}}));
            change=-static_cast<long>(remove);
        }

        for(text::Position *position : {&anchor, &caret})
        {
            if(position->line == line)
                position->byte=static_cast<size_t>(std::max<long>(0, static_cast<long>(position->byte)+change));
        }
    }
    m_ownEdit=false;

    if(edits.empty())
        return;

    m_anchor=snapToStop(m_document, anchor);
    moveCaret(caret, true);

    text::UndoGroup group(m_undo, text::EditKind::Other);

    for(text::Edit &edit : edits)
        m_undo.record(std::move(edit), text::EditKind::Other, before, currentSelection());
}

///////////////////////////////////////////////////////////////////////////
// Keys

// Typed text: printable codepoints only (Tab and the controls are keys).
bool CodeEditor::onChar(gweni::UnicodeChar c)
{
    if(c < 0x20 || c == 0x7f)
        return false;

    const String str=utf8Encode(c);

    if(str.empty() || !editable())
        return false;

    insertText(str, text::EditKind::Insert);
    return true;
}

bool CodeEditor::onKeyReturn(bool down)
{
    if(!down || !editable())
        return true;

    const text::Position begin=hasSelection()?getSelection().begin:m_caret;
    const String &line=m_document.line(begin.line);

    insertText("\n"+line.substr(0, std::min(indentOf(line), begin.byte)), text::EditKind::Insert);
    return true;
}

bool CodeEditor::onKeyBackspace(bool down)
{
    if(!down || !editable())
        return true;

    const text::EditKind kind=text::EditKind::DeleteBackward;

    if(hasSelection())
        replaceRange(getSelection(), String(), kind);
    else if(m_caret.byte > 0)
        replaceRange({{m_caret.line, utf8Prev(m_document.line(m_caret.line), m_caret.byte)}, m_caret}, String(), kind);
    else if(m_caret.line > 0)
        replaceRange({{m_caret.line-1, m_document.line(m_caret.line-1).size()}, m_caret}, String(), kind);
    return true;
}

bool CodeEditor::onKeyDelete(bool down)
{
    if(!down || !editable())
        return true;

    const String &line=m_document.line(m_caret.line);
    const text::EditKind kind=text::EditKind::DeleteForward;

    if(hasSelection())
        replaceRange(getSelection(), String(), kind);
    else if(m_caret.byte < line.size())
        replaceRange({m_caret, {m_caret.line, text::nextCaretStop(line, m_caret.byte)}}, String(), kind);
    else if(m_caret.line+1 < m_document.lineCount())
        replaceRange({m_caret, {m_caret.line+1, 0}}, String(), kind);
    return true;
}

// Without Shift a selection collapses to the end the key points at.
bool CodeEditor::onKeyLeft(bool down)
{
    if(!down)
        return true;

    const bool extend=input::isShiftDown();

    if(hasSelection() && !extend && !input::isControlDown())
        moveCaret(getSelection().begin, false);
    else if(input::isControlDown())
        moveCaret(wordLeft(m_caret), extend);
    else if(m_caret.byte > 0)
        moveCaret({m_caret.line, text::prevCaretStop(m_document.line(m_caret.line), m_caret.byte)}, extend);
    else if(m_caret.line > 0)
        moveCaret({m_caret.line-1, m_document.line(m_caret.line-1).size()}, extend);
    else
        moveCaret(m_caret, extend);
    return true;
}

bool CodeEditor::onKeyRight(bool down)
{
    if(!down)
        return true;

    const bool extend=input::isShiftDown();
    const String &line=m_document.line(m_caret.line);

    if(hasSelection() && !extend && !input::isControlDown())
        moveCaret(getSelection().end, false);
    else if(input::isControlDown())
        moveCaret(wordRight(m_caret), extend);
    else if(m_caret.byte < line.size())
        moveCaret({m_caret.line, text::nextCaretStop(line, m_caret.byte)}, extend);
    else if(m_caret.line+1 < m_document.lineCount())
        moveCaret({m_caret.line+1, 0}, extend);
    else
        moveCaret(m_caret, extend);
    return true;
}

bool CodeEditor::onKeyUp(bool down)
{
    if(down)
        moveLines(-1, input::isShiftDown());
    return true;
}

bool CodeEditor::onKeyDown(bool down)
{
    if(down)
        moveLines(1, input::isShiftDown());
    return true;
}

// Home goes to the first non-blank, and from there to column 0.
bool CodeEditor::onKeyHome(bool down)
{
    if(!down)
        return true;

    if(input::isControlDown())
    {
        moveCaret(text::Position{0, 0}, input::isShiftDown());
        return true;
    }

    const size_t indent=indentOf(m_document.line(m_caret.line));

    moveCaret({m_caret.line, (m_caret.byte == indent)?0:indent}, input::isShiftDown());
    return true;
}

bool CodeEditor::onKeyend(bool down)
{
    if(!down)
        return true;

    if(input::isControlDown())
        moveCaret(m_document.endPosition(), input::isShiftDown());
    else
        moveCaret({m_caret.line, m_document.line(m_caret.line).size()}, input::isShiftDown());
    return true;
}

// The view and the caret move by a page together, so the caret keeps its
// place on screen.
bool CodeEditor::onKeyPageUp(bool down)
{
    if(down)
    {
        scrollByRows(-pageLines());
        moveLines(-pageLines(), input::isShiftDown());
    }
    return true;
}

bool CodeEditor::onKeyPageDown(bool down)
{
    if(down)
    {
        scrollByRows(pageLines());
        moveLines(pageLines(), input::isShiftDown());
    }
    return true;
}

// Tab edits unless Escape came just before it or Tab is given to the focus:
// spaces to the next tab stop in cells (over a selection inside one line, in
// its place), the selected lines indented, Shift+Tab outdenting.
bool CodeEditor::onKeyTab(bool down)
{
    if(!m_tabInsertsSpaces || m_readOnly || m_escapeArmed)
    {
        if(down)
            m_escapeArmed=false;
        return ParentClass::onKeyTab(down);
    }

    if(!down || !editable())
        return true;

    size_t first=0;
    size_t last=0;

    selectedLines(first, last);

    if(input::isShiftDown())
        indentLines(true);
    else if(hasSelection() && last > first)
        indentLines(false);
    else
    {
        const text::Position begin=hasSelection()?getSelection().begin:m_caret;
        const size_t column=text::stringCells(m_document.line(begin.line), 0, begin.byte);
        const size_t width=static_cast<size_t>(m_tabWidth);

        insertText(String(width-column%width, ' '), text::EditKind::Insert);
    }
    return true;
}

bool CodeEditor::onKeyEscape(bool down)
{
    if(down)
        m_escapeArmed=true;
    return false;
}

bool CodeEditor::onKeyPress(int key, bool press)
{
    if(press && key != keys::Tab && key != keys::Escape && key != keys::Shift && key != keys::Control
        && key != keys::Alt && key != keys::Command)
        m_escapeArmed=false;

    return ParentClass::onKeyPress(key, press);
}

void CodeEditor::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    m_escapeArmed=false;

    if(!m_readOnly)
    {
        m_caretBlink.keepLit(this);
        m_caretBlink.start(this);
    }
    caretChanged();
}

// Typing after the focus comes back is a step of its own.
void CodeEditor::onLostKeyboardFocus()
{
    ParentClass::onLostKeyboardFocus();
    m_caretBlink.stop(this);
    m_undo.seal();
    m_escapeArmed=false;
    caretChanged();
}

void CodeEditor::onMouseClickLeft(int x, int y, bool down)
{
    if(down)
    {
        m_escapeArmed=false;
        m_dragPoint=canvasPosToLocal(Point(x, y));
        moveCaret(positionAt(m_dragPoint), input::isShiftDown());
        MouseFocus=this;
    }
    else if(MouseFocus == this)
    {
        MouseFocus=nullptr;
        stopAutoScroll();
    }
}

// The caret follows the mouse; past the viewport's edge the auto-scroll moves
// the view, so the move itself scrolls nothing (no makeCaretVisible).
void CodeEditor::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);

    if(MouseFocus != this)
        return;

    m_dragPoint=canvasPosToLocal(Point(x, y));
    moveCaret(positionAt(m_dragPoint), true, false, false);

    const Point overflow=dragOverflow();

    if((overflow.x != 0 || overflow.y != 0) && !m_autoScroll)
    {
#if GWENI_ANIMATE
        class AutoScroll: public anim::Animation
        {
        public:
            void think() override
            {
                static_cast<CodeEditor *>(m_control)->autoScrollStep();
            }
        };

        m_autoScrollTime=platform::getTimeInSeconds();
        m_autoScrollCarry[0]=m_autoScrollCarry[1]=0.0f;
        m_autoScroll=new AutoScroll();
        anim::add(this, m_autoScroll);
#endif
    }
}

Point CodeEditor::dragOverflow() const
{
    const Rect &viewport=getViewport();
    Point overflow;

    if(m_dragPoint.x < viewport.x)
        overflow.x=m_dragPoint.x-viewport.x;
    else if(m_dragPoint.x >= viewport.x+viewport.w)
        overflow.x=m_dragPoint.x-(viewport.x+viewport.w-1);

    if(m_dragPoint.y < viewport.y)
        overflow.y=m_dragPoint.y-viewport.y;
    else if(m_dragPoint.y >= viewport.y+viewport.h)
        overflow.y=m_dragPoint.y-(viewport.y+viewport.h-1);

    return overflow;
}

void CodeEditor::stopAutoScroll()
{
#if GWENI_ANIMATE
    if(!m_autoScroll)
        return;

    anim::cancel(this, m_autoScroll);
    m_autoScroll=nullptr;
#endif
}

// Each frame the view moves by the time since the last one (the canvas clock,
// at most a tenth of a second of it) times the speed past each edge; the
// whole units are scrolled, the rest kept for the next frame. The caret goes
// to the point under the mouse in the new view, the anchor kept.
void CodeEditor::autoScrollStep()
{
    const Point overflow=dragOverflow();

    if(MouseFocus != this || (overflow.x == 0 && overflow.y == 0))
    {
        stopAutoScroll();
        return;
    }

    const float now=platform::getTimeInSeconds();
    const float elapsed=std::min(std::max(now-m_autoScrollTime, 0.0f), 0.1f);

    m_autoScrollTime=now;

    const int distance[2]={overflow.x, overflow.y};
    int step[2]={0, 0};

    for(int axis=0; axis < 2; ++axis)
    {
        if(distance[axis] == 0)
        {
            m_autoScrollCarry[axis]=0.0f;
            continue;
        }

        const float speed=AutoScrollBase+AutoScrollRate*static_cast<float>(std::abs(distance[axis]));

        m_autoScrollCarry[axis]+=((distance[axis] < 0)?-speed:speed)*elapsed;
        step[axis]=static_cast<int>(m_autoScrollCarry[axis]);
        m_autoScrollCarry[axis]-=static_cast<float>(step[axis]);
    }

    if(step[1] != 0)
        setVerticalScrollOffset(std::max(0, getVerticalScrollOffset()+step[1]));
    if(step[0] != 0)
        setHorizontalScrollOffset(gweni::clamp(getHorizontalScrollOffset()+step[0], 0,
            getMaxHorizontalScrollOffset()));

    if(step[0] != 0 || step[1] != 0)
        moveCaret(positionAt(m_dragPoint), true, false, false);
}

// The word, punctuation run or blank run the press is on (the one before it
// at a line's end).
void CodeEditor::onMouseDoubleClickLeft(int x, int y)
{
    const text::Position at=glyphAt(canvasPosToLocal(Point(x, y)));
    const String &line=m_document.line(at.line);

    if(line.empty())
    {
        moveCaret(at, false);
        return;
    }

    size_t probe=at.byte;

    if(probe >= line.size())
        probe=text::prevCaretStop(line, line.size());

    const CharClass run=charClass(text::utf8CodepointAt(line, probe));
    size_t begin=probe;
    size_t end=text::nextCaretStop(line, probe);

    while(begin > 0 && charClass(text::utf8CodepointAt(line, text::prevCaretStop(line, begin))) == run)
        begin=text::prevCaretStop(line, begin);
    while(end < line.size() && charClass(text::utf8CodepointAt(line, end)) == run)
        end=text::nextCaretStop(line, end);

    moveCaret(text::Position{at.line, begin}, false);
    moveCaret(text::Position{at.line, end}, true);
}

void CodeEditor::onCopy(event::Info)
{
    if(hasSelection())
        platform::setClipboardText(getSelectedText());
}

void CodeEditor::onCut(event::Info)
{
    if(!hasSelection())
        return;

    platform::setClipboardText(getSelectedText());

    if(editable())
        replaceRange(getSelection(), String(), text::EditKind::Cut);
}

void CodeEditor::onPaste(event::Info)
{
    const String text=platform::getClipboardText();

    if(!text.empty() && editable())
    {
        text::UndoGroup group(m_undo, text::EditKind::Paste);

        insertText(text, text::EditKind::Paste);
    }
}

void CodeEditor::onUndo(event::Info)
{
    m_escapeArmed=false;
    undo();
}

void CodeEditor::onRedo(event::Info)
{
    m_escapeArmed=false;
    redo();
}

void CodeEditor::onSelectAll(event::Info)
{
    selectAll();
}

// The view's scroll is final here: a caret whose place on screen moved with
// it is drawn again.
void CodeEditor::contentsPlaced(skin::Base *skin)
{
    ParentClass::contentsPlaced(skin);
    placeLineNumbers(skin);

    if(m_caretScroll[0] != getHorizontalScrollOffset() || m_caretScroll[1] != getVerticalScrollOffset())
    {
        m_caretScroll[0]=getHorizontalScrollOffset();
        m_caretScroll[1]=getVerticalScrollOffset();
        caretChanged();
    }
}

bool CodeEditor::metricsCurrent(skin::Base *skin) const
{
    const renderer::Base *renderer=skin?skin->getRenderer():nullptr;
    const Canvas *canvas=const_cast<CodeEditor *>(this)->getCanvas();
    const float scale=canvas?canvas->scale():1.0f;

    return m_lineHeight > 0 && m_metricsFont == m_font && m_metricsRenderer == renderer
        && renderer && m_metricsSerial == renderer->textLayoutSerial() && m_metricsScale == scale;
}

// The cell is the advance of '0', the width a fixed-width run gives every
// cell, averaged over CellSample of them; a line is the font's line box.
void CodeEditor::readMetrics(skin::Base *skin)
{
    const renderer::Base *renderer=skin?skin->getRenderer():nullptr;
    Canvas *canvas=getCanvas();

    m_metricsFont=m_font;
    m_metricsRenderer=renderer;
    m_metricsSerial=renderer?renderer->textLayoutSerial():0;
    m_metricsScale=canvas?canvas->scale():1.0f;

    m_lineHeight=std::max(1, measureLineHeight(m_font));
    m_cellWidth=std::max(1, measureText(m_font, String(CellSample, '0')).x)/static_cast<float>(CellSample);
}

// The metrics are read before anything is sized from them: the row height
// they give is placement, which this pass lays out next.
void CodeEditor::calculateSize(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X && !metricsCurrent(skin))
    {
        PlacementScope placement;

        readMetrics(skin);
        setRowHeight(m_lineHeight);
    }

    // a new digit count or cell width: the gutter's room, for this pass
    if(dim == Dim::X && gutterWidthFor() != m_gutterWidth)
    {
        PlacementScope placement;

        updateGutterPadding();
    }

    ParentClass::calculateSize(skin, dim);
}

Size CodeEditor::contentSize() const
{
    Size content=ParentClass::contentSize();

    content.width=2*TextInset+static_cast<int>(std::ceil(static_cast<double>(m_longest+1)*m_cellWidth));
    return content;
}

}//namespace controls
}//namespace gweni
