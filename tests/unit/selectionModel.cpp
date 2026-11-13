/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::SelectionModel and text::wordAt, headless. It checks:
 *  - positions order by block path (a block before the blocks inside it),
 *    then by byte;
 *  - start() and end() are the earlier and later end whichever way it was
 *    made, and range() gives the start block its bytes from the start, the
 *    end block its bytes to the end, a block between them whole, one outside
 *    them (or an empty selection) none, each end clamped to the block's
 *    length;
 *  - serial() moves on a change of either end and not on a call that changes
 *    nothing;
 *  - wordAt gives the word, blank run or punctuation run a byte is in, the
 *    run before it at the end or before a '\n', a '\n' alone, and keeps a
 *    combining mark and non-ASCII letters in their word.
 */
#include <gweni/text/selectionModel.h>

#include <doctest/doctest.h>

#include <utility>

using namespace gweni;
using text::SelectionModel;
using text::SelectionPosition;

TEST_CASE("SelectionModel: positions order by block path, then by byte")
{
    CHECK(SelectionPosition(3) < SelectionPosition(4));
    CHECK(SelectionPosition({ 1 }, 9) < SelectionPosition({ 1, 0 }, 0));
    CHECK(SelectionPosition({ 1, 0 }, 9) < SelectionPosition({ 2 }, 0));
    CHECK(SelectionPosition({ 0, 5 }, 0) < SelectionPosition({ 1 }, 0));
    CHECK_FALSE(SelectionPosition({ 1 }, 2) < SelectionPosition({ 1 }, 2));
    CHECK(SelectionPosition({ 1 }, 2) <= SelectionPosition({ 1 }, 2));
    CHECK(SelectionPosition({ 1 }, 2) != SelectionPosition({ 2 }, 2));
}

TEST_CASE("SelectionModel: range gives each block its part, a block between the ends whole")
{
    SelectionModel model;
    std::pair<size_t, size_t> bytes;

    CHECK(model.empty());
    CHECK_FALSE(model.range({}, 10, bytes));

    // made backwards: start and end are still in reading order
    model.select(SelectionPosition({ 2 }, 4), SelectionPosition({ 0 }, 3));
    CHECK(model.start() == SelectionPosition({ 0 }, 3));
    CHECK(model.end() == SelectionPosition({ 2 }, 4));
    CHECK(model.anchor() == SelectionPosition({ 2 }, 4));

    REQUIRE(model.range({ 0 }, 10, bytes));
    CHECK(bytes == std::make_pair<size_t, size_t>(3, 10));
    REQUIRE(model.range({ 1 }, 6, bytes));
    CHECK(bytes == std::make_pair<size_t, size_t>(0, 6));
    REQUIRE(model.range({ 1, 3 }, 6, bytes));
    CHECK(bytes == std::make_pair<size_t, size_t>(0, 6));
    REQUIRE(model.range({ 2 }, 10, bytes));
    CHECK(bytes == std::make_pair<size_t, size_t>(0, 4));
    CHECK_FALSE(model.range({ 3 }, 10, bytes));
    CHECK_FALSE(model.range({ 2, 0 }, 10, bytes));
    // the start block shorter than the start: none of it
    CHECK_FALSE(model.range({ 0 }, 2, bytes));
    // the end block shorter than the end: all of it
    REQUIRE(model.range({ 2 }, 3, bytes));
    CHECK(bytes == std::make_pair<size_t, size_t>(0, 3));

    // within one block
    model.select(SelectionPosition(2), SelectionPosition(5));
    REQUIRE(model.range({}, 10, bytes));
    CHECK(bytes == std::make_pair<size_t, size_t>(2, 5));
    CHECK_FALSE(model.range({ 0 }, 10, bytes));

    model.clear();
    CHECK(model.empty());
    CHECK_FALSE(model.range({}, 10, bytes));
}

TEST_CASE("SelectionModel: serial moves on every change and on nothing else")
{
    SelectionModel model;
    const unsigned start=model.serial();

    model.clear();
    CHECK(model.serial() == start);
    model.setCaret(SelectionPosition(4));
    CHECK(model.serial() == start+1);
    CHECK(model.empty());
    model.setCaret(SelectionPosition(4));
    CHECK(model.serial() == start+1);
    model.extendTo(SelectionPosition(7));
    CHECK(model.serial() == start+2);
    CHECK(model.anchor().offset == 4);
    CHECK(model.focus().offset == 7);
    model.extendTo(SelectionPosition(7));
    CHECK(model.serial() == start+2);
    model.select(SelectionPosition(4), SelectionPosition(7));
    CHECK(model.serial() == start+2);
    model.clear();
    CHECK(model.serial() == start+3);
}

TEST_CASE("wordAt: the word, blank run or punctuation run a byte is in")
{
    const String text="call foo_bar(x, 42);  done\nnext";
    auto word=[&](size_t at) { return text.substr(text::wordAt(text, at).first,
        text::wordAt(text, at).second-text::wordAt(text, at).first); };

    CHECK(word(0) == "call");
    CHECK(word(3) == "call");
    CHECK(word(4) == " ");
    CHECK(word(7) == "foo_bar");
    CHECK(word(12) == "(");
    CHECK(word(14) == ",");
    CHECK(word(15) == " ");
    CHECK(word(17) == "42");
    CHECK(word(19) == ");");
    CHECK(word(21) == "  ");
    // before the '\n': the word it ends
    CHECK(word(text.find('\n')) == "done");
    CHECK(word(text.find('\n')+1) == "next");
    // at the end: the word before it
    CHECK(word(text.size()) == "next");

    // a '\n' after a '\n' is a run of its own
    const String lines="a\n\nb";

    CHECK(text::wordAt(lines, 2) == std::make_pair<size_t, size_t>(2, 3));

    CHECK(text::wordAt("", 0) == std::make_pair<size_t, size_t>(0, 0));

    // non-ASCII letters and a combining mark stay in their word
    const String accents="na\xc3\xafve cafe\xcc\x81!";

    CHECK(text::wordAt(accents, 1) == std::make_pair<size_t, size_t>(0, 6));
    CHECK(text::wordAt(accents, 9) == std::make_pair<size_t, size_t>(7, 13));
}
