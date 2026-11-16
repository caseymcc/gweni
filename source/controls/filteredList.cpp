/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/filteredList.h>

#include <algorithm>
#include <cstdint>

namespace gweni
{
namespace controls
{

FilteredList::FilteredList():
    m_matcher(substringMatcher())
{
}

String FilteredList::lowerAscii(const String &text)
{
    String lower=text;

    for(char &c : lower)
    {
        if((c >= 'A') && (c <= 'Z'))
            c=static_cast<char>(c-'A'+'a');
    }
    return lower;
}

FilteredList::Matcher FilteredList::substringMatcher()
{
    return [](const FilteredList &list, std::vector<size_t> &matches)
        {
            const String &query=list.lowerQuery();

            for(size_t i=0; i < list.textCount(); ++i)
            {
                if(query.empty() || (list.lowerText(i).find(query) != String::npos))
                    matches.push_back(i);
            }
        };
}

void FilteredList::setMatcher(Matcher matcher)
{
    m_matcher=matcher?std::move(matcher):substringMatcher();
}

void FilteredList::addText(const String &text)
{
    m_lowerTexts.push_back(lowerAscii(text));
}

void FilteredList::clearTexts()
{
    m_lowerTexts.clear();
}

void FilteredList::setQuery(const String &query)
{
    m_query=query;
    m_lowerQuery=lowerAscii(query);
}

void FilteredList::clearQuery()
{
    m_query.clear();
    m_lowerQuery.clear();
}

void FilteredList::match()
{
    m_matches.clear();
    m_matcher(*this, m_matches);
}

void FilteredList::clearMatches()
{
    m_matches.clear();
    clearTrailingRow();
}

void FilteredList::setTrailingRow(size_t key, bool highlightable, bool alwaysRebind)
{
    m_trailing=true;
    m_trailingKey=key;
    m_trailingHighlightable=highlightable;
    m_trailingRebind=alwaysRebind;
}

void FilteredList::clearTrailingRow()
{
    m_trailing=false;
    m_trailingKey=NoItem;
    m_trailingHighlightable=false;
    m_trailingRebind=false;
}

size_t FilteredList::rowKey(size_t position) const
{
    if(position < m_matches.size())
        return m_matches[position];

    return m_trailingKey;
}

void FilteredList::rowKeys(std::vector<size_t> &keys) const
{
    keys.resize(shownRowCount());

    for(size_t i=0; i < keys.size(); ++i)
        keys[i]=rowKey(i);
}

size_t FilteredList::positionOf(size_t option) const
{
    for(size_t i=0; i < m_matches.size(); ++i)
    {
        if(m_matches[i] == option)
            return i;
    }
    return NoItem;
}

void FilteredList::setRows(VirtualList *rows, RowHighlighter highlighter)
{
    m_rows=rows;
    m_highlighter=std::move(highlighter);
}

// The rows still showing what they showed keep their binding; only their
// highlight can have moved.
void FilteredList::rebindChanged(const std::vector<size_t> &previous)
{
    if(!m_rows)
        return;

    const size_t count=shownRowCount();

    m_rows->setItemCount(count);

    for(Base *control : m_rows->getRows())
    {
        const size_t item=m_rows->itemForRow(control);

        if((item == NoItem) || (item >= count))
            continue;

        const size_t before=(item < previous.size())?previous[item]:NoItem;
        const size_t now=rowKey(item);

        if((before != now) || (m_trailingRebind && (item >= m_matches.size())))
            m_rows->refreshItem(item);
        else if(m_highlighter)
            m_highlighter(control, item == m_highlightPos);
    }
}

void FilteredList::setHighlightState(size_t position)
{
    m_highlightPos=position;
    m_highlight=(position < m_matches.size())?static_cast<int>(m_matches[position]):NoOption;
}

void FilteredList::highlight(size_t position, bool scroll)
{
    if(position >= highlightableCount())
        position=NoItem;

    if(position == m_highlightPos)
        return;

    if(m_rows && m_highlighter && (m_highlightPos != NoItem))
    {
        if(Base *row=m_rows->rowForItem(m_highlightPos))
            m_highlighter(row, false);
    }

    setHighlightState(position);

    if(!m_rows || (position == NoItem))
        return;

    if(m_highlighter)
    {
        if(Base *row=m_rows->rowForItem(position))
            m_highlighter(row, true);
    }

    if(scroll)
        m_rows->scrollToItem(position);
}

void FilteredList::step(int step)
{
    const size_t count=highlightableCount();

    if(count == 0)
        return;

    size_t position;

    if(m_highlightPos == NoItem)
        position=(step > 0)?0:count-1;
    else
        position=static_cast<size_t>(((static_cast<int64_t>(m_highlightPos)+step)%static_cast<int64_t>(count)
            +static_cast<int64_t>(count))%static_cast<int64_t>(count));

    highlight(position, true);
}

void FilteredList::page(int pages, size_t pageRows)
{
    const size_t count=highlightableCount();

    if((count == 0) || (pages == 0))
        return;

    int64_t position;

    if(m_highlightPos == NoItem)
        position=(pages > 0)?0:static_cast<int64_t>(count)-1;
    else
        position=static_cast<int64_t>(m_highlightPos)+static_cast<int64_t>(pages)*static_cast<int64_t>(std::max<size_t>(pageRows, 1));

    position=std::max<int64_t>(0, std::min<int64_t>(position, static_cast<int64_t>(count)-1));
    highlight(static_cast<size_t>(position), true);
}

void FilteredList::rowHovered(const Base *row)
{
    if(!m_rows)
        return;

    const size_t item=m_rows->itemForRow(row);

    if(item < highlightableCount())
        highlight(item, false);
}

}//namespace controls
}//namespace gweni
