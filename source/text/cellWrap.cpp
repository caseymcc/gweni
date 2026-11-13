/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/cellWrap.h>
#include <gweni/text/cells.h>

namespace gweni
{
namespace text
{

namespace
{

// One walk over the line's caret stops, calling rowStart(byte) for each row
// after the first. The row so far is rowCells cells; wordStart is where the
// last word that began on it after a blank starts (0: none, a row's first
// word does not count), wordCells the row's cells before it.
template<typename RowStart>
void walkRows(const String &line, size_t width, RowStart &&rowStart)
{
    if(width == 0)
        return;

    size_t rowBegin=0;
    size_t rowCells=0;
    size_t wordStart=0;
    size_t wordCells=0;
    bool afterBlank=false;

    for(size_t i=0; i < line.size();)
    {
        const uint32_t codepoint=utf8CodepointAt(line, i);
        const size_t next=nextCaretStop(line, i);
        const size_t cells=stringCells(line, i, next);

        if(codepoint == ' ' || codepoint == '\t')
        {
            // blanks hang past the edge
            rowCells+=cells;
            afterBlank=true;
            i=next;
            continue;
        }

        if(afterBlank)
        {
            wordStart=i;
            wordCells=rowCells;
            afterBlank=false;
        }

        if(rowCells+cells > width && rowCells > 0)
        {
            if(wordStart > rowBegin)
            {
                // the word goes to the next row whole
                rowBegin=wordStart;
                rowCells-=wordCells;
                rowStart(rowBegin);
            }

            if(rowCells+cells > width && rowCells > 0)
            {
                // a word wider than the row: cut before this stop
                rowBegin=i;
                rowCells=0;
                rowStart(rowBegin);
            }
            wordStart=rowBegin;
        }

        rowCells+=cells;
        i=next;
    }
}

}//namespace

void wrapCells(const String &line, size_t width, std::vector<size_t> &starts)
{
    starts.clear();
    starts.push_back(0);
    walkRows(line, width, [&starts](size_t byte) { starts.push_back(byte); });
}

size_t wrapRowCount(const String &line, size_t width)
{
    size_t rows=1;

    walkRows(line, width, [&rows](size_t) { ++rows; });
    return rows;
}

}//namespace text
}//namespace gweni
