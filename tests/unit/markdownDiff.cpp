/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::markdownDiff, written as one word per new block: =o for a block
 *  Kept from old block o, ~o for one Changed from it (a container's children
 *  in {} after it), + for a New one. It pins:
 *  - an unchanged document keeps every block, and an empty old one builds
 *    every block new;
 *  - a paragraph edited at the start, in the middle and at the end is Changed
 *    from its old block and every other block Kept;
 *  - a block inserted at the start, in the middle and at the end is New, a
 *    deleted one named by nothing, the rest Kept at their old indexes;
 *  - two blocks swapped are both Kept, the moved one from its old index, and
 *    a block moved further is Kept too; a block moved and edited at once is
 *    New, its old place being in another gap of the subsequence;
 *  - a block of another type in the edited place (a paragraph made a
 *    heading) is New, not Changed;
 *  - a list item edited inside a nested list: the top list and the item
 *    holding the nested list Changed, the nested list Changed, the edited
 *    item's paragraph Changed, every other item and block Kept;
 *  - an item added to a nested list: New inside the Changed lists;
 *  - repeated equal blocks: an edit of one keeps the others, each old block
 *    named once at most;
 *  - a middle past MarkdownDiffLimit still keeps the prefix, the suffix and
 *    the moved blocks.
 */
#include "markdownTesting.h"

#include <doctest/doctest.h>

#include <set>
#include <string>
#include <vector>

using gweni::text::MarkdownBlock;
using gweni::text::MarkdownBlockMatch;
using gweni::text::markdownDiff;
using gweni::text::parseMarkdown;

namespace
{

std::string describeMatches(const std::vector<MarkdownBlockMatch> &matches)
{
    std::string out;

    for(const MarkdownBlockMatch &match:matches)
    {
        if(!out.empty())
            out+=' ';
        switch(match.kind)
        {
        case MarkdownBlockMatch::Kind::Kept:
            out+="="+std::to_string(match.old);
            break;
        case MarkdownBlockMatch::Kind::Changed:
            out+="~"+std::to_string(match.old);
            if(!match.children.empty())
                out+="{"+describeMatches(match.children)+"}";
            break;
        case MarkdownBlockMatch::Kind::New:
            out+="+";
            break;
        }
    }
    return out;
}

//! The diff of two documents, checked to name each old block once at most
std::string diff(const std::string &before, const std::string &after)
{
    std::vector<MarkdownBlock> old=parseMarkdown(before);
    std::vector<MarkdownBlock> now=parseMarkdown(after);
    std::vector<MarkdownBlockMatch> matches=markdownDiff(old, now);

    REQUIRE(matches.size() == now.size());

    std::set<size_t> named;

    for(const MarkdownBlockMatch &match:matches)
    {
        if(match.kind == MarkdownBlockMatch::Kind::New)
            continue;
        REQUIRE(match.old < old.size());
        CHECK(named.insert(match.old).second);
        if(match.kind == MarkdownBlockMatch::Kind::Kept)
            CHECK(old[match.old].hash == now[size_t(&match-matches.data())].hash);
        else
            CHECK(old[match.old].type == now[size_t(&match-matches.data())].type);
    }
    return describeMatches(matches);
}

const char *document=
    "# Title\n"
    "\n"
    "First paragraph.\n"
    "\n"
    "Second paragraph.\n"
    "\n"
    "> A quote.\n"
    "\n"
    "Last paragraph.\n";

}//namespace

TEST_CASE("Markdown diff: an unchanged document keeps every block, an empty one builds every block")
{
    CHECK(diff(document, document) == "=0 =1 =2 =3 =4");
    CHECK(diff("", document) == "+ + + + +");
    CHECK(diff(document, "") == "");
}

