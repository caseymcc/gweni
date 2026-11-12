/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::Document against a naive model, the whole text as one string, over
 *  seeded random sequences of inserts, deletes and replacements, single- and
 *  multi-line ("\r\n" in the inserted text included): after every edit the
 *  text, the line count and each line are the model's, the record's removed
 *  text is what the model's range held, its textEnd is where the model's
 *  inserted text ends, the observer's LineChange covers exactly the lines
 *  that changed (those before it untouched, those after it the old ones
 *  moved), and applying the records' inverses in reverse order gives every
 *  earlier text back down to the first. Also the edge cases by hand:
 *  setText()'s split, "\r\n", an empty text being one empty line, clamping
 *  and a reversed range, textIn(), and observers added and removed.
 */
#include <gweni/text/document.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <random>
#include <vector>

using namespace gweni;
using text::Document;
using text::Edit;
using text::LineChange;
using text::Position;
using text::Range;

namespace
{

//! The Position of byte \p offset of \p model.
Position positionOf(const String &model, size_t offset)
{
    Position position;
    size_t lineStart=0;

    for(size_t i=0; i < offset; ++i)
    {
        if(model[i] == '\n')
        {
            ++position.line;
            lineStart=i+1;
        }
    }
    position.byte=offset-lineStart;
    return position;
}

std::vector<String> linesOf(const Document &document)
{
    std::vector<String> lines;

    for(size_t i=0; i < document.lineCount(); ++i)
        lines.push_back(document.line(i));
    return lines;
}

//! The pieces the random text is made of: letters, a space, a two-byte
//! codepoint, newlines and a "\r\n" the document reads as one newline.
const char *const Pieces[]={"a", "b", "xyz", " ", "\xc3\xa9", "\n", "\n", "\r\n", "\n\n"};

String randomText(std::mt19937 &random, size_t maxPieces)
{
    String text;
    size_t count=random()%(maxPieces+1);

    for(size_t i=0; i < count; ++i)
        text+=Pieces[random()%(sizeof(Pieces)/sizeof(Pieces[0]))];
    return text;
}

String normalized(const String &text)
{
    String result;

    for(size_t i=0; i < text.size(); ++i)
    {
        if(text[i] == '\r' && i+1 < text.size() && text[i+1] == '\n')
            continue;
        result.push_back(text[i]);
    }
    return result;
}

void checkDocument(const Document &document, const String &model)
{
    REQUIRE(document.text() == model);

    size_t newlines=0;
    for(char c:model)
        newlines+=(c == '\n');
    REQUIRE(document.lineCount() == newlines+1);

    size_t start=0;
    for(size_t i=0; i < document.lineCount(); ++i)
    {
        size_t end=model.find('\n', start);
        if(end == String::npos)
            end=model.size();
        REQUIRE(document.line(i) == model.substr(start, end-start));
        start=end+1;
    }
}

}//namespace

TEST_CASE("text::Document: random edits match a single-string model, and the inverses give every text back")
{
    for(uint32_t seed=1; seed <= 40; ++seed)
    {
        CAPTURE(seed);
        std::mt19937 random(seed);

        String model=normalized(randomText(random, 30));
        Document document(model);
        checkDocument(document, model);

        LineChange observed;
        size_t calls=0;
        document.addObserver([&](const LineChange &change) { observed=change; ++calls; });

        std::vector<Edit> edits;
        std::vector<String> history{model};

        for(int step=0; step < 200; ++step)
        {
            CAPTURE(step);

            //an insert, a delete or a replacement, at any two offsets
            size_t a=random()%(model.size()+1);
            size_t b=a;
            int kind=random()%3;

            if(kind != 0)
                b=random()%(model.size()+1);

            String inserted=(kind == 1)?String():randomText(random, 6);
            size_t from=std::min(a, b);
            size_t to=std::max(a, b);
            std::vector<String> before=linesOf(document);

            Edit edit;
            //given in either order: apply() swaps them
            edit.range={positionOf(model, a), positionOf(model, b)};
            edit.text=inserted;
            calls=0;

            LineChange change=document.apply(edit);
            String normal=normalized(inserted);

            REQUIRE(edit.range.begin == positionOf(model, from));
            REQUIRE(edit.range.end == positionOf(model, to));
            REQUIRE(edit.removed == model.substr(from, to-from));
            REQUIRE(edit.text == normal);

            model.replace(from, to-from, normal);
            checkDocument(document, model);
            REQUIRE(edit.textEnd == positionOf(model, from+normal.size()));

            //the observer was told once, of the change apply() returned
            REQUIRE(calls == 1);
            REQUIRE(observed.first == change.first);
            REQUIRE(observed.removed == change.removed);
            REQUIRE(observed.inserted == change.inserted);

            //the lines it names are the edited ones, the rest kept
            std::vector<String> after=linesOf(document);
            REQUIRE(change.first == edit.range.begin.line);
            REQUIRE(change.removed == edit.range.end.line-edit.range.begin.line+1);
            REQUIRE(change.inserted == edit.textEnd.line-edit.range.begin.line+1);
            REQUIRE(after.size() == before.size()-change.removed+change.inserted);
            for(size_t i=0; i < change.first; ++i)
                REQUIRE(after[i] == before[i]);
            for(size_t i=change.first+change.removed; i < before.size(); ++i)
                REQUIRE(after[i-change.removed+change.inserted] == before[i]);

            edits.push_back(edit);
            history.push_back(model);
        }

        //every edit taken back, newest first
        for(size_t i=edits.size(); i-- > 0;)
        {
            Edit inverse=edits[i].inverse();
            document.apply(inverse);
            REQUIRE(inverse.removed == edits[i].text);
            REQUIRE(inverse.textEnd == edits[i].range.end);
            checkDocument(document, history[i]);
        }
    }
}

