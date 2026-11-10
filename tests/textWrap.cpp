/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A wrapped Text splits its string into one child Text per line. The lines
 *  must stack one row apart, show no '\n' (freetype-gl breaks a run there, so
 *  a line ending in one measured 0 wide and drew below its box), stay within
 *  the parent's current width, and map character indices into the whole
 *  string, which still counts every '\n', to the line and row holding them.
 *  The lines are kept while the string, font and width are unchanged, and
 *  updated in place when they change: only the difference is created or
 *  deleted. The helpers read the wrap as data, re-wrapped when a helper asks
 *  after an edit, so between an edit and its layout they already answer for
 *  the edited string, and layout does not wrap it a second time.
 *
 *  The cases wrap a two-paragraph string in a container of known width and
 *  check the line children (one row apart, no '\n' in any line string, one
 *  empty line for "\n\n", none wider than the width), the char <-> line
 *  helpers and caret rows across the '\n's, a re-wrap at a narrower width in
 *  the first frame, that a layout with unchanged inputs keeps the same line
 *  pointers and writes no text slots, that a new width or string reuses the
 *  surviving lines (and matches a fresh wrap), that a text colour reaches the
 *  lines, and TextBoxMultiline's Home/End/Up/Down, with a frame between keys
 *  and with none. After a setText with no layout between, every char <-> line
 *  helper (index and geometry, for every index and row, and clicks aimed at
 *  every caret, the midpoints between them and every row's edges, asked
 *  first) must answer as it does after the layout; before the first layout
 *  they read the string unwrapped; and an edit is wrapped once, by a helper or
 *  by layout (Text::wrapCount()), the children being synced from that wrap in
 *  arrange.
 *
 *  An edit is wrapped again by the paragraph and only the changed lines'
 *  children are synced, which must give what a fresh Text holding the edited
 *  string gives (the wrap the helpers read before and after the layout, the
 *  line children's strings, lengths and bounds, and the children in the
 *  lines' order): a '\n' deleted (two paragraphs joined) and typed again, one
 *  typed inside a paragraph, edits at the start and the end (a final '\n'
 *  included), multi-byte UTF-8 typed and deleted, several lines pasted, two
 *  edits with no wrap between, a new font (a whole wrap), and a seeded random
 *  run of inserts and deletes on the real renderer's measures. An edit in the
 *  second paragraph keeps every line child before it and writes no glyph of
 *  theirs. tests/unit/textWrapEdits.cpp runs the long random check and the
 *  counts on the StubRenderer.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/primitiveIds.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/utility.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{

using gweni::Point;
using gweni::String;
using gweni::controls::internal::Text;

const String paragraphs=
    "The quick brown fox jumps over the lazy dog. Pack my box with five dozen "
    "liquor jugs. How vexingly quick daft zebras jump!\n\n"
    "Sphinx of black quartz, judge my vow. The five boxing wizards jump quickly.";

//! A plain greedy wrap of `text` in `font` at `width`, the oracle for the line count:
//! lines break at '\n' and before a word (with its trailing space) that would
//! take the line past the width. No word here is wider than the widths used.
std::vector<String> greedyWrap(const gweni::Font &font, const String &text, int width)
{
    gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();
    std::vector<String> lines;
    String line;
    String word;

    const auto addWord=[&]()
    {
        if(!line.empty() && renderer.measureText(font, line+word).x > width)
        {
            lines.push_back(line);
            line.clear();
        }
        line+=word;
        word.clear();
    };

    for(char c:text)
    {
        if(c == '\n')
        {
            addWord();
            lines.push_back(line);
            line.clear();
            continue;
        }

        word+=c;
        if(c == ' ')
            addWord();
    }
    addWord();
    lines.push_back(line);
    return lines;
}

//! A canvas holding a container of the given width with one wrapped Text.
struct WrapScene
{
    explicit WrapScene(int width)
    {
        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();
        const Point viewSize=gweni::test::viewSize();

        skin=std::make_unique<gweni::skin::textured::TexturedSkin>(&renderer);
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=std::make_unique<gweni::controls::Canvas>(skin.get());
        canvas->setSize(viewSize.x, viewSize.y);

        container=canvas->newChild<gweni::controls::Base>();
        container->setBounds(10, 10, width, 400);

        text=container->newChild<Text>();
        text->setWrap(true);
        text->setText(paragraphs);
    }

    ~WrapScene()
    {
        canvas.reset();
        skin.reset();

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.begin();
        renderer.end();
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    int lineHeight()
    {
        return gweni::test::gpuRenderer().measureText(text->getFont(), " ").y;
    }

    int measure(const String &s)
    {
        return gweni::test::gpuRenderer().measureText(text->getFont(), s).x;
    }

    //! The lines' layout and strings, against the oracle at `width`.
    void checkLines(int width, const String &source=paragraphs)
    {
        const std::vector<String> expected=greedyWrap(text->getFont(), source, width);
        const int rowHeight=lineHeight();

        REQUIRE(rowHeight > 0);
        REQUIRE(text->numLines() == expected.size());

        String rebuilt;

        for(unsigned i=0; i < text->numLines(); ++i)
        {
            CAPTURE(i);
            Text *line=text->getLine(static_cast<int>(i));

            REQUIRE(line != nullptr);
            CHECK(line->getText() == expected[i]);
            CHECK(line->getText().find('\n') == String::npos);
            CHECK(line->getX() == 0);
            CHECK(line->getY() == static_cast<int>(i)*rowHeight);
            CHECK(line->getHeight() == rowHeight);
            CHECK(measure(line->getText()) <= width);

            // a non-empty line is as wide as its glyphs
            if(!line->getText().empty())
                CHECK(line->getWidth() == measure(line->getText()));

            // each line holds its shown characters and the '\n' that ended it
            const int shown=static_cast<int>(line->getText().length());
            const int held=text->getLineLength(static_cast<int>(i));

            CHECK((held == shown || held == shown+1));
            rebuilt+=line->getText();
            if(held == shown+1)
                rebuilt+='\n';
        }

        CHECK(rebuilt == source);

        Text *last=text->getLine(static_cast<int>(text->numLines())-1);

        CHECK(last->bottom() == text->getHeight());
    }

    std::vector<Text *> lines()
    {
        std::vector<Text *> result;

        for(unsigned i=0; i < text->numLines(); ++i)
            result.push_back(text->getLine(static_cast<int>(i)));
        return result;
    }

    std::unique_ptr<gweni::skin::textured::TexturedSkin> skin;
    std::unique_ptr<gweni::controls::Canvas> canvas;
    gweni::controls::Base *container=nullptr;
    Text *text=nullptr;
};

}//namespace

TEST_CASE("Wrapped text: lines stack one row apart, show no '\\n' and stay within the width")
{
    const int width=200;
    WrapScene scene(width);

    scene.frame();
    scene.checkLines(width);

    // "\n\n" is one blank row: the line after the first paragraph's last line
    const int firstBreak=static_cast<int>(paragraphs.find('\n'));
    const int paragraphEnd=scene.text->getLineFromChar(firstBreak);
    Text *blank=scene.text->getLine(paragraphEnd+1);

    REQUIRE(blank != nullptr);
    CHECK(blank->getText().empty());
    CHECK(scene.text->getLineLength(paragraphEnd+1) == 1);
    CHECK(scene.text->getLine(paragraphEnd+2)->getText().rfind("Sphinx", 0) == 0);
}

TEST_CASE("Wrapped text: character indices map to the line and row holding them")
{
    WrapScene scene(200);

    scene.frame();

    Text *text=scene.text;
    const int length=text->length();
    const int lines=static_cast<int>(text->numLines());

    REQUIRE(lines > 3);

    // the lines' source ranges tile the string
    CHECK(text->getStartCharFromLine(0) == 0);
    for(int i=0; i+1 < lines; ++i)
        CHECK(text->getEndCharFromLine(i) == text->getStartCharFromLine(i+1));
    CHECK(text->getEndCharFromLine(lines-1) == length);

    for(int c=0; c <= length; ++c)
    {
        CAPTURE(c);
        const int line=text->getLineFromChar(c);
        const int start=text->getStartCharFromLine(line);

        REQUIRE(line >= 0);
        REQUIRE(line < lines);
        CHECK(start <= c);
        CHECK((c < text->getEndCharFromLine(line) || (c == length && line == lines-1)));
        CHECK(text->getCharPosOnLine(c) == c-start);
        CHECK(c <= text->getLastCaretOnLine(line));

        // the caret is drawn on that line's row, a '\n' at its line's end
        const gweni::Rect pos=text->getCharacterPosition(c);
        Text *row=text->getLine(line);

        CHECK(pos.y == row->getY());
        if(c < length && paragraphs[c] == '\n')
        {
            CHECK(c == text->getLastCaretOnLine(line));
            CHECK(pos.x == scene.measure(row->getText()));
        }

        // and a click there finds it again
        CHECK(text->getClosestCharacter(Point(pos.x, pos.y+1)) == c);
    }
}

TEST_CASE("Wrapped text: a narrower parent re-wraps at the new width in the first frame")
{
    const int wide=240;
    const int narrow=140;
    WrapScene scene(wide);

    scene.frame();
    scene.frame();
    scene.checkLines(wide);

    const unsigned wideLines=scene.text->numLines();

    scene.container->setWidth(narrow);
    scene.frame();

    CHECK(scene.text->numLines() > wideLines);
    scene.checkLines(narrow);
}

namespace
{

size_t liveTextIds()
{
    return gweni::textIdCount()-gweni::releasedTextIdCount();
}

//! What a line shows and where, to compare two wraps of the same string.
struct LineState
{
    String text;
    int length;
    gweni::Rect bounds;

    bool operator==(const LineState &rhs) const
    {
        return text == rhs.text && length == rhs.length && bounds == rhs.bounds;
    }
};

std::vector<LineState> lineStates(Text *text)
{
    std::vector<LineState> states;

    for(unsigned i=0; i < text->numLines(); ++i)
    {
        Text *line=text->getLine(static_cast<int>(i));

        states.push_back({line->getText(), text->getLineLength(static_cast<int>(i)), line->getBounds()});
    }
    return states;
}

}//namespace

TEST_CASE("Wrapped text: a layout with unchanged inputs keeps the lines and allocates nothing")
{
    const int width=200;
    WrapScene scene(width);

    scene.frame();
    scene.frame();

    const std::vector<Text *> before=scene.lines();
    const size_t idCount=gweni::textIdCount();
    const size_t live=liveTextIds();

    REQUIRE(before.size() > 3);

    // lay the text and its container out again, nothing about them changed
    scene.text->invalidate();
    scene.container->invalidate();
    scene.frame();

    CHECK(scene.lines() == before);
    CHECK(gweni::textIdCount()-idCount == 0);
    CHECK(liveTextIds() == live);
    CHECK(scene.text->numChildren() == before.size());
    CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten == 0);
    scene.checkLines(width);
}

TEST_CASE("Wrapped text: a new width keeps the lines that are still needed")
{
    const int wide=240;
    const int narrow=140;
    WrapScene scene(wide);

    scene.frame();

    const std::vector<Text *> wideLines=scene.lines();
    const size_t live=liveTextIds();

    // narrower: more lines, the ones there were are the first of them
    scene.container->setWidth(narrow);
    scene.frame();
    scene.checkLines(narrow);

    const std::vector<Text *> narrowLines=scene.lines();

    REQUIRE(narrowLines.size() > wideLines.size());
    for(size_t i=0; i < wideLines.size(); ++i)
    {
        CAPTURE(i);
        CHECK(narrowLines[i] == wideLines[i]);
    }
    CHECK(liveTextIds()-live == narrowLines.size()-wideLines.size());
    CHECK(scene.text->numChildren() == narrowLines.size());

    // and wider again: the surplus lines are deleted, the rest kept
    scene.container->setWidth(wide);
    scene.frame();
    scene.checkLines(wide);

    const std::vector<Text *> wideAgain=scene.lines();

    REQUIRE(wideAgain.size() == wideLines.size());
    for(size_t i=0; i < wideAgain.size(); ++i)
    {
        CAPTURE(i);
        CHECK(wideAgain[i] == wideLines[i]);
    }
    CHECK(liveTextIds() == live);
    CHECK(scene.text->numChildren() == wideLines.size());
}

TEST_CASE("Wrapped text: a new string reuses the lines and matches a fresh wrap of it")
{
    const int width=200;
    // the same first paragraph, the rest shorter
    const String edited=paragraphs.substr(0, paragraphs.find('\n'))+"\nShort.";
    std::vector<LineState> reused;

    {
        WrapScene scene(width);

        scene.frame();
        scene.frame();

        const std::vector<Text *> before=scene.lines();
        const size_t firstBreak=paragraphs.find('\n');
        const int unchanged=scene.text->getLineFromChar(static_cast<int>(firstBreak));

        scene.text->setText(edited);
        scene.frame();
        scene.checkLines(width, edited);

        const std::vector<Text *> after=scene.lines();

        REQUIRE(after.size() < before.size());
        for(size_t i=0; i < after.size(); ++i)
        {
            CAPTURE(i);
            CHECK(after[i] == before[i]);
        }

        // the lines of the first paragraph did not change, so only the
        // glyphs of the ones after it were written
        size_t changedGlyphs=0;

        for(size_t i=unchanged+1; i < after.size(); ++i)
            changedGlyphs+=after[i]->getText().size();

        CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten <= changedGlyphs);
        reused=lineStates(scene.text);
    }

    WrapScene fresh(width);

    fresh.text->setText(edited);
    fresh.frame();

    CHECK(lineStates(fresh.text) == reused);
}

