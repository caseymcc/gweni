/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/document.h>

#include <algorithm>
#include <utility>

namespace gweni
{
namespace text
{

String normalizeNewlines(const String &text)
{
    if(text.find("\r\n") == String::npos)
        return text;

    String result;
    result.reserve(text.size());
    for(size_t i=0; i < text.size(); ++i)
    {
        if(text[i] == '\r' && i+1 < text.size() && text[i+1] == '\n')
            continue;
        result.push_back(text[i]);
    }
    return result;
}

namespace
{

//! \p text's lines, split on '\n' (n newlines give n+1 lines)
void splitLines(const String &text, std::vector<String> &lines)
{
    size_t start=0;

    while(true)
    {
        size_t newline=text.find('\n', start);

        if(newline == String::npos)
        {
            lines.push_back(text.substr(start));
            return;
        }
        lines.push_back(text.substr(start, newline-start));
        start=newline+1;
    }
}

}//namespace

Document::Document():
    m_lines(1)
{}

Document::Document(const String &text)
{
    splitLines(normalizeNewlines(text), m_lines);
}

void Document::setText(const String &text)
{
    size_t removed=m_lines.size();

    m_lines.clear();
    splitLines(normalizeNewlines(text), m_lines);
    notify({0, removed, m_lines.size()});
}

String Document::text() const
{
    size_t size=m_lines.size()-1;

    for(const String &line:m_lines)
        size+=line.size();

    String result;
    result.reserve(size);
    for(size_t i=0; i < m_lines.size(); ++i)
    {
        if(i > 0)
            result.push_back('\n');
        result+=m_lines[i];
    }
    return result;
}

Position Document::endPosition() const
{
    return {m_lines.size()-1, m_lines.back().size()};
}

Position Document::clamp(Position position) const
{
    if(position.line >= m_lines.size())
        return endPosition();
    position.byte=std::min(position.byte, m_lines[position.line].size());
    return position;
}

String Document::textIn(Range range) const
{
    Position begin=clamp(range.begin);
    Position end=clamp(range.end);

    if(end < begin)
        std::swap(begin, end);

    if(begin.line == end.line)
        return m_lines[begin.line].substr(begin.byte, end.byte-begin.byte);

    String result=m_lines[begin.line].substr(begin.byte);
    for(size_t line=begin.line+1; line < end.line; ++line)
    {
        result.push_back('\n');
        result+=m_lines[line];
    }
    result.push_back('\n');
    result.append(m_lines[end.line], 0, end.byte);
    return result;
}

LineChange Document::apply(Edit &edit)
{
    Position begin=clamp(edit.range.begin);
    Position end=clamp(edit.range.end);

    if(end < begin)
        std::swap(begin, end);
    edit.range={begin, end};
    edit.text=normalizeNewlines(edit.text);
    edit.removed=textIn(edit.range);

    const String &text=edit.text;
    size_t firstNewline=text.find('\n');
    LineChange change;

    change.first=begin.line;
    change.removed=end.line-begin.line+1;

    if(firstNewline == String::npos)
    {
        //the edit leaves one line where its range's lines were
        String &line=m_lines[begin.line];

        if(begin.line == end.line)
            line.replace(begin.byte, end.byte-begin.byte, text);
        else
        {
            line.erase(begin.byte);
            line+=text;
            line.append(m_lines[end.line], end.byte, String::npos);
            m_lines.erase(m_lines.begin()+begin.line+1, m_lines.begin()+end.line+1);
        }
        edit.textEnd={begin.line, begin.byte+text.size()};
        change.inserted=1;
    }
    else
    {
        //the new lines: the first line's head and the text's first piece, its
        //middle pieces, and its last piece with the last line's tail
        std::vector<String> pieces;
        splitLines(text, pieces);

        size_t inserted=pieces.size();
        String tail=m_lines[end.line].substr(end.byte);

        edit.textEnd={begin.line+inserted-1, pieces.back().size()};
        pieces.back()+=tail;

        String &first=m_lines[begin.line];
        first.erase(begin.byte);
        first+=pieces.front();

        //lines begin.line+1 .. end.line become pieces[1 ..]: reuse the
        //slots there are, then insert or erase the difference
        size_t replaced=end.line-begin.line;
        size_t reused=std::min(replaced, inserted-1);

        for(size_t i=0; i < reused; ++i)
            m_lines[begin.line+1+i]=std::move(pieces[1+i]);

        auto at=m_lines.begin()+begin.line+1+reused;

        if(inserted-1 > reused)
            m_lines.insert(at, std::make_move_iterator(pieces.begin()+1+reused), std::make_move_iterator(pieces.end()));
        else if(replaced > reused)
            m_lines.erase(at, at+(replaced-reused));
        change.inserted=inserted;
    }

    notify(change);
    return change;
}

Edit Document::replace(Range range, const String &text)
{
    Edit edit;

    edit.range=range;
    edit.text=text;
    apply(edit);
    return edit;
}

size_t Document::addObserver(Observer observer)
{
    size_t id=m_nextObserverId++;

    m_observers.push_back({id, std::move(observer)});
    return id;
}

void Document::removeObserver(size_t id)
{
    m_observers.erase(std::remove_if(m_observers.begin(), m_observers.end(),
        [id](const ObserverEntry &entry) { return entry.id == id; }), m_observers.end());
}

void Document::notify(const LineChange &change)
{
    for(const ObserverEntry &entry:m_observers)
        entry.observer(change);
}

}//namespace text
}//namespace gweni
