/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/codeEditor.h>
#include <gweni/controls/canvas.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/skin.h>
#include <gweni/text/cells.h>

#include <algorithm>
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
//CodeEditor

// Both ways over virtual content: the rows are the lines, the inner panel the
// viewport with nothing around it, inside a padding that keeps the rows off
// the rounded border the skin draws.
GWENI_CONTROL_CONSTRUCTOR(CodeEditor)
{
    setScroll(true, true);
    setPadding(Padding(2, 2, 2, 2));

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
    invalidate();
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

    m_newCells.resize(change.inserted);
    for(size_t i=0; i < change.inserted; ++i)
    {
        m_newCells[i]=text::stringCells(m_document.line(first+i));
        addWidth(m_newCells[i]);
    }

    for(size_t i=0; i < change.removed && first+i < m_lineCells.size(); ++i)
        removeWidth(m_lineCells[first+i]);

    if(change.inserted > change.removed)
        m_lineCells.insert(m_lineCells.begin()+static_cast<std::ptrdiff_t>(first+change.removed),
            change.inserted-change.removed, 0);
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