TEST_CASE("Wrapped text: a text colour reaches the lines")
{
    WrapScene scene(200);

    scene.frame();

    const gweni::Color red(255, 0, 0, 255);

    scene.text->setTextColor(red);
    for(Text *line:scene.lines())
        CHECK((line->getTextColor() == red));
}

namespace
{

//! The x of every caret (codepoint boundary) in [first, last] of `text`'s string.
std::vector<int> caretXs(Text *text, int first, int last)
{
    const String &str=text->getText();
    std::vector<int> xs;

    for(size_t c=static_cast<size_t>(first); c <= static_cast<size_t>(last); c=gweni::utf8Next(str, c))
    {
        xs.push_back(text->getCharacterPosition(static_cast<unsigned>(c)).x);
        if(c >= str.size())
            break;
    }
    return xs;
}

//! Click x's along one row of carets at `xs`: before the row's start, past
//! its end, and at every `stride`th caret (and the last) its x, x-1 and x+1
//! and the midpoint to the caret after it, both sides of it when the gap is odd.
void addRowXs(std::set<int> &probes, const std::vector<int> &xs, int pastEnd, size_t stride=1)
{
    stride=std::max<size_t>(stride, 1);
    probes.insert(-4);
    probes.insert(pastEnd);
    for(size_t k=0; k < xs.size(); k=(k+1 < xs.size())?std::min(k+stride, xs.size()-1):xs.size())
    {
        probes.insert({xs[k]-1, xs[k], xs[k]+1});
        if(k+1 < xs.size())
            probes.insert({(xs[k]+xs[k+1])/2, (xs[k]+xs[k+1]+1)/2});
    }
}

//! The points helperAnswers() clicks at, from the carets of the scene's laid
//! out Text. getClosestCharacter picks the row from y alone and the nearest
//! caret on it from x, so its answer can only change at a row's edge and at
//! the midpoint between two carets: each row gets every caret's x, x+-1 and
//! the midpoints between them (where a tie goes, and where a caret one pixel
//! off would show) at its middle, before its start and past its end; its top
//! and bottom pixel, above the first row and below the last, get the row's
//! ends and first and last caret. A uniform grid as dense costs a multiple:
//! each click measures every prefix of its row.
std::vector<Point> clickProbes(WrapScene &scene)
{
    Text *text=scene.text;
    const int lines=static_cast<int>(text->numLines());
    const int rowHeight=scene.lineHeight();
    const int pastEnd=text->getWidth()+8;
    std::vector<Point> probes;

    for(int i=0; i < lines; ++i)
    {
        const std::vector<int> xs=caretXs(text, text->getStartCharFromLine(i), text->getLastCaretOnLine(i));
        const int top=text->getLineBox(i).y;
        std::set<int> along;

        addRowXs(along, xs, pastEnd);
        for(int x:along)
            probes.emplace_back(x, top+rowHeight/2);

        std::vector<int> edges={top, top+rowHeight-1};

        if(i == 0)
            edges.insert(edges.end(), {top-rowHeight/2, top-1});
        if(i+1 == lines)
            edges.insert(edges.end(), {top+rowHeight, top+rowHeight+rowHeight/2});
        for(int y:edges)
            for(int x:{-4, xs.front(), xs.back(), pastEnd})
                probes.emplace_back(x, y);
    }
    return probes;
}

//! clickProbes() of a fresh Text holding `source`, laid out at `width`: the
//! points to click an edit of a scene's Text to `source` at, the same before
//! its layout and after it, found without asking that Text anything.
std::vector<Point> laidOutProbes(int width, const String &source)
{
    WrapScene fresh(width);

    fresh.text->setText(source);
    fresh.frame();
    return clickProbes(fresh);
}

//! The character a click finds at each of `probes`, then every char <-> line
//! helper's answer for every index and row of the string, one line each, so
//! two states can be compared answer by answer. The clicks come first, so
//! between an edit and its layout getClosestCharacter is the helper that has
//! to see the edit, with no other having wrapped it.
std::vector<String> helperAnswers(WrapScene &scene, const std::vector<Point> &probes)
{
    Text *text=scene.text;
    std::vector<String> answers;

    const auto rect=[](const gweni::Rect &r)
    {
        return std::to_string(r.x)+","+std::to_string(r.y)+","+std::to_string(r.w)+","+std::to_string(r.h);
    };

    for(const Point &p:probes)
    {
        answers.push_back("getClosestCharacter("+std::to_string(p.x)+","+std::to_string(p.y)+")="
            +std::to_string(text->getClosestCharacter(p)));
    }

    const int length=text->length();
    const int lines=static_cast<int>(text->numLines());

    answers.push_back("numLines="+std::to_string(lines));

    for(int c=0; c <= length; ++c)
    {
        const String at="("+std::to_string(c)+")=";

        answers.push_back("getLineFromChar"+at+std::to_string(text->getLineFromChar(c)));
        answers.push_back("getCharPosOnLine"+at+std::to_string(text->getCharPosOnLine(c)));
        answers.push_back("getCharacterPosition"+at+rect(text->getCharacterPosition(c)));
    }

    // one row past the last, which the helpers clamp
    for(int i=0; i <= lines; ++i)
    {
        const String at="("+std::to_string(i)+")=";

        answers.push_back("getStartCharFromLine"+at+std::to_string(text->getStartCharFromLine(i)));
        answers.push_back("getEndCharFromLine"+at+std::to_string(text->getEndCharFromLine(i)));
        answers.push_back("getLastCaretOnLine"+at+std::to_string(text->getLastCaretOnLine(i)));
        answers.push_back("getLineLength"+at+std::to_string(text->getLineLength(i)));
        answers.push_back("getLineBox"+at+rect(text->getLineBox(i)));
    }

    return answers;
}

//! The answers given between an edit and its layout against those after it:
//! each that differs fails on its own, the first few named.
void checkSameAnswers(const std::vector<String> &beforeLayout, const std::vector<String> &afterLayout)
{
    REQUIRE(beforeLayout.size() == afterLayout.size());

    int differing=0;

    for(size_t i=0; i < beforeLayout.size(); ++i)
    {
        if(beforeLayout[i] == afterLayout[i])
            continue;

        if(++differing <= 8)
            CHECK(beforeLayout[i] == afterLayout[i]);
    }

    CHECK(differing == 0);
}

//! The string with the characters [at, at+count) replaced by insert.
String edited(const String &source, size_t at, size_t count, const String &insert)
{
    String result=source;

    result.replace(at, count, insert);
    return result;
}

}//namespace

