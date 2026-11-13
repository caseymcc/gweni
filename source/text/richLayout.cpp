/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/text/richLayout.h>
#include <gweni/text/cells.h>
#include <gweni/utility.h>

#include <algorithm>
#include <cstring>

namespace gweni
{
namespace text
{

RichMeasure RichMeasure::of(renderer::Base &renderer)
{
    RichMeasure measure;

    measure.carets=[&renderer](const gweni::Font &font, const gweni::String &text, std::vector<int> &xs)
        {
            renderer.measureCarets(font, text, xs);
        };
    measure.metrics=[&renderer](const gweni::Font &font) { return renderer.fontMetrics(font); };
    return measure;
}

namespace
{

//! A run of atoms of one span on the line being built
struct Run
{
    size_t span=0;
    size_t segment=0;
    size_t start=0;
    size_t visibleEnd=0;
    size_t end=0;
};

bool urlBreak(char c)
{
    return (c == '/') || (c == '-') || (c == '?') || (c == '&') || (c == '=');
}

}//namespace

int RichLayout::Segment::x(size_t byte) const
{
    const size_t k=std::lower_bound(bounds.begin(), bounds.end(), byte)-bounds.begin();

    return xs.empty()?0:xs[std::min(k, xs.size()-1)];
}

void RichLayout::layout(const std::vector<RichLayoutSpan> &spans, int width, const RichMeasure &measure)
{
    setSpans(spans, measure);
    breakLines(width, measure);
}

void RichLayout::setSpans(const std::vector<RichLayoutSpan> &spans, const RichMeasure &measure)
{
    m_lines.clear();
    m_pieces.clear();
    m_spanOffsets.clear();
    m_size=gweni::Point(0, 0);
    m_textLength=0;
    m_spans=spans;
    m_metrics.clear();
    m_segments.clear();
    m_atoms.clear();

    std::vector<renderer::FontMetrics> &metrics=m_metrics;

    for(const RichLayoutSpan &span:spans)
    {
        m_spanOffsets.push_back(m_textLength);
        m_textLength+=span.text.size();
        metrics.push_back(measure.metrics(span.font));
    }

    // every span's segments, each measured once, and their atoms
    std::vector<Segment> &segments=m_segments;
    std::vector<Atom> &atoms=m_atoms;

    for(size_t s=0; s < spans.size(); ++s)
    {
        const gweni::String &text=spans[s].text;

        for(size_t begin=0; begin <= text.size(); )
        {
            const size_t newline=text.find('\n', begin);
            const size_t end=(newline == gweni::String::npos)?text.size():newline;
            const bool hard=(newline != gweni::String::npos);
            Segment segment;

            segment.span=s;
            segment.start=begin;
            segment.end=end;
            if(end > begin)
            {
                for(size_t i=begin; i < end; i=utf8Next(text, i))
                    segment.bounds.push_back(i);
                segment.bounds.push_back(end);
                measure.carets(spans[s].font, text.substr(begin, end-begin), segment.xs);
            }

            const size_t index=segments.size();

            segments.push_back(std::move(segment));

            for(size_t pos=begin; pos < end; )
            {
                Atom atom;
                size_t q=pos;
                bool brokeAtUrl=false;

                atom.segment=index;
                atom.span=s;
                atom.start=pos;
                while((q < end) && (text[q] != ' '))
                {
                    const char c=text[q];

                    q=utf8Next(text, q);
                    if(spans[s].url && urlBreak(c) && (q < end) && (text[q] != ' '))
                    {
                        brokeAtUrl=true;
                        break;
                    }
                }
                atom.visibleEnd=q;
                while((q < end) && (text[q] == ' '))
                    ++q;
                atom.end=q;
                atom.after=(brokeAtUrl || (q > atom.visibleEnd))?Break::Soft:Break::Glue;
                atoms.push_back(atom);
                pos=q;
            }

            if(!hard)
                break;

            // the '\n' ends the line on the segment's last atom, or on an empty one
            if(atoms.empty() || (atoms.back().segment != index))
            {
                Atom marker;

                marker.segment=index;
                marker.span=s;
                marker.start=marker.visibleEnd=marker.end=end;
                atoms.push_back(marker);
            }
            atoms.back().after=Break::Hard;
            begin=end+1;
        }
    }

}

void RichLayout::breakLines(int width, const RichMeasure &measure)
{
    m_lines.clear();
    m_pieces.clear();
    m_size=gweni::Point(0, 0);

    const std::vector<RichLayoutSpan> &spans=m_spans;
    const std::vector<renderer::FontMetrics> &metrics=m_metrics;
    const std::vector<Segment> &segments=m_segments;
    const std::vector<Atom> &atoms=m_atoms;

    // the line being built: its runs, and where its content ends
    std::vector<Run> runs;
    int fullX=0;        // past the last atom, its spaces included
    int contentX=0;     // past the last atom's last visible character
    size_t lineBegin=0;
    size_t lineEnd=0;
    size_t lineSpan=0;  // the span of its first atom, for a line with no pieces
    bool lineOpen=false;
    int top=0;

    auto widthOf=[&](const Atom &atom, size_t end)
        {
            const Segment &segment=segments[atom.segment];

            return segment.x(end)-segment.x(atom.start);
        };

    // where the line's content would end with these atoms after it, its last
    // piece's right padding included
    auto extent=[&](const Atom *first, const Atom *last, bool onEmpty)
        {
            int full=onEmpty?0:fullX;
            int content=onEmpty?0:contentX;
            bool open=!onEmpty && !runs.empty();
            size_t openSpan=open?runs.back().span:0;
            size_t openEnd=open?runs.back().end:0;
            size_t openSegment=open?runs.back().segment:0;

            for(const Atom *atom=first; atom != last; ++atom)
            {
                if(!open || (openSpan != atom->span) || (openSegment != atom->segment) || (openEnd != atom->start))
                {
                    if(open)
                        full+=spans[openSpan].padding;
                    full+=spans[atom->span].padding;
                }
                content=full+widthOf(*atom, atom->visibleEnd);
                full+=widthOf(*atom, atom->end);
                open=true;
                openSpan=atom->span;
                openEnd=atom->end;
                openSegment=atom->segment;
            }
            return content+(open?spans[openSpan].padding:0);
        };

    auto finishLine=[&]()
        {
            RichLine line;

            line.top=top;
            line.firstPiece=m_pieces.size();
            line.begin=lineBegin;
            line.end=lineEnd;

            // the spaces ending the line take no width and are in no piece
            while(!runs.empty())
            {
                runs.back().end=runs.back().visibleEnd;
                if(runs.back().end > runs.back().start)
                    break;
                runs.pop_back();
            }

            int x=0;
            int ascender=0;
            int descender=0;

            for(const Run &run:runs)
            {
                if(run.end <= run.start)
                    continue;

                const RichLayoutSpan &span=spans[run.span];
                const Segment &segment=segments[run.segment];
                RichPiece piece;

                piece.span=run.span;
                piece.start=run.start;
                piece.end=run.end;
                piece.offset=m_spanOffsets[run.span]+run.start;
                piece.text=span.text.substr(run.start, run.end-run.start);
                piece.padding=span.padding;
                piece.metrics=metrics[run.span];

                // a piece at its segment's start is a prefix of the segment's measure
                if(run.start == segment.start)
                {
                    const size_t count=std::lower_bound(segment.bounds.begin(), segment.bounds.end(), run.end)
                        -segment.bounds.begin()+1;

                    piece.carets.assign(segment.xs.begin(), segment.xs.begin()+std::min(count, segment.xs.size()));
                }
                else
                    measure.carets(span.font, piece.text, piece.carets);
                piece.width=piece.carets.empty()?0:piece.carets.back();

                x+=piece.padding;
                piece.pos.x=x;
                x+=piece.width+piece.padding;
                ascender=std::max(ascender, piece.metrics.ascender);
                descender=std::max(descender, piece.metrics.descender);
                m_pieces.push_back(std::move(piece));
            }

            line.pieceCount=m_pieces.size()-line.firstPiece;
            if(line.pieceCount == 0)
            {
                const renderer::FontMetrics &own=metrics[std::min(lineSpan, metrics.size()-1)];

                ascender=own.ascender;
                descender=own.descender;
            }
            line.baseline=ascender;
            line.height=ascender+descender;
            line.width=x;
            for(size_t i=line.firstPiece; i < m_pieces.size(); ++i)
                m_pieces[i].pos.y=top+ascender-m_pieces[i].metrics.ascender;

            top+=line.height;
            m_size.x=std::max(m_size.x, line.width);
            m_lines.push_back(line);

            runs.clear();
            fullX=contentX=0;
            lineOpen=false;
        };

    auto place=[&](const Atom &atom)
        {
            const size_t begin=m_spanOffsets[atom.span]+atom.start;

            if(!lineOpen)
            {
                lineOpen=true;
                lineBegin=begin;
                lineSpan=atom.span;
            }
            lineEnd=m_spanOffsets[atom.span]+atom.end+((atom.after == Break::Hard)?1:0);

            const bool extend=!runs.empty() && (runs.back().span == atom.span) &&
                (runs.back().segment == atom.segment) && (runs.back().end == atom.start);

            if(!extend)
            {
                if(!runs.empty())
                    fullX+=spans[runs.back().span].padding;
                fullX+=spans[atom.span].padding;

                Run run;

                run.span=atom.span;
                run.segment=atom.segment;
                run.start=atom.start;
                runs.push_back(run);
            }
            contentX=fullX+widthOf(atom, atom.visibleEnd);
            fullX+=widthOf(atom, atom.end);
            runs.back().visibleEnd=atom.visibleEnd;
            runs.back().end=atom.end;

            if(atom.after == Break::Hard)
                finishLine();
        };

    for(size_t first=0; first < atoms.size(); )
    {
        // one word: atoms glued up to a break opportunity
        size_t last=first;

        while((last < atoms.size()) && (atoms[last].after == Break::Glue))
            ++last;
        last=std::min(last+1, atoms.size());

        const Atom *begin=atoms.data()+first;
        const Atom *end=atoms.data()+last;
        const bool fits=(width <= 0) || (extent(begin, end, !lineOpen) <= width);

        if(!fits && (extent(begin, end, true) > width))
        {
            // wider than a line: broken at any caret stop (a combining mark kept
            // with its base), from where the line is
            for(const Atom *atom=begin; atom != end; ++atom)
            {
                const gweni::String &text=spans[atom->span].text;
                size_t pos=atom->start;

                do
                {
                    Atom piece=*atom;
                    const size_t next=(pos < atom->visibleEnd)?nextCaretStop(text, pos):atom->end;

                    piece.start=pos;
                    piece.visibleEnd=std::min(next, atom->visibleEnd);
                    piece.end=(next >= atom->visibleEnd)?atom->end:next;
                    if(piece.end < atom->end)
                        piece.after=Break::Soft;
                    pos=piece.end;

                    if(lineOpen && (extent(&piece, &piece+1, false) > width))
                        finishLine();
                    place(piece);
                }
                while(pos < atom->end);
            }
        }
        else
        {
            if(!fits && lineOpen)
                finishLine();
            for(const Atom *atom=begin; atom != end; ++atom)
                place(*atom);
        }
        first=last;
    }

    if(lineOpen)
        finishLine();
    else if(!atoms.empty() && (atoms.back().after == Break::Hard))
    {
        // the empty line after a final '\n'
        lineBegin=lineEnd=m_textLength;
        lineSpan=atoms.back().span;
        finishLine();
    }

    m_size.y=top;
}

}//namespace text
}//namespace gweni
