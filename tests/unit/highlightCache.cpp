/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::HighlightCache over a toy tokenizer (keywords, numbers, '#' comments
 *  and a """ block string that runs over lines, the one state a line carries),
 *  and the language registry:
 *  - a line is tokenized when it is first asked for, with the lines before it,
 *    each once;
 *  - an edit that leaves its line's end state tokenizes that line alone, and one
 *    that changes it (an opened """) tokenizes through the last line on screen
 *    and no further, the lines below stale until asked for;
 *  - a line inserted or removed shifts the entries after it, which are not
 *    tokenized again;
 *  - the change handler gets the lines whose tokens an edit's pass changed;
 *  - without a tokenizer it keeps no entry per line, through edits too, and its
 *    screen still moves with its lines;
 *  - after seeded random edits (inserts, removals, line breaks, joins, opened and
 *    closed block strings) and random screens, every line asked for is what a
 *    fresh tokenize of the whole text gives, and no edit's pass goes past the
 *    screen;
 *  - the registry finds what was registered, and maps file names to languages.
 */
#include <doctest/doctest.h>

#include <gweni/text/highlightCache.h>

#include <algorithm>
#include <cctype>
#include <random>

using namespace gweni;
using namespace gweni::text;

namespace
{

//! The toy language: inside a """ block (state 1) every byte is String up to the
//! closing """; outside, "if" and "else" are keywords, digit runs numbers, '#'
//! starts a comment to the line's end and """ opens a block.
class ToyTokenizer: public Tokenizer
{
public:
    TokenState tokenize(const String &line, const TokenState &start,
        std::vector<Token> &tokens) const override
    {
        TokenState state=start;
        size_t i=0;

        while(i < line.size())
        {
            if(state.words[0] == 1)
            {
                const size_t close=line.find("\"\"\"", i);
                const size_t end=(close == String::npos)?line.size():close+3;

                tokens.push_back({uint32_t(i), uint32_t(end), TokenKind::String});
                if(close != String::npos)
                    state.words[0]=0;
                i=end;
                continue;
            }
            if(line.compare(i, 3, "\"\"\"") == 0)
            {
                state.words[0]=1;
                tokens.push_back({uint32_t(i), uint32_t(i+3), TokenKind::String});
                i+=3;
                continue;
            }
            if(line[i] == '#')
            {
                tokens.push_back({uint32_t(i), uint32_t(line.size()), TokenKind::Comment});
                break;
            }
            if(isdigit(static_cast<unsigned char>(line[i])))
            {
                size_t end=i;

                while((end < line.size()) && isdigit(static_cast<unsigned char>(line[end])))
                    ++end;
                tokens.push_back({uint32_t(i), uint32_t(end), TokenKind::Number});
                i=end;
                continue;
            }
            if(isalpha(static_cast<unsigned char>(line[i])))
            {
                size_t end=i;

                while((end < line.size()) && isalpha(static_cast<unsigned char>(line[end])))
                    ++end;

                const String word=line.substr(i, end-i);

                if((word == "if") || (word == "else"))
                    tokens.push_back({uint32_t(i), uint32_t(end), TokenKind::Keyword});
                i=end;
                continue;
            }
            ++i;
        }
        return state;
    }
};

//! Every line's tokens, the text tokenized from the top
std::vector<std::vector<Token>> freshTokens(const Document &document, const Tokenizer &tokenizer)
{
    std::vector<std::vector<Token>> lines(document.lineCount());
    TokenState state=tokenizer.initialState();

    for(size_t i=0; i < document.lineCount(); ++i)
        state=tokenizer.tokenize(document.line(i), state, lines[i]);
    return lines;
}

//! count lines "if line <i> # note" with a number in each
String numberedLines(size_t count)
{
    String text;

    for(size_t i=0; i < count; ++i)
    {
        if(i > 0)
            text+="\n";
        text+="if line "+std::to_string(i)+" # note";
    }
    return text;
}

//! Whether every line of the cache is what a fresh tokenize gives (making them all fresh)
bool allFresh(HighlightCache &cache, const Document &document, const Tokenizer &tokenizer)
{
    const std::vector<std::vector<Token>> fresh=freshTokens(document, tokenizer);

    for(size_t i=0; i < fresh.size(); ++i)
    {
        if(cache.tokens(i) != fresh[i])
            return false;
    }
    return true;
}

}//namespace