TEST_CASE("Wrapped text: before the first layout the helpers read the string unwrapped")
{
    WrapScene scene(200);
    Text *text=scene.text;
    // the same string, never wrapped, outside the container, which it would
    // widen to its widest paragraph
    Text *plain=scene.canvas->newChild<Text>();

    plain->setText(paragraphs);

    const int length=text->length();
    const int rowHeight=scene.lineHeight();

    // no width yet, so nothing to wrap at: as an unwrapped Text
    CHECK(text->numLines() == 0);
    for(int c=0; c <= length; ++c)
    {
        CAPTURE(c);
        CHECK(text->getLineFromChar(c) == plain->getLineFromChar(c));
        CHECK(text->getCharPosOnLine(c) == plain->getCharPosOnLine(c));
        CHECK((text->getCharacterPosition(c) == plain->getCharacterPosition(c)));
    }
    CHECK(text->getStartCharFromLine(0) == plain->getStartCharFromLine(0));
    CHECK(text->getLastCaretOnLine(0) == plain->getLastCaretOnLine(0));
    CHECK(text->getLineLength(0) == plain->getLineLength(0));
    CHECK((text->getLineBox(0) == plain->getLineBox(0)));

    // clickProbes' points on the string's one row, from the unwrapped Text's
    // carets; a click measures every prefix of the whole string, so only five
    // carets along it are aimed at, and above and below the row only its ends
    const std::vector<int> xs=caretXs(plain, 0, length);
    const int top=plain->getLineBox(0).y;
    const int pastEnd=xs.back()+8;
    std::set<int> along;
    std::vector<Point> probes;

    addRowXs(along, xs, pastEnd, xs.size()/4);
    for(int x:along)
        probes.emplace_back(x, top+rowHeight/2);
    for(int y:{top-rowHeight/2, top, top+rowHeight-1, top+rowHeight+rowHeight/2})
        for(int x:{-4, pastEnd})
            probes.emplace_back(x, y);
    for(const Point &p:probes)
    {
        CAPTURE(p.x);
        CAPTURE(p.y);
        CHECK(text->getClosestCharacter(p) == plain->getClosestCharacter(p));
    }
    CHECK(text->wrapCount() == 0);

    // and laid out, wrapped
    scene.frame();
    CHECK(text->numLines() > 3);
    CHECK(text->wrapCount() == 1);
}

