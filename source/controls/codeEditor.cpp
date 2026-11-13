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
#include <gweni/text/cellWrap.h>
#include <gweni/text/cells.h>
#include <gweni/utility.h>

#include <algorithm>
#include <climits>
#include <cmath>

namespace gweni
{
namespace controls
{

static_assert(skin::CodeTokenKinds == static_cast<size_t>(text::TokenKind::Count),
    "the skin keeps a colour per token kind");

///////////////////////////////////////////////////////////////////////////
//CodeLine

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(CodeLineText)
{}

void CodeLineText::setColorSpans(std::vector<renderer::ColorSpan> &spans)
{
    if(spans == m_spans)
        return;

    m_spans.swap(spans);
    addStateChange(StateChange_Text);
}

}//namespace internal

// The row draws its backgrounds, its Text the line; neither takes the mouse,
// which is the editor's.
GWENI_CONTROL_CONSTRUCTOR(CodeLine)
{
    setMouseInputEnabled(false);

    m_text=newChild<internal::CodeLineText>();
    m_text->setPlacement(Placement::Owner);
    m_text->setMouseInputEnabled(false);
    m_text->setSkinTextColor(internal::SkinTextColor::CodePlain);
}

// Unwrapped, the row is its one Text. Wrapped, it is a Text per visual row,
// made as the line needs them and deleted when it needs fewer, so a row holds
// what a row bound to that line at that width holds.
void CodeLine::bind(size_t line, const String &text, size_t wrapCells)
{
    m_line=line;
    if(wrapCells == 0)
        m_rowStarts.clear();
    else
        text::wrapCells(text, wrapCells, m_rowStarts);

    const size_t rows=getRowCount();

    while(m_pieces.size() > rows-1)
    {
        delete m_pieces.back();    // ~Base takes it out of the children, gives its ids back
        m_pieces.pop_back();
    }
    while(m_pieces.size() < rows-1)
    {
        internal::CodeLineText *piece=nullptr;

        {
            PartScope part;

            piece=newChild<internal::CodeLineText>();
        }
        piece->setPlacement(Placement::Owner);
        piece->setMouseInputEnabled(false);
        piece->setSkinTextColor(internal::SkinTextColor::CodePlain);
        if(m_editor)
            piece->setFont(m_editor->getFont());
        m_pieces.push_back(piece);
    }

    if(rows == 1)
    {
        m_lineText.clear();
        m_text->setText(text);
    }
    else
    {
        m_lineText=text;
        for(size_t row=0; row < rows; ++row)
        {
            const size_t begin=m_rowStarts[row];
            const size_t end=(row+1 < rows)?m_rowStarts[row+1]:text.size();

            getRowText(row)->setText(text.substr(begin, end-begin));
        }
    }
    updateColorSpans();
}

// Wrapped, each visual row's Text takes the spans over its bytes, shifted to
// its own start.
void CodeLine::updateColorSpans()
{
    if(!m_editor)
        return;

    // built in the editor's scratch and swapped with the Text's, so a row
    // bound again reuses both vectors
    std::vector<renderer::ColorSpan> &spans=m_editor->m_spanScratch;

    if(m_line == NoLine)
        spans.clear();
    else
        m_editor->tokenSpans(m_line, spans);

    // the row background follows the same tokens: set as the row is bound
    // or its line re-coloured, so a scroll marks the rows it binds alone
    const DiffKind kind=(m_line == NoLine)?DiffKind::None:m_editor->diffKind(m_line);

    if(kind != m_diffKind)
    {
        m_diffKind=kind;
        addStateChange(StateChange_Background);
    }

    if(m_pieces.empty())
    {
        m_text->setColorSpans(spans);
        return;
    }

    std::vector<renderer::ColorSpan> &rowSpans=m_editor->m_rowSpanScratch;
    const size_t rows=getRowCount();

    for(size_t row=0; row < rows; ++row)
    {
        const size_t begin=m_rowStarts[row];
        const size_t end=(row+1 < rows)?m_rowStarts[row+1]:m_lineText.size();

        rowSpans.clear();
        for(const renderer::ColorSpan &span : spans)
        {
            if(span.end <= begin || span.begin >= end)
                continue;
            rowSpans.push_back(renderer::ColorSpan{std::max(span.begin, begin)-begin,
                std::min(span.end, end)-begin, span.color});
        }
        (row == 0?m_text:m_pieces[row-1])->setColorSpans(rowSpans);
    }
}

void CodeLine::updateColors()
{
    ParentClass::updateColors();
    updateColorSpans();
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
    Rect rect;

    for(size_t row=0; row < getRowCount(); ++row)
    {
        if(selectionRectOnRow(row, rect))
            return rect;
    }
    return Rect();
}

void CodeLine::getSelectionRects(std::vector<Rect> &rects) const
{
    Rect rect;

    rects.clear();
    for(size_t row=0; row < getRowCount(); ++row)
    {
        if(selectionRectOnRow(row, rect))
            rects.push_back(rect);
    }
}

// The selection's cells [begin, end) are the line's; a visual row holds the
// line's cells [rowBegin, rowEnd), and its rect the part of the selection in
// them, from the row's own first cell.
bool CodeLine::selectionRectOnRow(size_t row, Rect &rect) const
{
    if(!m_editor || m_selectionEnd <= m_selectionBegin)
        return false;

    if(!m_editor->isWrapped())
    {
        if(row != 0)
            return false;

        const int x0=m_editor->columnX(m_selectionBegin);
        const int x1=m_editor->columnX(m_selectionEnd);

        rect=Rect(x0, 0, x1-x0, getHeight());
        return true;
    }

    const String &text=getText();
    const size_t rows=getRowCount();
    const bool last=(row+1 == rows);
    const size_t start=getRowStart(row);
    const size_t stop=last?text.size():getRowStart(row+1);
    const size_t rowBegin=text::stringCells(text, 0, start);
    const size_t rowEnd=rowBegin+text::stringCells(text, start, stop);

    if(m_selectionEnd <= rowBegin || m_selectionBegin >= (last?m_selectionEnd:rowEnd))
        return false;

    // a row the selection goes on past is covered to its end; the last row
    // keeps the line end's cell
    const size_t cells=m_editor->getWrapCells();
    const size_t limit=(cells == 0)?static_cast<size_t>(-1):(last?cells+1:cells);
    const size_t begin=std::min(std::max(m_selectionBegin, rowBegin)-rowBegin, limit);
    const size_t end=std::min((last?m_selectionEnd:std::min(m_selectionEnd, rowEnd))-rowBegin, limit);

    if(end <= begin)
        return false;

    const int lineHeight=(m_editor->getLineHeight() > 0)?m_editor->getLineHeight():getHeight();
    const int x0=m_editor->columnX(begin);
    const int x1=m_editor->columnX(end);

    rect=Rect(x0, static_cast<int>(row)*lineHeight, x1-x0, lineHeight);
    return true;
}

void CodeLine::fontChanged()
{
    for(size_t row=0; row < getRowCount(); ++row)
    {
        internal::Text *text=getRowText(row);

        text->textChanged();
        text->addStateChange(StateChange_Text);
        text->invalidate();
    }
}

// As wide as its widest visual row after the inset, a line high per visual
// row: the editor places it at the content's width and that height anyway.
void CodeLine::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        int width=0;

