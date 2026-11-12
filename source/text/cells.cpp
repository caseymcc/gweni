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

const char *cellWidthUnicodeVersion()
{
    return GWENI_CELL_WIDTH_UNICODE_VERSION;
}

}//namespace text
}//namespace gweni
