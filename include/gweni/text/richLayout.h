/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_richLayout_h_
#define _gweni_text_richLayout_h_

#include <gweni/export.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/platformTypes.h>

#include <functional>
#include <vector>

namespace gweni
{
namespace text
{

//! How a RichLayout measures: a control's Base::measureCarets and its
//! renderer's fontMetrics, or a renderer's own (of(), for a test). Both in
//! canvas units.
struct GWENI_EXPORT RichMeasure
{
    //! renderer::Base::measureCarets: the x of every codepoint boundary
    std::function<void(const gweni::Font &, const gweni::String &, std::vector<int> &)> carets;
    //! renderer::Base::fontMetrics
    std::function<renderer::FontMetrics(const gweni::Font &)> metrics;

    //! The renderer's own measureCarets and fontMetrics
    static RichMeasure of(renderer::Base &renderer);
};

//! One span of a paragraph as RichLayout takes it: its text in one font
struct RichLayoutSpan
{
    gweni::String text;
    gweni::Font font;
    //! A URL: a line may also break after a '/', '-', '?', '&' or '=' in it
    //! that a character other than a space follows
    bool url=false;
    //! Room left of and right of each of the span's pieces (inline code's
    //! background reaching past its text), counted in the line's width
    int padding=0;
};

//! One span's run on one line: what renderText draws at pos
struct RichPiece
{
    size_t span=0;      //!< its span's index
    size_t start=0;     //!< its first byte in the span's text
    size_t end=0;       //!< past its last byte; spaces ending its line left out
    size_t offset=0;    //!< start in the paragraph's text (every span's, in order)
    gweni::String text; //!< the span's text from start to end
    //! the top-left of its line box, as renderText takes it: its baseline is
    //! the line's, ascender below pos.y
    gweni::Point pos;
    int width=0;        //!< measureText(text).x, carets.back()
    int padding=0;      //!< its span's: its box starts padding left of pos.x
    renderer::FontMetrics metrics; //!< its font's
    //! the x of every codepoint boundary of text, from pos.x (measureCarets)
    std::vector<int> carets;

    //! The box it takes in its line: its text's width and line box, and its
    //! span's padding on either side
    gweni::Rect box() const
    {
        return gweni::Rect(pos.x-padding, pos.y, width+2*padding, metrics.lineHeight);
    }
};

//! One line of a paragraph
struct RichLine
{
    int top=0;          //!< from the paragraph's top
    int height=0;       //!< its largest ascender plus its largest descender
    int baseline=0;     //!< below top: its pieces' largest ascender
    int width=0;        //!< the right edge of its last piece's box
    size_t firstPiece=0;
    size_t pieceCount=0;
    //! the paragraph text it covers, [begin, end): a '\n' ending it and the
    //! spaces before a break are its, though they show nothing
    size_t begin=0;
    size_t end=0;
};

//! A paragraph of styled spans laid out as one: lines of positioned pieces,
//! each piece one span's run on one line, kept as data (no controls), for
//! controls::RichText and whatever else shows runs of several fonts together.
//!
//! A line breaks at a '\n' (which it holds and does not show), after a run
//! of spaces, and in a url span after '/', '-', '?', '&' or '=' (a "word" is
//! the text between two such places, across span edges). A word goes on the
//! line if the line's pieces up to the word's last visible character fit in
//! the width, and on the next line otherwise; a word wider than the width by
//! itself breaks at any codepoint, filling what is left of its line first,
//! and a line holds a codepoint at least. Spaces ending a line take no width
//! and are in no piece. A line's baseline is the largest ascender of its
//! pieces, each piece's box top at baseline - its ascender, and its height
//! reaches the largest descender below that; a line with no pieces is its
//! span's font's line box. Kerning across a span edge, and across a line
//! break inside a span, is lost.
//!
//! Each span is measured once (measureCarets of its text, and its font's
//! metrics); a piece that starts inside its span (after a break in it) is
//! measured again on its own, so every piece's width and carets are the
//! measure of what it draws. A line's width is that of its pieces, which can
//! pass the width the breaks were chosen for by a unit, the rounding of a
//! piece cut out of its span.
class GWENI_EXPORT RichLayout
{
public:
    //! Lays spans out in lines no wider than width (width <= 0: lines end
    //! only at '\n'), replacing what was laid out before. No spans give no
    //! lines.
    void layout(const std::vector<RichLayoutSpan> &spans, int width, const RichMeasure &measure);
    //! layout() in two steps: setSpans keeps the spans and measures each
    //! one's text and font, breakLines breaks them into lines at a width,
    //! measuring only the pieces that start inside a span, so the same spans
    //! laid out at several widths are measured once. setSpans leaves no lines
    //! until breakLines.
    void setSpans(const std::vector<RichLayoutSpan> &spans, const RichMeasure &measure);
    void breakLines(int width, const RichMeasure &measure);
    //! The spans setSpans was given
    const std::vector<RichLayoutSpan> &spans() const { return m_spans; }

    const std::vector<RichLine> &lines() const { return m_lines; }
    const std::vector<RichPiece> &pieces() const { return m_pieces; }
    //! The widest line's width and the lines' height
    gweni::Point size() const { return m_size; }
    //! Each span's first byte in the paragraph's text
    const std::vector<size_t> &spanOffsets() const { return m_spanOffsets; }
    //! The paragraph's text: every span's, in order
    size_t textLength() const { return m_textLength; }

private:
    //! A span's text between two '\n's (or its ends), measured once
    struct Segment
    {
        size_t span=0;
        size_t start=0;
        size_t end=0;
        //! the byte offset of every codepoint boundary, and its x
        std::vector<size_t> bounds;
        std::vector<int> xs;

        int x(size_t byte) const;
    };

    enum class Break
    {
        Glue,   //!< the next atom is the same word's
        Soft,   //!< a line may break after it
        Hard    //!< a '\n' follows it: the line ends after it
    };

    //! The text between two break opportunities inside one segment
    struct Atom
    {
        size_t segment=0;
        size_t span=0;
        size_t start=0;
        size_t visibleEnd=0; //!< end less the spaces ending it
        size_t end=0;
        Break after=Break::Glue;
    };

    std::vector<RichLayoutSpan> m_spans;
    std::vector<renderer::FontMetrics> m_metrics;
    std::vector<Segment> m_segments;
    std::vector<Atom> m_atoms;

    std::vector<RichLine> m_lines;
    std::vector<RichPiece> m_pieces;
    std::vector<size_t> m_spanOffsets;
    size_t m_textLength=0;
    gweni::Point m_size;
};

}//namespace text
}//namespace gweni

#endif//_gweni_text_richLayout_h_
