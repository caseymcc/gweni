/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The diff tokenizer (findLanguage("diff")) on a unified diff, the tokens
 *  written out by hand: the file headers Meta, a hunk header Hunk up to its
 *  second "@@", and in the hunk '-' lines Removed and '+' lines Added even
 *  when they look like "--- "/"+++ " headers, context lines and the
 *  "\ No newline" Comment; the hunk ends when its counts are used up, so a
 *  "--- " after it is a header again, and a commit message before the
 *  headers is Plain. The end state carries the hunk's counts. Then seeded
 *  random edits keep the cache's lines what a fresh tokenize gives.
 */
#include "highlightTesting.h"

#include <gweni/text/tokenizer.h>

using gweni::text::findLanguage;
using gweni::text::Tokenizer;
using gweni::text::TokenState;

namespace
{

const Tokenizer &diff()
{
    const Tokenizer *tokenizer=findLanguage("diff");

    REQUIRE(tokenizer != nullptr);
    return *tokenizer;
}

}//namespace

TEST_CASE("diff: headers, hunks, added and removed lines")
{
    test::checkLines(diff(),
        "Fix the thing\n"
        "diff --git a/f.txt b/f.txt\n"
        "index 1234..5678 100644\n"
        "--- a/f.txt\n"
        "+++ b/f.txt\n"
        "@@ -1,4 +1,4 @@ context\n"
        " same\n"
        "--- removed, looks like a header\n"
        "+++ added, looks like a header\n"
        "-old\n"
        "+new\n"
        "\n"
        "\\ No newline at end of file\n"
        "--- a/g.txt\n"
        "+++ b/g.txt\n"
        "@@ -0,0 +1 @@\n"
        "+only\n"
        "+not in the hunk",
        {
            "",
            "meta'diff --git a/f.txt b/f.txt'",
            "meta'index 1234..5678 100644'",
            "meta'--- a/f.txt'",
            "meta'+++ b/f.txt'",
            "hunk'@@ -1,4 +1,4 @@'",
            "",
            "removed'--- removed, looks like a header'",
            "added'+++ added, looks like a header'",
            "removed'-old'",
            "added'+new'",
            "",
            "comment'\\ No newline at end of file'",
            "meta'--- a/g.txt'",
            "meta'+++ b/g.txt'",
            "hunk'@@ -0,0 +1 @@'",
            "added'+only'",
            ""
        });

    std::vector<gweni::text::Token> tokens;
    const TokenState hunk=diff().tokenize("@@ -10,3 +12,5 @@ fn()", TokenState(), tokens);

    CHECK(hunk.words[0] == 1);
    CHECK(hunk.words[1] == 3);
    CHECK(hunk.words[2] == 5);

    const TokenState afterAdd=diff().tokenize("+x", hunk, tokens);

    CHECK(afterAdd.words[1] == 3);
    CHECK(afterAdd.words[2] == 4);
}

TEST_CASE("diff: after seeded random edits every line is what a fresh tokenize gives")
{
    std::string text="commit message\n";

    for(int i=0; i < 6; ++i)
    {
        text+="diff --git a/f"+std::to_string(i)+" b/f"+std::to_string(i)+"\n"
            "--- a/f"+std::to_string(i)+"\n"
            "+++ b/f"+std::to_string(i)+"\n"
            "@@ -1,3 +1,3 @@\n"
            " a\n"
            "-b\n"
            "+c\n"
            " d\n";
    }
    test::checkRandomEdits(diff(), text,
        { "@@ -1,2 +1,2 @@\n", "+", "-", " ", "\n", "--- x\n", "+++ y\n", "\\ No newline\n", "@@ -1 +1 @@", "x" });
}
