/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  An edit of a wrapped internal::Text is wrapped again paragraph by
 *  paragraph, on the StubRenderer: setText finds the bytes that changed, the
 *  next wrap re-wraps only the paragraphs they touch and splices their lines
 *  in, and layout re-syncs only the line children of those lines. These cases
 *  pin that the result is always a fresh wrap of the final string, and that it
 *  costs the paragraph, not the document.
 *
 *  - A seeded random run of a few hundred inserts, deletes, replacements and
 *    pastes (words, spaces, '\n's, 2-, 3- and 4-byte codepoints, several lines
 *    at once, at the start, the end and anywhere between), with a layout, a
 *    helper or nothing between them, and two edits with no wrap between:
 *    after each step the wrap the helpers read (line count, every line's start
 *    and length) is that of a fresh Text holding the final string, and after
 *    each layout so are the line children (strings, bounds), in the lines'
 *    order among the Text's children, one child per line.
 *  - An edit inside one paragraph marks, and re-measures, only that
 *    paragraph's changed line children: every other child keeps its pointer
 *    and has no change pending.
 *  - An edit that adds a line to a paragraph keeps every child below it,
 *    moved down a row, and one that takes a line away moves them up; joining
 *    two paragraphs (a '\n' deleted) and splitting one (a '\n' typed) too.
 *  - One keystroke into the middle of a long TextBoxMultiline measures the
 *    same number of times, wrap and layout together, at two document lengths.
 *  - A new width still wraps the whole string again.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/utility.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace gweni;
using controls::internal::Text;

namespace
{

//! A canvas holding a container of the given width with one wrapped Text.
struct WrapScene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *container;
    Text *text;

    explicit WrapScene(int width, const String &source)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        container=canvas->newChild<controls::Base>();
        container->setBounds(0, 0, width, 500);

        text=container->newChild<Text>();
        text->setWrap(true);
        text->setText(source);
    }

    ~WrapScene()
    {
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }
};

//! The wrap as the helpers read it: one "start+length" per line.
std::vector<std::string> wrapData(Text *text)
{
    std::vector<std::string> lines;

    for(unsigned i=0; i < text->numLines(); ++i)
    {
        const int line=static_cast<int>(i);

        lines.push_back(std::to_string(text->getStartCharFromLine(line))+"+"
            +std::to_string(text->getLineLength(line)));
    }
    return lines;
}

//! The line children as layout left them: string and bounds, top to bottom.
std::vector<std::string> childData(Text *text)
{
    std::vector<std::string> lines;

    for(unsigned i=0; i < text->numLines(); ++i)
    {
        Text *line=text->getLine(static_cast<int>(i));

        if(!line)
        {
            lines.push_back("(no child)");
            continue;
        }

        const Rect &b=line->getBounds();

        lines.push_back("'"+line->getText()+"' "+std::to_string(b.x)+","+std::to_string(b.y)+","
            +std::to_string(b.w)+","+std::to_string(b.h));
    }
    return lines;
}

//! A fresh Text holding source at width, laid out once.
struct Fresh
{
    Fresh(int width, const String &source): scene(width, source)
    {
        scene.frame();
        wrap=wrapData(scene.text);
        children=childData(scene.text);
    }

    WrapScene scene;
    std::vector<std::string> wrap;
    std::vector<std::string> children;
};

//! The children are the lines, one each, in the lines' order.
void checkChildOrder(Text *text)
{
    const controls::Base::List &children=text->getChildren();

    REQUIRE(children.size() == text->numLines());

    unsigned i=0;

    for(controls::Base *child : children)
    {
        CAPTURE(i);
        CHECK(child == text->getLine(static_cast<int>(i)));
        ++i;
    }
}

//! A deterministic generator: the same edits on every platform.
struct Random
{
    std::mt19937 engine{20261108u};

    size_t below(size_t n)
    {
        return n == 0 ? 0 : static_cast<size_t>(engine()%n);
    }
};

const char *const snippets[]=
{
    "a", "word ", " ", "\n", "\n\n", "x y z ", "\xC3\xA9", "\xE2\x82\xAC ",
    "\xF0\x9F\x98\x80", "supercalifragilisticexpialidocious ",
    "line one\nline two\n", "pasted paragraph of several words\nand a second one",
    "  ", "end.\n"
};

}//namespace