        for(size_t row=0; row < getRowCount(); ++row)
            width=std::max(width, getRowText(row)->getPreferredSize().width);
        m_preferredSize.width=preferredFromContent(CodeEditor::TextInset+width, Dim::X);
    }
    else
    {
        const int line=m_editor?m_editor->getLineHeight():m_text->getPreferredSize().height;

        m_preferredSize.height=preferredFromContent(line*static_cast<int>(getRowCount()), Dim::Y);
    }
}

void CodeLine::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    for(size_t row=0; row < getRowCount(); ++row)
    {
        internal::Text *text=getRowText(row);

        placeChild(text, CodeEditor::TextInset, text->getY(), text->getPreferredSize().width, text->getHeight());
    }
}

// Unwrapped, the Text is the row's height; wrapped, each visual row's Text a
// line high, one below the other.
void CodeLine::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    if(getRowCount() == 1)
    {
        placeChild(m_text, m_text->getX(), 0, m_text->getWidth(), getHeight());
        return;
    }

    const int line=(m_editor && m_editor->getLineHeight() > 0)?m_editor->getLineHeight():getHeight();

    for(size_t row=0; row < getRowCount(); ++row)
    {
        internal::Text *text=getRowText(row);

        placeChild(text, text->getX(), static_cast<int>(row)*line, text->getWidth(), line);
    }
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

    position.byte=text::caretStopAt(line, position.byte);
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

    // the cache follows the document through linesChanged(), before the rows
    // are bound again; the lines its pass gives new tokens get new spans there
    m_highlight.setDocument(&m_document);
    m_highlight.setChangeHandler([this](const std::vector<size_t> &lines)
        {
            m_recolor.insert(m_recolor.end(), lines.begin(), lines.end());
        });

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

bool CodeEditor::setLanguage(const String &name)
{
    const text::Tokenizer *tokenizer=name.empty()?nullptr:text::findLanguage(name);

    setTokenizer(tokenizer);
    if(!tokenizer)
        return name.empty();
    m_language=name;
    return true;
}

void CodeEditor::setTokenizer(const text::Tokenizer *tokenizer)
{
    m_language.clear();
    if(tokenizer == m_highlight.tokenizer())
        return;

    m_highlight.setTokenizer(tokenizer);
    syncHighlightWindow();
    // every row's spans from the new tokens
    for(Base *row : getRows())
    {
        if(CodeLine *line=gweni_cast<CodeLine>(row))
            line->updateColorSpans();
    }
}

