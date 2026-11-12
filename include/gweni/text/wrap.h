/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_wrap_h_
#define _gweni_text_wrap_h_

#include <gweni/export.h>
#include <gweni/platforms/platformTypes.h>

#include <functional>
#include <vector>

namespace gweni
{
namespace text
{

//! The width a string takes, in units: a control's
//! measureText(font, s).x (controls::Base::wrapText), or anything else for a
//! test.
using MeasureWidth=std::function<int(const gweni::String &)>;

//! A wrapped line: what it shows, and how many characters of the wrapped
//! string it holds (the shown ones and the '\n' that ended it, if one did).
struct WrappedLine
{
    gweni::String text;
    int length;
};

//! Splits s into words (each with its trailing space), "\n" tokens and, when
//! w > 0, pieces of words wider than w, broken between codepoints (a single
//! codepoint wider than w is a piece of its own). Measures only with w > 0.
GWENI_EXPORT void splitWords(const gweni::String &s, int w, const MeasureWidth &measure,
    std::vector<gweni::String> &elems);

//! Breaks s into lines no wider than w, appended to lines: a line ends at
//! each '\n', which it holds but does not show, and before a word that would
//! take it past w. A blank line is an empty line, and so is the one after a
//! final '\n', so there is always one line at least. With w <= 0 the lines
//! only break at '\n'. This is the wrap a wrapped controls::internal::Text
//! shows, the same measures in the same order, so a list measuring a row of
//! wrapped text through it gets the height the row's Text lays out at.
GWENI_EXPORT void wrapLines(const gweni::String &s, int w, const MeasureWidth &measure,
    std::vector<WrappedLine> &lines);

}//namespace text
}//namespace gweni

#endif//_gweni_text_wrap_h_
