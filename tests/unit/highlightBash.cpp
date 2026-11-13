/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The bash tokenizer (findLanguage("bash")) on reference snippets, the tokens
 *  written out by hand: comments (and a '#' inside a word, which is none),
 *  keywords as whole words, an assignment's name, numbers, '...' and "..."
 *  strings with the $variables cut out of the "..." ones, $name, ${...}, $1,
 *  $?, $( and the operators; strings that run over lines; heredocs (<<EOF and
 *  <<-'EOF'), whose body is a String up to the line that is the delimiter
 *  alone, after leading tabs for <<-, and not ended by a line that only holds
 *  the delimiter among other text, and a << inside $(( )) or (( )), which
 *  is a shift, opens none. Then seeded random edits keep the cache's
 *  lines what a fresh tokenize gives.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

using gweni::text::findLanguage;
using gweni::text::Tokenizer;
using gweni::text::TokenState;

namespace
{

const Tokenizer &bash()
{
    const Tokenizer *tokenizer=findLanguage("bash");

    REQUIRE(tokenizer != nullptr);
    return *tokenizer;
}

}//namespace

TEST_CASE("bash: comments, keywords, assignments, strings, variables and operators")
{
    test::checkLines(bash(),
        "#!/bin/bash\n"
        "# comment\n"
        "NAME=\"world $USER\" # greet\n"
        "if [[ -n \"$1\" ]]; then\n"
        "  echo 'it''s' ${HOME}/x $? 42 a#b\n"
        "fi\n"
        "out=$(ls | wc -l) && exit 0\n"
        "for f in *.txt; do echo $f; done\n"
        "echo \"multi\n"
        "line $x end\" done\n"
        "echo 'one\n"
        "two' three",
        {
            "comment'#!/bin/bash'",
            "comment'# comment'",
            "key'NAME' punctuation'=' string'\"world ' meta'$USER' string'\"' comment'# greet'",
            "keyword'if' punctuation'[[' string'\"' meta'$1' string'\"' punctuation']]' punctuation';' keyword'then'",
            "string''it'' string''s'' meta'${HOME}' meta'$?' number'42'",
            "keyword'fi'",
            "key'out' punctuation'=' punctuation'$(' punctuation'|' punctuation')' punctuation'&&' keyword'exit' "
                "number'0'",
            "keyword'for' keyword'in' punctuation';' keyword'do' meta'$f' punctuation';' keyword'done'",
            "string'\"multi'",
            "string'line ' meta'$x' string' end\"' keyword'done'",
            "string''one'",
            "string'two''"
        });
}

TEST_CASE("bash: a heredoc's body is a String up to its delimiter alone on a line")
{
    test::checkLines(bash(),
        "cat <<EOF > out.txt\n"
        "body $NOT_A_VAR\n"
        "  EOF\n"
        "EOF\n"
        "cat <<-'END'\n"
        "\tindented\n"
        "\tEND\n"
        "echo after",
        {
            "punctuation'<<' meta'EOF' punctuation'>'",
            "string'body $NOT_A_VAR'",
            "string'  EOF'",
            "meta'EOF'",
            "punctuation'<<-' meta''END''",
            "string'\tindented'",
            "meta'END'",
            ""
        });

    std::vector<gweni::text::Token> tokens;
    const TokenState heredoc=bash().tokenize("cat <<EOF", TokenState(), tokens);

    CHECK(heredoc != TokenState());
    CHECK(bash().tokenize("EOFX", heredoc, tokens) == heredoc);
    CHECK(bash().tokenize("EOF", heredoc, tokens) == TokenState());
}

TEST_CASE("bash: a << inside $(( )) or (( )) is a shift, not a heredoc")
{
    test::checkLines(bash(),
        "x=$((1 << 4))\n"
        "echo \"$x\"\n"
        "(( y = x<<2 ))\n"
        "cat <<EOF\n"
        "body\n"
        "EOF",
        {
            "key'x' punctuation'=' punctuation'$(' punctuation'(' number'1' punctuation'<' punctuation'<' "
                "number'4' punctuation')' punctuation')'",
            "string'\"' meta'$x' string'\"'",
            "punctuation'(' punctuation'(' punctuation'<' punctuation'<' number'2' punctuation')' punctuation')'",
            "punctuation'<<' meta'EOF'",
            "string'body'",
            "meta'EOF'"
        });

    std::vector<gweni::text::Token> tokens;

    CHECK(bash().tokenize("x=$((1 << 4))", TokenState(), tokens) == TokenState());
}

TEST_CASE("bash: after seeded random edits every line is what a fresh tokenize gives")
{
    std::string text="#!/bin/sh\n";

    for(int i=0; i < 10; ++i)
    {
        text+="if [ -f \"$FILE"+std::to_string(i)+"\" ]; then\n"
            "  cat <<EOF\n"
            "  text "+std::to_string(i)+"\n"
            "EOF\n"
            "  echo 'done' # note\n"
            "fi\n";
    }
    test::checkRandomEdits(bash(), text,
        { "<<EOF\n", "EOF\n", "'", "\"", "#", "$x", "${y}", "\n", "<<-X\n\tX\n", "if ", "done", "$((1 << 4))", "((", "))" });
}
