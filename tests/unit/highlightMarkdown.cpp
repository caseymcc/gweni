/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The Markdown tokenizer (findLanguage("markdown")) on reference snippets,
 *  the tokens written out by hand: ATX headings (and seven '#'s, which are
 *  none), emphasis and strong with '*' and '_' (a '_' inside a word opens
 *  nothing), code spans closed by as many backticks, links, images,
 *  reference links and autolinks, backslash escapes, quote and list markers,
 *  thematic breaks, HTML comments over lines, and fences: a fence's body in
 *  its info string's language (yaml, bash) with that language's state nested
 *  in the fence's (a YAML block scalar inside one), Code for any other, and
 *  closed only by the same character at least as many times. Then seeded
 *  random edits keep the cache's lines what a fresh tokenize gives.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

using gweni::text::findLanguage;
using gweni::text::Tokenizer;
using gweni::text::TokenState;

namespace
{

const Tokenizer &markdown()
{
    const Tokenizer *tokenizer=findLanguage("markdown");

    REQUIRE(tokenizer != nullptr);
    return *tokenizer;
}

}//namespace

TEST_CASE("Markdown: headings, inline spans, quotes, lists, breaks and comments")
{
    test::checkLines(markdown(),
        "# Title\n"
        "###### Six\n"
        "####### seven is no heading\n"
        "Some *em* and **strong** and _under_ and __bold__ text.\n"
        "A `code span` and ``a ` tick`` and an unclosed ` tick.\n"
        "A [link](http://x.y) and ![image](a.png) and [ref][1] and <https://auto.link>.\n"
        "snake_case_word stays plain, \\*escaped\\* too, and * spaced *.\n"
        "> quoted *text*\n"
        "- item\n"
        "  1. numbered\n"
        "* star\n"
        "---\n"
        "<!-- a comment\n"
        "still comment --> after *em*",
        {
            "heading'# Title'",
            "heading'###### Six'",
            "",
            "emphasis'*em*' strong'**strong**' emphasis'_under_' strong'__bold__'",
            "code'`code span`' code'``a ` tick``'",
            "link'[link](http://x.y)' link'![image](a.png)' link'[ref][1]' link'<https://auto.link>'",
            "",
            "punctuation'>' emphasis'*text*'",
            "punctuation'-'",
            "punctuation'1.'",
            "punctuation'*'",
            "punctuation'---'",
            "comment'<!-- a comment'",
            "comment'still comment -->' emphasis'*em*'"
        });
}

TEST_CASE("Markdown: a fence's body is tokenized in its language, its state nested")
{
    test::checkLines(markdown(),
        "```yaml\n"
        "key: value # c\n"
        "text: |\n"
        "  # in the block scalar\n"
        "```\n"
        "~~~\n"
        "plain code\n"
        "```\n"
        "~~~\n"
        "```unknown\n"
        "x = 1\n"
        "```\n"
        "````bash\n"
        "echo \"$HOME\"\n"
        "```\n"
        "````\n"
        "# after",
        {
            "punctuation'```' meta'yaml'",
            "key'key' punctuation':' comment'# c'",
            "key'text' punctuation':' punctuation'|'",
            "string'# in the block scalar'",
            "punctuation'```'",
            "punctuation'~~~'",
            "code'plain code'",
            "code'```'",
            "punctuation'~~~'",
            "punctuation'```' meta'unknown'",
            "code'x = 1'",
            "punctuation'```'",
            "punctuation'````' meta'bash'",
            "string'\"' meta'$HOME' string'\"'",
            "",
            "punctuation'````'",
            "heading'# after'"
        });

    // the fence's language's state nested after the fence's word
    std::vector<gweni::text::Token> tokens;
    const TokenState fence=markdown().tokenize("```yaml", TokenState(), tokens);
    const TokenState block=markdown().tokenize("a: |", fence, tokens);

    CHECK(fence.words[0] != 0);
    CHECK(fence.words[1] == 0);
    CHECK(block.words[0] == fence.words[0]);
    CHECK(block.words[1] == 1);
    CHECK(markdown().tokenize("```", block, tokens) == TokenState());
}

TEST_CASE("Markdown: after seeded random edits every line is what a fresh tokenize gives")
{
    std::string text;

    for(int i=0; i < 8; ++i)
    {
        text+="## Section "+std::to_string(i)+"\n"
            "Some *text* with `code` and a [link](x).\n"
            "```yaml\n"
            "key: |\n"
            "  body\n"
            "```\n"
            "- item\n"
            "<!-- note -->\n";
    }
    test::checkRandomEdits(markdown(), text,
        { "```\n", "```yaml\n", "```bash\n", "~~~", "<!--", "-->", "*", "**", "`", "# ", "\n", "key: |\n",
            "  x\n", "<<EOF\n", "EOF\n" });
}
