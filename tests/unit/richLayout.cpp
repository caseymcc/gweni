/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::RichLayout on the stub fixture's measures (StubRenderer: a byte is
 *  font.size*0.4 units wide, so 4 at size 10 and 8 at size 20; a font's line
 *  box is its size, its baseline at four fifths of it). It pins:
 *  - a line breaks after spaces, which end the line in no piece, and a word
 *    goes on the next line whole when it does not fit;
 *  - a word made of two spans (a span ending mid-word) stays together;
 *  - a url span also breaks after '/', '-', '?', '&' and '=', the same text
 *    in a plain span not;
 *  - a word wider than the line breaks at any codepoint, filling what is left
 *    of its line first, and a line holds a codepoint at least;
 *  - a line's baseline is its pieces' largest ascender, each piece's box top
 *    at baseline - its ascender, the line as high as that plus the largest
 *    descender, the next line below it;
 *  - '\n' ends a line (and a final one leaves an empty line), empty spans
 *    lay out nothing, and no text gives no lines;
 *  - a span's padding is room either side of each of its pieces;
 *  - each span's text is measured once, a piece starting inside its span
 *    (after a break in it) once more, and every piece's carets are its own.
 */
#include "headlessFixture.h"

#include <gweni/text/richLayout.h>

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

using gweni::text::RichLayout;
using gweni::text::RichLayoutSpan;
using gweni::text::RichMeasure;
using gweni::text::RichPiece;

namespace
{

RichLayoutSpan span(const gweni::String &text, float size=10.0f, bool url=false, int padding=0)
{
    RichLayoutSpan made;

    made.text=text;
    made.font.facename="OpenSans";
    made.font.size=size;
    made.url=url;
    made.padding=padding;
    return made;
}

//! Each line's pieces' text, joined by '|'
std::vector<std::string> lineTexts(const RichLayout &layout)
{
    std::vector<std::string> texts;

    for(const gweni::text::RichLine &line:layout.lines())
    {
        std::string text;

        for(size_t i=line.firstPiece; i < line.firstPiece+line.pieceCount; ++i)
        {
            if(i > line.firstPiece)
                text+='|';
            text+=layout.pieces()[i].text;
        }
        texts.push_back(text);
    }
    return texts;
}

using Lines=std::vector<std::string>;

std::pair<int, int> xy(const gweni::Point &p)
{
    return { p.x, p.y };
}

}//namespace

TEST_CASE("RichLayout: lines break after spaces, a word that does not fit going to the next line whole")
{
    test::StubRenderer renderer;
    RichLayout layout;

    layout.layout({ span("aaa bbb ccc") }, 30, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "aaa bbb", "ccc" });
    REQUIRE(layout.lines().size() == 2);
    CHECK(layout.lines()[0].begin == 0);
    CHECK(layout.lines()[0].end == 8);   // the space before "ccc" is the first line's
    CHECK(layout.lines()[1].begin == 8);
    CHECK(layout.lines()[1].end == 11);
    CHECK(layout.lines()[0].width == 28);
    CHECK(xy(layout.size()) == std::make_pair(28, 20));

    // one unit narrower than "aaa bbb"
    layout.layout({ span("aaa bbb ccc") }, 27, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "aaa", "bbb", "ccc" });

    // no width: one line
    layout.layout({ span("aaa bbb ccc") }, 0, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "aaa bbb ccc" });
}

TEST_CASE("RichLayout: a span that ends mid-word keeps the word together")
{
    test::StubRenderer renderer;
    RichLayout layout;

    // the words are "aaa ", "bb"+"cc " and "dd"
    layout.layout({ span("aaa bb"), span("cc dd") }, 32, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "aaa bb|cc", "dd" });

    layout.layout({ span("aaa bb"), span("cc dd") }, 31, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "aaa", "bb|cc dd" });
    REQUIRE(layout.pieces().size() == 3);
    CHECK(layout.pieces()[1].span == 0);
    CHECK(layout.pieces()[1].start == 4);
    CHECK(layout.pieces()[1].offset == 4);
    CHECK(layout.pieces()[2].span == 1);
    CHECK(layout.pieces()[2].offset == 6);
    CHECK(layout.pieces()[2].pos.x == 8);
}

TEST_CASE("RichLayout: a url span breaks after / - ? & =, a plain one only at any character")
{
    test::StubRenderer renderer;
    RichLayout layout;
    const gweni::String url="http://example.com/path/to?x=1&y=2-3";

    // 15 bytes a line
    layout.layout({ span(url, 10.0f, true) }, 60, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "http://", "example.com/", "path/to?x=1&y=", "2-3" });

    layout.layout({ span(url) }, 60, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "http://example.", "com/path/to?x=1", "&y=2-3" });

    // a break character a space follows is no url break
    layout.layout({ span("a/ b- c", 10.0f, true) }, 12, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "a/", "b-", "c" });
}

TEST_CASE("RichLayout: a word wider than the line breaks at any codepoint, filling its line first")
{
    test::StubRenderer renderer;
    RichLayout layout;

    layout.layout({ span("ab cccccccccccccccccccc dd") }, 40, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "ab ccccccc", "cccccccccc", "ccc dd" });

    // narrower than one codepoint: one a line
    layout.layout({ span("abc") }, 2, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "a", "b", "c" });

    // across a span edge, and a multi-byte codepoint never split
    layout.layout({ span("xx"), span("y\xC3\xA9y") }, 12, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "xx|y", "\xC3\xA9y" });
}