TEST_CASE("Wrapped text: an edit is seen by every helper before it is laid out")
{
    const int width=260;

    // an insert at the start of the first row pushes its last word down a
    // row, and the rows after it; a delete of the start of the second row
    // pulls the third row's first word up
    SUBCASE("insert")
    {
        WrapScene scene(width);
        Text *text=scene.text;

        scene.frame();

        const String insert="Remarkably enough, ";
        const String source=edited(paragraphs, 0, 0, insert);
        const int oldSecond=text->getStartCharFromLine(1);
        const std::vector<Point> probes=laidOutProbes(width, source);

        text->setText(source);

        const std::vector<String> beforeLayout=helperAnswers(scene, probes);

        scene.frame();
        scene.checkLines(width, source);
        // it did reflow: the second row starts before the old one's moved start
        REQUIRE(text->getStartCharFromLine(1) < oldSecond+static_cast<int>(insert.length()));
        checkSameAnswers(beforeLayout, helperAnswers(scene, probes));
    }

    SUBCASE("delete")
    {
        WrapScene scene(width);
        Text *text=scene.text;

        scene.frame();

        const int second=text->getStartCharFromLine(1);
        const int third=text->getStartCharFromLine(2);
        // the second row's first two words
        const size_t firstSpace=paragraphs.find(' ', second);
        const size_t count=paragraphs.find(' ', firstSpace+1)+1-second;
        const String source=edited(paragraphs, second, count, "");
        const std::vector<Point> probes=laidOutProbes(width, source);

        text->setText(source);

        const std::vector<String> beforeLayout=helperAnswers(scene, probes);

        scene.frame();
        scene.checkLines(width, source);
        // it did reflow: the third row starts after the old one's moved start
        REQUIRE(text->getStartCharFromLine(2) > third-static_cast<int>(count));
        checkSameAnswers(beforeLayout, helperAnswers(scene, probes));
    }
}