TEST_CASE("text::Document: setText splits on newlines, reads \\r\\n as \\n, and an empty text is one empty line")
{
    Document document;
    REQUIRE(document.lineCount() == 1);
    REQUIRE(document.line(0).empty());
    REQUIRE(document.text().empty());
    REQUIRE(document.endPosition() == Position{0, 0});

    LineChange observed;
    document.addObserver([&](const LineChange &change) { observed=change; });

    document.setText("one\r\ntwo\n\nfour\r");
    REQUIRE(document.lineCount() == 4);
    REQUIRE(document.line(0) == "one");
    REQUIRE(document.line(1) == "two");
    REQUIRE(document.line(2).empty());
    //a lone '\r' is kept
    REQUIRE(document.line(3) == "four\r");
    REQUIRE(document.text() == "one\ntwo\n\nfour\r");
    REQUIRE(observed.first == 0);
    REQUIRE(observed.removed == 1);
    REQUIRE(observed.inserted == 4);

    document.setText("tail\n");
    REQUIRE(document.lineCount() == 2);
    REQUIRE(document.line(1).empty());
    REQUIRE(observed.removed == 4);
    REQUIRE(observed.inserted == 2);
}

TEST_CASE("text::Document: positions are clamped, a reversed range is swapped, and textIn joins lines")
{
    Document document("alpha\nbeta\ngamma");

    REQUIRE(document.clamp({1, 99}) == Position{1, 4});
    REQUIRE(document.clamp({7, 2}) == Position{2, 5});
    REQUIRE(document.textIn({{0, 3}, {2, 2}}) == "ha\nbeta\nga");
    REQUIRE(document.textIn({{2, 2}, {0, 3}}) == "ha\nbeta\nga");
    REQUIRE(document.textIn({{1, 1}, {1, 3}}) == "et");

    Edit edit=document.replace({{1, 99}, {0, 2}}, "X");
    REQUIRE(edit.range.begin == Position{0, 2});
    REQUIRE(edit.range.end == Position{1, 4});
    REQUIRE(edit.removed == "pha\nbeta");
    REQUIRE(edit.textEnd == Position{0, 3});
    REQUIRE(document.text() == "alX\ngamma");

    //an insert past the end lands at the end
    edit=document.insert({9, 9}, "\r\n!");
    REQUIRE(edit.range.begin == Position{1, 5});
    REQUIRE(edit.text == "\n!");
    REQUIRE(edit.textEnd == Position{2, 1});
    REQUIRE(document.text() == "alX\ngamma\n!");

    edit=document.erase({{0, 0}, document.endPosition()});
    REQUIRE(edit.removed == "alX\ngamma\n!");
    REQUIRE(document.lineCount() == 1);
    REQUIRE(document.text().empty());
}

TEST_CASE("text::Document: observers are told in order until removed")
{
    Document document("a\nb");
    std::vector<int> calls;

    size_t first=document.addObserver([&](const LineChange &) { calls.push_back(1); });
    size_t second=document.addObserver([&](const LineChange &) { calls.push_back(2); });
    REQUIRE(first != second);

    document.insert({0, 1}, "x");
    REQUIRE(calls == std::vector<int>{1, 2});

    document.removeObserver(first);
    document.insert({0, 1}, "y\nz");
    REQUIRE(calls == std::vector<int>{1, 2, 2});

    document.removeObserver(second);
    document.setText("");
    REQUIRE(calls.size() == 3);
}
