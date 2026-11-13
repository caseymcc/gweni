/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_cellwrap_h_
#define _gweni_text_cellwrap_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <cstddef>
#include <vector>

namespace gweni
{
namespace text
{

//! The visual rows a line of a fixed-width run wraps into at \p width cells,
//! counted from cellCount() alone (wide glyphs 2 cells, marks 0), with no
//! measuring: \p starts is set to the byte each row starts at, the first 0,
//! so a line always has one row at least. The line holds no '\n'.
//!
//! A row takes as many caret stops (a codepoint and the marks after it) as
//! fit in \p width cells. Blanks (' ', '\t') always fit: a run of them that
//! goes past the edge hangs on the row it ends, as a textarea's spaces do, so
//! a row never starts with the blanks that ended a word. A word (a run of
//! anything else) that does not fit goes to the next row whole, the row
//! breaking where it starts, unless it starts the row: then the row breaks
//! at the stop that does not fit (a word wider than the row is cut between
//! stops), and a stop wider than the row (a wide glyph at 1 cell) is a row of
//! its own. A \p width of 0 does not wrap: one row.
GWENI_EXPORT void wrapCells(const String &line, size_t width, std::vector<size_t> &starts);

//! How many rows wrapCells() gives, without keeping where they start.
GWENI_EXPORT size_t wrapRowCount(const String &line, size_t width);

}//namespace text
}//namespace gweni

#endif//_gweni_text_cellwrap_h_