TEST_CASE("Wrapped text: a click right after an edit finds the edited text's characters")
{
    WrapScene scene(260);
    Text *text=scene.text;

    scene.frame();

    const String source=edited(paragraphs, 0, 0, "Remarkably enough, ");

    text->setText(source);

    // every caret position, found again at its own place, with no layout
    for(int c=0; c <= text->length(); ++c)
    {
        CAPTURE(c);
        const gweni::Rect pos=text->getCharacterPosition(c);

        CHECK(text->getClosestCharacter(Point(pos.x, pos.y+1)) == c);
    }
}

TEST_CASE("Wrapped text: an edit is wrapped once, by a helper or by layout")
{
    WrapScene scene(260);
    Text *text=scene.text;

    scene.frame();

    const size_t laidOut=text->wrapCount();
    const String first=edited(paragraphs, 0, 0, "Remarkably enough, ");
    // every helper is asked, a click included; what they answer is the case
    // above's, so one click does
    const std::vector<Point> click={Point(4, scene.lineHeight()/2)};

    // asked before its layout: the helpers wrap it, layout uses that wrap
    text->setText(first);
    CHECK(text->wrapCount() == laidOut);
    helperAnswers(scene, click);
    helperAnswers(scene, click);
    CHECK(text->wrapCount() == laidOut+1);
    scene.frame();
    CHECK(text->wrapCount() == laidOut+1);
    scene.checkLines(260, first);

    // not asked: layout wraps it
    const String second=edited(first, 0, 0, "Truly ");

    text->setText(second);
    scene.frame();
    CHECK(text->wrapCount() == laidOut+2);
    scene.checkLines(260, second);

    // and a frame with no change wraps nothing
    scene.frame();
    helperAnswers(scene, click);
    CHECK(text->wrapCount() == laidOut+2);
}

