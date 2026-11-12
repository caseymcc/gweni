/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/cells.h>

#include <algorithm>
#include <iterator>

namespace gweni
{
namespace text
{

namespace
{

struct CellRange
{
    uint32_t first;
    uint32_t last;
};

#include "cellWidthTable.inc"

//! Whether codepoint is in one of the sorted, disjoint ranges
template<size_t N>
bool inRanges(const CellRange (&ranges)[N], uint32_t codepoint)
{
    const CellRange *end=std::end(ranges);
    // the first range whose last is not below codepoint
    const CellRange *range=std::lower_bound(std::begin(ranges), end, codepoint,
        [](const CellRange &r, uint32_t cp) { return r.last < cp; });

    return (range != end) && (range->first <= codepoint);
}

}//namespace

int cellCount(uint32_t codepoint)
{
    // nothing below U+0300 is wide or zero-width but the controls, which take one
    // cell as any other codepoint does here
    if(codepoint < 0x300)
        return 1;
    if(inRanges(zeroCellRanges, codepoint))
        return 0;
    if(inRanges(wideCellRanges, codepoint))
        return 2;
    return 1;
}

uint32_t utf8CodepointAt(const String &str, size_t pos, size_t *next)
{
    const size_t size=str.size();

    if(pos >= size)
    {
        if(next)
            *next=size;
        return 0;
    }

    const unsigned char lead=static_cast<unsigned char>(str[pos]);
    size_t length=0;
    uint32_t codepoint=0;
    uint32_t least=0;

    if(lead < 0x80)
    {
        if(next)
            *next=pos+1;
        return lead;
    }
    else if((lead & 0xe0) == 0xc0)
    {
        length=2;
        codepoint=lead & 0x1f;
        least=0x80;
    }
    else if((lead & 0xf0) == 0xe0)
    {
        length=3;
        codepoint=lead & 0x0f;
        least=0x800;
    }
    else if((lead & 0xf8) == 0xf0)
    {
        length=4;
        codepoint=lead & 0x07;
        least=0x10000;
    }

    bool valid=(length > 0) && (pos+length <= size);

    for(size_t i=1; valid && i < length; ++i)
    {
        const unsigned char byte=static_cast<unsigned char>(str[pos+i]);

        if((byte & 0xc0) != 0x80)
            valid=false;
        else
            codepoint=(codepoint << 6) | (byte & 0x3f);
    }

    // overlong forms, surrogates and past U+10FFFF are malformed too
    if(!valid || codepoint < least || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
    {
        if(next)
            *next=pos+1;
        return 0xfffd;
    }

    if(next)
        *next=pos+length;
    return codepoint;
}

size_t stringCells(const String &str, size_t begin, size_t end)
{
    end=std::min(end, str.size());

    size_t cells=0;

    for(size_t i=begin; i < end;)
    {
        // ASCII is one cell a byte, without decoding
        if(static_cast<unsigned char>(str[i]) < 0x80)
        {
            ++cells;
            ++i;
            continue;
        }

        size_t next=i;
        const uint32_t codepoint=utf8CodepointAt(str, i, &next);

        cells+=static_cast<size_t>(cellCount(codepoint));
        i=next;
    }

    return cells;
}

size_t byteAtCell(const String &str, size_t cells)
{
    size_t taken=0;
    size_t i=0;

    while(i < str.size())
    {
        size_t next=i;
        const int width=cellCount(utf8CodepointAt(str, i, &next));

        if(taken+static_cast<size_t>(width) > cells)
            break;

        taken+=static_cast<size_t>(width);
        i=next;
    }
    return i;
}

size_t byteNearestCell(const String &str, double cells)
{
    double taken=0.0;
    size_t i=0;

    while(i < str.size())
    {
        size_t next=i;
        const int width=cellCount(utf8CodepointAt(str, i, &next));

        // a mark (0 cells) goes with the codepoint before it
        if(width > 0 && taken+width*0.5 >= cells)
            break;

        taken+=width;
        i=next;
    }
    return i;
}

// The nearest byte within 4 before pos that is not 10xxxxxx, if the sequence
// it leads ends at pos, else pos-1 (a stray continuation byte, or the lead of
// a cut sequence, is one byte long).
size_t utf8PrevCodepoint(const String &str, size_t pos)
{
    pos=std::min(pos, str.size());
    if(pos == 0)
        return 0;

    const size_t least=(pos > 4) ? pos-4 : 0;
    size_t lead=pos-1;

    while(lead > least && (static_cast<unsigned char>(str[lead]) & 0xc0) == 0x80)
        --lead;

    size_t next=lead;

    utf8CodepointAt(str, lead, &next);
    return (next == pos) ? lead : pos-1;
}

// A boundary followed by a codepoint of no cells (a combining mark) is not a
// stop: the mark belongs to the codepoint before it.
static bool isCaretStop(const String &str, size_t pos)
{
    return pos == 0 || pos >= str.size() || cellCount(utf8CodepointAt(str, pos)) != 0;
}

size_t nextCaretStop(const String &str, size_t pos)
{
    if(pos >= str.size())
        return str.size();

    size_t next=pos;

    utf8CodepointAt(str, pos, &next);
    while(!isCaretStop(str, next))
        utf8CodepointAt(str, next, &next);
    return next;
}

size_t prevCaretStop(const String &str, size_t pos)
{
    pos=std::min(pos, str.size());

    if(pos == 0)
        return 0;

    pos=utf8PrevCodepoint(str, pos);
    while(!isCaretStop(str, pos))
        pos=utf8PrevCodepoint(str, pos);
    return pos;
}

size_t caretStopAt(const String &str, size_t pos)
{
    if(pos >= str.size())
        return str.size();

    // the start of the codepoint pos is inside of, if any: a lead within 3
    // bytes before it whose whole sequence reaches past it
    const size_t least=(pos > 3) ? pos-3 : 0;
    size_t lead=pos;

    while(lead > least && (static_cast<unsigned char>(str[lead]) & 0xc0) == 0x80)
        --lead;

    size_t next=lead;

    utf8CodepointAt(str, lead, &next);
    if(lead < pos && next > pos)
        pos=lead;

    while(!isCaretStop(str, pos))
        pos=utf8PrevCodepoint(str, pos);
    return pos;
}

const char *cellWidthUnicodeVersion()
{
    return GWENI_CELL_WIDTH_UNICODE_VERSION;
}

}//namespace text
}//namespace gweni
