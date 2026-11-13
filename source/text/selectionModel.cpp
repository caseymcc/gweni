/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/text/selectionModel.h>
#include <gweni/text/cells.h>

#include <algorithm>

namespace gweni
{
namespace text
{

bool SelectionModel::set(const SelectionPosition &anchor, const SelectionPosition &focus)
{
    if((anchor == m_anchor) && (focus == m_focus))
        return false;

    m_anchor=anchor;
    m_focus=focus;
    ++m_serial;
    return true;
}

void SelectionModel::setCaret(const SelectionPosition &at)
{
    set(at, at);
}

void SelectionModel::extendTo(const SelectionPosition &at)
{
    set(m_anchor, at);
}

void SelectionModel::select(const SelectionPosition &anchor, const SelectionPosition &focus)
{
    set(anchor, focus);
}

void SelectionModel::clear()
{
    set(SelectionPosition(), SelectionPosition());
}

bool SelectionModel::range(const std::vector<size_t> &block, size_t length, std::pair<size_t, size_t> &bytes) const
{
    bytes={ 0, 0 };
    if(empty())
        return false;

    const SelectionPosition &first=start();
    const SelectionPosition &last=end();

    if((block < first.block) || (last.block < block))
        return false;

    const size_t from=(block == first.block)?std::min(first.offset, length):0;
    const size_t to=(block == last.block)?std::min(last.offset, length):length;

    if(from >= to)
        return false;

    bytes={ from, to };
    return true;
}

namespace
{

enum class CharClass
{
    Blank,
    Word,
    Punctuation,
    Newline
};

CharClass charClass(uint32_t codepoint)
{
    if(codepoint == '\n')
        return CharClass::Newline;
    if(codepoint == ' ' || codepoint == '\t')
        return CharClass::Blank;
    if(codepoint >= 0x80 || codepoint == '_' || (codepoint >= '0' && codepoint <= '9')
        || (codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 'A' && codepoint <= 'Z'))
        return CharClass::Word;
    return CharClass::Punctuation;
}

}//namespace

std::pair<size_t, size_t> wordAt(const String &text, size_t at)
{
    if(text.empty())
        return { at, at };

    size_t probe=caretStopAt(text, std::min(at, text.size()));

    // at the end, or on the '\n' ending a line: the run before it
    if((probe > 0) && ((probe >= text.size()) || ((text[probe] == '\n') && (text[probe-1] != '\n'))))
        probe=prevCaretStop(text, probe);

    const CharClass run=charClass(utf8CodepointAt(text, probe));
    size_t first=probe;
    size_t last=nextCaretStop(text, probe);

    // a '\n' is a run of its own
    if(run == CharClass::Newline)
        return { first, last };

    while((first > 0) && (charClass(utf8CodepointAt(text, prevCaretStop(text, first))) == run))
        first=prevCaretStop(text, first);
    while((last < text.size()) && (charClass(utf8CodepointAt(text, last)) == run))
        last=nextCaretStop(text, last);
    return { first, last };
}

}//namespace text
}//namespace gweni