TEST_CASE("Highlight cache: a line is tokenized when it is asked for, with the lines before it, once")
{
    ToyTokenizer toy;
    Document document(numberedLines(100));
    HighlightCache cache;

    cache.attach(&document);
    cache.setTokenizer(&toy);

    CHECK(cache.tokenizedLines() == 0);
    CHECK(!cache.isFresh(0));

    const std::vector<Token> &line50=cache.tokens(50);

    CHECK(cache.tokenizedLines() == 51);
    CHECK(cache.frontier() == 51);
    CHECK(line50 == freshTokens(document, toy)[50]);
    cache.tokens(10);
    cache.tokens(50);
    CHECK(cache.tokenizedLines() == 51);
    CHECK(!cache.isFresh(51));

    SUBCASE("no tokenizer, no tokens")
    {
        cache.setTokenizer(nullptr);
        CHECK(cache.tokens(10).empty());
        CHECK(cache.tokenizedLines() == 51);
    }
}

TEST_CASE("Highlight cache: an edit that leaves its line's end state tokenizes that line alone")
{
    ToyTokenizer toy;
    Document document(numberedLines(200));
    HighlightCache cache;

    cache.attach(&document);
    cache.setTokenizer(&toy);
    cache.setVisibleLines(10, 30);
    cache.tokens(199);

    const size_t before=cache.tokenizedLines();

    SUBCASE("on screen")
    {
        document.insert(Position{20, 3}, " 42 else");
        CHECK(cache.tokenizedLines() == before+1);
        CHECK(cache.frontier() == 40);
    }
    SUBCASE("below the screen: nothing until asked for")
    {
        document.insert(Position{120, 3}, " 42 else");
        CHECK(cache.tokenizedLines() == before);
        cache.tokens(150);
        CHECK(cache.tokenizedLines() == before+1);
    }
    SUBCASE("a comment line, which carries no state either")
    {
        document.insert(Position{25, 0}, "# ");
        CHECK(cache.tokenizedLines() == before+1);
    }
    CHECK(allFresh(cache, document, toy));
}

TEST_CASE("Highlight cache: a state-changing edit tokenizes through the lines on screen and no further")
{
    ToyTokenizer toy;
    Document document(numberedLines(200));
    HighlightCache cache;
    std::vector<std::vector<size_t>> reported;

    cache.attach(&document);
    cache.setTokenizer(&toy);
    cache.setVisibleLines(10, 20);
    cache.setChangeHandler([&](const std::vector<size_t> &lines) { reported.push_back(lines); });
    cache.tokens(199);

    size_t before=cache.tokenizedLines();

    // opens a block string on line 15: lines 15..29 are tokenized, the rest wait
    document.insert(Position{15, 0}, "\"\"\"");
    CHECK(cache.tokenizedLines() == before+15);
    CHECK(cache.frontier() == 30);
    CHECK(!cache.isFresh(30));
    CHECK(!cache.isFresh(199));
    REQUIRE(reported.size() == 1);
    CHECK(reported[0].size() == 15);
    CHECK(reported[0].front() == 15);
    CHECK(reported[0].back() == 29);

    // the lines below are done when asked for
    before=cache.tokenizedLines();
    CHECK(cache.tokens(100) == freshTokens(document, toy)[100]);
    CHECK(cache.tokenizedLines() == before+71);

    // closing it on the next line: lines 16..29 again, and still no further
    before=cache.tokenizedLines();
    document.insert(Position{16, 0}, "\"\"\"");
    CHECK(cache.tokenizedLines() == before+14);
    CHECK(cache.frontier() == 30);

    // asked for, the lines below the screen that were tokenized in the block
    // (30..100) are tokenized again; those that never were since it opened
    // (101..199) start in the state they were tokenized from, and are compared alone
    before=cache.tokenizedLines();
    cache.tokens(199);
    CHECK(cache.tokenizedLines() == before+71);
    CHECK(allFresh(cache, document, toy));
}