TEST_CASE("RichLayout: a line's baseline is its largest ascender, each piece on it")
{
    test::StubRenderer renderer;
    RichLayout layout;

    // size 10: line box 10, ascender 8; size 20: 20 and 16; size 5: 5 and 4
    layout.layout({ span("small "), span("BIG", 20.0f), span(" tiny\nnext", 5.0f) }, 0,
        RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "small |BIG| tiny", "next" });
    REQUIRE(layout.lines().size() == 2);
    REQUIRE(layout.pieces().size() == 4);

    const gweni::text::RichLine &first=layout.lines()[0];

    CHECK(first.top == 0);
    CHECK(first.baseline == 16);
    CHECK(first.height == 16+4);
    CHECK(xy(layout.pieces()[0].pos) == std::make_pair(0, 16-8));
    CHECK(xy(layout.pieces()[1].pos) == std::make_pair(24, 0));
    CHECK(xy(layout.pieces()[2].pos) == std::make_pair(24+24, 16-4));
    // each piece's baseline is the line's
    for(size_t i=0; i < 3; ++i)
        CHECK(layout.pieces()[i].pos.y+layout.pieces()[i].metrics.ascender == first.top+first.baseline);

    const gweni::text::RichLine &second=layout.lines()[1];

    CHECK(second.top == 20);
    CHECK(second.baseline == 4);
    CHECK(second.height == 5);
    CHECK(xy(layout.pieces()[3].pos) == std::make_pair(0, 20));
    CHECK(xy(layout.size()) == std::make_pair(24+24+10, 25));
}

TEST_CASE("RichLayout: '\\n' ends a line, empty spans lay out nothing, no text no lines")
{
    test::StubRenderer renderer;
    RichLayout layout;

    layout.layout({ span("ab\ncd") }, 0, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "ab", "cd" });
    CHECK(layout.lines()[0].end == 3);
    CHECK(layout.lines()[1].begin == 3);

    // a final '\n' leaves an empty line, and a blank line is one
    layout.layout({ span("ab\n\n") }, 0, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "ab", "", "" });
    CHECK(layout.size().y == 30);

    layout.layout({ span(""), span("abc"), span("") }, 0, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "abc" });
    CHECK(layout.pieces()[0].span == 1);
    CHECK(layout.spanOffsets() == std::vector<size_t>{ 0, 0, 3 });
    CHECK(layout.textLength() == 3);

    layout.layout({ span(""), span("") }, 30, RichMeasure::of(renderer));
    CHECK(layout.lines().empty());
    CHECK(layout.pieces().empty());
    CHECK(xy(layout.size()) == std::make_pair(0, 0));

    layout.layout({}, 30, RichMeasure::of(renderer));
    CHECK(layout.lines().empty());
}

TEST_CASE("RichLayout: a span's padding is room either side of each of its pieces")
{
    test::StubRenderer renderer;
    RichLayout layout;

    layout.layout({ span("a "), span("code", 10.0f, false, 2), span(" b") }, 0, RichMeasure::of(renderer));
    REQUIRE(layout.pieces().size() == 3);
    CHECK(layout.pieces()[1].pos.x == 8+2);
    const gweni::Rect box=layout.pieces()[1].box();

    CHECK(xy(gweni::Point(box.x, box.y)) == std::make_pair(8, 0));
    CHECK(xy(gweni::Point(box.w, box.h)) == std::make_pair(16+4, 10));
    CHECK(layout.pieces()[2].pos.x == 8+16+4);

    // the padding counts in the fit: "a code" is 24 wide and 28 with it
    layout.layout({ span("a "), span("code", 10.0f, false, 2) }, 27, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "a", "code" });
    CHECK(layout.pieces()[1].pos.x == 2);
}

TEST_CASE("RichLayout: a span is measured once, a piece starting inside it once more")
{
    test::StubRenderer renderer;
    RichLayout layout;

    size_t before=renderer.caretMeasures;

    layout.layout({ span("one two"), span("three", 20.0f) }, 0, RichMeasure::of(renderer));
    CHECK(renderer.caretMeasures-before == 2);
    CHECK(layout.pieces()[0].carets == std::vector<int>{ 0, 4, 8, 12, 16, 20, 24, 28 });

    before=renderer.caretMeasures;
    layout.layout({ span("one two"), span(" three") }, 20, RichMeasure::of(renderer));
    CHECK(lineTexts(layout) == Lines{ "one", "two", "three" });
    // two segments, and "two" and "three" each start inside their span
    CHECK(renderer.caretMeasures-before == 4);

    const RichPiece &two=layout.pieces()[1];

    CHECK(two.carets == std::vector<int>{ 0, 4, 8, 12 });
    CHECK(two.width == 12);
    CHECK(two.offset == 4);
    CHECK(layout.pieces()[2].offset == 8);
    CHECK(layout.pieces()[0].carets == std::vector<int>{ 0, 4, 8, 12 });
}
