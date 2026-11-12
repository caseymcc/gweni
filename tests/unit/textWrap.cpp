/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::wrapLines and text::splitWords, the line breaking a wrapped
 *  internal::Text does, as a pure function of the string, the width and a
 *  width measure. With a measure of 10 units a byte: words keep their
 *  trailing space, a line breaks before the word that would take it past the
 *  width, '\n' always breaks one (held, not shown), a blank line and the line
 *  after a final '\n' are empty lines, a word wider than the width is broken
 *  between codepoints (never inside a UTF-8 sequence), a codepoint wider
 *  than the width is a piece of its own, and with no width the lines break
 *  only at '\n' and nothing is measured. Then, on the StubRenderer, that
 *  Base::wrappedTextLines() and wrappedTextHeight(), the row measurer's way
 *  in, agree with a wrapped Text laid out in a parent of that width, line
 *  for line and in its height, over strings and widths, measuring no more
 *  than the Text's layout does.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/text/wrap.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::internal::Text;

namespace
{

//! The lines, each as what it shows and how many bytes it holds.
std::vector<std::string> wrapped(const String &s, int w, int *measures=nullptr)
{
    std::vector<text::WrappedLine> lines;
    int count=0;

    text::wrapLines(s, w, [&count](const String &str) { ++count; return 10*static_cast<int>(str.size()); }, lines);
    if(measures)
        *measures=count;

    std::vector<std::string> out;

    for(const text::WrappedLine &line:lines)
        out.push_back("'"+line.text+"'"+std::to_string(line.length));
    return out;
}

using Lines=std::vector<std::string>;

}//namespace

TEST_CASE("Text wrap: lines break before the word that would not fit")
{
    // "aa bb cc" at 60 units (6 bytes): "aa bb " is 60, "aa bb cc" 80
    CHECK(wrapped("aa bb cc", 60) == Lines{"'aa bb '6", "'cc'2"});
    CHECK(wrapped("aa bb cc", 80) == Lines{"'aa bb cc'8"});
    CHECK(wrapped("aa bb cc", 30) == Lines{"'aa '3", "'bb '3", "'cc'2"});
    // an empty string is one empty line
    CHECK(wrapped("", 50) == Lines{"''0"});
}

TEST_CASE("Text wrap: a newline ends a line it holds but does not show")
{
    CHECK(wrapped("ab\ncd", 100) == Lines{"'ab'3", "'cd'2"});
    // a blank line, and the empty line after a final newline
    CHECK(wrapped("ab\n\ncd\n", 100) == Lines{"'ab'3", "''1", "'cd'3", "''0"});
}

TEST_CASE("Text wrap: a word wider than the line is broken between codepoints")
{
    CHECK(wrapped("abcdefgh", 30) == Lines{"'abc'3", "'def'3", "'gh'2"});
    // "é" is two bytes (20 units): never split, and alone wider than 10 it is
    // a piece of its own
    CHECK(wrapped("a\xC3\xA9" "b", 30) == Lines{"'a\xC3\xA9'3", "'b'1"});
    CHECK(wrapped("\xC3\xA9\xC3\xA9", 10) == Lines{"'\xC3\xA9'2", "'\xC3\xA9'2"});
}

TEST_CASE("Text wrap: with no width the lines break only at newlines and nothing is measured")
{
    int measures=-1;

    CHECK(wrapped("a long line of words\nand another", 0, &measures)
        == Lines{"'a long line of words'21", "'and another'11"});
    CHECK(measures == 0);

    std::vector<String> words;

    text::splitWords("one two\nthree", 0, nullptr, words);
    CHECK(words == std::vector<String>{"one ", "two", "\n", "three"});
}

TEST_CASE("Text wrap: Base::wrappedTextHeight agrees with a wrapped Text laid out at that width")
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=new controls::Canvas(&skin);

    canvas->setSize(1200, 800);

    const String strings[]={
        "",
        "short",
        "The quick brown fox jumps over the lazy dog, again and again.",
        "Supercalifragilisticexpialidocious and a few more words after it",
        "two\nparagraphs, the second of them a little longer than the first",
        "na\xC3\xAFve caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9 \xE2\x80\x94 \xF0\x9F\x99\x82 trailing words\n",
    };
    const int widths[]={7, 40, 97, 150, 333, 1000};

    for(const String &s:strings)
    {
        for(int width:widths)
        {
            CAPTURE(s);
            CAPTURE(width);

            controls::Base *container=canvas->newChild<controls::Base>();

            container->setBounds(0, 0, width, 700);

            Text *text=container->newChild<Text>();

            text->setWrap(true);
            text->setText(s);

            size_t before=renderer.measures;

            canvas->updateCanvas();
            canvas->renderCanvas();

            size_t textMeasures=renderer.measures-before;

            before=renderer.measures;

            const int lines=canvas->wrappedTextLines(text->getFont(), s, width);

            const size_t wrapMeasures=renderer.measures-before;

            CHECK(lines == static_cast<int>(text->numLines()));
            CHECK(canvas->wrappedTextHeight(text->getFont(), s, width) == text->getHeight());
            // the Text's layout measured its wrap, its line height and its
            // longest word (no width was asked of its parent), no fewer
            CHECK(wrapMeasures <= textMeasures);

            delete container;
        }
    }
    delete canvas;
}