TEST_CASE("Highlight cache: a line inserted or removed shifts the lines after it without tokenizing them")
{
    ToyTokenizer toy;
    Document document(numberedLines(100));
    HighlightCache cache;

    cache.attach(&document);
    cache.setTokenizer(&toy);
    cache.setVisibleLines(0, 100);
    cache.tokens(99);

    size_t before=cache.tokenizedLines();

    SUBCASE("a line broken in two: both halves")
    {
        document.insert(Position{20, 4}, "\n");
        CHECK(document.lineCount() == 101);
        CHECK(cache.tokenizedLines() == before+2);
    }
    SUBCASE("a line inserted whole: the line it went into and the new one")
    {
        document.insert(Position{20, 0}, "else 7\n");
        CHECK(cache.tokenizedLines() == before+2);
    }
    SUBCASE("two lines joined: the joined line alone")
    {
        document.erase(Range{Position{20, document.line(20).size()}, Position{21, 0}});
        CHECK(document.lineCount() == 99);
        CHECK(cache.tokenizedLines() == before+1);
    }
    SUBCASE("three lines removed whole: the line they end on")
    {
        document.erase(Range{Position{20, 0}, Position{23, 0}});
        CHECK(document.lineCount() == 97);
        CHECK(cache.tokenizedLines() == before+1);
    }
    // fresh through the last line on screen, the 100th
    CHECK(cache.frontier() == std::min<size_t>(100, document.lineCount()));
    CHECK(allFresh(cache, document, toy));
}

TEST_CASE("Highlight cache: the change handler gets the lines whose tokens an edit's pass changed")
{
    ToyTokenizer toy;
    Document document(numberedLines(50));
    HighlightCache cache;
    std::vector<std::vector<size_t>> reported;

    cache.attach(&document);
    cache.setTokenizer(&toy);
    cache.setVisibleLines(0, 30);
    cache.setChangeHandler([&](const std::vector<size_t> &lines) { reported.push_back(lines); });
    cache.tokens(29);

    // a new token on line 5: that line
    document.insert(Position{5, 0}, "else ");
    REQUIRE(reported.size() == 1);
    CHECK(reported[0] == std::vector<size_t>{ 5 });

    // a letter of a word that is no token replaced: the same tokens, nothing
    reported.clear();
    document.replace(Range{Position{6, 3}, Position{6, 4}}, "k");
    CHECK(reported.empty());

    // an edit below the screen: no pass, nothing
    document.insert(Position{40, 0}, "\"\"\"");
    CHECK(reported.empty());
}

TEST_CASE("Highlight cache: after seeded random edits every line is what a fresh tokenize gives")
{
    ToyTokenizer toy;
    const char *pieces[]={ "\"\"\"", "#", "if ", "else", "12", "\n", "a\nb", "\n\n", "x", " 3 # c", "\"\"\"\n" };

    for(unsigned seed=1; seed <= 8; ++seed)
    {
        std::mt19937 random(seed);
        Document document(numberedLines(60));
        HighlightCache cache;
        auto pick=[&](size_t count) { return static_cast<size_t>(random()%count); };

        CAPTURE(seed);
        cache.attach(&document);
        cache.setTokenizer(&toy);

        for(int step=0; step < 300; ++step)
        {
            CAPTURE(step);

            const size_t lines=document.lineCount();
            const size_t first=pick(lines);
            const size_t count=1+pick(25);

            cache.setVisibleLines(first, count);
            // the rows on screen are bound before the edit, as an editor binds them
            for(size_t i=first; i < std::min(first+count, lines); ++i)
                cache.tokens(i);

            const size_t frontierBefore=cache.frontier();
            const size_t line=pick(lines);
            const Position at{line, pick(document.line(line).size()+1)};
            LineChange change;

            if(pick(3) == 0)
            {
                // a removal of up to three lines' worth, joins included
                const size_t endLine=std::min(lines-1, line+pick(3));
                const Position end{endLine, pick(document.line(endLine).size()+1)};
                Edit edit;

                edit.range=(end < at)?Range{end, at}:Range{at, end};
                change=document.apply(edit);
            }
            else
            {
                Edit edit;

                edit.range=Range{at, at};
                edit.text=pieces[pick(sizeof(pieces)/sizeof(pieces[0]))];
                change=document.apply(edit);
            }

            // the pass went through the last line on screen and stopped there
            // a screen below the lines replaced moved with its lines
            const size_t shown=(first >= change.first+change.removed)?first+change.inserted-change.removed:first;
            const size_t lastVisible=std::min(shown+count, document.lineCount())-1;

            CHECK(cache.visibleFirst() == shown);
            if((shown < document.lineCount()) && (change.first <= lastVisible))
                CHECK(cache.frontier() == lastVisible+1);
            else
                CHECK(cache.frontier() == std::min(frontierBefore, change.first));

            // the lines on screen, as a fresh tokenize gives them
            const std::vector<std::vector<Token>> fresh=freshTokens(document, toy);

            for(size_t i=shown; i <= lastVisible && i < document.lineCount(); ++i)
                CHECK(cache.tokens(i) == fresh[i]);
            if(step%50 == 49)
                CHECK(allFresh(cache, document, toy));
        }
        CHECK(allFresh(cache, document, toy));
    }
}

