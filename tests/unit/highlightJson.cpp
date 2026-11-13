/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The JSON tokenizer (findLanguage("json")) on reference snippets, the tokens
 *  written out by hand: keys (a string followed by ':') and other strings,
 *  escapes in them, numbers, true/false/null, the punctuation, and JSONC's
 *  comments: // to the line's end, and block comments within a line and over
 *  lines, whose end state is the open comment. Then seeded random edits keep the
 *  cache's lines what a fresh tokenize gives.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

using gweni::text::findLanguage;
using gweni::text::Tokenizer;
using gweni::text::TokenState;

namespace
{

const Tokenizer &json()
{
    const Tokenizer *tokenizer=findLanguage("json");

    REQUIRE(tokenizer != nullptr);
    return *tokenizer;
}

}//namespace

TEST_CASE("JSON: keys, strings, numbers, literals, punctuation and comments")
{
    test::checkLines(json(),
        "{\n"
        "  \"name\": \"gweni\", // trailing\n"
        "  \"version\": 1.5,\n"
        "  \"tags\": [\"a\", \"b \\\"q\\\" c\"],\n"
        "  \"ok\": true, \"none\": null, \"no\": false,\n"
        "  /* a block\n"
        "     comment */ \"n\": -2e10,\n"
        "  \"spaced\" : /* inline */ 0\n"
        "}",
        {
            "punctuation'{'",
            "key'\"name\"' punctuation':' string'\"gweni\"' punctuation',' comment'// trailing'",
            "key'\"version\"' punctuation':' number'1.5' punctuation','",
            "key'\"tags\"' punctuation':' punctuation'[' string'\"a\"' punctuation',' string'\"b \\\"q\\\" c\"' "
                "punctuation']' punctuation','",
            "key'\"ok\"' punctuation':' keyword'true' punctuation',' key'\"none\"' punctuation':' keyword'null' "
                "punctuation',' key'\"no\"' punctuation':' keyword'false' punctuation','",
            "comment'/* a block'",
            "comment'comment */' key'\"n\"' punctuation':' number'-2e10' punctuation','",
            "key'\"spaced\"' punctuation':' comment'/* inline */' number'0'",
            "punctuation'}'"
        });

    std::vector<gweni::text::Token> tokens;
    const TokenState open=json().tokenize("/* open", TokenState(), tokens);

    CHECK(open != TokenState());
    CHECK(json().tokenize("still", open, tokens) == open);
    CHECK(json().tokenize("done */", open, tokens) == TokenState());
}

TEST_CASE("JSON: after seeded random edits every line is what a fresh tokenize gives")
{
    std::string text="{\n";

    for(int i=0; i < 40; ++i)
        text+="  \"key"+std::to_string(i)+"\": [1, \"two\", true], /* c */\n";
    text+="  \"end\": null\n}";
    test::checkRandomEdits(json(), text,
        { "/*", "*/", "\"", "//", ": ", "\n", "{\"a\": 1}", "[", "null", "-3.5", "/* x\ny */" });
}
