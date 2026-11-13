/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The Jinja overlay over Markdown (findLanguage("markdown+jinja")) on
 *  reference snippets, the tokens written out by hand: {{ }} and {% %} tags
 *  are Template, a {% %}'s statement name (after its whitespace control '-')
 *  a Keyword and a quoted string in a tag a String, whose "}}" closes
 *  nothing; {# #} is a Comment; the Markdown around a tag reads as without
 *  it (the heading holding it, the ** or ` around it, cut at the tag); tags
 *  and comments run over lines, the state carrying them, and a line wholly
 *  inside one leaves the Markdown state as it was (a fence goes on past a
 *  comment line, and a YAML block scalar in it past one at column 0); a tag
 *  inside a fenced YAML block cuts the YAML's tokens. Then seeded random
 *  edits keep the cache's lines what a fresh tokenize gives.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

using gweni::text::findLanguage;
using gweni::text::Tokenizer;
using gweni::text::TokenState;

namespace
{

const Tokenizer &jinja()
{
    const Tokenizer *tokenizer=findLanguage("markdown+jinja");

    REQUIRE(tokenizer != nullptr);
    return *tokenizer;
}

}//namespace

TEST_CASE("Jinja over Markdown: tags cut out of the Markdown around them")
{
    test::checkLines(jinja(),
        "# {{ title }}\n"
        "Hello **{{ name }}**, see `{{ code }}`.\n"
        "{% if user.admin %}\n"
        "- item {{ \"quoted }}\" }}\n"
        "{% endif %}\n"
        "{# a comment\n"
        "\n"
        "   over lines #} after *em*\n"
        "{%- for x in items -%}\n"
        "{{ x\n"
        "  | upper }}\n"
        "plain { not a tag }",
        {
            "heading'# ' template'{{ title }}'",
            "strong'**' template'{{ name }}' strong'**' code'`' template'{{ code }}' code'`'",
            "template'{% ' keyword'if' template' user.admin %}'",
            "punctuation'-' template'{{ ' string'\"quoted }}\"' template' }}'",
            "template'{% ' keyword'endif' template' %}'",
            "comment'{# a comment'",
            "",
            "comment'   over lines #}' emphasis'*em*'",
            "template'{%- ' keyword'for' template' x in items -%}'",
            "template'{{ x'",
            "template'  | upper }}'",
            ""
        });
}

TEST_CASE("Jinja over Markdown: a fence goes on through tags and comment lines")
{
    test::checkLines(jinja(),
        "```yaml\n"
        "key: {{ value }} # c\n"
        "{# a note\n"
        "```\n"
        "#}\n"
        "other: 1\n"
        "text: |\n"
        "{# not the end of the block #}\n"
        "  still the block\n"
        "```",
        {
            "punctuation'```' meta'yaml'",
            "key'key' punctuation':' template'{{ value }}' comment'# c'",
            "comment'{# a note'",
            "comment'```'",
            "comment'#}'",
            "key'other' punctuation':' number'1'",
            "key'text' punctuation':' punctuation'|'",
            "comment'{# not the end of the block #}'",
            "string'still the block'",
            "punctuation'```'"
        });

    // the open comment in word 0, the Markdown's fence after it
    std::vector<gweni::text::Token> tokens;
    const TokenState fence=jinja().tokenize("```yaml", jinja().initialState(), tokens);
    const TokenState comment=jinja().tokenize("{# open", fence, tokens);

    CHECK(fence.words[0] == 0);
    CHECK(fence.words[1] != 0);
    CHECK(comment.words[0] == 3);
    CHECK(comment.inner() == fence.inner());
    CHECK(jinja().tokenize("still", comment, tokens) == comment);
}

TEST_CASE("Jinja over Markdown: after seeded random edits every line is what a fresh tokenize gives")
{
    std::string text;

    for(int i=0; i < 8; ++i)
    {
        text+="# {{ title"+std::to_string(i)+" }}\n"
            "{% for x in items %}\n"
            "- **{{ x.name }}**: {{ x.value | default(\"}}\") }}\n"
            "{% endfor %}\n"
            "{# note\n"
            "   over lines #}\n"
            "```yaml\n"
            "key: {{ v }}\n"
            "```\n";
    }
    test::checkRandomEdits(jinja(), text,
        { "{{", "}}", "{%", "%}", "{#", "#}", "\"", "```\n", "```yaml\n", "\n", "**", "`", "{# x\ny #}" });
}