namespace
{

//! The keys and markers of the TextBoxMultiline case; with layoutBetween a
//! frame after each marker, as between keystrokes, without it every key in
//! the frame of the edits before it, as GLFW delivers queued events.
void homeEndUpDown(bool layoutBetween)
{
    WrapScene scene(200);
    auto *box=scene.canvas->newChild<gweni::controls::TextBoxMultiline>();

    box->setBounds(10, 220, 200, 120);
    box->setText("one two\n\nthree four");
    scene.frame();

    const int rowHeight=scene.lineHeight();
    const int textY=box->getCharacterPosition(0).y;

    // Each step moves the caret, then types a marker where it landed.
    const auto type=[&](const String &marker, int row, const String &expected)
    {
        CHECK(box->getCurrentLine() == row);
        box->insertText(marker);
        if(layoutBetween)
            scene.frame();
        CHECK(box->getText() == expected);
    };

    box->setCursorPos(10);
    box->setCursorend(10);
    CHECK(box->getCharacterPosition(10).y == textY+2*rowHeight);

    box->onKeyHome(true);
    type("X", 2, "one two\n\nXthree four");

    // onto the blank row: column 1 is past its end, so its start
    box->onKeyUp(true);
    CHECK(box->getCharacterPosition(8).y == textY+rowHeight);
    type("Y", 1, "one two\nY\nXthree four");

    box->onKeyUp(true);
    type("Z", 0, "oZne two\nY\nXthree four");

    // the end of a line is before its '\n'
    box->onKeyend(true);
    type("W", 0, "oZne twoW\nY\nXthree four");

    box->onKeyDown(true);
    type("V", 1, "oZne twoW\nYV\nXthree four");

    box->onKeyDown(true);
    type("U", 2, "oZne twoW\nYV\nXtUhree four");

    box->onKeyend(true);
    type("R", 2, "oZne twoW\nYV\nXtUhree fourR");

    // and Down on the last line, Up on the first, stay there
    box->onKeyDown(true);
    CHECK(box->getCurrentLine() == 2);
    box->onKeyHome(true);
    box->onKeyUp(true);
    box->onKeyUp(true);
    box->onKeyUp(true);
    type("Q", 0, "QoZne twoW\nYV\nXtUhree fourR");
}

}//namespace