TEST_CASE("Markdown diff: a paragraph edited at the start, middle and end is Changed")
{
    CHECK(diff(document,
        "# Title!\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "~0 =1 =2 =3 =4");
    CHECK(diff(document,
        "# Title\n\nFirst paragraph.\n\nSecond *paragraph*.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=0 =1 ~2 =3 =4");
    CHECK(diff(document,
        "# Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph, longer.\n") ==
        "=0 =1 =2 =3 ~4");
    // a heading's level is its content: Changed, not New
    CHECK(diff(document,
        "## Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "~0 =1 =2 =3 =4");
    // an edit inside the quote: the quote Changed, its paragraph too
    CHECK(diff(document,
        "# Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A new quote.\n\nLast paragraph.\n") ==
        "=0 =1 =2 ~3{~0} =4");
}

TEST_CASE("Markdown diff: inserts and deletes keep the other blocks")
{
    CHECK(diff(document,
        "New.\n\n# Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "+ =0 =1 =2 =3 =4");
    CHECK(diff(document,
        "# Title\n\nFirst paragraph.\n\n---\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=0 =1 + =2 =3 =4");
    CHECK(diff(document,
        "# Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph.\n\n    code\n") ==
        "=0 =1 =2 =3 =4 +");
    CHECK(diff(document, "First paragraph.\n\nSecond paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=1 =2 =3 =4");
    CHECK(diff(document, "# Title\n\nFirst paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=0 =1 =3 =4");
    CHECK(diff(document, "# Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n") ==
        "=0 =1 =2 =3");
    // a delete and an edit next to it in one change: the edit is Changed
    CHECK(diff(document, "# Title\n\nFirst, edited.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=0 ~1 =3 =4");
}

TEST_CASE("Markdown diff: swapped and moved blocks are Kept")
{
    CHECK(diff(document,
        "# Title\n\nSecond paragraph.\n\nFirst paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=0 =2 =1 =3 =4");
    CHECK(diff(document,
        "Last paragraph.\n\n# Title\n\nFirst paragraph.\n\nSecond paragraph.\n\n> A quote.\n") ==
        "=4 =0 =1 =2 =3");
    CHECK(diff("a\n\nb\n", "b\n\na\n") == "=1 =0");
    // moved and edited at once: the subsequence keeps q and r in order,
    // leaving p's gap before them, so the edited p after them is New
    CHECK(diff("p\n\nq\n\nr\n", "q\n\nr\n\np!\n") == "=1 =2 +");
}

TEST_CASE("Markdown diff: a block of another type in the edited place is New")
{
    CHECK(diff(document,
        "# Title\n\nFirst paragraph.\n\n## Second paragraph.\n\n> A quote.\n\nLast paragraph.\n") ==
        "=0 =1 + =3 =4");
    // a paragraph and a list where one paragraph was
    CHECK(diff("a\n\nb\n\nc\n", "a\n\n- b\n\nc\n") == "=0 + =2");
}

TEST_CASE("Markdown diff: a list item edited inside a nested list")
{
    const char *before=
        "Intro.\n"
        "\n"
        "- one\n"
        "- two\n"
        "  - nested a\n"
        "  - nested b\n"
        "  - nested c\n"
        "- three\n"
        "\n"
        "Outro.\n";

    // the list: Changed; its items one and three Kept; two Changed, its
    // paragraph Kept and its nested list Changed, holding a and c Kept and b
    // Changed (its paragraph Changed)
    CHECK(diff(before,
        "Intro.\n\n- one\n- two\n  - nested a\n  - nested B!\n  - nested c\n- three\n\nOutro.\n") ==
        "=0 ~1{=0 ~1{=0 ~1{=0 ~1{~0} =2}} =2} =2");
    // an item added to the nested list
    CHECK(diff(before,
        "Intro.\n\n- one\n- two\n  - nested a\n  - nested b\n  - added\n  - nested c\n- three\n\nOutro.\n") ==
        "=0 ~1{=0 ~1{=0 ~1{=0 =1 + =2}} =2} =2");
    // a top item removed
    CHECK(diff(before,
        "Intro.\n\n- two\n  - nested a\n  - nested b\n  - nested c\n- three\n\nOutro.\n") ==
        "=0 ~1{=1 =2} =2");
}

TEST_CASE("Markdown diff: repeated equal blocks")
{
    CHECK(diff("x\n\nx\n\nx\n\nx\n", "x\n\nx\n\ny\n\nx\n") == "=0 =1 ~2 =3");
    CHECK(diff("x\n\nx\n", "x\n\nx\n\nx\n") == "=0 =1 +");
    CHECK(diff("x\n\ny\n\nx\n", "x\n") == "=0");
}

TEST_CASE("Markdown diff: a middle past the limit keeps the prefix, the suffix and moved blocks")
{
    // 1100 distinct paragraphs reversed between a fixed first and last one:
    // the middle is 1098 x 1098 > MarkdownDiffLimit, so no subsequence, but
    // each paragraph is found by its hash
    std::string before="first\n\n";
    std::string after="first\n\n";
    const size_t count=1100;

    REQUIRE((count-2)*(count-2) > gweni::text::MarkdownDiffLimit);
    for(size_t i=1; i+1 < count; ++i)
    {
        before+="p"+std::to_string(i)+"\n\n";
        after+="p"+std::to_string(count-1-i)+"\n\n";
    }
    before+="last\n";
    after+="last\n";

    std::vector<MarkdownBlock> old=parseMarkdown(before);
    std::vector<MarkdownBlock> now=parseMarkdown(after);

    REQUIRE(old.size() == count);
    REQUIRE(now.size() == count);

    std::vector<MarkdownBlockMatch> matches=markdownDiff(old, now);

    REQUIRE(matches.size() == count);
    for(size_t n=0; n < count; ++n)
    {
        INFO(n);
        CHECK(matches[n].kind == MarkdownBlockMatch::Kind::Kept);
        CHECK(matches[n].old == (((n == 0) || (n+1 == count))?n:count-1-n));
    }
}
