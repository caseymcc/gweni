/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The YAML tokenizer (findLanguage("yaml")) on reference snippets, the
 *  tokens written out by hand: document markers and directives, keys and
 *  their ':', plain scalars (numbers, the literals, the rest Plain), quoted
 *  ones, comments (and a '#' that is no comment), sequence dashes, flow
 *  collections with keys in them, anchors, aliases and tags, and block
 *  scalars: their lines Strings while indented past the node they belong to
 *  (a key's column, an entry's '-'), blank lines inside them, and the first
 *  line that is not indented so ending them. The end states: in a block
 *  scalar and out of it. Then seeded random edits keep the cache's lines what
 *  a fresh tokenize gives.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

using gweni::text::findLanguage;
using gweni::text::Tokenizer;
using gweni::text::TokenState;

namespace
{

const Tokenizer &yaml()
{
    const Tokenizer *tokenizer=findLanguage("yaml");

    REQUIRE(tokenizer != nullptr);
    return *tokenizer;
}

}//namespace

TEST_CASE("YAML: scalars, keys, comments, flow collections, anchors and tags")
{
    test::checkLines(yaml(),
        "--- # doc\n"
        "%YAML 1.2\n"
        "key: value\n"
        "name: \"quoted # no comment\"\n"
        "single: 'it''s'\n"
        "count: 42\n"
        "ratio: -1.5e3\n"
        "hex: 0x1F\n"
        "big: 1_000\n"
        "on: yes\n"
        "nothing: ~\n"
        "pi: .inf\n"
        "version: 1.2.3\n"
        "list:\n"
        "  - item\n"
        "  - &anchor {a: 1, b: [x, 'y']}\n"
        "  - *anchor\n"
        "tag: !!str 123\n"
        "# a comment\n"
        "url: http://x.y/z # trailing\n"
        "a#b: c#d\n"
        "\"quoted key\": v\n"
        "...",
        {
            "meta'---' comment'# doc'",
            "meta'%YAML 1.2'",
            "key'key' punctuation':'",
            "key'name' punctuation':' string'\"quoted # no comment\"'",
            "key'single' punctuation':' string''it''s''",
            "key'count' punctuation':' number'42'",
            "key'ratio' punctuation':' number'-1.5e3'",
            "key'hex' punctuation':' number'0x1F'",
            "key'big' punctuation':' number'1_000'",
            "key'on' punctuation':' keyword'yes'",
            "key'nothing' punctuation':' keyword'~'",
            "key'pi' punctuation':' number'.inf'",
            "key'version' punctuation':'",
            "key'list' punctuation':'",
            "punctuation'-'",
            "punctuation'-' meta'&anchor' punctuation'{' key'a' punctuation':' number'1' punctuation',' "
                "key'b' punctuation':' punctuation'[' punctuation',' string''y'' punctuation']' punctuation'}'",
            "punctuation'-' meta'*anchor'",
            "key'tag' punctuation':' meta'!!str' number'123'",
            "comment'# a comment'",
            "key'url' punctuation':' comment'# trailing'",
            "key'a#b' punctuation':'",
            "key'\"quoted key\"' punctuation':'",
            "meta'...'"
        });
}

TEST_CASE("YAML: a block scalar's lines are a String while indented past its node")
{
    test::checkLines(yaml(),
        "text: |\n"
        "  line one\n"
        "  key: not a key\n"
        "\n"
        "  # not a comment\n"
        "after: >- # folded\n"
        "    folded\n"
        "- seq: |2\n"
        "   deep\n"
        "  not deep enough\n"
        "outer:\n"
        "  inner: |+\n"
        "    kept\n"
        "  next: 1\n"
        "- |\n"
        " entry\n"
        "last: x",
        {
            "key'text' punctuation':' punctuation'|'",
            "string'line one'",
            "string'key: not a key'",
            "",
            "string'# not a comment'",
            "key'after' punctuation':' punctuation'>-' comment'# folded'",
            "string'folded'",
            "punctuation'-' key'seq' punctuation':' punctuation'|2'",
            "string'deep'",
            "",
            "key'outer' punctuation':'",
            "key'inner' punctuation':' punctuation'|+'",
            "string'kept'",
            "key'next' punctuation':' number'1'",
            "punctuation'-' punctuation'|'",
            "string'entry'",
            "key'last' punctuation':'"
        });

    // the state: the block and the column its lines are indented past
    std::vector<gweni::text::Token> tokens;
    const TokenState open=yaml().tokenize("  inner: |", TokenState(), tokens);

    CHECK(open.words[0] == 1);
    CHECK(open.words[1] == 2);
    CHECK(yaml().tokenize("    kept", open, tokens) == open);
    CHECK(yaml().tokenize("", open, tokens) == open);
    CHECK(yaml().tokenize("  next: 1", open, tokens) == TokenState());
    // a '|' that is not the end of the value opens nothing
    CHECK(yaml().tokenize("a: | b", TokenState(), tokens) == TokenState());
}

TEST_CASE("YAML: after seeded random edits every line is what a fresh tokenize gives")
{
    std::string text;

    for(int i=0; i < 12; ++i)
    {
        text+="item"+std::to_string(i)+":\n"
            "  name: \"value "+std::to_string(i)+"\" # note\n"
            "  body: |\n"
            "    line a\n"
            "    line b\n"
            "  list: [1, two, &a 3]\n";
    }
    test::checkRandomEdits(yaml(), text,
        { "key: |\n", "  ", "- ", "#", "\"", "'", "---\n", "\n", "x: 1\n", ": ", "  text\n", ">-", "{", "]" });
}
