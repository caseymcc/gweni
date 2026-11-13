/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Every built-in language (languageNames()) on seeded random lines of the
 *  bytes its syntax turns on, quotes, brackets, '#', '$', '<<', '@@' and
 *  multi-byte UTF-8 among them: each line's tokens are well formed
 *  (ascending, not overlapping, not empty, inside the line) whatever state it
 *  starts in, and tokenizing a line twice from one state gives the same
 *  tokens and end state (a tokenizer keeps nothing between calls). The
 *  registry holds yaml, json, diff and bash, the languages file names map to.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

#include <algorithm>

using namespace gweni::text;

TEST_CASE("Languages: the built-ins are registered")
{
    const std::vector<gweni::String> names=languageNames();

    for(const char *name:{ "yaml", "json", "diff", "bash" })
    {
        CAPTURE(name);
        CHECK(std::find(names.begin(), names.end(), name) != names.end());
        CHECK(findLanguage(name) != nullptr);
    }
    CHECK(findLanguage(languageForFileName("a.yml")) == findLanguage("yaml"));
    CHECK(findLanguage(languageForFileName("a.patch")) == findLanguage("diff"));
}

TEST_CASE("Languages: random lines give well-formed tokens, the same from the same state")
{
    const char *const pieces[]=
    {
        "\"", "'", "`", "#", "$", "${", "}", "{", "[", "]", "(", ")", ":", ": ", "- ", "|", ">", "<<", "<<-",
        "EOF", "@@ -1,2 +1,2 @@", "+", "-", " ", "  ", "\t", "\\", "/*", "*/", "//", "&a", "*a", "!t", "---",
        "key", "if", "then", "done", "true", "12", "-3.5e2", "0x1f", "\xc3\xa9", "\xe2\x9a\xa1", "\xff", "="
    };

    for(const gweni::String &name:languageNames())
    {
        const Tokenizer *tokenizer=findLanguage(name);

        CAPTURE(name);
        REQUIRE(tokenizer != nullptr);

        std::mt19937 random(7);
        TokenState state=tokenizer->initialState();
        bool wellFormed=true, repeatable=true;

        for(int line=0; line < 3000; ++line)
        {
            gweni::String text;
            const size_t count=random()%12;

            for(size_t i=0; i < count; ++i)
                text+=pieces[random()%(sizeof(pieces)/sizeof(pieces[0]))];

            std::vector<Token> tokens, again;
            const TokenState end=tokenizer->tokenize(text, state, tokens);
            const TokenState endAgain=tokenizer->tokenize(text, state, again);

            wellFormed=wellFormed && test::wellFormed(text, tokens);
            repeatable=repeatable && (tokens == again) && (end == endAgain);
            state=end;
        }
        CHECK(wellFormed);
        CHECK(repeatable);
    }
}
