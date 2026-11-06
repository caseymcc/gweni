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
 *  deleted.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/text.h>
#include <gweni/controls/textBox.h>
#include <gweni/primitiveIds.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <memory>
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

//! A plain greedy wrap of `text` at `width`, the oracle for the line count:
//! lines break at '\n' and before a word (with its trailing space) that would
//! take the line past the width. No word here is wider than the widths used.
std::vector<String> greedyWrap(const String &text, int width)
{
    gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
    const gweni::Font &font=gweni::getDefaultFont();
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
        gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();
        const Point viewSize=gweni::test::viewSize();

        // No setDefaultFont here: the skin frees its default font when it is
        // deleted, which would free the shared test::openSans() with it.
        skin=std::make_unique<gweni::skin::textured::TexturedSkin>(&renderer);
        REQUIRE(skin->init("DefaultSkin.json"));

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

        gweni::renderer::OpenGLCore &renderer=gweni::test::gpuRenderer();

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
        const std::vector<String> expected=greedyWrap(source, width);
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

TEST_CASE("TextBoxMultiline: Home, End, Up and Down land on the rows of the wrapped text")
{
    WrapScene scene(200);
    auto *box=scene.canvas->newChild<gweni::controls::TextBoxMultiline>();

    box->setBounds(10, 220, 200, 120);
    box->setText("one two\n\nthree four");
    scene.frame();

    const int rowHeight=scene.lineHeight();
    const int textY=box->getCharacterPosition(0).y;

    // Each step moves the caret, then types a marker where it landed. The
    // edit is laid out (re-wrapped) by the next frame, as between keystrokes.
    const auto type=[&](const String &marker, int row, const String &expected)
    {
        CHECK(box->getCurrentLine() == row);
        box->insertText(marker);
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