TEST_CASE("TextBoxMultiline: Home, End, Up and Down land on the rows of the wrapped text")
{
    homeEndUpDown(true);
}

TEST_CASE("TextBoxMultiline: keys in the frame of an edit land on the rows of the edited text")
{
    homeEndUpDown(false);
}

namespace
{

//! The wrap the helpers read, one "start+length" per line, to compare a wrap
//! before its layout with a fresh one.
std::vector<String> wrapData(Text *text)
{
    std::vector<String> lines;

    for(unsigned i=0; i < text->numLines(); ++i)
    {
        const int line=static_cast<int>(i);

        lines.push_back(std::to_string(text->getStartCharFromLine(line))+"+"
            +std::to_string(text->getLineLength(line)));
    }
    return lines;
}

//! The scene's Text, edited to `source` (and given a helper's question
//! before its layout, when asked), against a fresh Text holding it: the same
//! wrap before the layout and after, and the same line children after it, in
//! the lines' order among the Text's children.
void checkAgainstFresh(WrapScene &scene, int width, const String &source, bool askFirst)
{
    scene.text->setText(source);
    if(askFirst)
        scene.text->getCharacterPosition(static_cast<unsigned>(source.size()));

    WrapScene fresh(width);

    fresh.text->setText(source);
    fresh.frame();

    CHECK(wrapData(scene.text) == wrapData(fresh.text));
    scene.frame();
    CHECK(wrapData(scene.text) == wrapData(fresh.text));
    CHECK(lineStates(scene.text) == lineStates(fresh.text));

    std::vector<gweni::controls::Base *> order;

    for(Text *line : scene.lines())
        order.push_back(line);
    CHECK(std::vector<gweni::controls::Base *>(scene.text->getChildren().begin(),
        scene.text->getChildren().end()) == order);
}

}//namespace