void CodeEditor::tokenSpans(size_t line, std::vector<renderer::ColorSpan> &spans)
{
    spans.clear();
    if(!m_highlight.tokenizer() || line >= m_document.lineCount())
        return;

    const skin::Base *skin=getSkin();

    if(!skin)
        return;

    for(const text::Token &token : m_highlight.tokens(line))
        spans.push_back(renderer::ColorSpan{token.begin, token.end, skin->Colors.CodeEditor.Tokens[static_cast<size_t>(token.kind)]});
}

CodeLine::DiffKind CodeEditor::diffKind(size_t line)
{
    if(!showsDiffRows() || !m_highlight.tokenizer() || line >= m_document.lineCount())
        return CodeLine::DiffKind::None;

    const std::vector<text::Token> &tokens=m_highlight.tokens(line);

    if(tokens.empty() || tokens.front().begin != 0)
        return CodeLine::DiffKind::None;

    switch(tokens.front().kind)
    {
    case text::TokenKind::Added:
        return CodeLine::DiffKind::Added;
    case text::TokenKind::Removed:
        return CodeLine::DiffKind::Removed;
    case text::TokenKind::Hunk:
        return CodeLine::DiffKind::Hunk;
    default:
        return CodeLine::DiffKind::None;
    }
}

void CodeEditor::syncHighlightWindow()
{
    size_t first=NoItem;
    size_t last=0;

    for(size_t i=0; i < poolSize(); ++i)
    {
        const size_t item=poolItem(i);

        if(item == NoItem)
            continue;
        first=std::min(first, item);
        last=std::max(last, item);
    }
    if(first == NoItem)
        m_highlight.setVisibleLines(0, 0);
    else
        m_highlight.setVisibleLines(first, last-first+1);
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

        // on the line's first visual row
        const int height=(m_wrap && m_lineHeight > 0)?std::min(row->getHeight(), m_lineHeight):row->getHeight();

        placeChild(number, 0, row->getY(), m_gutterWidth-GutterInset, height);
    }
}

void CodeEditor::setWrap(bool wrap)
{
    if(m_wrap == wrap)
        return;

    const bool shown=caretLineShown();

    m_wrap=wrap;
    m_wrapCells=0;
    m_wrapViewportHeight=-1;
    m_goalColumn=NoGoal;
    m_caretUpstream=false;
    m_revealCaretRow=false;

    if(wrap)
    {
        setHorizontalScrollOffset(0);
        setScroll(false, m_verticalScrolls);
        // variable mode now (its bar shown before the layout decides the
        // viewport), every line estimated again once the layout knows the
        // wrap width
        applyWrapMeasurer();
    }
    else
    {
        setItemMeasurer(nullptr);
        setItemEstimator(nullptr);
        setScroll(true, m_verticalScrolls);
    }

    // every row on screen bound again, wrapped or not
    refresh();
    invalidate();

    // the caret's line stays on screen (by the next layout, which knows the
    // lines' new heights), its visual row wrapped and its column unwrapped
    if(shown)
    {
        if(wrap)
            revealCaretRow();
        else
            scrollToItem(m_caret.line);
        makeCaretColumnVisible();
    }
    caretChanged();
}

void CodeEditor::setAutoHeight(int minLines, int maxLines)
{
    minLines=std::max(1, minLines);
    maxLines=(maxLines <= 0)?0:std::max(minLines, maxLines);

    if(m_autoMin == minLines && m_autoMax == maxLines)
        return;

    m_autoMin=minLines;
    m_autoMax=maxLines;
    m_contentRows=0;
    if(maxLines == 0)
        setVerticalScrolling(true);
    invalidate();
}

void CodeEditor::setVerticalScrolling(bool scrolls)
{
    if(m_verticalScrolls == scrolls)
        return;

    m_verticalScrolls=scrolls;
    setScroll(canScrollHorizontal(), scrolls);
}

void CodeEditor::setPlain(bool plain)
{
    if(m_plain == plain)
        return;

    m_plain=plain;

    if(plain)
    {
        setLineNumbers(false);
        setWrap(true);
    }
    setTabInsertsSpaces(!plain);
    syncCurrentLine();
    // the field's look
    addStateChange(StateChange_Background);
    redraw();
}

size_t CodeEditor::countRows(int width, size_t limit) const
{
    const size_t lines=m_document.lineCount();

    if(!m_wrap)
        return std::min(lines, limit+1);

    const size_t cells=wrapCellsFor(width);
    size_t rows=0;

    for(size_t line=0; line < lines && rows <= limit; ++line)
        rows+=text::wrapRowCount(m_document.line(line), cells);
    // a line's rows are counted whole: one long line can pass the limit by more
    return std::min(rows, limit+1);
}