TEST_CASE("Highlight cache: a cache attached to a document follows it until it is gone")
{
    ToyTokenizer toy;
    Document document(numberedLines(10));

    {
        HighlightCache cache;

        cache.attach(&document);
        cache.setTokenizer(&toy);
        document.insert(Position{3, 0}, "a\nb\n");
        CHECK(allFresh(cache, document, toy));
        document.setText("if 1\n\"\"\"\nx");
        CHECK(allFresh(cache, document, toy));
        CHECK(cache.tokens(2) == std::vector<Token>{ Token{0, 1, TokenKind::String} });
    }
    // the cache took its observer away with it
    document.insert(Position{0, 0}, "else\n");
    CHECK(document.lineCount() == 4);
}

TEST_CASE("Highlight cache: without a tokenizer it keeps no entry per line")
{
    ToyTokenizer toy;
    Document document(numberedLines(1000));
    HighlightCache cache;

    cache.attach(&document);
    CHECK(cache.entryCount() == 0);
    cache.setVisibleLines(500, 20);
    document.insert(Position{3, 0}, "a\nb\n");
    document.erase(Range{Position{10, 0}, Position{11, 0}});
    CHECK(cache.entryCount() == 0);
    CHECK(cache.tokens(5).empty());
    // the screen moved with its lines, as with a tokenizer
    CHECK(cache.visibleFirst() == 501);

    cache.setTokenizer(&toy);
    CHECK(cache.entryCount() == document.lineCount());
    document.insert(Position{3, 0}, "c\n");
    CHECK(cache.entryCount() == document.lineCount());
    CHECK(allFresh(cache, document, toy));

    cache.setTokenizer(nullptr);
    CHECK(cache.entryCount() == 0);
    document.insert(Position{3, 0}, "d\n");
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("Token kinds: every kind has its name")
{
    CHECK(String(tokenKindName(TokenKind::Plain)) == "plain");
    CHECK(String(tokenKindName(TokenKind::Keyword)) == "keyword");
    CHECK(String(tokenKindName(TokenKind::Meta)) == "meta");
    CHECK(String(tokenKindName(TokenKind::Count)).empty());
    for(int kind=0; kind < int(TokenKind::Count); ++kind)
        CHECK(String(tokenKindName(TokenKind(kind))).size() > 0);
}

TEST_CASE("Language registry: a registered tokenizer is found by its name")
{
    auto owned=std::make_unique<ToyTokenizer>();
    const Tokenizer *toy=owned.get();

    registerLanguage("toy-test", std::move(owned));
    CHECK(findLanguage("toy-test") == toy);

    const std::vector<String> names=languageNames();

    CHECK(std::find(names.begin(), names.end(), "toy-test") != names.end());
    CHECK(findLanguage("no-such-language") == nullptr);

    registerLanguage("toy-test", nullptr);
    CHECK(findLanguage("toy-test") == nullptr);
}

TEST_CASE("Language registry: a file's language by its name")
{
    CHECK(languageForFileName("config.yaml") == "yaml");
    CHECK(languageForFileName("agents/coder.YML") == "yaml");
    CHECK(languageForFileName("package.json") == "json");
    CHECK(languageForFileName("README.md") == "markdown");
    CHECK(languageForFileName("prompts/plan.md.j2") == "markdown+jinja");
    CHECK(languageForFileName("fix.diff") == "diff");
    CHECK(languageForFileName("fix.patch") == "diff");
    CHECK(languageForFileName("tools/run.sh") == "bash");
    CHECK(languageForFileName("main.cpp").empty());
    CHECK(languageForFileName("notes.j2").empty());
    CHECK(languageForFileName(".yaml").empty());
    CHECK(languageForFileName("dir/.json").empty());
    CHECK(languageForFileName("").empty());
}