TEST_CASE("Wrapped text: each kind of edit matches a fresh wrap of the edited string")
{
    const int width=200;
    WrapScene scene(width);
    String source=paragraphs;

    scene.frame();

    const size_t firstBreak=source.find('\n');

    SUBCASE("a '\\n' deleted joins two paragraphs, typed again splits them")
    {
        source.erase(firstBreak, 1);
        checkAgainstFresh(scene, width, source, false);
        source.insert(firstBreak, "\n");
        checkAgainstFresh(scene, width, source, true);
    }

    SUBCASE("a '\\n' typed inside a paragraph splits it")
    {
        source.insert(source.find("Pack"), "\n");
        checkAgainstFresh(scene, width, source, true);
    }

    SUBCASE("at the start and at the end")
    {
        source.insert(0, "Remarkably enough, ");
        checkAgainstFresh(scene, width, source, false);
        source+=" And then some more words at the end.";
        checkAgainstFresh(scene, width, source, true);
        source+="\n";
        checkAgainstFresh(scene, width, source, false);
        source.erase(0, 11);
        checkAgainstFresh(scene, width, source, true);
    }

    SUBCASE("multi-byte UTF-8")
    {
        source.insert(source.find("fox"), "\xC3\xA9t\xC3\xA9 \xE2\x82\xAC\xF0\x9F\x98\x80 ");
        checkAgainstFresh(scene, width, source, true);
        source.erase(source.find("\xE2\x82\xAC"), 3);
        checkAgainstFresh(scene, width, source, false);
    }

    SUBCASE("several lines pasted")
    {
        source.insert(source.find("How"), "First pasted line.\nSecond pasted line, longer than "
            "the width it is wrapped at.\n\nAfter a blank one.\n");
        checkAgainstFresh(scene, width, source, true);
    }

    SUBCASE("two edits before a wrap")
    {
        String first=source;

        first.insert(source.find("Sphinx"), "Then ");
        scene.text->setText(first);
        source=first;
        source.insert(4, "very ");
        checkAgainstFresh(scene, width, source, false);
    }

    SUBCASE("a new font wraps it whole")
    {
        gweni::Font bigger=scene.text->getFont();

        bigger.size+=4;
        source.insert(0, "Big ");
        scene.text->setText(source);
        scene.text->setFont(bigger);
        scene.frame();

        WrapScene fresh(width);

        fresh.text->setFont(bigger);
        fresh.text->setText(source);
        fresh.frame();

        CHECK(lineStates(scene.text) == lineStates(fresh.text));
        // the font outlives the canvases' last frames
        scene.text->setFont(gweni::test::openSans());
        fresh.text->setFont(gweni::test::openSans());
        scene.frame();
        fresh.frame();
    }
}

TEST_CASE("Wrapped text: an edit in one paragraph keeps the other lines and writes no glyph of theirs")
{
    const int width=200;
    WrapScene scene(width);

    scene.frame();
    scene.frame();

    const std::vector<Text *> before=scene.lines();
    const size_t at=paragraphs.find("Sphinx");
    const int firstEdited=scene.text->getLineFromChar(static_cast<int>(at));
    String source=paragraphs;

    // a word into the second paragraph's first line
    source.insert(at+7, "big ");
    scene.text->setText(source);
    scene.frame();
    scene.checkLines(width, source);

    const std::vector<Text *> after=scene.lines();

    REQUIRE(after.size() >= before.size());
    for(int i=0; i < firstEdited; ++i)
    {
        CAPTURE(i);
        CHECK(after[i] == before[i]);
    }

    // only the lines of the edited paragraph were written
    size_t editedGlyphs=0;

    for(size_t i=firstEdited; i < after.size(); ++i)
        editedGlyphs+=after[i]->getText().size();
    CHECK(gweni::test::gpuRenderer().getFrameStats().textSlotsWritten <= editedGlyphs);
}

TEST_CASE("Wrapped text: random edits on the real renderer match a fresh wrap")
{
    const int width=180;
    WrapScene scene(width);
    String source=paragraphs;
    // a fixed sequence, the same on every run
    uint32_t seed=12345u;
    const auto below=[&](size_t n)
    {
        seed=seed*1664525u+1013904223u;
        return n == 0 ? size_t(0) : static_cast<size_t>((seed >> 8)%n);
    };
    const char *const inserts[]={"a", "word ", " ", "\n", "\xC3\xA9", "\xF0\x9F\x98\x80 ",
        "two\nlines ", "averyveryverylongwordindeed "};

    scene.frame();

    for(int step=0; step < 40; ++step)
    {
        CAPTURE(step);

        const size_t at=gweni::utf8Snap(source, below(source.size()+1));

        if(below(3) == 0)
        {
            size_t end=at;

            for(size_t n=1+below(10); n > 0 && end < source.size(); --n)
                end=gweni::utf8Next(source, end);
            source.erase(at, end-at);
        }
        else
        {
            source.insert(at, inserts[below(sizeof(inserts)/sizeof(inserts[0]))]);
        }

        CAPTURE(source);
        checkAgainstFresh(scene, width, source, below(2) == 0);
    }
}
