/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::fuzzyMatch and text::fuzzyRank, the CommandPalette's scorer: a query
 *  matches a text when its code points appear in it in order, and not out of
 *  order, missing or longer than it; an empty query matches with score 0.
 *  The score orders a prefix above a word start, a word start above a camel
 *  hump, a hump above a scattered match and a consecutive run above a gapped
 *  one, and one score is pinned to the documented constants. fuzzyRank puts
 *  the best first, equal scores by the tie rank and then by index, the same
 *  on every call. ASCII letters compare case-insensitively (in the query and
 *  in the text) with the bonuses read from the text's own case; other code
 *  points compare as code points, not bytes (é is not the bytes of "Ã©"), an
 *  invalid UTF-8 byte is a code point of its own, and non-ASCII labels rank
 *  in a stable order.
 */
#include <doctest/doctest.h>

#include <gweni/text/fuzzyMatch.h>

#include <vector>

using gweni::String;
using namespace gweni::text;

namespace
{
int scoreOf(const String &query, const String &text)
{
    int score=-1000;

    REQUIRE(fuzzyMatch(query, text, score));
    return score;
}

bool matches(const String &query, const String &text)
{
    int score=0;

    return fuzzyMatch(query, text, score);
}
}

TEST_CASE("fuzzyMatch: the query's characters in order match, missing or out of order ones do not")
{
    CHECK(matches("tgt", "Toggle Terminal"));
    CHECK(matches("vtt", "View: Toggle Terminal"));
    CHECK(matches("abc", "abc"));
    CHECK(matches("ac", "abc"));

    CHECK_FALSE(matches("xyz", "Toggle Terminal"));
    CHECK_FALSE(matches("ba", "ab"));
    CHECK_FALSE(matches("abcd", "abc"));
    CHECK_FALSE(matches("a", ""));

    int score=-5;

    CHECK(fuzzyMatch("", "anything", score));
    CHECK(score == 0);
    CHECK(fuzzyMatch("", "", score));

    // a text that does not match leaves the score as it was
    score=123;
    CHECK_FALSE(fuzzyMatch("q", "abc", score));
    CHECK(score == 123);
}

TEST_CASE("fuzzyMatch: a prefix scores above a word start, a word start above a camel hump, a hump above a scattered match")
{
    const int prefix=scoreOf("fo", "foox");
    const int word=scoreOf("fo", "x foo");
    const int camel=scoreOf("fo", "xFoo");
    const int scattered=scoreOf("fo", "xfxo");

    CHECK(prefix > word);
    CHECK(word > camel);
    CHECK(camel > scattered);

    // the documented constants: two matches, the first at the start, the
    // second consecutive
    CHECK(prefix == 2*FuzzyMatchScore+FuzzyStartBonus+FuzzyConsecutiveBonus);
    // and a one-code-point gap after a match with no bonus
    CHECK(scattered == 2*FuzzyMatchScore-FuzzyGapStart);

    // every separator starts a word
    for(const char *text : {"x foo", "x:foo", "x-foo", "x_foo", "x/foo", "x.foo"})
    {
        CAPTURE(text);
        CHECK(scoreOf("fo", text) == word);
    }
}

TEST_CASE("fuzzyMatch: a consecutive run scores above the same characters with gaps, a longer gap below a shorter")
{
    CHECK(scoreOf("abc", "xabcx") > scoreOf("abc", "xaxbxc"));
    CHECK(scoreOf("ab", "xaxb") > scoreOf("ab", "xaxxxb"));
    // the best of the ways the query appears: "ab" in "a ab" is the run
    CHECK(scoreOf("ab", "a ab") == scoreOf("ab", "x ab"));
}

TEST_CASE("fuzzyRank: best score first, equal scores by the tie rank and then by index, the same on every call")
{
    const std::vector<String> texts={"xaxb", "y ab", "ab", "nope", "z ab", "w ab"};
    std::vector<size_t> ranked;

    fuzzyRank("ab", texts, ranked);
    CHECK(ranked == std::vector<size_t>({2, 1, 4, 5, 0}));

    std::vector<size_t> again;

    fuzzyRank("ab", texts, again);
    CHECK(again == ranked);

    // ties among 1, 4 and 5 go by the rank, lower first
    const std::vector<int> tie={0, 3, 9, 0, 1, 3};

    fuzzyRank("ab", texts, ranked, &tie);
    CHECK(ranked == std::vector<size_t>({2, 4, 1, 5, 0}));

    // an empty query lists every text, all at 0: by the rank, then index
    fuzzyRank("", texts, ranked, &tie);
    CHECK(ranked == std::vector<size_t>({0, 3, 4, 1, 5, 2}));
    fuzzyRank("", texts, ranked);
    CHECK(ranked == std::vector<size_t>({0, 1, 2, 3, 4, 5}));
}

TEST_CASE("fuzzyMatch: ASCII letters match whatever their case, the bonuses read from the text's case")
{
    CHECK(matches("TT", "toggle terminal"));
    CHECK(matches("tog", "TOGGLE"));
    CHECK(scoreOf("tt", "Toggle Terminal") == scoreOf("TT", "Toggle Terminal"));
    CHECK(scoreOf("tT", "Toggle Terminal") == scoreOf("Tt", "Toggle Terminal"));
    // the hump is the text's upper case, whatever the query's
    CHECK(scoreOf("f", "xFoo") == scoreOf("F", "xFoo"));
    CHECK(scoreOf("f", "xFoo") > scoreOf("f", "xfoo"));
}

TEST_CASE("fuzzyMatch: non-ASCII text is compared as UTF-8 code points, an invalid byte one of its own")
{
    CHECK(matches("é", "Café menu"));
    CHECK(matches("cém", "Café menu"));
    // é (U+00E9, C3 A9) is not the bytes C3 .. A9 of "Ã©" (U+00C3 U+00A9)
    CHECK_FALSE(matches("é", "\xc3\x83\xc2\xa9"));
    // only ASCII is folded
    CHECK_FALSE(matches("É", "é"));
    // a gap is counted in code points: one two-byte é is a one-point gap
    CHECK(scoreOf("ab", "aéb") == scoreOf("ab", "axb"));

    // invalid UTF-8, a truncated sequence included: no crash, bytes as code
    // points
    CHECK(matches("\xff", "a\xff" "b"));
    CHECK(matches("b", "\xc3" "b"));
    CHECK_FALSE(matches("\xc3\xa9", "\xc3"));

    const std::vector<String> texts={"zu", "Müller", "über", "Übersicht", "Müll ü"};
    std::vector<size_t> ranked;

    fuzzyRank("ü", texts, ranked);
    // "über" a prefix, "Müll ü" a word start, "Müller" neither; "Übersicht"
    // has Ü, which is not folded
    CHECK(ranked == std::vector<size_t>({2, 4, 1}));

    std::vector<size_t> again;

    fuzzyRank("ü", texts, again);
    CHECK(again == ranked);
}