TEST_CASE("Wrapped text edits: random edits always give a fresh wrap of the final string")
{
    const int width=120;
    String source="The quick brown fox jumps over the lazy dog.\n\nPack my box with five dozen "
        "liquor jugs.\nHow vexingly quick daft zebras jump!";
    WrapScene scene(width, source);
    Random random;

    scene.frame();

    const size_t snippetCount=sizeof(snippets)/sizeof(snippets[0]);

    for(int step=0; step < 400; ++step)
    {
        CAPTURE(step);

        // one or two edits before anything wraps them
        const int edits=random.below(5) == 0 ? 2 : 1;

        for(int e=0; e < edits; ++e)
        {
            const size_t length=source.size();
            // a codepoint boundary, the start and the end more often
            size_t at=random.below(4) == 0 ? (random.below(2) ? 0 : length) : random.below(length+1);

            at=utf8Snap(source, at);

            switch(random.below(4))
            {
            case 0: // delete up to 12 codepoints
            case 1:
            {
                size_t end=at;

                for(size_t n=1+random.below(12); n > 0 && end < source.size(); --n)
                    end=utf8Next(source, end);
                source.erase(at, end-at);
                break;
            }
            case 2: // replace
            {
                const size_t end=utf8Next(source, std::min(at, source.size()));

                source.replace(at, std::min(end, source.size())-at, snippets[random.below(snippetCount)]);
                break;
            }
            default: // insert
                source.insert(at, snippets[random.below(snippetCount)]);
                break;
            }

            // keep it from growing without bound
            if(source.size() > 600)
                source.erase(utf8Snap(source, 300), String::npos);

            scene.text->setText(source);
        }

        CAPTURE(source);

        // then a layout, a helper wrapping it first, or nothing yet
        const size_t next=random.below(3);

        if(next == 1)
            scene.text->getCharacterPosition(static_cast<unsigned>(source.size()/2));

        if(next != 2)
        {
            Fresh fresh(width, source);

            CHECK(wrapData(scene.text) == fresh.wrap);

            scene.frame();
            CHECK(wrapData(scene.text) == fresh.wrap);
            CHECK(childData(scene.text) == fresh.children);
            checkChildOrder(scene.text);
        }
    }

    scene.frame();

    Fresh fresh(width, source);

    CHECK(wrapData(scene.text) == fresh.wrap);
    CHECK(childData(scene.text) == fresh.children);
}

namespace
{

//! Paragraphs of a few short words each, wrapped to two or more lines.
String document(int paragraphs)
{
    String text;

    for(int p=0; p < paragraphs; ++p)
    {
        if(p > 0)
            text+='\n';
        text+="alpha beta gamma delta epsilon zeta eta theta";
    }
    return text;
}

const int docWidth=100;

}//namespace

TEST_CASE("Wrapped text edits: an edit inside a paragraph re-syncs that paragraph's lines only")
{
    String source=document(10);
    WrapScene scene(docWidth, source);

    scene.frame();

    Text *text=scene.text;
    const unsigned lines=text->numLines();
    std::vector<Text *> before;

    for(unsigned i=0; i < lines; ++i)
        before.push_back(text->getLine(static_cast<int>(i)));

    // a letter into the second word of the fifth paragraph: its first line
    size_t at=0;

    for(int p=0; p < 4; ++p)
        at=source.find('\n', at)+1;

    const int firstLine=text->getLineFromChar(static_cast<int>(at));
    const int lastLine=text->getLineFromChar(static_cast<int>(source.find('\n', at)));

    REQUIRE(lastLine > firstLine);

    source.insert(at+7, "x");
    text->setText(source);

    const size_t measures=controls::measureTextCalls();

    scene.canvas->updateCanvas();

    REQUIRE(text->numLines() == lines);
    for(unsigned i=0; i < lines; ++i)
    {
        CAPTURE(i);
        Text *line=text->getLine(static_cast<int>(i));

        CHECK(line == before[i]);
        if(static_cast<int>(i) < firstLine || static_cast<int>(i) > lastLine)
            CHECK(line->getStateChange() == controls::StateChange_Nothing);
    }
    CHECK(text->getLine(firstLine)->getStateChange() != controls::StateChange_Nothing);

    // the wrap and layout of one paragraph: its wrap measures every prefix of
    // each word and each word on its line, then each changed line is
    // measured; a wrap of the whole document would be ten times that
    const size_t paragraphLength=source.find('\n', at)-at;
    const size_t measured=controls::measureTextCalls()-measures;

    CHECK(measured > 0);
    CHECK(measured <= 2*paragraphLength);

    scene.canvas->renderCanvas();

    Fresh fresh(docWidth, source);

    CHECK(childData(text) == fresh.children);
}

