/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  What the language tests share: a line's tokens written as text (each token
 *  as kind'its bytes', in order, the Plain bytes left out), so the expected
 *  tokens of a snippet can be written out by hand; a check that a line's
 *  tokens are well formed; and seeded random edits of a document in a
 *  language, after each of which the lines on screen, and every so often all
 *  of them, must be what a fresh tokenize of the whole text gives.
 */
#ifndef _gweni_tests_unit_highlightTesting_h_
#define _gweni_tests_unit_highlightTesting_h_

#include <doctest/doctest.h>

#include <gweni/text/highlightCache.h>

#include <random>
#include <string>
#include <vector>

namespace test
{

//! \p tokens of \p line as "kind'bytes' kind'bytes' ..."
inline std::string describeTokens(const std::string &line, const std::vector<gweni::text::Token> &tokens)
{
    std::string text;

    for(const gweni::text::Token &token:tokens)
    {
        if(!text.empty())
            text+=" ";
        text+=gweni::text::tokenKindName(token.kind);
        text+="'";
        if((token.begin <= token.end) && (token.end <= line.size()))
            text+=line.substr(token.begin, token.end-token.begin);
        else
            text+="<out of the line>";
        text+="'";
    }
    return text;
}

//! Whether \p tokens are ascending, not overlapping, not empty and inside \p line
inline bool wellFormed(const std::string &line, const std::vector<gweni::text::Token> &tokens)
{
    uint32_t last=0;

    for(const gweni::text::Token &token:tokens)
    {
        if((token.begin < last) || (token.end <= token.begin) || (token.end > line.size()) ||
            (token.kind >= gweni::text::TokenKind::Count))
            return false;
        last=token.end;
    }
    return true;
}

//! Every line's tokens, the text tokenized from the top
inline std::vector<std::vector<gweni::text::Token>> freshTokens(const gweni::text::Document &document,
    const gweni::text::Tokenizer &tokenizer)
{
    std::vector<std::vector<gweni::text::Token>> lines(document.lineCount());
    gweni::text::TokenState state=tokenizer.initialState();

    for(size_t i=0; i < document.lineCount(); ++i)
        state=tokenizer.tokenize(document.line(i), state, lines[i]);
    return lines;
}

//! \p text's lines, each tokenized from the state the one before it ended in,
//! described (describeTokens); every line's tokens are checked to be well formed
inline std::vector<std::string> describeLines(const gweni::text::Tokenizer &tokenizer, const std::string &text)
{
    const gweni::text::Document document(text);
    const std::vector<std::vector<gweni::text::Token>> tokens=freshTokens(document, tokenizer);
    std::vector<std::string> lines;

    for(size_t i=0; i < document.lineCount(); ++i)
    {
        CHECK(wellFormed(document.line(i), tokens[i]));
        lines.push_back(describeTokens(document.line(i), tokens[i]));
    }
    return lines;
}

//! Checks describeLines(tokenizer, text) line by line against \p expected
inline void checkLines(const gweni::text::Tokenizer &tokenizer, const std::string &text,
    const std::vector<std::string> &expected)
{
    const std::vector<std::string> lines=describeLines(tokenizer, text);
    const gweni::text::Document document(text);

    REQUIRE(lines.size() == expected.size());
    for(size_t i=0; i < lines.size(); ++i)
    {
        CAPTURE(i);
        CAPTURE(document.line(i));
        CHECK(lines[i] == expected[i]);
    }
}

//! Whether every line of the cache is what a fresh tokenize gives (making them all fresh)
inline bool cacheMatchesFresh(gweni::text::HighlightCache &cache, const gweni::text::Document &document,
    const gweni::text::Tokenizer &tokenizer)
{
    const std::vector<std::vector<gweni::text::Token>> fresh=freshTokens(document, tokenizer);

    for(size_t i=0; i < fresh.size(); ++i)
    {
        if(!wellFormed(document.line(i), cache.tokens(i)) || (cache.tokens(i) != fresh[i]))
            return false;
    }
    return true;
}

//! Seeded random edits of \p text in \p tokenizer's language through a cache
//! with a random screen before each: an insert of one of \p pieces (which may
//! hold '\n's, so lines are inserted) at a random place, or a removal of up to
//! three lines' worth (joins and whole lines removed). After each, the edit's
//! pass must have stopped at the screen's last line, and the lines on screen
//! (and every 50 steps all of them) must be what a fresh tokenize gives.
inline void checkRandomEdits(const gweni::text::Tokenizer &tokenizer, const std::string &text,
    const std::vector<std::string> &pieces, unsigned seeds=6, int steps=250)
{
    using namespace gweni::text;

    for(unsigned seed=1; seed <= seeds; ++seed)
    {
        std::mt19937 random(seed);
        Document document(text);
        HighlightCache cache;
        auto pick=[&](size_t count) { return static_cast<size_t>(random()%count); };

        CAPTURE(seed);
        cache.attach(&document);
        cache.setTokenizer(&tokenizer);

        for(int step=0; step < steps; ++step)
        {
            CAPTURE(step);

            const size_t lines=document.lineCount();
            const size_t first=pick(lines);
            const size_t count=1+pick(25);

            cache.setVisibleLines(first, count);
            for(size_t i=first; i < std::min(first+count, lines); ++i)
                cache.tokens(i);

            const size_t frontierBefore=cache.frontier();
            const size_t line=pick(lines);
            const Position at{line, pick(document.line(line).size()+1)};
            Edit edit;

            if(pick(3) == 0)
            {
                const size_t endLine=std::min(lines-1, line+pick(3));
                const Position end{endLine, pick(document.line(endLine).size()+1)};

                edit.range=(end < at)?Range{end, at}:Range{at, end};
            }
            else
            {
                edit.range=Range{at, at};
                edit.text=pieces[pick(pieces.size())];
            }

            const LineChange change=document.apply(edit);
            const size_t lastVisible=std::min(first+count, document.lineCount())-1;

            if((first < document.lineCount()) && (change.first <= lastVisible))
                CHECK(cache.frontier() == lastVisible+1);
            else
                CHECK(cache.frontier() == std::min(frontierBefore, change.first));

            const std::vector<std::vector<Token>> fresh=freshTokens(document, tokenizer);

            for(size_t i=first; (i <= lastVisible) && (i < document.lineCount()); ++i)
            {
                CAPTURE(i);
                CHECK(wellFormed(document.line(i), cache.tokens(i)));
                CHECK(cache.tokens(i) == fresh[i]);
            }
            if(step%50 == 49)
                CHECK(cacheMatchesFresh(cache, document, tokenizer));
        }
        CHECK(cacheMatchesFresh(cache, document, tokenizer));
    }
}

}//namespace test

#endif//_gweni_tests_unit_highlightTesting_h_