// Turning the measurer on (again) counts every line at the estimator, its
// cells over the wrap width's: O(N) arithmetic, once per wrap width.
void CodeEditor::applyWrapMeasurer()
{
    m_wrapLineHeight=m_lineHeight;
    // the estimator first: turning the measurer on counts every line at it
    setItemEstimator([this](size_t line, int) { return estimateLine(line); });
    setItemMeasurer([this](size_t line, int width) { return measureLine(line, width); });
}

size_t CodeEditor::wrapCellsFor(int width) const
{
    if(m_cellWidth <= 0.f)
        return 0;

    const double cells=std::floor(static_cast<double>(width-2*TextInset)/m_cellWidth);

    return static_cast<size_t>(std::max(1.0, cells));
}

size_t CodeEditor::getLineRows(size_t line) const
{
    if(!m_wrap || m_wrapCells == 0 || line >= m_document.lineCount())
        return 1;
    return text::wrapRowCount(m_document.line(line), m_wrapCells);
}

int CodeEditor::measureLine(size_t line, int width) const
{
    const int lineHeight=std::max(1, m_lineHeight);

    if(line >= m_document.lineCount())
        return lineHeight;

    const size_t rows=text::wrapRowCount(m_document.line(line), wrapCellsFor(width));

    return static_cast<int>(std::min<size_t>(rows, static_cast<size_t>(INT_MAX/lineHeight)))*lineHeight;
}