TEST_CASE("Wrapped text edits: a line added or removed keeps the children below and moves them")
{
    String source=document(6);
    WrapScene scene(docWidth, source);

    scene.frame();

    Text *text=scene.text;

    const auto children=[&]()
    {
        std::vector<Text *> result;

        for(unsigned i=0; i < text->numLines(); ++i)
            result.push_back(text->getLine(static_cast<int>(i)));
        return result;
    };

    const auto check=[&](const std::vector<Text *> &before, int belowOld, int belowNew)
    {
        const std::vector<Text *> after=children();

        Fresh fresh(docWidth, source);

        CHECK(childData(text) == fresh.children);
        checkChildOrder(text);

        // the same children from the first line below the edit on
        REQUIRE(after.size()-belowNew == before.size()-belowOld);
        for(size_t i=0; i+belowNew < after.size(); ++i)
        {
            CAPTURE(i);
            CHECK(after[belowNew+i] == before[belowOld+i]);
        }
    };

    // the third paragraph's start, and the line after its last
    size_t third=0;

    for(int p=0; p < 2; ++p)
        third=source.find('\n', third)+1;

    const size_t thirdEnd=source.find('\n', third);

    SUBCASE("a line more")
    {
        const std::vector<Text *> before=children();
        const int below=text->getLineFromChar(static_cast<int>(thirdEnd))+1;

        source.insert(thirdEnd, " iota kappa lambda mu nu xi omicron");
        text->setText(source);
        scene.frame();

        const int belowNew=text->getLineFromChar(static_cast<int>(source.find('\n', third)))+1;

        REQUIRE(belowNew > below);
        check(before, below, belowNew);
    }

    SUBCASE("a line fewer")
    {
        const std::vector<Text *> before=children();
        const int below=text->getLineFromChar(static_cast<int>(thirdEnd))+1;

        source.erase(third, thirdEnd-third-5);
        text->setText(source);
        scene.frame();

        const int belowNew=text->getLineFromChar(static_cast<int>(source.find('\n', third)))+1;

        REQUIRE(belowNew < below);
        check(before, below, belowNew);
    }

    SUBCASE("two paragraphs joined, then split again")
    {
        const std::vector<Text *> before=children();
        // the fourth paragraph's end
        const size_t fourthEnd=source.find('\n', thirdEnd+1);
        const int below=text->getLineFromChar(static_cast<int>(fourthEnd))+1;

        source.erase(thirdEnd, 1);
        text->setText(source);
        scene.frame();

        const int belowJoined=text->getLineFromChar(static_cast<int>(fourthEnd-1))+1;

        check(before, below, belowJoined);

        source.insert(thirdEnd, "\n");
        text->setText(source);
        scene.frame();
        check(before, below, below);
    }
}

namespace
{

//! Measures made by typing one letter into the middle paragraph of a laid-out
//! TextBoxMultiline of the given number of paragraphs, and laying it out.
size_t measuresForKeystroke(int paragraphs)
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);

    canvas->setSize(800, 600);

    controls::TextBoxMultiline *box=canvas->newChild<controls::TextBoxMultiline>();
    const String source=document(paragraphs);

    box->setBounds(0, 0, 200, 500);
    box->setText(source);
    canvas->updateCanvas();
    canvas->renderCanvas();

    size_t at=0;

    for(int p=0; p < paragraphs/2; ++p)
        at=source.find('\n', at)+1;

    box->setCursorPos(static_cast<int>(at+3));
    box->setCursorend(static_cast<int>(at+3));
    canvas->updateCanvas();
    canvas->renderCanvas();

    const size_t measures=controls::measureTextCalls();

    box->insertText("q");
    canvas->updateCanvas();
    canvas->renderCanvas();

    const size_t measured=controls::measureTextCalls()-measures;

    delete canvas;
    return measured;
}

}//namespace

TEST_CASE("Wrapped text edits: one keystroke measures the same in a long document as in a short one")
{
    const size_t small=measuresForKeystroke(20);
    const size_t large=measuresForKeystroke(400);

    CHECK(small > 0);
    CHECK(large == small);
}

TEST_CASE("Wrapped text edits: a new width wraps the whole string again")
{
    String source=document(8);
    WrapScene scene(docWidth, source);

    scene.frame();
    source.insert(5, "x");
    scene.text->setText(source);
    scene.container->setWidth(docWidth+60);
    scene.frame();

    Fresh fresh(docWidth+60, source);

    CHECK(wrapData(scene.text) == fresh.wrap);
    CHECK(childData(scene.text) == fresh.children);
    checkChildOrder(scene.text);
}
