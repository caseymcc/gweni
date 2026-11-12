/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/logView.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(LogViewRow)
{
    setWrap(true);
    setPadding(Padding(LogView::RowPadX, LogView::RowPadY, LogView::RowPadX, LogView::RowPadY));
    setTextPadding(Padding(0, 0, 0, 0));
}

void LogViewRow::fontChanged()
{
    m_text->textChanged();
    m_text->addStateChange(StateChange_Text);
    m_text->invalidate();
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//LogView

namespace
{

//! The text a row shows.
const String &shownText(const LogRow &row)
{
    return row.text;
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(LogView)
{
    if(skin::Base *skin=getSkin())
        m_font=skin->getDefaultFont();

    setRowFactory([this](Base *parent) -> Base *
        {
            internal::LogViewRow *row=parent->newChild<internal::LogViewRow>();

            // the row's Text holds a pointer to m_font: setFont changes it in place
            row->setFont(m_font);
            return row;
        });
    setRowBinder([this](Base *row, size_t position) { bindRow(row, position); });
    setItemMeasurer([this](size_t position, int width) { return measureRow(position, width); });
}

LogView::RowId LogView::append(const LogRow &row)
{
    applyCap();

    const RowId id=m_nextId++;

    m_rows.push_back(Entry{id, row});
    insertItems(m_rows.size()-1, 1);
    return id;
}

bool LogView::replace(RowId id, const LogRow &row)
{
    const size_t position=rowPosition(id);

    if(position == NoItem)
        return false;

    m_rows[position].row=row;
    itemChanged(position);
    return true;
}

LogView::RowId LogView::prepend(const std::vector<LogRow> &rows)
{
    if(rows.empty())
        return NoRow;

    // rows[0], the oldest, gets the lowest id: below every one given so far
    const RowId first=m_lowestId-static_cast<RowId>(rows.size());

    m_lowestId=first;
    for(size_t i=rows.size(); i-- > 0;)
        m_rows.push_front(Entry{first+static_cast<RowId>(i), rows[i]});
    insertItems(0, rows.size());
    return first;
}

void LogView::clear()
{
    if(m_rows.empty())
        return;

    m_rows.clear();
    removeItems(0, getItemCount());
}

void LogView::setMaxRows(size_t rows)
{
    m_maxRows=rows;
}

void LogView::applyCap()
{
    if(m_maxRows == 0 || m_rows.size() < m_maxRows)
        return;

    const size_t drop=m_rows.size()-(m_maxRows-1);

    m_rows.erase(m_rows.begin(), m_rows.begin()+static_cast<std::ptrdiff_t>(drop));
    removeItems(0, drop);
}

LogView::RowId LogView::firstRowId() const
{
    return m_rows.empty()?NoRow:m_rows.front().id;
}

LogView::RowId LogView::lastRowId() const
{
    return m_rows.empty()?NoRow:m_rows.back().id;
}

// Ids rise with the positions, and the appended rows (every id from the last
// drop on) are one run at the end, the prepended ones one run per prepend at
// the start: an id is its distance from the last row's or the first row's,
// and only an id between two prepends' runs with rows dropped from the
// earlier one needs the binary search.
size_t LogView::rowPosition(RowId id) const
{
    if(m_rows.empty() || id < m_rows.front().id || id > m_rows.back().id)
        return NoItem;

    const RowId fromBack=m_rows.back().id-id;

    if(static_cast<uint64_t>(fromBack) < m_rows.size())
    {
        const size_t position=m_rows.size()-1-static_cast<size_t>(fromBack);

        if(m_rows[position].id == id)
            return position;
    }

    const RowId fromFront=id-m_rows.front().id;

    if(static_cast<uint64_t>(fromFront) < m_rows.size())
    {
        const size_t position=static_cast<size_t>(fromFront);

        if(m_rows[position].id == id)
            return position;
    }

    const auto found=std::lower_bound(m_rows.begin(), m_rows.end(), id,
        [](const Entry &entry, RowId value) { return entry.id < value; });

    if(found == m_rows.end() || found->id != id)
        return NoItem;
    return static_cast<size_t>(found-m_rows.begin());
}

LogView::RowId LogView::rowIdAt(size_t position) const
{
    return (position < m_rows.size())?m_rows[position].id:NoRow;
}

const LogRow *LogView::getRow(RowId id) const
{
    const size_t position=rowPosition(id);

    return (position == NoItem)?nullptr:&m_rows[position].row;
}

void LogView::setFont(const gweni::Font &font)
{
    if(m_font == font)
        return;

    m_font=font;
    for(Base *row : getRows())
    {
        if(internal::LogViewRow *logRow=gweni_cast<internal::LogViewRow>(row))
            logRow->fontChanged();
    }
    refresh();
}

internal::LogViewRow *LogView::rowControl(RowId id) const
{
    const size_t position=rowPosition(id);

    if(position == NoItem)
        return nullptr;
    return gweni_cast<internal::LogViewRow>(rowForItem(position));
}

void LogView::bindRow(Base *row, size_t position)
{
    internal::LogViewRow *logRow=gweni_cast<internal::LogViewRow>(row);

    if(!logRow || position >= m_rows.size())
        return;

    const LogRow &content=m_rows[position].row;

    logRow->setText(shownText(content));
    if(content.hasColor)
        logRow->setTextColor(content.color);
    else
        logRow->makeColorNormal();
}

// The wrap the row's Label does, inside its padding.
int LogView::measureRow(size_t position, int width)
{
    if(position >= m_rows.size())
        return getRowHeight();
    return wrappedTextHeight(m_font, shownText(m_rows[position].row), width-2*RowPadX)+2*RowPadY;
}

}//namespace controls
}//namespace gweni