// The rows the line's cells fill at the wrap width: what it wraps into with
// no word moved to the next row and no blank hanging past a row's end, so
// usually at most the measure, but not always (blanks hanging at the wrap
// points let a row hold more than the wrap width's cells: "aaaa    b" at 4
// cells is 3 rows by its cells and wraps into 2). Only an estimate: nothing
// relies on how it compares with the measure. One line before the layout has
// given a wrap width.
int CodeEditor::estimateLine(size_t line) const
{
    const int lineHeight=std::max(1, m_lineHeight);
    const size_t cells=m_wrapCells;

    if(cells == 0)
        return lineHeight;

    const size_t rows=std::max<size_t>(1, (getLineCells(line)+cells-1)/cells);

    return static_cast<int>(std::min<size_t>(rows, static_cast<size_t>(INT_MAX/lineHeight)))*lineHeight;
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

    codeLine->bind(line, m_document.line(line), m_wrap?m_wrapCells:0);
    codeLine->setCurrent(showsCurrentLine(line));

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

    // the tokens first, so a row bound below reads the edited lines' own; the
    // lines on screen whose tokens the pass changed are listed in m_recolor
    m_recolor.clear();
    m_highlight.linesChanged(change);

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

    // the caret's line among those replaced: the row it is drawn on is
    // worked out again from the new text, at the start of the row at a wrap
    // point (a line the change only moved keeps its rows)
    if(m_caret.line >= change.first && m_caret.line < change.first+change.removed)
        m_caretUpstream=false;
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
        {
            // wrapped, the line may take another number of rows
            if(m_wrap)
                itemChanged(first+i);
            else
                refreshItem(first+i);
        }
    }

    // the rows on screen whose line's tokens the edit changed, at their lines
    // after the shift (one the edit replaced is coloured again as it is bound)
    for(size_t line : m_recolor)
    {
        if(CodeLine *row=rowForLine(line))
            row->updateColorSpans();
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

void CodeEditor::moveCaret(const text::Position &position, bool extend, bool keepGoal, bool reveal, bool rowEnd)
{
    const size_t oldLine=m_caret.line;

    m_caret=snapToStop(m_document, position);
    if(!extend)
        m_anchor=m_caret;
    m_caretColumn=text::stringCells(m_document.line(m_caret.line), 0, m_caret.byte);
    // the end of the row above only where the caret is at a wrap point
    m_caretUpstream=false;
    if(rowEnd && m_wrap && m_caret.byte > 0)
    {
        rowStarts(m_caret.line, m_rowScratch);
        m_caretUpstream=rowOf(m_rowScratch, m_caret.byte, true) != rowOf(m_rowScratch, m_caret.byte, false);
    }

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

// Read-only there is no caret (setReadOnly() hid it), so a scroll or a move
// marks nothing of the editor's own.
void CodeEditor::caretChanged()
{
    if(!m_readOnly)
        addStateChange(StateChange_Caret);
}

void CodeEditor::syncCurrentLine()
{
    for(size_t i=0; i < poolSize(); ++i)
    {
        CodeLine *row=gweni_cast<CodeLine>(poolRow(i));

        if(row && poolItem(i) != NoItem)
            row->setCurrent(showsCurrentLine(poolItem(i)));
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
    else if(isVariableHeight())
    {
        // Wrapped: the caret's visual row, by the offset. A line measured at
        // this width has its rows' tops in the index's offsets, those of the
        // lines in view included (they are measured), so the offset puts the
        // row at the viewport's edge; a line not measured yet goes by the
        // list's scrollToItem, which measures it.
        const HeightIndex &heights=getHeightIndex();
        const size_t line=m_caret.line;

        if(line >= heights.size() || heights.generation(line) != getMeasureGeneration())
            revealCaretRow();
        else
        {
            const int64_t top=heights.offsetOf(line)+static_cast<int64_t>(getCaretRow())*lineHeight;
            const int64_t offset=getVerticalScrollOffset();
            const int64_t viewport=getViewportHeight();

            // a row past the last layout's content (an edit made the content
            // taller, an auto-height field grown past its rows) by the list,
            // at the content the next layout gives
            if(top+lineHeight > offset+viewport && top+lineHeight-viewport > getMaxVerticalScrollOffset())
                revealCaretRow();
            else if(top < offset)
                setVerticalScrollOffset(static_cast<int>(std::min<int64_t>(top, INT_MAX)));
            else if(top+lineHeight > offset+viewport)
                setVerticalScrollOffset(static_cast<int>(std::min<int64_t>(top+lineHeight-viewport, INT_MAX)));
        }
    }
    else
    {
        const int64_t top=static_cast<int64_t>(m_caret.line)*lineHeight;
        const int64_t offset=getVerticalScrollOffset();

        if(top < offset || top+lineHeight > offset+getViewportHeight())
            scrollToItem(m_caret.line);
    }

    makeCaretColumnVisible();
}

// The line by the next layout, and of a line taller than the viewport the
// caret's visual row: worked out in contentsPlaced, at the wrap width that
// layout gives the rows.
void CodeEditor::revealCaretRow()
{
    m_revealCaretRow=true;
    scrollToItem(m_caret.line);
}

void CodeEditor::makeCaretColumnVisible()
{
    const int width=getViewport().w;

    if(width <= 0 || !canScrollHorizontal())
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
    int64_t y=static_cast<int64_t>(viewport.y)+static_cast<int64_t>(m_caret.line)*m_lineHeight
        -getVerticalScrollOffset();
    int64_t x=static_cast<int64_t>(viewport.x)-getHorizontalScrollOffset()+columnX(m_caretColumn);

    // wrapped: its visual row's, below the line's top, at most at the row's
    // last cell (blanks hang past it)
    if(isVariableHeight() && m_caret.line < getHeightIndex().size())
    {
        y=static_cast<int64_t>(viewport.y)+getHeightIndex().offsetOf(m_caret.line)
            +static_cast<int64_t>(getCaretRow())*m_lineHeight-getVerticalScrollOffset();
        x=static_cast<int64_t>(viewport.x)+columnX(std::min(getCaretRowColumn(), m_wrapCells));
    }
    const int64_t x0=std::max<int64_t>(x, viewport.x);
    const int64_t y0=std::max<int64_t>(y, viewport.y);
    const int64_t x1=std::min<int64_t>(x+1, viewport.x+viewport.w);
    const int64_t y1=std::min<int64_t>(y+m_lineHeight, viewport.y+viewport.h);

    if(x1 <= x0 || y1 <= y0)
        return Rect();

    return Rect(static_cast<int>(x0), static_cast<int>(y0), static_cast<int>(x1-x0), static_cast<int>(y1-y0));
}

// The caret stop nearest \p cells cells into the bytes [begin, end) of \p
// str, a fraction of a cell included (byteNearestCell's rule), and the stop
// past the stops that fit whole in \p cells (byteAtCell's), over one row.
static size_t nearestCellIn(const String &str, size_t begin, size_t end, double cells)
{
    size_t pos=begin;
    double used=0.0;

    while(pos < end)
    {
        const size_t next=text::nextCaretStop(str, pos);
        const double width=static_cast<double>(text::stringCells(str, pos, next));

        if(cells <= used+width*0.5)
            break;
        used+=width;
        pos=next;
    }
    return pos;
}

static size_t byteAtCellIn(const String &str, size_t begin, size_t end, size_t cells)
{
    size_t pos=begin;
    size_t used=0;

    while(pos < end)
    {
        const size_t next=text::nextCaretStop(str, pos);
        const size_t width=text::stringCells(str, pos, next);

        if(used+width > cells)
            break;
        used+=width;
        pos=next;
    }
    return pos;
}

void CodeEditor::rowStarts(size_t line, std::vector<size_t> &starts) const
{
    if(!m_wrap || m_wrapCells == 0 || line >= m_document.lineCount())
    {
        starts.assign(1, 0);
        return;
    }
    text::wrapCells(m_document.line(line), m_wrapCells, starts);
}

size_t CodeEditor::rowOf(const std::vector<size_t> &starts, size_t byte, bool rowEnd)
{
    size_t row=static_cast<size_t>(std::upper_bound(starts.begin(), starts.end(), byte)-starts.begin());

    row=(row > 0)?row-1:0;
    if(rowEnd && row > 0 && starts[row] == byte)
        --row;
    return row;
}

size_t CodeEditor::rowEndByte(size_t line, const std::vector<size_t> &starts, size_t row) const
{
    return (row+1 < starts.size())?starts[row+1]:m_document.line(line).size();
}

size_t CodeEditor::getCaretRow() const
{
    if(!m_wrap)
        return 0;

    rowStarts(m_caret.line, m_rowScratch);
    return rowOf(m_rowScratch, m_caret.byte, m_caretUpstream);
}

size_t CodeEditor::getCaretRowColumn() const
{
    if(!m_wrap)
        return m_caretColumn;

    rowStarts(m_caret.line, m_rowScratch);

    const size_t row=rowOf(m_rowScratch, m_caret.byte, m_caretUpstream);

    return text::stringCells(m_document.line(m_caret.line), m_rowScratch[row], m_caret.byte);
}

bool CodeEditor::caretLineShown() const
{
    const Base *row=rowForItem(m_caret.line);

    return row && !row->hidden();
}

// Wrapped, the line under the point is the height index's item at its
// offset in the content, and the row the line height's step into it.
text::Position CodeEditor::positionAt(const Point &local, bool *rowEnd) const
{
    if(rowEnd)
        *rowEnd=false;

    if(m_lineHeight <= 0 || m_cellWidth <= 0.f)
        return m_caret;

    const Rect &viewport=getViewport();
    const int64_t y=static_cast<int64_t>(local.y)-viewport.y+getVerticalScrollOffset();
    const size_t last=m_document.lineCount()-1;

    if(y < 0)
        return text::Position{0, 0};

    if(!isVariableHeight())
    {
        const size_t line=static_cast<size_t>(y/m_lineHeight);

        if(line > last)
            return m_document.endPosition();

        return text::Position{line, text::byteNearestCell(m_document.line(line), textX(local)/m_cellWidth)};
    }

    int64_t top=0;
    const size_t line=getHeightIndex().itemAt(y, &top);

    if(line > last)
        return m_document.endPosition();

    rowStarts(line, m_rowScratch);

    const size_t row=std::min(static_cast<size_t>((y-top)/m_lineHeight), m_rowScratch.size()-1);
    const String &text=m_document.line(line);
    const size_t end=rowEndByte(line, m_rowScratch, row);
    const size_t byte=nearestCellIn(text, m_rowScratch[row], end, textX(local)/m_cellWidth);

    if(rowEnd)
        *rowEnd=(row+1 < m_rowScratch.size() && byte == end);
    return text::Position{line, byte};
}

double CodeEditor::textX(const Point &local) const
{
    const Rect &viewport=getViewport();

    if(local.x < viewport.x)
        return 0.0;
    return static_cast<double>(local.x)-viewport.x+getHorizontalScrollOffset()-TextInset;
}

text::Position CodeEditor::glyphAt(const Point &local) const
{
    bool rowEnd=false;
    const text::Position at=positionAt(local, &rowEnd);

    if(m_cellWidth <= 0.f)
        return at;

    const size_t cell=static_cast<size_t>(std::max(0.0, std::floor(textX(local)/m_cellWidth)));

    if(!m_wrap)
        return text::Position{at.line, text::byteAtCell(m_document.line(at.line), cell)};

    // on the visual row positionAt found
    rowStarts(at.line, m_rowScratch);

    const size_t row=rowOf(m_rowScratch, at.byte, rowEnd);

    return text::Position{at.line, byteAtCellIn(m_document.line(at.line), m_rowScratch[row],
        rowEndByte(at.line, m_rowScratch, row), cell)};
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

// Up from the first row goes to its start, down from the last to its end, as
// in a text field; anywhere else the goal column (in cells on the row) is kept
// for the next one. Wrapped, the rows are each line's visual rows, the walk
// going over the lines it crosses; landing at the end of a row that wraps
// leaves the caret drawn there (its row end).
void CodeEditor::moveRows(int64_t rows, bool extend)
{
    const size_t goal=(m_goalColumn == NoGoal)?getCaretRowColumn():m_goalColumn;
    const size_t last=m_document.lineCount()-1;
    size_t line=m_caret.line;

    if(!m_wrap)
    {
        const int64_t target=static_cast<int64_t>(line)+rows;

        if(target < 0)
            moveCaret(text::Position{0, 0}, extend);
        else if(target > static_cast<int64_t>(last))
            moveCaret(m_document.endPosition(), extend);
        else
        {
            moveCaret(atColumn(static_cast<size_t>(target), goal), extend, true);
            m_goalColumn=goal;
        }
        return;
    }

    rowStarts(line, m_rowScratch);

    int64_t row=static_cast<int64_t>(rowOf(m_rowScratch, m_caret.byte, m_caretUpstream))+rows;

    while(row < 0)
    {
        if(line == 0)
        {
            moveCaret(text::Position{0, 0}, extend);
            return;
        }
        --line;
        rowStarts(line, m_rowScratch);
        row+=static_cast<int64_t>(m_rowScratch.size());
    }
    while(row >= static_cast<int64_t>(m_rowScratch.size()))
    {
        if(line == last)
        {
            moveCaret(m_document.endPosition(), extend);
            return;
        }
        row-=static_cast<int64_t>(m_rowScratch.size());
        ++line;
        rowStarts(line, m_rowScratch);
    }

    const size_t at=static_cast<size_t>(row);
    const size_t end=rowEndByte(line, m_rowScratch, at);
    const size_t byte=byteAtCellIn(m_document.line(line), m_rowScratch[at], end, goal);
    const bool rowEnd=(at+1 < m_rowScratch.size() && byte == end);

    moveCaret(text::Position{line, byte}, extend, true, true, rowEnd);
    m_goalColumn=goal;
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
    // shown or hidden
    addStateChange(StateChange_Caret);
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
        replaceRange({{m_caret.line, text::utf8PrevCodepoint(m_document.line(m_caret.line), m_caret.byte)}, m_caret}, String(), kind);
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
        moveRows(-1, input::isShiftDown());
    return true;
}

bool CodeEditor::onKeyDown(bool down)
{
    if(down)
        moveRows(1, input::isShiftDown());
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

    // wrapped, a row after the line's first goes to its own start first
    if(m_wrap)
    {
        rowStarts(m_caret.line, m_rowScratch);

        const size_t row=rowOf(m_rowScratch, m_caret.byte, m_caretUpstream);

        if(row > 0 && (m_caret.byte != m_rowScratch[row] || m_caretUpstream))
        {
            moveCaret({m_caret.line, m_rowScratch[row]}, input::isShiftDown());
            return true;
        }
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
    {
        moveCaret(m_document.endPosition(), input::isShiftDown());
        return true;
    }

    // wrapped, a row before the line's last goes to its own end first (drawn
    // there, not at the next row's start)
    if(m_wrap)
    {
        rowStarts(m_caret.line, m_rowScratch);

        const size_t row=rowOf(m_rowScratch, m_caret.byte, m_caretUpstream);

        if(row+1 < m_rowScratch.size() && !(m_caretUpstream && m_caret.byte == m_rowScratch[row+1]))
        {
            moveCaret({m_caret.line, m_rowScratch[row+1]}, input::isShiftDown(), false, true, true);
            return true;
        }
    }

    moveCaret({m_caret.line, m_document.line(m_caret.line).size()}, input::isShiftDown());
    return true;
}

// The view and the caret move by a page together, so the caret keeps its
// place on screen.
bool CodeEditor::onKeyPageUp(bool down)
{
    if(down)
    {
        if(isVariableHeight())
            setVerticalScrollOffset(getVerticalScrollOffset()-pageLines()*m_lineHeight);
        else
            scrollByRows(-pageLines());
        moveRows(-pageLines(), input::isShiftDown());
    }
    return true;
}

bool CodeEditor::onKeyPageDown(bool down)
{
    if(down)
    {
        if(isVariableHeight())
            setVerticalScrollOffset(getVerticalScrollOffset()+pageLines()*m_lineHeight);
        else
            scrollByRows(pageLines());
        moveRows(pageLines(), input::isShiftDown());
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
        bool rowEnd=false;

        m_dragPoint=canvasPosToLocal(Point(x, y));

        const text::Position at=positionAt(m_dragPoint, &rowEnd);

        moveCaret(at, input::isShiftDown(), false, true, rowEnd);
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

    bool rowEnd=false;

    m_dragPoint=canvasPosToLocal(Point(x, y));

    const text::Position at=positionAt(m_dragPoint, &rowEnd);

    moveCaret(at, true, false, false, rowEnd);

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

    const double now=platform::getTimeInSeconds();
    const float elapsed=std::min(std::max(static_cast<float>(now-m_autoScrollTime), 0.0f), 0.1f);

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
    {
        bool rowEnd=false;
        const text::Position at=positionAt(m_dragPoint, &rowEnd);

        moveCaret(at, true, false, false, rowEnd);
    }
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

// The text may come later (the browser's paste event): the editor is looked
// up again then, and an answer for one deleted meanwhile does nothing.
void CodeEditor::onPaste(event::Info)
{
    const size_t id=getControlId();

    platform::requestClipboardText([id](const String &text)
    {
        CodeEditor *editor=dynamic_cast<CodeEditor *>(Base::getControlById(id));

        if(!editor || text.empty() || !editor->editable())
            return;

        text::UndoGroup group(editor->m_undo, text::EditKind::Paste);

        editor->insertText(text, text::EditKind::Paste);
    });
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
    // The rows' width is final: at a new wrap width (or line height) every
    // line is estimated again and the rows on screen bound again (the
    // measurer's refresh()) before the list measures and places them.
    if(m_wrap)
    {
        const size_t cells=wrapCellsFor(m_innerPanel->getWidth());
        const int viewport=getViewportHeight();
        const bool rewrap=(cells != m_wrapCells || m_lineHeight != m_wrapLineHeight);
        // revealCaretRow()'s request holds while its scrollToItem() is still
        // the list's: a scroll the app or the user asked for after it in the
        // frame (another scrollToItem(), a scrollByRows()) is the one kept
        bool reveal=m_revealCaretRow && getPendingItem() == m_caret.line;

        if(rewrap || viewport != m_wrapViewportHeight)
        {
            // the caret's line stays on screen through a new size: found in
            // the rows the last layout bound, put in view by this one
            const bool shown=caretLineShown() && m_wrapViewportHeight >= 0;

            m_wrapViewportHeight=viewport;
            if(rewrap)
            {
                m_wrapCells=cells;
                applyWrapMeasurer();
                caretChanged();
            }
            if(shown)
                reveal=true;
        }

        // the caret's visual row (the whole line where it fits the viewport)
        if(reveal && m_lineHeight > 0)
            scrollToItem(m_caret.line, static_cast<int>(std::min<size_t>(getCaretRow(),
                static_cast<size_t>(INT_MAX/m_lineHeight-1)))*m_lineHeight, m_lineHeight);
    }
    m_revealCaretRow=false;

    ParentClass::contentsPlaced(skin);

    // a spare row keeps no visual rows: it holds what a fresh row holds
    for(size_t i=0; i < poolSize(); ++i)
    {
        CodeLine *row=gweni_cast<CodeLine>(poolRow(i));

        if(row && poolItem(i) == NoItem && row->getRowCount() > 1)
            row->bind(CodeLine::NoLine, String(), 0);
    }

    placeLineNumbers(skin);
    syncHighlightWindow();

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

    const bool read=m_lineHeight > 0;
    const float cellWidth=m_cellWidth;

    m_lineHeight=std::max(1, measureLineHeight(m_font));
    m_cellWidth=std::max(1, measureText(m_font, String(CellSample, '0')).x)/static_cast<float>(CellSample);

    // the selection rects and the caret are in cells: a new cell width moves
    // them, even where the rows keep their bounds (the same line height)
    if(read && m_cellWidth != cellWidth)
    {
        for(Base *row : getRows())
        {
            CodeLine *line=gweni_cast<CodeLine>(row);

            if(line && line->getSelectionEnd() > line->getSelectionBegin())
                line->addStateChange(StateChange_Background);
        }
        caretChanged();
    }
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

    // Auto-height: the rows at the width the parent gave (final here), with
    // no vertical bar, which shows only past the most rows; at that width
    // the rows only grow, so the bar it narrows the rows by changes nothing
    // of the answer.
    if(dim == Dim::Y && hasAutoHeight() && m_lineHeight > 0)
    {
        PlacementScope placement;
        const Margin &inset=m_innerPanel->getMargin();
        const int open=getWidth()-m_padding.left-m_padding.right-inset.left-inset.right;
        const size_t limit=static_cast<size_t>(m_autoMax);

        m_contentRows=countRows(open, limit);

        const bool scrolls=m_contentRows > limit;
        const size_t rows=gweni::clamp(m_contentRows, static_cast<size_t>(m_autoMin), limit);
        int height=static_cast<int>(rows)*m_lineHeight+m_padding.top+m_padding.bottom+inset.top+inset.bottom;

        setVerticalScrolling(scrolls);

        // unwrapped, a line wider than the viewport brings the horizontal bar
        if(!m_wrap)
        {
            const Margin &barMargin=m_horizontalScrollBar->getMargin();
            const Margin &sideMargin=m_verticalScrollBar->getMargin();
            const int viewport=open-(scrolls?m_barSize.width+sideMargin.left+sideMargin.right:0);

            if(contentSize().width > viewport)
                height+=m_barSize.height+barMargin.top+barMargin.bottom;
        }

        m_preferredSize.height=preferredFromContent(height, Dim::Y);
    }
}

Size CodeEditor::contentSize() const
{
    Size content=ParentClass::contentSize();

    // wrapped, the rows are the viewport's width
    if(m_wrap)
    {
        content.width=0;
        return content;
    }

    content.width=2*TextInset+static_cast<int>(std::ceil(static_cast<double>(m_longest+1)*m_cellWidth));
    return content;
}

}//namespace controls
}//namespace gweni
